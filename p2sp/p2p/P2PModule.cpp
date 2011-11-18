//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/p2p/P2PModule.h"
#include "p2sp/AppModule.h"
#include "p2sp/p2p/P2PDownloader.h"
#include "p2sp/p2p/P2SPConfigs.h"
#include "p2sp/p2p/SessionPeerCache.h"

#ifdef NOTIFY_ON
#include "p2sp/notify/NotifyModule.h"
#endif

#ifdef DUMP_OBJECT
#include "count_object_allocate.h"
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

    P2PModule::p P2PModule::inst_;

    P2PModule::P2PModule()
        : p2p_timer_(global_250ms_timer(), 250, boost::bind(&P2PModule::OnTimerElapsed, this, &p2p_timer_))
        , is_running_(false), global_request_send_count_(0)
        , is_connection_policy_enable_(true)
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

        UploadModule::Inst()->Start(config_path);

        p2p_timer_.start();

        session_cache_ = SessionPeerCache::create();

        BootStrapGeneralConfig::Inst()->AddUpdateListener(shared_from_this());
    }

    void P2PModule::Stop()
    {
        if (is_running_ == false) return;

        UploadModule::Inst()->Stop();

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
        UploadModule::Inst()->SetUploadUserSpeedLimitInKBps(MaxUploadP2PSpeed);
    }

    boost::int32_t P2PModule::GetUploadSpeedLimitInKBps() const
    {
        if (false == is_running_) return 0;

        return UploadModule::Inst()->GetUploadSpeedLimitInKBps();
    }

    boost::int32_t P2PModule::GetMaxConnectLimitSize() const
    {
        if (!is_running_)
        {
            return 0;
        }

        return UploadModule::Inst()->GetMaxConnectLimitSize();
    }

    boost::int32_t P2PModule::GetMaxUploadLimitSize() const
    {
        if (!is_running_)
        {
            return 0;
        }

        return UploadModule::Inst()->GetMaxUploadLimitSize();
    }

    P2PDownloader::p P2PModule::CreateP2PDownloader(const RID& rid)
    {
		if (is_running_ == false)
        {
            return P2PDownloader::p();
        }

        // 如果 在 rid_indexer_ 中找到了 这个 P2PDownloader, 就是用这个 P2PDownloader
        if (rid_indexer_.find(rid) != rid_indexer_.end())
        {
            P2PDownloader::p downloader = rid_indexer_[rid];
            storage::IStorage::p storage = storage::Storage::Inst();
            storage::Instance::p instance = boost::dynamic_pointer_cast<storage::Instance>(storage->GetInstanceByRID(rid));
            assert(instance);
            assert(false == instance->GetRID().is_empty());

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

    void P2PModule::AddCandidatePeers(RID rid, const std::vector<protocol::CandidatePeerInfo>& peers, bool is_live_udpserver)
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
            p2p_downloader->AddCandidatePeers(peers, is_live_udpserver);
        }
    }

    void P2PModule::OnUdpRecv(protocol::Packet const & packet_)
    {
        if (is_running_ == false) return;

        // Upload Packet
        if (UploadModule::Inst()->TryHandlePacket(packet_))
        {
            return;
        }

        // Notify Packet
        protocol::VodPeerPacket const & packet = (protocol::VodPeerPacket const &)packet_;

#ifdef NOTIFY_ON
        if (packet.PacketAction == protocol::ConnectPacket::Action)
        {
            const protocol::ConnectPacket & connect_packet = (const protocol::ConnectPacket &)packet;
            LOGX(__DEBUG, "upload", "P2PModule::OnUdpRecv ConnectPacket connect_packet->IsRequest()" << ((protocol::ConnectPacket&)packet).IsRequest());

            // 如果是特殊RID，Notify处理
            RID spec_rid;
            const string str_rid = "00000000000000000000000000000001";
            spec_rid.from_string(str_rid);

            if (connect_packet.connect_type_ == protocol::CONNECT_NOTIFY ||
                connect_packet.resource_id_ == spec_rid)
            {
                p2sp::NotifyModule::Inst()->OnUdpRecv(connect_packet);
                return;
            }
        }
#endif
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
                AppModule::Inst()->DoSendPacket(error_packet, packet.protocol_version_);
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
        UploadModule::Inst()->OnP2PTimer(times);
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

    // 设置上传开关，用于控制是否启用上传
    void P2PModule::SetUploadSwitch(bool is_disable_upload)
    {
        UploadModule::Inst()->SetUploadSwitch(is_disable_upload);
    }

    boost::uint32_t P2PModule::GetUploadBandWidthInBytes()
    {
        return UploadModule::Inst()->GetUploadBandWidthInBytes();
    }

    boost::uint32_t P2PModule::GetUploadBandWidthInKBytes()
    {
        return GetUploadBandWidthInBytes() / 1024;
    }

    bool P2PModule::NeedUseUploadPingPolicy()
    {
        return UploadModule::Inst()->NeedUseUploadPingPolicy();
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

    void P2PModule::OnConfigUpdated()
    {
        is_connection_policy_enable_ = BootStrapGeneralConfig::Inst()->IsConnectionPolicyEnable();
    }

    bool P2PModule::IsConnectionPolicyEnable()
    {
        return is_connection_policy_enable_;
    }
}
