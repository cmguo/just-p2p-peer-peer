//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/p2p/P2PModule.h"
#include "p2sp/AppModule.h"
#include "p2sp/p2p/P2PDownloader.h"
#include "p2sp/p2p/UploadManager.h"
#include "p2sp/p2p/P2SPConfigs.h"
#include "p2sp/p2p/SessionPeerCache.h"

#ifdef NOTIFY_ON
#include "p2sp/notify/NotifyModule.h"
#endif

#ifdef DUMP_OBJECT
#include "count_object_allocate.h"
#endif

#ifdef COUNT_CPU_TIME
#include "count_cpu_time.h"
#endif

#include "storage/Storage.h"
#include "storage/IStorage.h"
#include "struct/SubPieceContent.h"
#include "statistic/StatisticModule.h"

#include "p2sp/p2p/LiveP2PDownloader.h"

#include "storage/LiveInstance.h"

#define P2P_DEBUG(s) LOG(__DEBUG, "P2P", s)
#define P2P_INFO(s)    LOG(__INFO, "P2P", s)
#define P2P_EVENT(s) LOG(__EVENT, "P2P", s)
#define P2P_WARN(s)    LOG(__WARN, "P2P", s)
#define P2P_ERROR(s) LOG(__ERROR, "P2P", s)

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("p2p");

    P2PModule::p P2PModule::inst_(new P2PModule());

    P2PModule::P2PModule()
        : p2p_timer_(global_250ms_timer(), 250, boost::bind(&P2PModule::OnTimerElapsed, this, &p2p_timer_))
        , is_running_(false), global_request_send_count_(0)
    {
    }

    void P2PModule::Start(const string& config_path)
    {
        if (is_running_ == true) return;

        is_running_ = true;

        request_count_ = 0;
        window_size_ = 0;
        sent_count_ = 0;
        max_download_speed_ = 100 * 1024;
        max_historical_upload_speed_ = 0;
        max_historical_download_speed_ = 0;

        is_increased_window_size_ = false;
        avg_available_window_size_ = 0;


        upload_manager_ = UploadManager::create();
        upload_manager_->Start(config_path);


        p2p_timer_.start();


        session_cache_ = SessionPeerCache::create();
    }

    void P2PModule::Stop()
    {
        if (is_running_ == false) return;


        assert(upload_manager_);
        upload_manager_->Stop();


        for (std::map<RID, P2PDownloader::p>::iterator iter  = rid_indexer_.begin(); iter != rid_indexer_.end(); iter ++)
        {
            P2PDownloader::p downloader = iter->second;
            downloader->Stop();
        }
        rid_indexer_.clear();


        p2p_timer_.stop();

        is_running_ = false;
        inst_.reset();
    }

    void P2PModule::SetMaxUploadSpeedInKBps(boost::int32_t MaxUploadP2PSpeed)
    {
        if (is_running_ == false)
            return;
        // 只是设置一个上限值
        upload_manager_->SetUploadUserSpeedLimitInKBps(MaxUploadP2PSpeed);
    }

    boost::int32_t P2PModule::GetUploadSpeedLimitInKBps() const
    {
        if (false == is_running_) return 0;

        if (upload_manager_) return upload_manager_->GetUploadSpeedLimitInKBps();

        return 0;
    }

    P2PDownloader::p P2PModule::CreateP2PDownloader(const RID& rid)
    {
        // !TEST
        // return P2PDownloader::p();
        if (is_running_ == false) return P2PDownloader::p();

        storage::IStorage::p storag = storage::Storage::Inst();
        storage::Instance::p instance = boost::dynamic_pointer_cast<storage::Instance>(storag->GetInstanceByRID(rid));
        assert(instance);
        assert(false == instance->GetRID().is_empty());
        // 如果 在 rid_indexer_ 中找到了 这个 P2PDownloader, 就是用这个 P2PDownloader
        if (rid_indexer_.find(rid) != rid_indexer_.end())
        {
            P2PDownloader::p downloader = rid_indexer_[rid];
            // ! 有问题
            if (downloader->GetInstance() != instance) {
                LOG(__DEBUG, "storage", __FUNCTION__ << " downloader->instance_ != instance, change from " << downloader->GetInstance() << " to " << instance);
                downloader->SetInstance(instance);
            }
            // LOG(__EVENT, "leak", __FUNCTION__ << " found: " << rid << " downloader: " << downloader);
            return downloader;
        }

        // 如果 在 rid_indexer_ 没有找到 这个 P2P 创建RID对应的 P2PDownloader
        P2PDownloader::p downloader = P2PDownloader::create(rid);
        rid_indexer_[rid] = downloader;
        downloader->Start();
        //
        // LOG(__EVENT, "leak", __FUNCTION__ << " create new: " << rid << " downloader: " << downloader);
        return downloader;
    }

    void P2PModule::AddCandidatePeers(RID rid, const std::vector<protocol::CandidatePeerInfo>& peers)
    {
        if (is_running_ == false)
            return;

        if (rid_indexer_.find(rid) != rid_indexer_.end())
        {
            P2PDownloader::p p2p_downloader = rid_indexer_.find(rid)->second;
            p2p_downloader->AddCandidatePeers(peers);
        }

        if (live_rid_index_.find(rid) != live_rid_index_.end())
        {
            LiveP2PDownloader::p p2p_downloader = live_rid_index_.find(rid)->second;
            p2p_downloader->AddCandidatePeers(peers);
        }
    }

    void P2PModule::OnUdpRecv(protocol::Packet const & packet_)
    {
        if (is_running_ == false) return;
        // 如果是 Connect(Request) 报文，
        //        RequestAnnounce 报文，
        //        RequestSubPiece 报文
        //     则 下发给 UploadManager 模块
        // P2P_EVENT("P2PModule::OnUdpRecv 0x" << std::hex << (u_int)packet->GetAction());
        if (packet_.PacketAction == protocol::RequestSubPiecePacket::Action)
        {
            upload_manager_->OnRequestSubPiecePacket((protocol::RequestSubPiecePacket const &)packet_);
            return;
        }

        if (packet_.PacketAction == protocol::LiveRequestAnnouncePacket::Action)
        {
            upload_manager_->OnLiveRequestAnnouncePacket((protocol::LiveRequestAnnouncePacket const &)packet_);
            return;
        }

        if (packet_.PacketAction == protocol::LiveRequestSubPiecePacket::Action)
        {
            upload_manager_->OnLiveRequestSubPiecePacket((protocol::LiveRequestSubPiecePacket const &)packet_);
            return ;
        }

        protocol::PeerPacket const & packet = (protocol::PeerPacket const &)packet_;

        if (packet.PacketAction == protocol::ConnectPacket::Action)
        {
            const protocol::ConnectPacket & connect_packet = (const protocol::ConnectPacket &)packet;
            LOGX(__DEBUG, "upload", "P2PModule::OnUdpRecv ConnectPacket connect_packet->IsRequest()" << ((protocol::ConnectPacket&)packet).IsRequest());

#ifdef NOTIFY_ON
            // 如果是特殊RID，Notify处理
            RID spec_rid;
            const string str_rid = "00000000000000000000000000000001";
            spec_rid.from_string(str_rid);

            if (connect_packet.resource_id_ == spec_rid)
            {
                p2sp::NotifyModule::Inst()->OnUdpRecv(connect_packet);
            }
            else
#endif
            {
                if (connect_packet.IsRequest())
                {
                    upload_manager_->OnConnectPacket(connect_packet);
                    return;
                }
            }
        }
        if (packet.PacketAction == protocol::RequestAnnouncePacket::Action)
        {
            LOGX(__DEBUG, "upload", "RequestAnnouncePacket Request!");
            upload_manager_->OnRequestAnnouncePacket((const protocol::RequestAnnouncePacket &)packet);
            return;
        }
        if (packet.PacketAction == protocol::RequestSubPiecePacketOld::Action)
        {
            upload_manager_->OnRequestSubPiecePacketOld((protocol::RequestSubPiecePacketOld const &)packet);
            return;
        }
        if (packet.PacketAction == protocol::RIDInfoRequestPacket::Action)
        {
            LOGX(__DEBUG, "upload", "OnRIDInfoRequestPacket Request!");
            upload_manager_->OnRIDInfoRequestPacket((protocol::RIDInfoRequestPacket const &)packet);
            return;
        }
        if (packet.PacketAction == protocol::ReportSpeedPacket::Action)
        {
            const protocol::ReportSpeedPacket & speed_packet = (protocol::ReportSpeedPacket const &)packet;
            LOGX(__DEBUG, "upload", "OnReportSpeedPacket Request! ep:" << speed_packet.end_point << ", speed:" << speed_packet.speed_);
            upload_manager_->OnReportSpeedPacket(speed_packet);
            return;
        }
        // 否则
        //        下发给 RID 对应的 P2PDownloader 模块
        //          但是 如果 找不到 RID 对应的 P2PDownloader 模块
        //             则 不管

        if (rid_indexer_.find(packet.resource_id_) != rid_indexer_.end())
        {

            P2PDownloader__p p2p_downloader = rid_indexer_.find(packet.resource_id_)->second;
            p2p_downloader->OnUdpRecv(packet);
        }
        if (live_rid_index_.find(packet.resource_id_) != live_rid_index_.end())
        {
            LiveP2PDownloader__p p2p_downloader = live_rid_index_.find(packet.resource_id_)->second;
            p2p_downloader->OnUdpRecv(packet);
        }
        else if (packet.PacketAction == protocol::PeerExchangePacket::Action)
        {
            const protocol::PeerExchangePacket & exchange_packet = (const protocol::PeerExchangePacket &)packet;
            if (exchange_packet.IsRequest())
            {

                // 当前没有下载此资源，没有Peer可供交换
                protocol::ErrorPacket  error_packet((protocol::ErrorPacket)packet);
                error_packet.peer_guid_ = AppModule::Inst()->GetPeerGuid();
                error_packet.error_code_ =  protocol::ErrorPacket::PPV_EXCHANGE_NOT_DOWNLOADING;
                AppModule::Inst()->DoSendPacket(error_packet, packet.sequece_id_);
            }
        }
        else
        {
        }
    }

    void P2PModule::OnTimerElapsed(framework::timer::Timer * pointer)
    {
        if (is_running_ == false) return;
        uint32_t times = pointer->times();
        if (pointer == &p2p_timer_)
        {
            OnP2PTimer(times);
        }
        else
        {
            assert(0);
        }
    }

    void P2PModule::OnP2PTimer(uint32_t times)
    {
#ifdef COUNT_CPU_TIME
        count_cpu_time(__FUNCTION__);
#endif
        if (is_running_ == false) return;

        // P2P Timer 是所有P2P模块都要使用的定时器，所以要下发给下面的说有的模块

#ifdef DUMP_OBJECT
        // 1分钟打印一次对象个数
        if (times % 240 == 0)
        {
            object_counter::get_counter()->dump_all_objects();
        }
#endif

#ifdef COUNT_CPU_TIME
        if (times % 40 == 0)
        {
            cpu_time_record::get_record()->dump();
        }
#endif

        // 统计信息
        if (times % 4 == 0)
        {
            statistic::StatisticModule::Inst()->SetGlobalRequestSendCount(global_request_send_count_);
            global_request_send_count_ = 0;
        }

        // 1分钟检查一次
        if (times % (4 * 60 * 1) == 0)
        {
            // SessionPeerCache过期淘汰
            session_cache_->ExpireCache();
        }

        // 按每秒计算
        if (times % 4 == 0)
        {
            statistic::SPEED_INFO speed_info = statistic::StatisticModule::Inst()->GetSpeedInfo();
            uint32_t now_download_speed = statistic::StatisticModule::Inst()->GetTotalDownloadSpeed();
            uint32_t now_upload_speed = statistic::StatisticModule::Inst()->GetUploadDataSpeed();  // speed_info.NowUploadSpeed;

            // 设置共享内存全局 window_size_
            statistic::StatisticModule::Inst()->SetGlobalWindowSize(window_size_);

            if (max_download_speed_ < now_download_speed || times % (4 * 60) == 0)  // 1鍒嗛挓
            {
                if (now_download_speed > 1024)
                {
                    max_download_speed_ = now_download_speed;
                }
                else
                {
                    max_download_speed_ = max_download_speed_ * 3 / 4;
                }
                LIMIT_MIN(max_download_speed_, 100*1024);
            }

            window_size_ = max_download_speed_ / 1000 * 4 / 3;

            P2PDownloader::p p2p_downloader;
            if (rid_indexer_.size() == 1 && true == (p2p_downloader = rid_indexer_.begin()->second)->NeedIncreaseWindowSize())
            {
                is_increased_window_size_ = true;
                LIMIT_MIN_MAX(window_size_, P2SPConfigs::P2P_MIN_TOTAL_WINDOW_SIZE + 20, P2SPConfigs::P2P_MAX_TOTAL_WINDOW_SIZE);
                uint32_t used_window_size = p2p_downloader->GetTotalWindowSize();
                if (window_size_ > used_window_size)
                {
                    uint32_t active_peer_count = p2p_downloader->GetActivePeerCount();
                    avg_available_window_size_ = static_cast<uint32_t>((window_size_ - used_window_size + 0.0) / active_peer_count + 0.5);
                }
                else
                {
                    avg_available_window_size_ = 0;
                }
                LOG(__EVENT, "upload", __FUNCTION__ << " is_increase_window_size_=" << avg_available_window_size_ << " avg_available_window_size_=" << avg_available_window_size_);
            }
            else
            {
                is_increased_window_size_ = false;
                avg_available_window_size_ = 0;
                LIMIT_MIN_MAX(window_size_, P2SPConfigs::P2P_MIN_TOTAL_WINDOW_SIZE, P2SPConfigs::P2P_MAX_TOTAL_WINDOW_SIZE);
            }
            // request_count_ = 0;

            // P2P_EVENT(__FUNCTION__ << " WindowSize: " << window_size_ << " RequestCount: " << request_count_ << " SentCount: " << sent_count_);
            sent_count_ = 0;


            if (max_historical_upload_speed_ < now_upload_speed)
                max_historical_upload_speed_ = now_upload_speed;
            if (max_historical_download_speed_ < now_download_speed)
                max_historical_download_speed_ = now_download_speed;
        }

#ifdef DISK_MODE
        // 首先 UploadManager 调用这个
        assert(upload_manager_);
        upload_manager_->OnP2PTimer(times);
#endif
        // P2PDownloader->OnP2PTimer 所有的
        for (std::map<RID, P2PDownloader::p>::iterator iter  = rid_indexer_.begin(); iter != rid_indexer_.end(); iter ++)
        {
            P2PDownloader::p downloader = iter->second;
            downloader->OnP2PTimer(times);
        }

        // LiveP2PDownloader->OnP2PTimer
        for (std::map<RID, LiveP2PDownloader::p>::iterator iter = live_rid_index_.begin();
            iter != live_rid_index_.end(); ++iter)
        {
            LiveP2PDownloader__p downloader = iter->second;
            downloader->OnP2PTimer(times);
        }
    }

    void P2PModule::OnP2PDownloaderWillStop(P2PDownloader::p p2p_downloader)
    {
        if (is_running_ == false) return;

        if (rid_indexer_.find(p2p_downloader->GetRid()) == rid_indexer_.end())
        {
            return;
        }

        rid_indexer_.erase(p2p_downloader->GetRid());
    }

    P2PDownloader::p P2PModule::GetP2PDownloader(const RID& rid)
    {
        if (false == is_running_) {
            return P2PDownloader::p();
        }
        RIDIndexerMap::const_iterator it = rid_indexer_.find(rid);
        if (it != rid_indexer_.end()) {
            return it->second;
        }
        return P2PDownloader::p();
    }

    void P2PModule::SetMaxUploadCacheSizeInMB(uint32_t nMaxUploadCacheSizeInMB)
    {
        if (false == is_running_) {
            LOGX(__DEBUG, "upload", " Not running");
            return;
        }
        if (nMaxUploadCacheSizeInMB == 0) {
            LOGX(__DEBUG, "upload", "nMaxUploadCacheSizeInMB = " << nMaxUploadCacheSizeInMB);
            return;
        }

        if (upload_manager_)
        {
            boost::uint32_t count = (nMaxUploadCacheSizeInMB + 1) / 2;
            upload_manager_->SetMaxUploadCacheLength(count);
            LOGX(__DEBUG, "upload", "MaxUploadCacheSizeInMB = " << nMaxUploadCacheSizeInMB << " SetMaxUploadCacheLength = " << count);
        }
        // P2SPConfigs::UPLOAD_MAX_CACHE_LENGTH = count;
        // LOGX(__DEBUG, "upload", "MaxUploadCacheSizeInMB = " << nMaxUploadCacheSizeInMB << " Count = " << P2SPConfigs::UPLOAD_MAX_CACHE_LENGTH);
    }

    // 设置上传开关，用于控制是否启用上传
    void P2PModule::SetUploadSwitch(bool is_disable_upload)
    {
        upload_manager_->SetUploadSwitch(is_disable_upload);
    }

    boost::uint32_t P2PModule::GetUploadBandWidthInBytes()
    {
        return upload_manager_->GetUploadBandWidthInBytes();
    }

    boost::uint32_t P2PModule::GetUploadBandWidthInKBytes()
    {
        return GetUploadBandWidthInBytes() / 1024;
    }

    bool P2PModule::NeedUseUploadPingPolicy()
    {
        return upload_manager_->NeedUseUploadPingPolicy();
    }

    LiveP2PDownloader__p P2PModule::CreateLiveP2PDownloader(const RID& rid, storage::LiveInstance__p live_instance)
    {
        if (live_rid_index_.find(rid) != live_rid_index_.end())
        {
            return live_rid_index_[rid];
        }

        LiveP2PDownloader::p live_p2p_downloader = LiveP2PDownloader::Create(rid, live_instance);
        live_p2p_downloader->Start();

        // 插入直播索引，定时器根据直播索引触发每个LiveP2PDownloader的OnP2PTimer
        live_rid_index_.insert(std::make_pair(rid, live_p2p_downloader));
        return live_p2p_downloader;
    }

    void P2PModule::OnLiveP2PDownloaderStop(LiveP2PDownloader::p p2p_downloader)
    {
        if( is_running_ == false )
            return;

        if (live_rid_index_.find(p2p_downloader->GetRid()) == live_rid_index_.end())
        {
            return;
        }

        live_rid_index_.erase(p2p_downloader->GetRid());
    }
}
