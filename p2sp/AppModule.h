//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef APP_MODULE_H
#define APP_MODULE_H

/**
* @file
* @brief AppModule 类的包含文件
*/
// const int LOCAL_PEER_VERSION = 0x00000001;

#include "protocol/UdpServer.h"
#include "protocol/Packet.h"
#include "protocol/PeerPacket.h"
#include "statistic/UploadStatisticModule.h"
#include "statistic/StatisticModule.h"

#include "network/tcp/TcpServer.h"
#include <boost/thread.hpp>

enum PeerState
{
    PEERSTATE_MAIN_STATE    = 0x00010000,   // 主力形态
    PEERSTATE_RESIDE_STATE  = 0x00020000,   // 驻留形态
};

boost::asio::io_service & global_io_svc();

namespace statistic
{
    class StatisticsCollectionController;
    class BufferringMonitor;
}

#ifndef PEER_PC_CLIENT
void SubmitStopLog(string dac);
typedef
void (*LPSUBMITSTOPLOG)(string dac);
#endif

namespace p2sp
{
class AppModuleStartInterface
    : public boost::noncopyable
    , public boost::enable_shared_from_this<AppModuleStartInterface>
{
    public:
    typedef boost::shared_ptr<AppModuleStartInterface> p;
    static p create(
        boost::uint16_t local_udp_port,
        boost::uint16_t local_http_procy_port,
        string url,  // IndexServer
        boost::uint16_t port, bool bUseDisk, boost::uint64_t ullDiskLimit, string disk_path, 
        string guid_str, string config_path, bool use_push, bool disk_read_only,
        bool http_proxy_enabled, 
#ifndef PEER_PC_CLIENT
        LPSUBMITSTOPLOG submit_stop_log,
#endif
        boost::uint8_t memory_pool_size_in_MB)
    {
        Guid peer_guid(guid_str);
        assert(config_path.length() != 0);

        return p(new AppModuleStartInterface(local_udp_port, local_http_procy_port, url, port, bUseDisk, ullDiskLimit,
            disk_path, peer_guid, config_path, use_push, disk_read_only,
            http_proxy_enabled,
#ifndef PEER_PC_CLIENT
            submit_stop_log,
#endif
            memory_pool_size_in_MB));
    }
    public:
    boost::uint16_t local_udp_port_;
    boost::uint16_t local_http_procy_port_;
    string url_;
    boost::uint16_t port_;
    bool bUseDisk_;
    boost::uint64_t ullDiskLimit_;
    string disk_path_;
    Guid peer_guid_;
    string config_path_;
    bool use_push_;
    bool disk_read_only_;
    bool http_proxy_enabled_;
#ifndef PEER_PC_CLIENT
    LPSUBMITSTOPLOG submit_stop_log_;
#endif
    boost::uint8_t memory_pool_size_in_MB_;

    private:
    AppModuleStartInterface(
        boost::uint16_t local_udp_port,
        boost::uint16_t local_http_procy_port,
        string url,  // IndexServer
        boost::uint16_t port,
        bool bUseDisk,
        boost::uint64_t ullDiskLimit,
        string disk_path,
        Guid peer_guid,
        string config_path,
        bool use_push,
        bool disk_read_only,
        bool http_proxy_enabled,
#ifndef PEER_PC_CLIENT
        LPSUBMITSTOPLOG submit_stop_log,
#endif
        boost::uint8_t memory_pool_size_in_MB
        )
        : local_udp_port_(local_udp_port)
        , local_http_procy_port_(local_http_procy_port)
        , url_(url)
        , port_(port)
        , bUseDisk_(bUseDisk)
        , ullDiskLimit_(ullDiskLimit)
        , disk_path_(disk_path)
        , peer_guid_(peer_guid)
        , config_path_(config_path)
        , use_push_(use_push)
        , disk_read_only_(disk_read_only)
        , http_proxy_enabled_(http_proxy_enabled)
#ifndef PEER_PC_CLIENT
        , submit_stop_log_(submit_stop_log)
#endif
        , memory_pool_size_in_MB_(memory_pool_size_in_MB)
    {
    }
   ;
};

class AppModule: public boost::noncopyable,
    public boost::enable_shared_from_this<AppModule>,
    public protocol::IUdpServerListener
{
public:
    typedef boost::shared_ptr<AppModule> p;

    bool Start(
        boost::asio::io_service & io_svc,
        AppModuleStartInterface::p appmodule_start_interface,
        boost::function<void()> fun,
        boost::uint16_t * local_http_port);

    void Stop();

    void OnUdpRecv(protocol::Packet const & packet);

    void OnPacketRecv(protocol::Packet const & packet);

    template<typename type>
    void DoSendPacket(type const & packet);

    template<typename type>
    void DoSendPacket(type const & packet,
        boost::uint16_t dest_protocol_version);

    /**
    * @brief 给资源RID添加 多个 Peer
    */
    void AddCandidatePeers(RID rid, const std::vector<protocol::CandidatePeerInfo>& peers, bool is_live_udpserver);

    boost::uint8_t GenUploadPriority();

    boost::uint8_t GetIdleTimeInMins();

    std::set<RID> GetVodResource(boost::uint32_t mod_number, boost::uint32_t group_count);
    std::set<RID> GetLiveResource(boost::uint32_t mod_number, boost::uint32_t group_count);

    Guid GetPeerGuid() const
    {
        return guid_;
    }

    Guid GetUniqueGuid() const
    {

        return unique_guid_;
    }

    boost::uint32_t GetPeerVersion() const
    {
        return protocol::PEER_VERSION;
    }

    boost::uint32_t GetPeerState() const
    {
        return peer_state_;
    }

    void SetPeerState(boost::uint32_t ps)
    {
        peer_state_ = ps;
    }

    protocol::CandidatePeerInfo GetCandidatePeerInfo() const;

    protocol::PEER_DOWNLOAD_INFO GetPeerDownloadInfo(const RID& rid) const;

    protocol::PEER_DOWNLOAD_INFO GetPeerDownloadInfo() const;

    boost::uint16_t GetLocalUdpPort() const
    {
        if (is_running_ == false)
            return 0;
        if (!udp_server_)
            return 0;
        return udp_server_->GetUdpPort();
    }

    boost::uint16_t GetLocalTcpPort() const
    {
        if (!is_running_)
        {
            return 0;
        }

        return tcp_server_->GetTcpPort();
    }

    void SetUpnpPortForTcpUpload(boost::uint16_t upnp_port)
    {
        upnp_port_ = upnp_port;
    }

    boost::uint16_t GetUpnpPortForTcpUpload();

    bool IsRunning() const
    {
        return is_running_;
    }
    
    void RegisterAllPackets();

    void RegisterTcpPackets();

    static protocol::VERSION_INFO GetKernelVersionInfo();

    boost::shared_ptr<statistic::BufferringMonitor> CreateBufferringMonitor(const RID& rid);

    std::string GetInValidIpCountString()
    {
        std::map<boost::uint32_t, boost::uint32_t> invalid_map = udp_server_->GetInvalidIpCountMap();

        std::ostringstream os;

        for (std::map<boost::uint32_t, boost::uint32_t>::iterator iter = invalid_map.begin();
            iter != invalid_map.end(); ++iter)
        {
            os << iter->first << ":" << iter->second;
            
            if (iter->first != invalid_map.rbegin()->first)
            {
                os << ",";
            }
        }

        return os.str();
    }

    void ClearInvalidIpCountMap()
    {
        udp_server_->ClearInvalidIpCountMap();
    }

#ifndef PEER_PC_CLIENT
    LPSUBMITSTOPLOG submit_stop_log_;
#endif

    private:
    boost::shared_ptr<protocol::UdpServer> udp_server_;

    Guid guid_;
    Guid unique_guid_;

    volatile bool is_running_;

    boost::uint32_t peer_state_;

    boost::shared_ptr<statistic::StatisticsCollectionController> statistics_collection_controller_;

    boost::shared_ptr<network::TcpServer> tcp_server_;
    boost::shared_ptr<network::TcpServer> tcp_server_843_; // 监听843端口，为flash p2p提供socket master policy file.

    boost::uint16_t upnp_port_;

private:
    AppModule();
    static AppModule::p inst_;
public:
    static AppModule::p Inst()
    {
        if (!inst_)
        {
            inst_.reset(new AppModule());
        }
        return inst_;
    }
};

template<typename type>
void AppModule::DoSendPacket(type const & packet)
{
    if (is_running_ == false)
        return;

#define COMPILE_TIME_ASSERT(expr) typedef char assert_type[expr ? 1 : -1]

    COMPILE_TIME_ASSERT(!(type::Action >= 0x50 && type::Action < 0x60) && 
        !(type::Action >= 0xA0 && type::Action < 0xB0));
    udp_server_->send_packet(packet, protocol::PEER_VERSION);
}

template<typename type>
void AppModule::DoSendPacket(type const & packet,
    boost::uint16_t dest_protocol_version)
{
    if (is_running_ == false)
        return;

    if (type::Action == protocol::SubPiecePacket::Action)
    {
        protocol::SubPiecePacket const & sub_piece_pack = (protocol::SubPiecePacket const &)packet;
        statistic::UploadStatisticModule::Inst()->SubmitUploadSpeedInfo(packet.end_point.address(), 
            sub_piece_pack.sub_piece_length_);
        statistic::StatisticModule::Inst()->SubmitUploadDataBytes(sub_piece_pack.sub_piece_length_);

#ifndef STATISTIC_OFF
        statistic::UploadStatisticModule::Inst()->SubmitUploadOneSubPiece();
#endif
    }
    else if (type::Action == protocol::LiveSubPiecePacket::Action)
    {
        protocol::LiveSubPiecePacket const & live_sub_piece_pack = (protocol::LiveSubPiecePacket const &)packet;
        statistic::UploadStatisticModule::Inst()->SubmitUploadSpeedInfo(packet.end_point.address(), packet.length());
        statistic::StatisticModule::Inst()->SubmitUploadDataBytes(live_sub_piece_pack.sub_piece_length_);
#ifndef STATISTIC_OFF
        statistic::UploadStatisticModule::Inst()->SubmitUploadOneSubPiece();
#endif
    }

    udp_server_->send_packet(packet, dest_protocol_version);
}
}

#endif  // APP_MODULE_H
