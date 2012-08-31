//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/p2p/P2PDownloader.h"
#include "p2sp/p2p/Exchanger.h"
#include "p2sp/p2p/IpPool.h"
#include "p2sp/p2p/PeerConnector.h"
#include "p2sp/download/DownloadDriver.h"
#include "p2sp/p2p/P2PModule.h"

#include "MainThread.h"
#include "storage/Storage.h"
#include "storage/Instance.h"
#include "statistic/StatisticModule.h"
#include "p2sp/tracker/TrackerModule.h"
#include "random.h"

#include "p2sp/p2p/SNConnection.h"

#include <framework/network/Endpoint.h>

namespace p2sp
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_p2p_downloader = log4cplus::Logger::getInstance("[p2p_downloader]");
#endif

    const boost::uint32_t VIP_URGENT_TIME_IN_SECOND = 15*1000;

    P2PDownloader::P2PDownloader(const RID& rid, boost::uint32_t vip_level)
        : rid_(rid)
        , is_connected_(false)
        , start_time_counter_(0)
        , is_p2p_pausing_(true)
        , data_rate_(0)
        , active_peer_count_(0)
        , non_consistent_size_(0)
        , is_openservice_(false)
        , dl_mode_(NORMAL_MODE)
        , once_timer_(global_second_timer(), P2SPConfigs::P2P_DOWNLOAD_CLOSE_TIME_IN_MILLISEC,
            boost::bind(&P2PDownloader::OnTimerElapsed, this, &once_timer_))
        , download_speed_limiter_(600, 2500), download_priority_(protocol::RequestSubPiecePacket::DEFAULT_PRIORITY)
        , dolist_count_(0)
        , last_dolist_time_(false)
        , downloading_time_in_seconds_(0)
        , seconds_elapsed_until_connection_full_(0)
        , is_connect_full_(false)
        , is_sn_enable_(false)
        , vip_level_(vip_level)
        , connected_available_block_peer_count_(0)
    {
    }

    P2PDownloader::~P2PDownloader()
    {
        TrackerModule::Inst()->DeleteRidRecord(GetRid());
    }

    void P2PDownloader::Start()
    {
        if (is_running_ == true)
            return;
        is_running_ = true;

        LOG4CPLUS_INFO_LOG(logger_p2p_downloader, "P2PDownloader Start" << shared_from_this());

        // init
        is_p2p_pausing_ = true;
        active_peer_count_ = 0;

        // download speed limiter
        // download_speed_limiter_ = DownloadSpeedLimiter::Create(600, 2500);
        // download_speed_limiter_->Start();

        p2p_max_connect_count_ = BootStrapGeneralConfig::Inst()->GetMaxPeerConnectionCount();
        p2p_min_connect_count_ = BootStrapGeneralConfig::Inst()->GetMinPeerConnectionCount();

        instance_ = boost::static_pointer_cast<storage::Instance>(storage::Storage::Inst()->GetInstanceByRID(rid_));
        assert(instance_);
        if (!instance_)
            return;
        block_size_ = instance_->GetBlockSize();
        LOG4CPLUS_DEBUG_LOG(logger_p2p_downloader, "P2PDownloader::Start RID = " << rid_ << 
            " instance = " << instance_);

        statistic_ = statistic::StatisticModule::Inst()->AttachP2PDownloaderStatistic(this->GetRid());
        assert(statistic_);
        statistic_->SetFileLength(instance_->GetFileLength());
        statistic_->SetBlockNum(instance_->GetBlockCount());
        statistic_->SetBlockSize(block_size_);

        ippool_ = IpPool::create(BootStrapGeneralConfig::Inst()->GetDesirableVodIpPoolSize());
        ippool_->Start();

        exchanger_ = Exchanger::create(shared_from_this(), ippool_, false);
        exchanger_->Start();

        connector_ = PeerConnector::create(shared_from_this(), ippool_);
        connector_->Start();

        assigner_ = Assigner::create(shared_from_this());
        assigner_->Start();

        subpiece_request_manager_.Start(shared_from_this());

        data_rate_ = 0;

        connected_full_block_peer_count_ = 0;

        is_connected_ = false;
        can_connect_ = false;
        start_time_counter_.reset();
    }

    void P2PDownloader::Stop()
    {
        if (is_running_ == false)
        {
            return;
        }

       LOG4CPLUS_INFO_LOG(logger_p2p_downloader, "P2PDownloader Stop" << shared_from_this());

        // 首先停止所有的 Peer
        std::map<boost::asio::ip::udp::endpoint, ConnectionBasePointer>::iterator iter;

        for (iter = peers_.begin(); iter != peers_.end(); iter++)
        {
            iter->second->Stop();
        }

        peers_.clear();

        for (iter = sn_.begin(); iter != sn_.end(); iter++)
        {
            iter->second->Stop();
        }

        sn_.clear();

        //

        assert(exchanger_);
        if (exchanger_)
        {
            exchanger_->Stop();
            exchanger_.reset();
        }

        assert(ippool_);
        if (ippool_)
        {
            ippool_->Stop();
            ippool_.reset();
        }

        assert(connector_);
        if (connector_)
        {
            connector_->Stop();
            connector_.reset();
        }
        // 停止 Assigner
        assert(assigner_);
        if (assigner_)
        {
            assigner_->Stop();
            assigner_.reset();
        }
        // 停止 SubPieceRequestManager
        subpiece_request_manager_.Stop();

        // 释放downloader_drivers的引用
        download_driver_s_.clear();
        // 清除piece_tasks
        piece_tasks_.clear();

        once_timer_.stop();

        // 限速模块停止
        download_speed_limiter_.Stop();

        // 取消所有的 Statistic 关联
        assert(statistic_);
        if (statistic_)
        {
#ifdef BOOST_WINDOWS_API
            if (MainThread::IsRunning())
            {
                uint32_t this_thread_id = ::GetCurrentThreadId();
                uint32_t main_thread_id = MainThread::GetThreadId();
                if (this_thread_id != main_thread_id)
                {
                    int random = Random::GetGlobal().Next();
                    if (random % 500 == 11)
                    {
                        // 千分之二的概率崩溃掉
                        int * a = NULL;
                        *a = 0;
                    }
                }
            }
#endif
            statistic::StatisticModule::Inst()->DetachP2PDownloaderStatistic(statistic_);
            statistic_.reset();
        }

        is_running_ = false;
    }

    bool P2PDownloader::IsPlayByRID()
    {
        if (false == is_running_)
            return false;
        std::set<DownloadDriver::p>::const_iterator it;
        for (it = download_driver_s_.begin(); it != download_driver_s_.end(); ++it)
        {
            DownloadDriver::p dd = *it;
            if (dd && false == dd->IsPlayByRID())
                return false;
        }
        return true;
    }

    void P2PDownloader::PutPieceTask(const std::deque<protocol::PieceInfoEx> & piece_info_ex_s, DownloadDriver__p downloader_driver)
    {
        if (!is_running_)
        {
            return;
        }

        if (piece_info_ex_s.size() != 1)
        {
            assert(false);
            LOG4CPLUS_DEBUG_LOG(logger_p2p_downloader, "This is a bug in PutPieceTask");
        }

        protocol::PieceInfoEx piece_info_ex = piece_info_ex_s.front();

        LOG4CPLUS_INFO_LOG(logger_p2p_downloader, "P2PDownloader::PutPieceTask " << shared_from_this() << " " << 
            *(download_driver_s_.begin()) << " PieceInfoEx " << piece_info_ex);
        LOG4CPLUS_DEBUG_LOG(logger_p2p_downloader, __FUNCTION__ << ":" << __LINE__ << " " << 
            downloader_driver->GetDownloadDriverID() << " " << piece_info_ex);

        // 如果 piece_tasks_ mutilmap 里面存在 (piece_info_ex, downloader_driver) 二元组
        //    ppassert() 并且不管 return

        // 已经分配过(piece_info_ex, download_driver)，就不再分配
        std::multimap<protocol::PieceInfoEx, DownloadDriver__p>::iterator iter;
        iter = piece_tasks_.find(piece_info_ex);
        while (iter != piece_tasks_.end() && iter->first == piece_info_ex.GetPieceInfo())
        {
            if (iter->second == downloader_driver)
            {
                // SubPieceInfo s_p;
                // s_p.block_index_ = piece_info_ex.block_index_;
                // s_p.subpiece_index_ = piece_info_ex.piece_index_ * 128 + piece_info_ex.subpiece_index_;
                // bool t;
                // t = instance_->HasSubPiece(s_p);
                // assert(0);
                return;
            }
            iter ++;
        }

        // 否则 piece_tasks_ mutilmap 里面添加 (piece_info_ex, downloader_driver) 二元组

        piece_tasks_.insert(std::make_pair(piece_info_ex, downloader_driver));
    }

    bool P2PDownloader::IsConnected()
    {
         // 从 Downloader 继承下来的
        if (is_running_ == false) return false;

        // 如果PeerConnection 的个数大于4个 或者 (PeerConncetion 的个数大于0 并且 存在时间已经有8s)
        //     则 返回true
        // 否则
        //     返回 false

//        if (peers_.size() > 4 || peers_.size() > 0 && start_time_counter_.GetElapsed() > 2 * 1000)
        if (can_connect_) return true;

        uint32_t elapsed = start_time_counter_.elapsed();

        if (peers_.size() >= 1)
        {
            if (IsPlayByRID())
            {
                return can_connect_ = true;
            }
            else if (elapsed > 1000)
            {
                return can_connect_ = true;
            }
        }

        LOG4CPLUS_INFO_LOG(logger_p2p_downloader, __FUNCTION__ << shared_from_this() << " elapsed: " << elapsed << 
            " peer.size() = " << peers_.size() << " return false");
        return can_connect_ = false;
    }

    bool P2PDownloader::GetUrlInfo(protocol::UrlInfo& url_info)
    {
        // 从 Downloader 继承下来的
        if (is_running_ == false) return false;

        // 返回一个 空的 url_info ("p2s://", "")
        url_info.url_ = "p2p";
        return false;
    }

    bool P2PDownloader::CanDownloadPiece(const protocol::PieceInfo& piece_info)
    {
        if (is_running_ == false) return false;

        // 看自己的所有的PeerConnection里面的BlockMap 是否 有一个存在这个Piece
        //    如果有一个存在，则返回true
        // 否则 都没有 返回false

        std::map<boost::asio::ip::udp::endpoint, ConnectionBasePointer> ::iterator iter = peers_.begin();

        for (; iter != peers_.end(); ++iter)
        {
            ConnectionBasePointer peer = iter->second;
            // Block是传播单元
            if (peer->HasBlock(piece_info.block_index_))
            {
                LOG4CPLUS_INFO_LOG(logger_p2p_downloader, "P2PDownloader::CanDownloadPiece true" << peer);
                return true;
            }
        }
        LOG4CPLUS_INFO_LOG(logger_p2p_downloader, "P2PDownloader::CanDownloadPiece false");
        return false;
    }

    void P2PDownloader::AttachDownloadDriver(DownloadDriver::p download_driver)
    {
        if (is_running_ == false)
            return;
        // 如果 本地的 download_drivers_ 里面已经 包含有这个 download_driver
        //      不管，直接返回
        //
        // download_drivers_->insert(download_driver);

        LOG4CPLUS_INFO_LOG(logger_p2p_downloader, "P2PDownloader::AttachDownloadDriver " << download_driver);

        if (download_driver_s_.find(download_driver) == download_driver_s_.end())
        {
            download_driver_s_.insert(download_driver);

            if (IsConnected() && !is_p2p_pausing_)
            {
                download_driver->RequestNextPiece(shared_from_this());
            }
        }

        file_name_ = download_driver->GetOpenServiceFileName();

        once_timer_.stop();
        InitPeerConnection();
    }

    void P2PDownloader::DettachDownloadDriver(DownloadDriver::p download_driver)
    {
        if (is_running_ == false) return;

        // 遍历DownloadDriver

        LOG4CPLUS_INFO_LOG(logger_p2p_downloader, "P2PDownloader::DettachDownloadDriver " << download_driver);

        // 如果 本地的 download_drivers_ 里面已经 没有有这个 download_driver
        //      不管，直接返回
        //
        if (download_driver_s_.find(download_driver) == download_driver_s_.end())
            return;
        // download_drivers_->erase(download_driver);
        //
        download_driver_s_.erase(download_driver);

        // 遍历 piece_tasks_ 里面所有的元素
        //        如果 iter->second == download_driver, 则这个 iter 要被erase调

        std::multimap<protocol::PieceInfoEx, DownloadDriver__p> ::iterator iter = piece_tasks_.begin();
        for (;iter != piece_tasks_.end();)
        {
            if (iter->second == download_driver)
            {
                piece_tasks_.erase(iter++);
            }
            else
                iter++;
        }
/*
        iter = piece_tasks_.begin();
        std::map<protocol::PieceInfo, std::bitset<storage::subpiece_num_per_piece_g_> >::iterator miter = piece_bitmaps.begin();
        for (; miter != piece_bitmaps.end(), iter != piece_tasks_.end();)
        {
            // if ()
            if (miter->first < iter->first.GetPieceInfo())
            {
                piece_bitmaps.erase(miter++);
            }
            else if (miter->first == iter->first.GetPieceInfo())
            {
                miter++; iter++;
            }
            else
            {
                iter++;
            }
        }

        if (iter == piece_tasks_.end())
        {
            piece_bitmaps.erase(miter, piece_bitmaps.end());
        }
*/

        if (download_driver_s_.size() == 0)
        {
            once_timer_.start();
            assert(statistic_);
            if (statistic_)
            {
                statistic_->ClearP2PDataBytes();
            }
        }
    }

    void P2PDownloader::OnTimerElapsed(framework::timer::Timer * pointer)
    {
        if (false == is_running_)
            return;

        if (pointer == &once_timer_)
        {
            Stop();

            //P2PModule::OnP2PDownloaderWillStop会去掉这个P2PDownloader，而那很可能是最后一个指向本downloader的shared_ptr。
            //也就是说调完OnP2PDownloaderWillStop后这个p2pdownloader很可能已经被析构了，我们务必不要在这之后继续访问p2pdownloader的任何成员。
            P2PModule::Inst()->OnP2PDownloaderWillStop(shared_from_this());
        }
    }

    void P2PDownloader::InitPeerConnection()
    {
        if (is_running_ == false) return;

        // 策略函数： 决定发起连接的个数
        //    当前要发起的连接数 = 最大连接数 - 已连接数 - 正在连接数
        //    当前要发起的连接数 最大值控制到 每秒最大发起连接数;
        // for (int i = 0; i < 当前要发起的连接数; i ++)
        // {
        //     endpoint = 调用 IpPool->GetForConnet
        //        如果调用失败，break
        //     Connector->Connect(endpoint);
        // }

        if (!connector_)
        {
            LOG4CPLUS_ERROR_LOG(logger_p2p_downloader, "P2PDownloader::InitPeerConnection, connector_ = " 
                << connector_);
            return;
        }

        if (true == assigner_->IsEndOfAssign() && peers_.size() >= 5) {
            LOG4CPLUS_DEBUG_LOG(logger_p2p_downloader, "End of assign, Do not connect peers");
            return;
        }

        if (!NeedConnectNewConnection())
        {
            statistic_->SubmitConnectCount(0);
            return;
        }

        boost::int32_t connect_count;
        if (IsDownloadInitialization())
        {
            connect_count = P2SPConfigs::P2P_DOWNLOAD_INIT_MAX_CONNECT_COUNT_PER_SEC;
        }
        else
        {
            connect_count = (p2p_max_connect_count_ - peers_.size()) * 2 - connector_->GetConnectingPeerCount();
            if (connect_count < 0)
                connect_count = 0;

            // 保证在连接不满时至少连一个节点
            if (p2p_max_connect_count_ > peers_.size())
            {
                LIMIT_MIN(connect_count, 1);
            }

            LIMIT_MAX(connect_count, P2SPConfigs::P2P_DOWNLOAD_MAX_CONNECT_COUNT_PER_SEC);
        }

        // 往共享内存写入每秒发起的连接数
        statistic_->SubmitConnectCount(connect_count);

        // !
        /*
        if (peers_.size() < P2SPConfigs::P2P_DOWNLOAD_MAX_CONNECT_COUNT)
            connect_count = P2SPConfigs::P2P_DOWNLOAD_MAX_CONNECT_COUNT_PER_SEC;*/

        LOG4CPLUS_INFO_LOG(logger_p2p_downloader, "P2PDownloader::InitPeerConnection p2p_downloader = " << 
            shared_from_this() << ", connect_count=" << connect_count << ", connecting=" << 
            connector_->GetConnectingPeerCount() << ", peer_size=" << peers_.size());

        LOG4CPLUS_DEBUG_LOG(logger_p2p_downloader, "P2PDownloader: " << shared_from_this() << ", ConnectCount: " 
            << connect_count);
        for (uint32_t i = 0; i < connect_count; ++i)
        {
            protocol::CandidatePeerInfo candidate_peer_info;
            if (false == ippool_->GetForConnect(candidate_peer_info))
            {
                LOG4CPLUS_DEBUG_LOG(logger_p2p_downloader, "P2PDownloader: " << shared_from_this() << 
                    ", GetForConnect Failed!");
                break;
            }

            connector_->Connect(candidate_peer_info);
        }
    }

    void P2PDownloader::KickPeerConnection()
    {
        if (is_running_ == false) return;

        std::map<boost::asio::ip::udp::endpoint, ConnectionBasePointer> peer_connections;
        std::map<boost::asio::ip::udp::endpoint, ConnectionBasePointer>::iterator iter;
        for (iter = peers_.begin(); iter != peers_.end(); iter++)
        {
            if (!sn_pool_object_.IsSn(iter->first))
            {
                peer_connections.insert(std::make_pair(iter->first, iter->second));
            }
        }

        boost::int32_t kick_count = 0;
        std::multimap<uint32_t, ConnectionBasePointer> peer_kick_map;

        if (peer_connections.size() > p2p_max_connect_count_)
        {
            kick_count = peer_connections.size() - p2p_max_connect_count_;
            std::map<boost::asio::ip::udp::endpoint, ConnectionBasePointer>::iterator iter;
            for (iter = peer_connections.begin(); iter != peer_connections.end(); iter++)
            {
                boost::uint32_t peer_now_speed = iter->second->GetStatistic()->GetSpeedInfo().NowDownloadSpeed;
                peer_kick_map.insert(std::make_pair(peer_now_speed, iter->second));
            }
        }
        else if(peers_.size() > p2p_min_connect_count_)
        {
            kick_count = p2p_max_connect_count_ / 10;
            if ((int32_t)peers_.size() - kick_count < p2p_min_connect_count_)
            {
                kick_count = peers_.size() - p2p_min_connect_count_;
            }
            
            if (kick_count != 0)
            {
                std::map<boost::asio::ip::udp::endpoint, ConnectionBasePointer>::iterator iter;
                for (iter = peer_connections.begin(); iter != peer_connections.end(); iter++)
                {
                    ConnectionBasePointer peer = iter->second;
                    if (peer->CanKick() && peer->GetStatistic())
                    {
                        peer_kick_map.insert(std::make_pair(peer->GetStatistic()->GetSpeedInfo().NowDownloadSpeed, peer));
                    }
                }
            }
        }

        LOG4CPLUS_INFO_LOG(logger_p2p_downloader, "P2PDownloader::KickPeerConnection kick_count" << kick_count);

        // 往共享内存写入每秒踢掉的连接数
        statistic_->SubmitKickCount(std::min(kick_count, (int32_t)peer_kick_map.size()));

        std::multimap<uint32_t, ConnectionBasePointer>::iterator iter_kick = peer_kick_map.begin();
        for (boost::int32_t i = 0; i < kick_count && iter_kick != peer_kick_map.end(); ++i, ++iter_kick)
        {
            DelPeer(iter_kick->second);
        }
    }

    uint32_t P2PDownloader::CalcConnectedFullBlockPeerCount()
    {
        if (is_running_ == false)
            return 0;

        connected_full_block_peer_count_ = 0;
        for (std::map<boost::asio::ip::udp::endpoint, ConnectionBasePointer>::iterator iter = peers_.begin(); iter != peers_.end(); ++iter)
        {
            if (iter->second->IsBlockFull())
            {
                ++connected_full_block_peer_count_;
            }
        }
        return connected_full_block_peer_count_;
    }

    uint32_t P2PDownloader::CalcConnectedAvailableBlockPeerCount()
    {
        if (false == is_running_)
            return 0;

        connected_available_block_peer_count_ = 0;
        if (piece_tasks_.size() == 0)
            return 0;
        uint32_t block_index = (piece_tasks_.begin()->first).block_index_;
        for (std::map<boost::asio::ip::udp::endpoint, ConnectionBasePointer>::iterator iter = peers_.begin(); iter != peers_.end(); ++iter)
        {
            if (iter->second->HasBlock(block_index))
            {
                ++connected_available_block_peer_count_;
            }
        }
        return connected_available_block_peer_count_;
    }

    void P2PDownloader::OnP2PTimer(uint32_t times)
    {
        // P2PModule调用，250ms执行一次
        if (!is_running_)
        {
            return;
        }

        //目前还没查清楚为何会出现is_running为true而statistic_为空的情况，但如果不处理会有好一些crash。
        assert(statistic_);
        if (!statistic_)
        {
            return;
        }

        if (times % 4 == 0)
        {
            std::set<DownloadDriver::p>::const_iterator it;
            for (it = download_driver_s_.begin(); it != download_driver_s_.end(); ++it)
            {
                DownloadDriver::p dd = *it;
                if (dd->GetVipLevel() > vip_level_)
                {
                    vip_level_ = dd->GetVipLevel();
                }
            }
            if (!is_p2p_pausing_)
            {
                downloading_time_in_seconds_++;
            }

            if (!is_connect_full_)
            {
                if (peers_.size() >= p2p_max_connect_count_)
                {
                    is_connect_full_ = true;
                }
                else
                {
                    seconds_elapsed_until_connection_full_++;
                }
            }

            // 每秒
            // IPPool Timer
            assert(ippool_);
            // 设置共享内存
            statistic_->SetIpPoolPeerCount(ippool_ ? ippool_->GetPeerCount() : 0);

            // 点播的P2P没有定时去Exchange(但是在PeerConnection的Start的时候，还是会Exchange一次的)
            // 所以这里把Exchange代码删除，但是exchange_对象不能彻底拿掉
            DoList();

            // 每秒
            // 驱动一次发起连接和断开连接
            assert(connector_);
            if (connector_)
            {
                connector_->OnP2PTimer(times);
                statistic_->SetConnectingPeerCount(connector_->GetConnectingPeerCount());
            }
            else
            {
                return;
            }

            // 每秒计算一次距离
            if (piece_tasks_.size() > 0)
            {
                std::multimap<protocol::PieceInfoEx, DownloadDriver__p>::iterator first = piece_tasks_.begin();
                std::multimap<protocol::PieceInfoEx, DownloadDriver__p>::reverse_iterator last = piece_tasks_.rbegin();

                non_consistent_size_ = last->first - first->first;
                LOG4CPLUS_DEBUG_LOG(logger_p2p_downloader, "first piece: " << first->first << ", last piece: " << 
                    last->first << ", distance: " << non_consistent_size_);
                statistic_->SetNonConsistentSize(non_consistent_size_);
            }
            else
                statistic_->SetNonConsistentSize(0);
        }

        if (times % 4 == 0)
        {
            if (false == is_p2p_pausing_)
            {
                KickPeerConnection();

                KickSnConnection();
            }
            else
            {
                UpdateConnectTime();
            }

            // 连接节点
            InitPeerConnection();

            // calculate active peer count
            CalculateActivePeerCount();

            // 统计FullBlockPeerCount个数
            CalcConnectedFullBlockPeerCount();
            CalcConnectedAvailableBlockPeerCount();
            GetStatistic()->SetFullBlockPeerCount(connected_full_block_peer_count_);

            std::set<DownloadDriver__p>::iterator dditer;
            for (dditer = download_driver_s_.begin(); dditer != download_driver_s_.end(); dditer++)
            {
                // TODO(nightsuns): 以后解决这个空指针的问题
                if (NULL == (*dditer)->GetStatistic()) {
                    continue;
                }

                (*dditer)->GetStatistic()->SetQueriedPeerCount(ippool_->GetPeerCount());
                (*dditer)->GetStatistic()->SetConnectedPeerCount(GetConnectedPeersCount());
                (*dditer)->GetStatistic()->SetFullPeerCount(connected_full_block_peer_count_);
                (*dditer)->GetStatistic()->SetMaxActivePeerCount(active_peer_count_);
            }
        }

        if (is_p2p_pausing_)
        {
            LOG4CPLUS_INFO_LOG(logger_p2p_downloader, __FUNCTION__ << " P2P is Paused..");
            if (times % 3*4 == 0)
            {
                KeepConnectionAlive();
            }

            return;
        }

        subpiece_request_manager_.OnP2PTimer(times);

        // 驱动其他子模块定时器
        assert(assigner_);
        if (IsConnected())
        {
            // 检查piece的完成情况
            if (times % 2 == 0)
            {
                CheckPieceComplete();
            }
            assigner_->OnP2PTimer(times);
        }

        // 所有的PeerConnection
        std::map<boost::asio::ip::udp::endpoint, ConnectionBasePointer>::iterator iter;
        for (iter = peers_.begin(); iter != peers_.end();)
        {
            if (iter->second->LongTimeNoSee())
            {
                LOG4CPLUS_INFO_LOG(logger_p2p_downloader, "P2PDownloader::KickPeerBecourseLongTimeNoResponse " 
                    << iter->second);
                iter->second->Stop();
                peers_.erase(iter++);
            }
            else
            {
                iter->second->OnP2PTimer(times);
                iter++;
            }
        }

        if (times % (4*30) == 0 && ippool_)
        {
            ippool_->KickTrivialCandidatePeers();
        }

        // 判断 连接状态 是否改变
        // if (connected_ == IsConnected()
        // {    如果 连接状态 没有改变
        //
        // }
        // else
        // {    连接状态发生改变
        //
        //      如果从 true 到 false
        //      {
        //            1. 通知所有的 DownloadDriver -> OnPieceFailed
        //             具体 遍历 piece_tasks_ multimap 对每个找到的piece_info都调用 OnPieceFailed
        //             清空 piece_tasks_ multimap
        //
        //            2.
        //          is_connected_ = false;
        //      }
        //      如果从 false 到 true
        //      {
        //            is_connected_ = ture;
        //      }
        // }

        if (is_connected_ != IsConnected())
        {
            if (is_connected_ == true && IsConnected() == false)
            {
                std::multimap<protocol::PieceInfoEx, DownloadDriver__p>::iterator iter = piece_tasks_.begin();

                for (;iter != piece_tasks_.end(); iter++)
                {
                    protocol::PieceInfo piece_info = iter->first;
                    DownloadDriver::p downloader_driver = iter->second;
                    downloader_driver->OnPieceFaild(piece_info, shared_from_this());
                }

                piece_tasks_.clear();
                is_connected_ = false;
            }
            else
            {
                // assert(piece_tasks_.size() == 0);
                std::set<DownloadDriver__p>::iterator iter;
                for (iter = download_driver_s_.begin(); iter != download_driver_s_.end(); iter++)
                {
                    // TODO(nightsuns): 以后解决这个空指针的问题
                    if (false == (*iter)->IsRunning()) {
                        continue;
                    }

                    // 某些dd 可能没任务

                    std::multimap<protocol::PieceInfoEx, DownloadDriver__p>::iterator iter_p;
                    for (iter_p = piece_tasks_.begin(); iter_p != piece_tasks_.end(); iter_p ++)
                        if (iter_p->second == *iter)
                            break;

                    if (iter_p == piece_tasks_.end())
                    {
                        LOG4CPLUS_DEBUG_LOG(logger_p2p_downloader, __FUNCTION__ << " RequestNextPiece Num = 1");
                        if (true == (*iter)->RequestNextPiece(shared_from_this()))
                        {
                            // 只有当有一个dd分配任务成功 才表明连接上
                            is_connected_ = true;
                        }
                    }
                }
            }
        }
    }

    void P2PDownloader::AddCandidatePeers(std::vector<protocol::CandidatePeerInfo> peers)
    {
        if (is_running_ == false) return;

        if (ippool_->GetPeerCount() == 0)
        {
            ippool_->AddCandidatePeers(peers, false, false);
            InitPeerConnection();
        }
        else
        {
            ippool_->AddCandidatePeers(peers, false, false);
        }
    }

    void P2PDownloader::OnUdpRecv(protocol::VodPeerPacket const & packet)
    {
        if (is_running_ == false) return;

        assert(statistic_);

        std::map<boost::asio::ip::udp::endpoint, ConnectionBasePointer>::iterator iter =
            peers_.find(packet.end_point);

        if (iter != peers_.end())
        {
            iter->second->SubmitDownloadedBytes(packet.length());
        }

        // 如果是 SubPiece 报文
        //     下发给SubPieceRequestManager

        if (packet.PacketAction == protocol::SubPiecePacket::Action)
        {
            if (false == HasSubPiece(((const protocol::SubPiecePacket &)packet).sub_piece_info_))
            {
                if (iter != peers_.end())
                {
                    iter->second->SubmitP2PDataBytesWithoutRedundance(
                        ((const protocol::SubPiecePacket &)packet).sub_piece_length_);
                }
            }

            if (iter != peers_.end())
            {
                iter->second->SubmitP2PDataBytesWithRedundance(
                    ((const protocol::SubPiecePacket &)packet).sub_piece_length_);
            }

            subpiece_request_manager_.OnSubPiece((protocol::SubPiecePacket const &)packet);

            return;
        }

        // 如果是 PeerExchange(Request)(Response)
        //    调自己的 OnPeerExchangePacket 处理
        // 如果是 PeerExchange (Response)
        //    下发给 Exchanger 模块
        if (packet.PacketAction == protocol::PeerExchangePacket::Action)
        {
            exchanger_->OnPeerExchangePacket((const protocol::PeerExchangePacket &)packet);
            return;
        }
        // 如果是
        //        Announce 报文
        //    下发给 该PeerGuid对应的 PeerConnection 处理
        //    如果不存在对应的 PeerGuid，则直接放弃
        else if (packet.PacketAction == protocol::AnnouncePacket::Action)
        {
            if (peers_.find(packet.end_point) != peers_.end())
            {
                assert(peers_[packet.end_point]);
#ifdef BOOST_WINDOWS_API
                boost::intrusive_ptr<PeerConnection> peer_connection = boost::dynamic_pointer_cast<PeerConnection>(peers_[packet.end_point]);
#else
                boost::intrusive_ptr<PeerConnection> peer_connection = boost::static_pointer_cast<PeerConnection>(peers_[packet.end_point]);
#endif
                assert(peer_connection);
                if (peer_connection)
                {
                    peer_connection->OnAnnounce((protocol::AnnouncePacket const &)packet);
                }
            }
            return;
        }
        // 如果是 RIDResponse 报文
        //   下发给对应的PeerGuid，否则放弃
        else if (BootStrapGeneralConfig::Inst()->OpenRIDInfoRequestResponse() && 
            packet.PacketAction == protocol::RIDInfoResponsePacket::Action)
        {
            std::map<boost::asio::ip::udp::endpoint, boost::intrusive_ptr<ConnectionBase> >::iterator peer = peers_.find(packet.end_point);
            if (peer != peers_.end())
            {
                boost::intrusive_ptr<PeerConnection> peer_connection = boost::static_pointer_cast<PeerConnection>(peer->second);
                peer_connection->OnRIDInfoResponse((protocol::RIDInfoResponsePacket const &)packet);
            }
            return;
        }
        // 如果是 Connect(Response) 报文
        //    下发给 Connector 模块处理

        else if (packet.PacketAction == protocol::ConnectPacket::Action)
        {
//          assert(!connect_packet->IsRequest());
            connector_->OnReConectPacket((protocol::ConnectPacket const &)packet);
            return;
        }

        // 如果是Error报文
        // {
        //    如果是 Exchange 相关的Error报文
        //        下发给 Exchanger 模块
        //    如果是 Connect 相关的Error报文
        //        下发给 Connector 模块
        //    如果是 Peer 相关的Error报文
        //        下发给 SubPieceRequestManager 模块
        //    如果是 Announce相关的 Error报文
        //        下发给 相应的PeerConnection;
        //    如果是 RIDdNotFound
        //        直接DelPeer(PeerConnection::p);
        else if (packet.PacketAction == protocol::ErrorPacket::Action)
        {
//  !! Error错误码未定义
            protocol::ErrorPacket const & error_packet = (protocol::ErrorPacket const &)packet;

            if (error_packet.error_code_ == protocol::ErrorPacket::PPV_EXCHANGE_NO_RESOURCEID
                ||error_packet.error_code_ == protocol::ErrorPacket::PPV_CONNECT_NO_RESOURCEID
                ||error_packet.error_code_ == protocol::ErrorPacket::PPV_SUBPIECE_NO_RESOURCEID
                ||error_packet.error_code_ == protocol::ErrorPacket::PPV_ANNOUCE_NO_RESOURCEID
                ||error_packet.error_code_ == protocol::ErrorPacket::PPV_SN_BUSY
                ||error_packet.error_code_ == protocol::ErrorPacket::PPV_SN_ERROR
               )
            {
                if (error_packet.error_code_ == protocol::ErrorPacket::PPV_EXCHANGE_NO_RESOURCEID) {
                    LOG4CPLUS_DEBUG_LOG(logger_p2p_downloader, 
                        "ErrorPacket::PPV_EXCHANGE_NO_RESOURCEID, p2p_downloader = " << shared_from_this() << 
                        ", endpoint = " << packet.end_point);
                }
                if (error_packet.error_code_ == protocol::ErrorPacket::PPV_CONNECT_NO_RESOURCEID) {
                    LOG4CPLUS_DEBUG_LOG(logger_p2p_downloader, 
                        "ErrorPacket::PPV_CONNECT_NO_RESOURCEID, p2p_downloader = " << shared_from_this() << 
                        ", endpoint = " << packet.end_point);
                }
                if (error_packet.error_code_ == protocol::ErrorPacket::PPV_SUBPIECE_NO_RESOURCEID) {
                    LOG4CPLUS_DEBUG_LOG(logger_p2p_downloader, 
                        "ErrorPacket::PPV_SUBPIECE_NO_RESOURCEID, p2p_downloader = " << shared_from_this() << 
                        ", endpoint = " << packet.end_point);
                }
                if (error_packet.error_code_ == protocol::ErrorPacket::PPV_ANNOUCE_NO_RESOURCEID) {
                    LOG4CPLUS_DEBUG_LOG(logger_p2p_downloader, 
                        "ErrorPacket::PPV_ANNOUCE_NO_RESOURCEID, p2p_downloader = " << shared_from_this() << 
                        ", endpoint = " << packet.end_point);
                }

                if (peers_.find(error_packet.end_point) != peers_.end()) {
                    DelPeer(peers_[error_packet.end_point]);
                }
            }
            else if (error_packet.error_code_ == protocol::ErrorPacket::PPV_CONNECT_CONNECTION_FULL)
            {
                connector_->OnErrorPacket(error_packet);
            }
            else if (error_packet.error_code_ == protocol::ErrorPacket::PPV_SUBPIECE_SUBPIECE_NOT_FOUND)
            {
                // subpiece_request_manager_->OnError(error_packet);
            }
            else if (error_packet.error_code_ == protocol::ErrorPacket::PPV_EXCHANGE_NOT_DOWNLOADING)
            {
                // nothing
            }
            else if (error_packet.error_code_ == protocol::ErrorPacket::PPV_SUBPIECE_UPLOAD_BUSY)
            {

            }
        }
    }

    void P2PDownloader::GetCandidatePeerInfos(std::vector<protocol::CandidatePeerInfo> &candidate_peers)
    {
        if (false == is_running_)
            return;

        assert(candidate_peers.size() == 0);
        uint32_t peer_left = peers_.size();
        uint32_t cand_left = P2SPConfigs::P2P_MAX_EXCHANGE_PEER_COUNT;
        std::map<boost::asio::ip::udp::endpoint, ConnectionBasePointer> ::iterator iter;
        for (iter = peers_.begin(); iter != peers_.end();iter++)
        {
            Random random;
            if ((uint32_t)random.Next(peer_left) < cand_left)
            {
                candidate_peers.push_back(iter->second->GetCandidatePeerInfo());
                cand_left--;
            }
            peer_left--;
        }
    }

    uint32_t P2PDownloader::GetDataRate()
    {
        if (false == is_running_)
            return 0;
        if (data_rate_ == 0)
        {
            if (instance_)
            {
                storage::MetaData meta = instance_->GetMetaData();
                uint32_t filelen = instance_->GetFileLength();
                uint32_t duration = meta.Duration;
                if (duration != 0)
                    data_rate_ = filelen / duration;
            }
        }
        LOG4CPLUS_INFO_LOG(logger_p2p_downloader, __FUNCTION__ << " DataRate = " << data_rate_);
        return data_rate_ == 0 ? 30 * 1024 : data_rate_;
    }

    uint32_t P2PDownloader::CalculateActivePeerCount()
    {
        if (false == is_running_)
            return 0;
        active_peer_count_ = 0;
        std::map<boost::asio::ip::udp::endpoint, ConnectionBasePointer>::iterator it;
        for (it = peers_.begin(); it != peers_.end(); ++it)
        {
            ConnectionBasePointer peer = it->second;
            if (peer && peer->GetStatistic()->GetSpeedInfo().NowDownloadSpeed > 500)
                ++active_peer_count_;
        }
        LOG4CPLUS_DEBUG_LOG(logger_p2p_downloader, __FUNCTION__ << " ActivePeerCount=" << active_peer_count_);
        return active_peer_count_;
    }

    //////////////////////////////////////////////////////////////////////////
    // IP2PControlTarget

    void P2PDownloader::Pause()
    {
        if (false == is_running_)
            return;

        is_p2p_pausing_ = true;
    }
    void P2PDownloader::Resume()
    {
        if (false == is_running_)
            return;

        is_p2p_pausing_ = false;
    }

    void P2PDownloader::SetDownloadMode(IP2PControlTarget::P2PDwonloadMode mode)
    {
        if (false == is_running_)
            return;
        LOG4CPLUS_INFO_LOG(logger_p2p_downloader, "SetDownloadMode " << mode);
        dl_mode_ = mode;
    }

    boost::uint32_t P2PDownloader::GetSecondDownloadSpeed()
    {
        if (false == is_running_)
            return 0;
        return GetSpeedInfoEx().SecondDownloadSpeed;
    }

    boost::uint32_t P2PDownloader::GetCurrentDownloadSpeed()
    {
        if (false == is_running_)
            return 0;
        return GetSpeedInfoEx().NowDownloadSpeed;
    }
    uint32_t P2PDownloader::GetMinuteDownloadSpeed()
    {
        if (false == is_running_)
            return 0;
        return GetSpeedInfo().MinuteDownloadSpeed;
    }
    uint32_t P2PDownloader::GetRecentDownloadSpeed()
    {
        if (false == is_running_)
            return 0;
        return GetSpeedInfoEx().RecentDownloadSpeed;
    }
    uint32_t P2PDownloader::GetPooledPeersCount()
    {
        if (false == is_running_)
            return 0;
        IpPool::p ip_pool = GetIpPool();
        if (ip_pool)
        {
            return ip_pool->GetPeerCount();
        }
        return 0;
    }
    uint32_t P2PDownloader::GetConnectedPeersCount()
    {
        if (false == is_running_)
            return 0;
        return peers_.size();
    }
    uint32_t P2PDownloader::GetFullBlockPeersCount()
    {
        if (false == is_running_)
            return 0;
        return connected_full_block_peer_count_;
    }

    uint32_t P2PDownloader::GetActivePeersCount()
    {
        if (false == is_running_)
            return 0;
        return active_peer_count_;
    }
    uint32_t P2PDownloader::GetAvailableBlockPeerCount()
    {
        if (false == is_running_)
            return 0;
        return connected_available_block_peer_count_;
    }

    protocol::PEER_COUNT_INFO P2PDownloader::GetPeerCountInfo() const
    {
        protocol::PEER_COUNT_INFO peer_count_info;
        if (false == is_running_) {
            return peer_count_info;
        }
        peer_count_info.ActivePeersCount = active_peer_count_;
        peer_count_info.ConnectedPeersCount = peers_.size();
        if (ippool_)
        {
            peer_count_info.PooledPeersCount = ippool_->GetPeerCount();
        }
        else
        {
            peer_count_info.PooledPeersCount = 0;
        }
        return peer_count_info;
    }

    bool P2PDownloader::CanPreemptive(DownloadDriver__p download_driver_, const protocol::PieceInfoEx & piece)
    {
        if (false == is_running_) {
            return false;
        }

        if (piece_tasks_.empty())
        {
            LOG4CPLUS_DEBUG_LOG(logger_p2p_downloader, "CanPreemptive piece_tasks_.empty()");
            return true;
        }

        bool is_first_piece = false;
        std::multimap<protocol::PieceInfoEx, DownloadDriver__p>::const_iterator iter = piece_tasks_.begin();
        for (; iter != piece_tasks_.end(); ++iter)
        {
            if (iter->second == download_driver_)
            {
                is_first_piece =  (piece == iter->first);
                break;
            }
        }

        if (!is_first_piece)
        {
            LOG4CPLUS_DEBUG_LOG(logger_p2p_downloader, "CanPreemptive: !is_first_piece");
            return false;
        }
        else
        {
            uint32_t piece_end = SUB_PIECE_COUNT_PER_PIECE;
            uint32_t end_position = piece.GetEndPosition(block_size_);
            uint32_t file_length = instance_->GetFileLength();
            if (end_position >= file_length)
            {
                if (end_position < file_length + PIECE_SIZE) {
                    piece_end = (file_length - (end_position - PIECE_SIZE) + SUB_PIECE_SIZE - 1) / SUB_PIECE_SIZE;
                }
                else {
                    assert(!"Invalid Piece Info");
                }
            }

            uint32_t missing_subpiece_count = 0;
            for (boost::uint32_t i = piece.subpiece_index_; i < piece_end; i++)
            {
                protocol::SubPieceInfo sub_piece(piece.block_index_, piece.piece_index_ * SUB_PIECE_COUNT_PER_PIECE + i);
                if (!HasSubPiece(sub_piece))
                    ++missing_subpiece_count;
            }

            LOG4CPLUS_DEBUG_LOG(logger_p2p_downloader, "missing_subpiece_count:" << missing_subpiece_count);


            if (missing_subpiece_count >= 10)
                return true;
            else
                return false;
        }
    }

    void P2PDownloader::OnPieceTimeout(DownloadDriver__p download_driver_, const protocol::PieceInfoEx & piece)
    {
        if (false == is_running_) {
            return;
        }

        if (piece_tasks_.empty())
            return;

        std::multimap<protocol::PieceInfoEx, DownloadDriver__p>::iterator iter = piece_tasks_.find(piece);
        for (; iter != piece_tasks_.end() && iter->first == piece;)
        {
            if (iter->second == download_driver_)
            {
                piece_tasks_.erase(iter++);
                LOG4CPLUS_INFO_LOG(logger_p2p_downloader, shared_from_this() << " Stop Download Piece " << piece << 
                    ", download_driver_:" << download_driver_);
            }
            else
            {
                ++iter;
            }
        }
    }

    void P2PDownloader::OnPieceRequest(const protocol::PieceInfo & piece)
    {
        std::multimap<protocol::PieceInfoEx, DownloadDriver__p>::iterator iter = piece_tasks_.begin();
        for (; iter != piece_tasks_.end(); ++iter)
        {
            if (iter->first.GetPieceInfo() == piece)
            {
                DownloadDriver__p dr = iter->second;
                dr->OnPieceRequest(piece);
            }
        }
    }

    // Modified by jeffrey 2011.1.23
    // P2P模块限速，输入为期望的每秒收到数
    // 会自动根据丢包率折算为每秒限制的发包数
    void P2PDownloader::SetSpeedLimitInKBps(boost::int32_t speed_limit_in_KBps)
    {
        if (false == is_running_)
        {
            return;
        }

        // p2p被某个策略限速了，向downloaddriver汇报
        if (speed_limit_in_KBps != P2SPConfigs::P2P_DOWNLOAD_SPEED_LIMIT &&
            speed_limit_in_KBps != -1 )
        {
            STL_FOR_EACH(std::set<DownloadDriver__p>, download_driver_s_, iter)
            {
                (*iter)->NoticeP2pSpeedLimited();
            }
        }

        if (P2SPConfigs::P2P_DOWNLOAD_SPEED_LIMIT_ENABEL)
            speed_limit_in_KBps = P2SPConfigs::P2P_DOWNLOAD_SPEED_LIMIT;

        // 按照丢包率预测出实际需要限制的发包数来限速
        float rate = -1;
        if (GetStatistic())
        {
            rate = GetStatistic()->GetUDPLostRate();
            rate = (1 - (float)GetStatistic()->GetUDPLostRate() / (float)100) + 0.001;
            LOG4CPLUS_DEBUG_LOG(logger_p2p_downloader, "RATE = " << rate);
        }

        boost::int32_t speed_limit = 0;

        if (rate > 0 && rate < 1)
        {
            speed_limit = speed_limit_in_KBps / rate;
        }
        else
        {
            speed_limit = speed_limit_in_KBps / 0.75;
        }

        LIMIT_MAX(speed_limit, P2SPConfigs::P2P_DOWNLOAD_SPEED_LIMIT);

        download_speed_limiter_.SetSpeedLimitInKBps(speed_limit);

/*
        if (speed_limit_in_KBps >= 0)
            p2p_max_connect_count_ = speed_limit_in_KBps / 5;
        else
            p2p_max_connect_count_ = P2SPConfigs::P2P_DOWNLOAD_MAX_CONNECT_COUNT;

        LIMIT_MIN_MAX(p2p_max_connect_count_, P2SPConfigs::P2P_DOWNLOAD_MIN_CONNECT_COUNT, P2SPConfigs::P2P_DOWNLOAD_MAX_CONNECT_COUNT);
*/
        LOG4CPLUS_INFO_LOG(logger_p2p_downloader, shared_from_this() << " SetSpeedLimitInKBps: " << 
            speed_limit_in_KBps << ", p2p_max_connect_count_ = " << p2p_max_connect_count_);
    }

    statistic::SPEED_INFO P2PDownloader::GetSpeedInfo()
    {
        if (false == is_running_) {
            return statistic::SPEED_INFO();
        }
        if (!statistic_ || !statistic_->IsRunning()) {
            return statistic::SPEED_INFO();
        }
        assert(statistic_);
        return statistic_->GetSpeedInfo();
    }

    statistic::SPEED_INFO_EX P2PDownloader::GetSpeedInfoEx()
    {
        if (false == is_running_) {
            return statistic::SPEED_INFO_EX();
        }
        if (!statistic_ || !statistic_->IsRunning()) {
            return statistic::SPEED_INFO_EX();
        }
        assert(statistic_);
        return statistic_->GetPeerSpeedInfoEx();
    }

    boost::uint32_t P2PDownloader::GetRTTPlus()
    {
        boost::int32_t max_rest_time = 0;
        DownloadDriver::p download_driver;
        for (std::set<DownloadDriver::p>::iterator iter = download_driver_s_.begin();
        iter != download_driver_s_.end(); iter++)
        {
            DownloadDriver::p dd = *iter;
            if (dd && dd->GetRestPlayableTime() > max_rest_time)
            {
                max_rest_time = dd->GetRestPlayableTime();
                download_driver = dd;
            }
        }

        if (download_driver && download_driver->GetDownloadMode() == IGlobalControlTarget::SLOW_MODE)
        {
            return 1800;
        }

        if (!assigner_->IsEndOfAssign() && max_rest_time > 30*1000)
        {
            return std::min((boost::int32_t)((max_rest_time - 30*1000) / 10 + 1000), (boost::int32_t)8000);
        }

        return 1500;
    }

    // Assigner驱动的检查piece的完成情况
    void P2PDownloader::CheckPieceComplete()
    {
        assert(is_running_);
        if (!is_running_)
        {
            return;
        }

        for (std::multimap<protocol::PieceInfoEx, DownloadDriver__p>::iterator piece_task_iter = piece_tasks_.begin();
            piece_task_iter != piece_tasks_.end();)
        {
            std::multimap<protocol::PieceInfoEx, DownloadDriver__p>::iterator iter = piece_task_iter++;
            if (true == GetInstance()->HasPiece(iter->first))
            {
                // Instance中已经有了这片piece，表示该piece已经下载完成
                iter->second->OnPieceComplete(iter->first, shared_from_this());
                piece_tasks_.erase(iter);
            }
            else
            {
                // Instance中没有这片piece，检查Instance中，是否拥有所有的subpiece
                if (iter->first.subpiece_index_ != 0)
                {
                    // 通常情况, subpiece_end 为 127
                    boost::uint32_t subpiece_end = iter->first.subpiece_index_end_;

                    if (iter->first.GetEndPosition(block_size_) > GetInstance()->GetFileLength())
                    {
                        // 文件末尾的情况, subpiece_end需要重新计算
                        subpiece_end = (GetInstance()->GetFileLength() - iter->first.GetPosition(block_size_) - 1) / SUB_PIECE_SIZE;
                    }

                    // 检查Instance中是否拥有了该piece的所有subpiece
                    bool has_piece = true;
                    for (boost::uint32_t i = iter->first.subpiece_index_; i <= subpiece_end; ++i)
                    {
                        protocol::SubPieceInfo sub_piece(iter->first.block_index_, iter->first.piece_index_ * SUB_PIECE_COUNT_PER_PIECE + i);
                        if (false == HasSubPiece(sub_piece))
                        {
                            has_piece = false;
                            break;
                        }
                    }

                    // 当前PieceInfoEx的Subpiece已经全部完成
                    if (has_piece)
                    {
                        iter->second->OnPieceComplete(iter->first, shared_from_this());
                        piece_tasks_.erase(iter);
                    }
                }
            }
        }

        if (piece_tasks_.size() == 0)
        {
            is_connected_ = false;
        }
    }

    // 设置共享内存中的每个连接经过上轮剩余的分配到的Subpiece个数
    void P2PDownloader::SetAssignedLeftSubPieceCount()
    {
        for (std::map<boost::asio::ip::udp::endpoint, ConnectionBasePointer> ::iterator iter = peers_.begin(); iter != peers_.end();iter++)
        {
            ConnectionBasePointer peer  = iter->second;
            LOG4CPLUS_INFO_LOG(logger_p2p_downloader, "Assigner::CaclPeerConnectionRecvTimeMap peer : " << peer << 
                ", TaskQueueRemaining:" << peer->GetTaskQueueSize());
            peer->GetStatistic()->SetAssignedLeftSubPieceCount(peer->GetTaskQueueSize());
        }
    }

    boost::uint32_t P2PDownloader::GetSpeedLimitRestCount()
    {
        return download_speed_limiter_.GetRestCount();
    }

    bool P2PDownloader::HasSubPiece(const protocol::SubPieceInfo& sub_piece)
    {
        if (false == is_running_)
            return false;

        // herain:2011-3-8:已经不在下载队列中的subpiece到达，可能有数据，也可能没有数据
        // 如果Piece是正常下载完成，那么这片subpiece是晚到达的冗余报文，如果是Piece超时后
        // 到达的正常报文，那么这片subpiece是本地没有的。
        return instance_->HasSubPiece(sub_piece);
    }

    void P2PDownloader::KeepConnectionAlive()
    {
        std::map<boost::asio::ip::udp::endpoint, ConnectionBasePointer>::iterator iter;
        for (iter = peers_.begin(); iter != peers_.end(); ++iter)
        {
            iter->second->KeepAlive();
        }
    }

    void P2PDownloader::DoList()
    {
        int max_do_list_count;
        if(is_openservice_)
        {
            max_do_list_count = 5;
        }
        else
        {
            max_do_list_count = BootStrapGeneralConfig::Inst()->GetMaxMaterialDolistCount();
        }

        if (ippool_ && ippool_->GetNotTriedPeerCount() < 30 && NeedConnectNewConnection())
        {
            if ((dolist_count_ < max_do_list_count && (!last_dolist_time_.running() || last_dolist_time_.elapsed() > 2 * 1000)) ||
                last_dolist_time_.elapsed() > 60 * 1000)
            {
                ++dolist_count_;
                TrackerModule::Inst()->DoList(GetRid(), true);
                last_dolist_time_.start();
            }
        }
    }

    bool P2PDownloader::NeedConnectNewConnection()
    {
        boost::uint32_t data_rate = GetDataRate();

        LOG4CPLUS_DEBUG_LOG(logger_p2p_downloader, "NeedConnectNewConnection = " << shared_from_this()
            << "IsEndOfAssign" << assigner_->IsEndOfAssign()
            << ", NowDownloadSpeed " << GetCurrentDownloadSpeed() 
            << ", data_rate " << data_rate);

        return !assigner_->IsEndOfAssign() && (GetCurrentDownloadSpeed() < data_rate + 30 * 1024)
            && (GetCurrentDownloadSpeed() < data_rate * 12 / 10)
            && (GetCurrentDownloadSpeed() < statistic::StatisticModule::Inst()->GetBandWidth() * 7 / 10);
    }

    void P2PDownloader::UpdateConnectTime()
    {
        std::map<boost::asio::ip::udp::endpoint, ConnectionBasePointer>::iterator iter;
        for (iter = peers_.begin(); iter != peers_.end(); ++iter)
        {
            iter->second->UpdateConnectTime();
        }
    }

    boost::uint32_t P2PDownloader::GetAvgConnectRTT() const
    {
        boost::uint32_t sum_rtt = 0;

        for (std::map<boost::asio::ip::udp::endpoint, ConnectionBasePointer>::const_iterator
            iter = peers_.begin(); iter != peers_.end(); ++iter)
        {
            sum_rtt += iter->second->GetConnectRTT();
        }

        if (peers_.empty())
        {
            return 0;
        }
        else
        {
            return sum_rtt / peers_.size();
        }
    }

    void P2PDownloader::AddRequestingSubpiece(const protocol::SubPieceInfo & subpiece_info,
        boost::uint32_t timeout, ConnectionBasePointer peer_connection)
    {
        subpiece_request_manager_.Add(subpiece_info, timeout, peer_connection);
    }

    const string & P2PDownloader::GetOpenServiceFileName()
    {
        return file_name_;
    }

    void P2PDownloader::KickSnConnection()
    {
        if (!is_sn_enable_)
        {
            return;
        }

        if (!sn_pool_object_.IsHaveReserveSn())
        {
            return;
        }

        if (statistic_->GetSpeedInfo().NowDownloadSpeed > GetDataRate())
        {
            return;
        }

        std::map<boost::asio::ip::udp::endpoint, ConnectionBasePointer>::iterator iter;

        boost::uint32_t min_speed = 32767*1024;
        ConnectionBasePointer kick_sn_connection;

        for (iter = peers_.begin(); iter != peers_.end(); iter++)
        {
            if (sn_pool_object_.IsSn(iter->first) && 
                iter->second->GetConnectedTime() > 5*1000)
            {
                if (iter->second->GetStatistic()->GetSpeedInfo().NowDownloadSpeed < min_speed)
                {
                    min_speed = iter->second->GetStatistic()->GetSpeedInfo().NowDownloadSpeed;
                    kick_sn_connection = iter->second;
                }
            }
        }

        if (kick_sn_connection)
        {
            DelPeer(kick_sn_connection);
            
            boost::intrusive_ptr<SNConnection> sn_connection = new SNConnection(shared_from_this(), sn_pool_object_.GetReserveSn());

            sn_connection->Start();

            AddPeer(sn_connection);
        }
    }

    void P2PDownloader::SetSnEnable(bool enable)
    {
        if (enable)
        {
            for (std::map<boost::asio::ip::udp::endpoint, ConnectionBasePointer>::iterator iter = sn_.begin();
                iter != sn_.end(); ++iter)
            {
                boost::intrusive_ptr<SNConnection> sn = boost::static_pointer_cast<SNConnection>(iter->second);
                sn->Start();
                AddPeer(iter->second);
            }

            sn_.clear();
        }
        else
        {
            for (std::map<boost::asio::ip::udp::endpoint, ConnectionBasePointer>::iterator
                iter = peers_.begin(); iter != peers_.end(); )
            {
                if (sn_pool_object_.IsSn(iter->first))
                {
                    assert(sn_.find(iter->first) == sn_.end());

                    sn_.insert(std::make_pair(iter->first, iter->second));

                    iter->second->Stop();

                    peers_.erase(iter++);
                }
                else
                {
                    ++iter;
                }
            }
        }

        is_sn_enable_ = enable;
    }

    void P2PDownloader::InitSnList(const std::list<boost::asio::ip::udp::endpoint> & sn_list)
    {
        sn_pool_object_.Add(sn_list);
        boost::uint32_t max_sn_list_sz = vip_level_ ? BootStrapGeneralConfig::Inst()->MaxSNListSizeForVIP() : 
            BootStrapGeneralConfig::Inst()->MaxSNListSize();

        while (sn_.size() < max_sn_list_sz && sn_pool_object_.IsHaveReserveSn())
        {
            boost::intrusive_ptr<SNConnection> sn_connection = new SNConnection(shared_from_this(), sn_pool_object_.GetReserveSn());

            sn_connection->Start();

            assert(sn_.find(sn_connection->GetEndpoint()) == sn_.end());

            sn_.insert(std::make_pair(sn_connection->GetEndpoint(), sn_connection));
        }
    }

    void P2PDownloader::AddSnOnCDN(const std::list<boost::asio::ip::udp::endpoint> & sn_list)
    {
        sn_pool_object_.Add(sn_list);
    }

    boost::int32_t P2PDownloader::GetDownloadPriority()
    {
        if (vip_level_ && GetMinRestTimeInMilliSecond() < VIP_URGENT_TIME_IN_SECOND)
        {
            return protocol::RequestSubPiecePacket::PRIORITY_VIP;
        }
        return download_priority_;
    }

    boost::uint32_t P2PDownloader::GetMinRestTimeInMilliSecond()
    {
        boost::uint32_t min_rest_time = std::numeric_limits<uint32_t>::max();
        for (std::set<DownloadDriver::p>::iterator iter = download_driver_s_.begin();
            iter != download_driver_s_.end(); ++iter)
        {
            DownloadDriver::p download_driver = *iter;
            if (download_driver && download_driver->GetRestPlayableTime() < min_rest_time)
            {
                min_rest_time = download_driver->GetRestPlayableTime();
            }
        }

        return min_rest_time;
    }
}