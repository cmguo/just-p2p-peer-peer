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

enum PeerState
{
    PEERSTATE_MAIN_STATE    = 0x00010000,   // 主力形态
    PEERSTATE_RESIDE_STATE  = 0x00020000,   // 驻留形态

    PEERSTATE_LIVE_NONE     = 0x00000001,   // 直播内核无工作
    PEERSTATE_LIVE_WORKING  = 0x00000002,   // 直播内核在工作
};

boost::asio::io_service & global_io_svc();

namespace statistic
{
    class StatisticsCollectionController;
    class BufferringMonitor;
}

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
        bool http_proxy_enabled)
    {
        Guid peer_guid(guid_str);

        return p(new AppModuleStartInterface(local_udp_port, local_http_procy_port, url, port, bUseDisk, ullDiskLimit,
            disk_path, peer_guid, config_path, use_push, disk_read_only,
            http_proxy_enabled));
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
        bool http_proxy_enabled
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
        boost::function<void()> fun);

    void Stop();

    void OnUdpRecv(protocol::Packet const & packet);

    void OnPacketRecv(protocol::Packet const & packet);

    template<typename type>
    void DoSendPacket(type const & packet);

    template<typename type>
    void DoSendPacket(type const & packet,
        boost::uint16_t dest_protocol_version);

    void DoSendBuffer(boost::asio::ip::udp::endpoint& end_point, const protocol::SubPieceBuffer& buffer);

    /**
    * @brief 客户端开始下载一个 url
    * @return 已经在下载了，返回false, 否则返回true
    */
    int StartDownload(protocol::UrlInfo url_info);

    /**
     * @brief IndexServer成功根据url探测到rid 后 通知 AppModule 处理
     */
    void AttachRidInfoToUrl(string url, protocol::RidInfo rid_info, MD5 content_md5, uint32_t content_bytes, int flag);

    // 根据content请求获得返回信息
    void AttachContentStatusByUrl(const string& url, bool is_need_to_add);

    /**
    * @brief 给资源RID添加 多个 HttpServer
    */
    void AddUrlInfo(RID rid, std::vector<protocol::UrlInfo> url_info_s);

    /**
    * @brief 给资源RID添加 多个 Peer
    */
    void AddCandidatePeers(RID rid, const std::vector<protocol::CandidatePeerInfo>& peers, bool is_live_udpserver);

    boost::uint8_t GenUploadPriority();

    boost::uint8_t GetIdleTimeInMins();

    std::set<RID> GetVodResource(uint32_t mod_number, uint32_t group_count);
    std::set<RID> GetLiveResource(uint32_t mod_number, uint32_t group_count);

    Guid GetPeerGuid() const
    {
        return guid_;
    }

    Guid GetUniqueGuid() const
    {

        return unique_guid_;
    }

    uint32_t GetPeerVersion() const
    {
        return protocol::PEER_VERSION;
    }

    uint32_t GetPeerState() const
    {
        return peer_state_;
    }

    void SetPeerState(uint32_t ps)
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

        return tcp_server_.GetTcpPort();
    }

    void SetUpnpPortForTcpUpload(boost::uint16_t upnp_port)
    {
        upnp_port_ = upnp_port;
    }

    boost::uint16_t GetUpnpPortForTcpUpload();

    void DoAddUrlRid(protocol::UrlInfo url_info, protocol::RidInfo rid_info, MD5 content_md5, uint32_t content_bytes, int flag);

    // 上传统计信息
    void SubmitToDataCollectionServer();

    bool IsRunning() const
    {
        return is_running_;
    }
    
    void RegisterAllPackets();

    void RegisterTcpPackets();

    static protocol::VERSION_INFO GetKernelVersionInfo();

    // make
    static string MakeUrlByRidInfo(const protocol::RidInfo& rid_info, uint32_t version = 1);

    static void AllocSubPieceConent()
    {

    }

    boost::shared_ptr<statistic::BufferringMonitor> CreateBufferringMonitor(const RID& rid);

    private:
    /**
    * @brief 用url来索引的 Instance 实例
    */
    // map<string, Instance::p> url_indexer_;

    /**
    * @brief 用rid来索引的 Instance 实例
    */
    // map<RID, Instance::p> rid_indexer_;

    /**
    * @biref 储存所有的 Instances 实例8
    */
    // set<Instance::p> instance_set_;
    protocol::UdpServer::pointer udp_server_;

    Guid guid_;
    Guid unique_guid_;

    volatile bool is_running_;

    uint32_t peer_state_;

    boost::shared_ptr<statistic::StatisticsCollectionController> statistics_collection_controller_;

    network::TcpServer tcp_server_;
    network::TcpServer tcp_server_843_; // 监听843端口，为flash p2p提供socket master policy file.

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
        statistic::UploadStatisticModule::Inst()->SubmitUploadSpeedInfo(packet.end_point.address(), packet.length());
        statistic::StatisticModule::Inst()->SubmitUploadDataBytes(sub_piece_pack.sub_piece_length_);
        statistic::UploadStatisticModule::Inst()->SubmitUploadOneSubPiece();
    }
    else if (type::Action == protocol::LiveSubPiecePacket::Action)
    {
        protocol::LiveSubPiecePacket const & live_sub_piece_pack = (protocol::LiveSubPiecePacket const &)packet;
        statistic::UploadStatisticModule::Inst()->SubmitUploadSpeedInfo(packet.end_point.address(), packet.length());
        statistic::StatisticModule::Inst()->SubmitUploadDataBytes(live_sub_piece_pack.sub_piece_length_);
        statistic::UploadStatisticModule::Inst()->SubmitUploadOneSubPiece();
    }

    udp_server_->send_packet(packet, dest_protocol_version);
}
}

#endif  // APP_MODULE_H
