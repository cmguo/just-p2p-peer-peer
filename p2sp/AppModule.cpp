//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/AppModule.h"
#include "p2sp/stun/StunModule.h"

#include "p2sp/index/IndexManager.h"
#include "p2sp/index/IndexManager.h"
#include "p2sp/tracker/TrackerModule.h"
#include "p2sp/proxy/ProxyModule.h"
#include "p2sp/proxy/MessageBufferManager.h"
#include "p2sp/p2p/P2PModule.h"
#include "p2sp/p2p/P2SPConfigs.h"
#include "p2sp/push/PushModule.h"
#include "p2sp/bootstrap/BootStrapGeneralConfig.h"
#include "message.h"
#include "storage/Storage.h"
#include "statistic/StatisticModule.h"
#include "statistic/DACStatisticModule.h"
#include "statistic/UploadStatisticModule.h"
#include "statistic/StatisticsCollectionController.h"
#include "statistic/BufferringMonitor.h"
#include "statistic/StatisticsReporter.h"
#include "downloadcenter/DownloadCenterModule.h"
#ifdef AUTO_SVN_VERSION
#include "autopeerversion.hpp"
#else
#include "PeerVersion.h"
#endif

#include <boost/algorithm/string/replace.hpp>

#ifdef NOTIFY_ON
#include "p2sp/notify/NotifyModule.h"
#endif

#ifdef NEED_TO_POST_MESSAGE
#include "WindowsMessage.h"
#endif

#include <framework/timer/AsioTimerManager.h>

using namespace protocol;
using namespace statistic;
using namespace storage;

boost::asio::io_service & global_io_svc()
{
    static boost::asio::io_service * io_svc = new boost::asio::io_service();
    return *io_svc;
}

framework::timer::TimerQueue & global_second_timer()
{
    static framework::timer::AsioTimerManager timer_manager(global_io_svc(), boost::posix_time::seconds(1));
    return timer_manager;
}

framework::timer::TimerQueue & global_250ms_timer()
{
    static framework::timer::AsioTimerManager timer_manager(global_io_svc(), boost::posix_time::milliseconds(250));
    return timer_manager;
}

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("app");

    AppModule::p AppModule::inst_(new AppModule());

    AppModule::AppModule()
        : is_running_(false)
    {
    }

    VERSION_INFO AppModule::GetKernelVersionInfo() { return VERSION_INFO(PEER_KERNEL_VERSION); }

    bool AppModule::Start(
        boost::asio::io_service & io_svc,
        AppModuleStartInterface::p appmodule_start_interface)
    {
        if (is_running_ == true)
        {
            return true;
        }

        is_running_ = true;
        
        LOG(__DEBUG, "app", "PeerVersion " << PEER_KERNEL_VERSION_STR);
        LOG(__DEBUG, "app", "AppModule::Start");

        peer_state_ = (PEERSTATE_MAIN_STATE | PEERSTATE_LIVE_NONE);

        // config
        P2SPConfigs::LoadConfig();

#ifdef USE_MEMORY_POOL
#ifndef DISK_MODE
        protocol::SubPieceContent::set_pool_capacity(3 * 1024 * 1024);
#else
        protocol::SubPieceContent::set_pool_capacity(30 * 1024 * 1024);
#endif
        protocol::LiveSubPieceContent::set_pool_capacity(20 * 1024 * 1024);
        protocol::LiveSubPieceContent::pointer live_subpiece_content = new protocol::LiveSubPieceContent();
        live_subpiece_content.reset();
#endif

        // flush interval 1s
        StatisticModule::Inst()->Start(1, appmodule_start_interface->config_path_);

        // local ips

        std::vector<uint32_t> local_ips;
        if (appmodule_start_interface->bUseDisk_ == true)
        {
            // LoadLocalIPs(local_ips);
        }

        StatisticModule::Inst()->SetLocalIPs(local_ips);

        unique_guid_ = appmodule_start_interface->peer_guid_;
        if (unique_guid_.is_empty())
        {
            unique_guid_.generate();
        }
        guid_.generate();

        // ResourceMap::LoadFromDisk();

        // 加载完成  instance_set_ url_indexr_ rid_indexer

        // 启动 ProxyModule 模块
        if (appmodule_start_interface->http_proxy_enabled_) {
            ProxyModule::CreateInst(io_svc)->Start(appmodule_start_interface->config_path_, appmodule_start_interface->local_http_procy_port_);
        } else {
            ProxyModule::CreateInst(io_svc)->Start(appmodule_start_interface->config_path_, 0);
        }

        if (false == ProxyModule::Inst()->IsRunning())
        {
            LOG(__DEBUG, "app", "Proxy Module Start Failed.");
#ifdef NEED_TO_POST_MESSAGE
            WindowsMessage::Inst().PostWindowsMessage(UM_STARTUP_FAILED, NULL, NULL);
#endif
            return false;
        }

        StatisticModule::Inst()->SetHttpProxyPort(ProxyModule::Inst()->GetHttpPort());

        // 启动Udp服务器
        udp_server_.reset(new protocol::UdpServer(io_svc, this));

        boost::uint16_t local_udp_port = appmodule_start_interface->local_udp_port_;
        boost::uint16_t try_count = 0;

        while (false == udp_server_->Listen(local_udp_port))
        {
            local_udp_port ++;
            try_count++;
            LOG(__WARN, "app", "Udp Listen To port " << local_udp_port << "Failed, so local_udp_port++");
            if (local_udp_port >= 65534 || try_count >= 1000)
            {
                LOG(__ERROR, "app", "Udp Listen To port " << local_udp_port << " So Failed");
                udp_server_->Close();
#ifdef NEED_TO_POST_MESSAGE
                WindowsMessage::Inst().PostWindowsMessage(UM_STARTUP_FAILED, NULL, NULL);
#endif
                return false;
            }
        }
        LOG(__DEBUG, "app", "UdpServer Listening on port: " << local_udp_port);

        is_running_ = true;

        RegisterAllPackets();

        udp_server_->Recv(40);

        // StorageModule
        LOG(__DEBUG, "app", "Begin to Start Storage Module.");
        uint32_t storage_mode =
            (appmodule_start_interface->disk_read_only_ ? STORAGE_MODE_READONLY : STORAGE_MODE_NORMAL);

        Storage::CreateInst(io_svc)->Start(
            appmodule_start_interface->bUseDisk_,
            appmodule_start_interface->ullDiskLimit_,
            appmodule_start_interface->disk_path_,
            appmodule_start_interface->config_path_,
            storage_mode
            );

        // 启动UploadStatistic模块
        UploadStatisticModule::Inst()->Start();

        // 启动DACStatisticModule模块
        DACStatisticModule::Inst()->Start();

        // 启动IndexServer模块
        IndexManager::CreateInst(io_svc)->Start(appmodule_start_interface->url_, appmodule_start_interface->port_);

        // LOG(__EVENT, "index", "Index Module has started successfully.");
#ifdef DISK_MODE
        downloadcenter::DownloadCenterModule::Inst()->Start(1000);
#endif  // #ifdef DISK_MODE
        // 启动TrackerManager模块  开启tracker模块是会做第一次commit

        bool need_report = appmodule_start_interface->bUseDisk_;

        P2PModule::Inst()->Start(appmodule_start_interface->config_path_);

        StunModule::Inst()->Start(appmodule_start_interface->config_path_);

        TrackerModule::Inst()->Start(appmodule_start_interface->config_path_);

        StatisticModule::Inst()->SetLocalPeerUdpPort(local_udp_port);
        StatisticModule::Inst()->SetLocalPeerVersion(protocol::PEER_VERSION);

#ifdef NEED_TO_POST_MESSAGE
        uint32_t port = MAKELONG(local_udp_port, 0);
        WindowsMessage::Inst().PostWindowsMessage(UM_STARTUP_SUCCED, (WPARAM)ProxyModule::Inst()->GetHttpPort(), (LPARAM)port);
#endif


#ifdef DISK_MODE
        if (appmodule_start_interface->use_push_) 
        {
            PushModule::Inst()->Start();
        }
#endif  // #ifdef DISK_MODE

        // 启动Notify模块
#ifdef NOTIFY_ON
        p2sp::NotifyModule::Inst()->Start();
#endif

        BootStrapGeneralConfig::Inst()->Start(appmodule_start_interface->config_path_);

        statistics_collection_controller_.reset(
            new statistic::StatisticsCollectionController(
                BootStrapGeneralConfig::Inst(), 
                appmodule_start_interface->config_path_));

        statistics_collection_controller_->Start();
        
        LOG(__DEBUG, "app", "Start Finish!");

        return true;
    }

    void AppModule::Stop()
    {
        if (is_running_ == false)
        {
            // framework::MainThread::Inst().SendWindowsMessage(UM_CORE_STOP, (WPARAM)0, (LPARAM)0);
            return;
        }

        LOG(__EVENT, "app", "AppModule is stopping...");

        // PushModule::Inst()->Stop();

        // 停止 ProxyModule 模块
        ProxyModule::Inst()->Stop();

        // 从共享内存中读取数据，然后向客户端发送 UM_CORE_STOP Windows消息
        // 计算需要发出的消息的参数
        CORESTOPDATA* lpCoreStopData = MessageBufferManager::Inst()->NewStruct<CORESTOPDATA>();
        memset(lpCoreStopData, 0, sizeof(CORESTOPDATA));
        lpCoreStopData->uSize = sizeof(lpCoreStopData);
        lpCoreStopData->fStoreUtilizationRatio = 0.0;
        lpCoreStopData->uCurrLocalResourceCount = 0;
        lpCoreStopData->uCurrUploadResourceCount = 0;
        lpCoreStopData->bIndexServerConnectSuccess = false;
        lpCoreStopData->uPublicIP = StatisticModule::Inst()->GetLocalDetectSocketAddress().IP;
        lpCoreStopData->bNATPeer = (StatisticModule::Inst()->GetLocalPeerAddress().IP != StatisticModule::Inst()->GetLocalDetectSocketAddress().IP);
        lpCoreStopData->usNATType = StunModule::Inst()->GetPeerNatType();
        lpCoreStopData->usNATKeeplivePeriods = 10*1000;
        lpCoreStopData->ulTotalDownloadBytes = StatisticModule::Inst()->GetSpeedInfo().TotalDownloadBytes;
        lpCoreStopData->ulTotalUploadBytes = StatisticModule::Inst()->GetSpeedInfo().TotalUploadBytes;
        lpCoreStopData->ulTotalP2pDownloadBytes = StatisticModule::Inst()->GetStatisticInfo().TotalP2PDownloadBytes;
        lpCoreStopData->ulTotalOtherServerDownloadBytes = StatisticModule::Inst()->GetStatisticInfo().TotalOtherServerDownloadBytes;
        // 停止本地Udp服务器
        // 停止IndexServer模块

        if (IndexManager::Inst())
        {
            // IndexManager有可能为空，如果9000端口监听失败
            // IndexManager并没有创建
            IndexManager::Inst()->Stop();
        }

        TrackerModule::Inst()->PPLeave();
        TrackerModule::Inst()->Stop();

        P2PModule::Inst()->Stop();

        StunModule::Inst()->Stop();

        StatisticModule::Inst()->Stop();

        // 启动UploadStatistic模块
        UploadStatisticModule::Inst()->Stop();

        // 启动DACStatisticModule模块
        DACStatisticModule::Inst()->Stop();

        LOGX(__EVENT, "app", "Storage::Inst()->Stop()");
        if (Storage::Inst())
        {
            // Storage有可能为空，如果9000端口或UDP 5041监听失败
            // Storage并没有创建
            Storage::Inst()->Stop();
        }

#ifdef DISK_MODE
        downloadcenter::DownloadCenterModule::Inst()->Stop();
#endif  // #ifdef DISK_MODE

        if (statistics_collection_controller_)
        {
            statistics_collection_controller_->Stop();
        }
        
        is_running_ = false;

        if (udp_server_)
        {
            udp_server_->Close();
            udp_server_.reset();
        }

        LOG(__EVENT, "app", "AppModule has stopped.");
        inst_.reset();

#ifdef NEED_TO_POST_MESSAGE
        WindowsMessage::Inst().PostWindowsMessage(UM_CORE_STOP, (WPARAM)0, (LPARAM)lpCoreStopData);
#endif
    }

    int AppModule::StartDownload(protocol::UrlInfo url_info)
    {
        // 1.在map<string, Instance::p> url_indexer_中查找是否已经存在
        //   如果不存在则
        //          创建Instance(url_info_)
        //       将这个Instance添加到 map<string, Instance::p> url_indexer_ 中
        //       将这个Instance添加到 set<Instance::p> instance_set_ 中
        //   如果存在则
        //       不管
        //
        // 2.Instance 是否正在下载
        //       如果不是，将Instance置为 正在下载
        //       如果是    返回
        //
        // 3.保存 instance_set_ 回磁盘
        //       ResourceMap::AsyncSaveToDisk();
        return 0;
    }

    void AppModule::DoSendBuffer(boost::asio::ip::udp::endpoint& end_point, const protocol::SubPieceBuffer& buffer)
    {
        if (is_running_ == false)
            return;

        // StatisticModule::Inst()->SubmitUploadedBytes(buffer.Length());

        // udp_server_->UdpSendTo(buffer, end_point);
    }

    void AppModule::AttachRidInfoToUrl(string url, protocol::RidInfo rid_info, MD5 content_md5, uint32_t content_bytes, int flag)
    {
        // ppassert(在map<string, Instance::p> url_indexer_中 已经存在 该url)
        // 1. 从 url_indexer_ 找到 url 对应的 Instance
        // 2. map<RID, Instance::p> rid_indexer; 是否找到RID
        //      如果找不到 则
        //       Instance->SetRidInfo(rid_info);
        //       将这个Instance添加到 map<RID, Instance::p> rid_indexer_ 中
        //    如果找到
        //       (比较复杂 已解决)
        //       判断该Instanse是否存在
        //            如果存在则直接返回
        //            如果不存在，将该 RID, Instanse添加到rid_indexer中，
        //       情况1:
        //         IndexServer 发送超时 收到两次
        //       情况2:
        //         两个url对应了一个rid, 这时要做归并
        //
        // 3.保存 instance_set_ 回磁盘
        //       ResourceMap::AsyncSaveToDisk();
        if (false == is_running_)
        {
            return;
        }
        Storage::Inst()->AttachRidByUrl(url, rid_info, content_md5, content_bytes, flag);
    }
    void AppModule::AttachContentStatusByUrl(const string& url, bool is_need_to_add)
    {
        if (false == is_running_)
        {
            return;
        }
        Storage::Inst()->AttachContentStatusByUrl(url, is_need_to_add);
    }

    void AppModule::AddUrlInfo(RID rid, std::vector<protocol::UrlInfo> url_info_s)
    {
        // 1. 在 map<RID, Instance::p> rid_indexer 找到 rid 对应的 Instance
        //    如果找不到
        //       ppassert(0);
        //    如果找得到
        //       遍历所有 url_info_s
        //            Instance->AddUrlInfo(url_info)
        //            map<string, Instance::p> url_indexer_[url] = Instance A
        //             如果 url_indexer_ 已经存在这个url, (Instanse B)
        //                (比较复杂 已解决)
        //                  如果其对应的Instanse A就是当前Instanse B，则不管。
        //                  如果不是，则stop该url对应的Instanse B，将该url对应现在的Instanse A;
        //
        //
        // 3.保存 instance_set_ 回磁盘
        //       ResourceMap::AsyncSaveToDisk();
        if (false == is_running_)
        {
            return;
        }
        LOG(__EVENT, "packet", "AppModule::AddUrlInfo   RID:" << rid);
        for (uint32_t i = 0; i < url_info_s.size(); i ++)
            LOG(__EVENT, "packet", "AppModule::AddUrlInfo     url:" << url_info_s[i].url_);

        Storage::Inst()->AttachHttpServerByRid(rid, url_info_s);
    }

    void AppModule::AddCandidatePeers(RID rid, const std::vector<protocol::CandidatePeerInfo>& peers)
    {
        if (false == is_running_)
        {
            return;
        }
        LOG(__DEBUG, "tracker", "Add Candidate Peers: ");
        // LOG(__DEBUG, "tracker", "RID: " << rid << " Peers: " << peers);

        // 1.在 map<RID, Instance::p> rid_indexer 找到 rid 对应的 Instance
        //     如果找不到ppassert(0);
        //     如果找到
        //        Instanse->AddPeers(peers);
        P2PModule::Inst()->AddCandidatePeers(rid, peers);
    }

    void AppModule::OnUdpRecv(Packet const & packet)
    {
        if (false == is_running_)
        {
            return;
        }

        // Push相关协议
        if (packet.PacketAction >= 0x1A && packet.PacketAction < 0x20)
        {
#ifdef DISK_MODE
            PushModule::Inst()->OnUdpRecv((protocol::ServerPacket const &)packet);
#endif
            return;
        }

        // IndexServer 相关协议
        if ((packet.PacketAction >= 0x10 && packet.PacketAction  < 0x1A) ||
            (packet.PacketAction >= 0x20 && packet.PacketAction  < 0x30))
        {
            IndexManager::Inst()->OnUdpRecv((protocol::ServerPacket const &)packet);
            return;
        }

        // TrackerServer 相关协议
        if (packet.PacketAction >= 0x30 && packet.PacketAction  < 0x50)
        {
            TrackerModule::Inst()->OnUdpRecv((protocol::ServerPacket const &)packet);
            return;
        }

        // Peer 相关协议
        if (packet.PacketAction  >= 0x50 && packet.PacketAction < 0x70||
            packet.PacketAction >= 0xC0 && packet.PacketAction < 0xC5)
        {
            P2PModule::Inst()->OnUdpRecv((protocol::Packet const &)packet);
            return;
        }

        if (packet.PacketAction  >= 0x70 && packet.PacketAction  < 0xA0)
        {
            // Instance->OnUdpRecv(peer_packet, buffer, end_point);
            StunModule::Inst()->OnUdpRecv((protocol::Packet const &)packet);
            return;
        }
#ifdef NOTIFY_ON
        if (packet.PacketAction >= 0xA0 && packet.PacketAction < 0xB0)
        {
            // Notify

            NotifyModule::Inst()->OnUdpRecv((protocol::Packet const &)packet);
            return;
        }
#endif
    }

    std::set<RID> AppModule::GetVodResource(uint32_t mod_number, uint32_t group_count)
    {
        std::set<RID> s;
        if (false == is_running_)
        {
            return s;
        }
        Storage::Inst()->GetVodResources(s, mod_number, group_count);
        return s;
    }

    std::set<RID> AppModule::GetLiveResource(uint32_t mod_number, uint32_t group_count)
    {
        std::set<RID> s;
        if (false == is_running_)
        {
            return s;
        }
        Storage::Inst()->GetLiveResources(s, mod_number, group_count);
        return s;
    }

    void AppModule::DoAddUrlRid(protocol::UrlInfo url_info, protocol::RidInfo rid_info, MD5 content_md5, uint32_t content_bytes, int flag)
    {
        if (false == is_running_)
        {
            return;
        }
        IndexManager::Inst()->DoAddUrlRid(url_info, rid_info, content_md5, content_bytes, flag);
    }

    protocol::CandidatePeerInfo AppModule::GetCandidatePeerInfo() const
    {
        if (false == is_running_)
        {
            return protocol::CandidatePeerInfo();
        }
        return StatisticModule::Inst()->GetLocalPeerInfo();
    }

    protocol::PEER_DOWNLOAD_INFO AppModule::GetPeerDownloadInfo(const RID& rid) const
    {
        if (false == is_running_)
        {
            return protocol::PEER_DOWNLOAD_INFO();
        }
        return StatisticModule::Inst()->GetLocalPeerDownloadInfo(rid);
    }

    protocol::PEER_DOWNLOAD_INFO AppModule::GetPeerDownloadInfo() const
    {
        if (false == is_running_)
        {
            return protocol::PEER_DOWNLOAD_INFO();
        }
        return StatisticModule::Inst()->GetLocalPeerDownloadInfo();
    }

    string AppModule::MakeUrlByRidInfo(const protocol::RidInfo& rid_info, uint32_t version)
    {
        if (true == rid_info.GetRID().is_empty())
            return "";

        std::stringstream oss;
        oss << "version=" << version;
        oss << "&filelength=" << rid_info.GetFileLength();
        oss << "&blocknum=" << rid_info.GetBlockCount();
        oss << "&blocksize=" << rid_info.GetBlockSize();
        oss << "&blockmd5=";
        for (uint32_t i = 0; i < rid_info.block_md5_s_.size(); ++i)
            oss << (i == 0?"":"@") << rid_info.block_md5_s_[i].to_string();
        return oss.str();
    }

    boost::uint8_t AppModule::GenUploadPriority()
    {
        if (false == is_running_)
        {
            return 0;
        }

        IStorage::p p_storage = Storage::Inst();

        boost::int64_t max_store_size = 2*1024*1024;
        max_store_size = max_store_size * 1024;

        boost::uint8_t upload_priority = 0;

        boost::uint64_t used_disk_space = 0;
#ifdef DISK_MODE
        max_store_size = p_storage->GetStoreSize() > max_store_size ?
            p_storage->GetStoreSize() : max_store_size;

        used_disk_space = p_storage->GetUsedDiskSpace();

        upload_priority = static_cast<boost::uint8_t>(
            used_disk_space * 255 / max_store_size
           );
#endif  // #ifdef DISK_MODE

        (void)used_disk_space;
        LOG(__DEBUG, "app", __FUNCTION__ << ":" << __LINE__ << " UploadPriority = " << (uint32_t)upload_priority << ", " << used_disk_space << " / " << max_store_size);

        return upload_priority;
    }

    boost::uint8_t AppModule::GetIdleTimeInMins()
    {
        if (false == is_running_) {
            return 0;
        }
        storage::DTType dtype = Performance::Inst()->GetCurrDesktopType();
        if (DT_SCREEN_SAVER == dtype) {
            return (boost::uint8_t)0xFFu;
        }
        if (DT_WINLOGON == dtype && Performance::Inst()->IsIdle(1)) {
            return (boost::uint8_t)0xFFu;
        }
        uint32_t idleTimeInMins = static_cast<uint32_t>(Performance::Inst()->GetIdleInSeconds()/60.0 + 0.5);
        LIMIT_MAX(idleTimeInMins, 0xFEu);
        return (boost::uint8_t)idleTimeInMins;
    }

    void AppModule::RegisterAllPackets()
    {
        if (false == is_running_)
            return;
        register_index_packet(*udp_server_);
        register_peer_packet(*udp_server_);
        register_stun_packet(*udp_server_);
        register_tracker_packet(*udp_server_);
        register_bootstrap_packet(*udp_server_);
        register_notify_packet(*udp_server_);
        register_live_peer_packet(*udp_server_);
        register_push_packet(*udp_server_);
    }

    boost::shared_ptr<statistic::BufferringMonitor> AppModule::CreateBufferringMonitor(const RID& rid)
    {
        boost::shared_ptr<statistic::StatisticsReporter> reporter;
        if (statistics_collection_controller_)
        {
            reporter = statistics_collection_controller_->GetStatisticsReporter();
        }

        boost::shared_ptr<statistic::BufferringMonitor> bufferring_monitor;
        if (reporter)
        {
            bufferring_monitor.reset(new statistic::BufferringMonitor(rid, reporter));
        }
        
        return bufferring_monitor;
    }
}
