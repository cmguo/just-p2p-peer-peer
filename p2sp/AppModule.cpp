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
#include "network/tcp/CrossDomainConfig.h"
#include "network/upnp/UpnpModule.h"
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
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_appmodule = log4cplus::Logger::getInstance("[app_module]");
#endif

    AppModule::p AppModule::inst_;

    AppModule::AppModule()
        : is_running_(false)
    {
    }

    VERSION_INFO AppModule::GetKernelVersionInfo() { return VERSION_INFO(PEER_KERNEL_VERSION); }

    bool AppModule::Start(
        boost::asio::io_service & io_svc,
        AppModuleStartInterface::p appmodule_start_interface,
        boost::function<void()> fun,
        boost::uint16_t * local_http_port)
    {
        if (is_running_ == true)
        {
            fun();
            return true;
        }

        is_running_ = true;

        ((framework::timer::AsioTimerManager &)global_second_timer()).start();
        ((framework::timer::AsioTimerManager &)global_250ms_timer()).start();
        
        LOG4CPLUS_DEBUG_LOG(logger_appmodule, "PeerVersion " << PEER_KERNEL_VERSION_STR);
        LOG4CPLUS_DEBUG_LOG(logger_appmodule, "AppModule::Start");

        peer_state_ = (PEERSTATE_MAIN_STATE | PEERSTATE_LIVE_NONE);

        // config
        P2SPConfigs::LoadConfig();

#ifdef USE_MEMORY_POOL
        protocol::SubPieceContent::set_pool_capacity(appmodule_start_interface->memory_pool_size_in_MB_ * 1024 * 1024);
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
            LOG4CPLUS_DEBUG_LOG(logger_appmodule, "Proxy Module Start Failed.");
#ifdef NEED_TO_POST_MESSAGE
            WindowsMessage::Inst().PostWindowsMessage(UM_STARTUP_FAILED, NULL, NULL);
#endif
            return false;
        }

        * local_http_port = ProxyModule::Inst()->GetHttpPort();
        StatisticModule::Inst()->SetHttpProxyPort(ProxyModule::Inst()->GetHttpPort());

        // 启动Udp服务器
        udp_server_.reset(new protocol::UdpServer(io_svc, shared_from_this()));

        boost::uint16_t local_udp_port = appmodule_start_interface->local_udp_port_;
        boost::uint16_t try_count = 0;

        while (false == udp_server_->Listen(local_udp_port))
        {
            local_udp_port ++;
            try_count++;
            LOG4CPLUS_WARN_LOG(logger_appmodule, "Udp Listen To port " << local_udp_port << 
                "Failed, so local_udp_port++");
            if (local_udp_port >= 65534 || try_count >= 1000)
            {
                LOG4CPLUS_ERROR_LOG(logger_appmodule, "Udp Listen To port " << local_udp_port << " So Failed");
                udp_server_->Close();
#ifdef NEED_TO_POST_MESSAGE
                WindowsMessage::Inst().PostWindowsMessage(UM_STARTUP_FAILED, NULL, NULL);
#endif
                return false;
            }
        }
        LOG4CPLUS_DEBUG_LOG(logger_appmodule, "UdpServer Listening on port: " << local_udp_port);

        is_running_ = true;

        RegisterAllPackets();

        udp_server_->Recv(40);

        // StorageModule
        LOG4CPLUS_DEBUG_LOG(logger_appmodule, "Begin to Start Storage Module.");
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

        // 启动TrackerManager模块  开启tracker模块是会做第一次commit

        bool need_report = appmodule_start_interface->bUseDisk_;

        P2PModule::Inst()->Start(appmodule_start_interface->config_path_);

        StunModule::Inst()->Start(appmodule_start_interface->config_path_);

        // 启动TCP Server
        tcp_server_.reset(new network::TcpServer(global_io_svc()));

        RegisterTcpPackets();

        // 记录端口，汇报
        upnp_port_ = 0;

        for (boost::uint32_t  port = 16000; port <=16010; port++)
        {
            if (tcp_server_->Start(port))
            {
#ifdef NEED_TO_POST_MESSAGE
                WindowsMessage::Inst().PostWindowsMessage(UM_INTERNAL_TCP_PORT_SUCCED, (WPARAM)0, (LPARAM)port);
#endif
                break;
            }
        }
#ifdef PEER_PC_CLIENT
        // 监听843端口，为flash p2p提供socket master policy file.
        // 参考http://macromedia.com/cn/devnet/flashplayer/articles/fplayer9_security.html
        tcp_server_843_.reset(new network::TcpServer(global_io_svc()));
        tcp_server_843_->Start(843);
#endif
        //绑定的端口都知道了，可以开始映射了
        map<uint16_t,uint16_t> tcpport,udpport;
        tcpport[tcp_server_->GetTcpPort()] = 0;
        tcpport[tcp_server_843_->GetTcpPort()] = tcp_server_843_->GetTcpPort();
        udpport[local_udp_port] = 0;
        UpnpModule::Inst()->Start();   

        UpnpModule::Inst()->AddUpnpPort(tcpport,udpport);

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
            PushModule::Inst()->Start(appmodule_start_interface->config_path_);
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

        //读取crossdomain配置文件
        network::CrossDomainConfig::GetInstance()->LoadConfig();

        fun();

        LOG4CPLUS_DEBUG_LOG(logger_appmodule, "Start Finish!");

        return true;
    }

    void AppModule::Stop()
    {
        if (is_running_ == false)
        {
            // framework::MainThread::Inst().SendWindowsMessage(UM_CORE_STOP, (WPARAM)0, (LPARAM)0);
            return;
        }

        is_running_ = false;

        ((framework::timer::AsioTimerManager &)global_second_timer()).stop();
        ((framework::timer::AsioTimerManager &)global_250ms_timer()).stop();

        if (tcp_server_)
        {
            tcp_server_->Stop();
        }

        if (tcp_server_843_)
        {
            tcp_server_843_->Stop();
        }

        LOG4CPLUS_INFO_LOG(logger_appmodule, "AppModule is stopping...");

#ifdef DISK_MODE
        PushModule::Inst()->Stop();
#endif

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
        lpCoreStopData->ulTotalP2pDownloadBytes = 0;
        lpCoreStopData->ulTotalOtherServerDownloadBytes = 0;
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

        UpnpModule::Inst()->Stop();

        StatisticModule::Inst()->Stop();

        // 启动UploadStatistic模块
        UploadStatisticModule::Inst()->Stop();

        // 启动DACStatisticModule模块
        DACStatisticModule::Inst()->Stop();

        LOG4CPLUS_INFO_LOG(logger_appmodule, "Storage::Inst()->Stop()");
        if (Storage::Inst())
        {
            // Storage有可能为空，如果9000端口或UDP 5041监听失败
            // Storage并没有创建
            Storage::Inst()->Stop();
        }

#ifdef NOTIFY_ON
        p2sp::NotifyModule::Inst()->Stop();
#endif

        if (statistics_collection_controller_)
        {
            statistics_collection_controller_->Stop();
        }
        
        if (udp_server_)
        {
            udp_server_->Close();
            udp_server_.reset();
        }

        LOG4CPLUS_INFO_LOG(logger_appmodule, "AppModule has stopped.");

        inst_.reset();

#ifdef NEED_TO_POST_MESSAGE
        WindowsMessage::Inst().PostWindowsMessage(UM_CORE_STOP, (WPARAM)0, (LPARAM)lpCoreStopData);
#endif
    }

    void AppModule::AddCandidatePeers(RID rid, const std::vector<protocol::CandidatePeerInfo>& peers, bool is_live_udpserver)
    {
        if (false == is_running_)
        {
            return;
        }
        LOG4CPLUS_DEBUG_LOG(logger_appmodule, "Add Candidate Peers: ");
        // 1.在 map<RID, Instance::p> rid_indexer 找到 rid 对应的 Instance
        //     如果找不到ppassert(0);
        //     如果找到
        //        Instanse->AddPeers(peers);
        P2PModule::Inst()->AddCandidatePeers(rid, peers, is_live_udpserver);
    }

    void AppModule::OnUdpRecv(protocol::Packet const & packet)
    {
        OnPacketRecv(packet);
    }

    void AppModule::OnPacketRecv(Packet const & packet)
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
            (packet.PacketAction >= 0x20 && packet.PacketAction  < 0x30) ||
            packet.PacketAction >= 0x40 && packet.PacketAction < 0x50)
        {
            IndexManager::Inst()->OnUdpRecv((protocol::ServerPacket const &)packet);
            return;
        }

        // TrackerServer 相关协议
        if (packet.PacketAction >= 0x30 && packet.PacketAction  < 0x40)
        {
            TrackerModule::Inst()->OnUdpRecv((protocol::ServerPacket const &)packet);
            return;
        }

        // Peer 相关协议
        if (packet.PacketAction  >= 0x50 && packet.PacketAction < 0x70 ||
            packet.PacketAction >= 0xC0 && packet.PacketAction < 0xC5 ||
            packet.PacketAction >= 0xB0 && packet.PacketAction < 0xC0)
        {
            P2PModule::Inst()->OnUdpRecv((protocol::Packet const &)packet);
            return;
        }

        if ((packet.PacketAction  >= 0x70 && packet.PacketAction  < 0xA0) ||
            (packet.PacketAction >= 0x81 && packet.PacketAction <= 0x83))
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

        if (packet.PacketAction >= 0xD0 && packet.PacketAction < 0xE0)
        {
            ProxyModule::Inst()->OnUdpRecv((protocol::Packet const &)packet);
            return;
        }
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

    boost::uint8_t AppModule::GenUploadPriority()
    {
        if (false == is_running_)
        {
            return 0;
        }

        IStorage::p p_storage = Storage::Inst();

        if (!p_storage)
        {
            return 0;
        }

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
        LOG4CPLUS_DEBUG_LOG(logger_appmodule, __FUNCTION__ << ":" << __LINE__ << " UploadPriority = " << 
            (uint32_t)upload_priority << ", " << used_disk_space << " / " << max_store_size);

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
        register_push_packetv3(*udp_server_);
        register_natcheck_packet(*udp_server_);
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

    void AppModule::RegisterTcpPackets()
    {
        tcp_server_->RegisterPacket<protocol::TcpAnnounceRequestPacket>();
        tcp_server_->RegisterPacket<protocol::TcpAnnounceResponsePacket>();
        tcp_server_->RegisterPacket<protocol::TcpSubPieceRequestPacket>();
        tcp_server_->RegisterPacket<protocol::TcpSubPieceResponsePacket>();
        tcp_server_->RegisterPacket<protocol::TcpReportSpeedPacket>();
        tcp_server_->RegisterPacket<protocol::TcpErrorPacket>();
        tcp_server_->RegisterPacket<protocol::TcpReportStatusPacket>();
        tcp_server_->RegisterPacket<protocol::TcpStartDownloadPacket>();
        tcp_server_->RegisterPacket<protocol::TcpStopDownLoadPacket>();
    }

    boost::uint16_t AppModule::GetUpnpPortForTcpUpload()
    {
        if (StunModule::Inst()->GetPeerNatType() == protocol::TYPE_PUBLIC)
        {
            return GetLocalTcpPort();
        }

        return upnp_port_;
    }
}
