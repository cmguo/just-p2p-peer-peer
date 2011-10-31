//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// P2PDownloader.h

#ifndef _P2SP_P2P_P2P_DOWNLOADER_H_
#define _P2SP_P2P_P2P_DOWNLOADER_H_

#include "p2sp/p2p/SubPieceRequestManager.h"
#include "p2sp/download/Downloader.h"
#include "p2sp/download/SwitchController.h"
#include "p2sp/p2p/Assigner.h"
#include "p2sp/p2p/PeerConnection.h"
#include "p2sp/p2p/DownloadSpeedLimiter.h"
#include "p2sp/AppModule.h"

#include "storage/storage_base.h"
#include "statistic/P2PDownloaderStatistic.h"
#include <bitset>

namespace storage
{
    class Instance;
    extern const boost::uint16_t subpiece_num_per_piece_g_;
}

namespace p2sp
{
    class IpPool;
    typedef boost::shared_ptr<IpPool> IpPool__p;
    class Exchanger;
    typedef boost::shared_ptr<Exchanger> Exchanger__p;
    class PeerConnector;
    typedef boost::shared_ptr<PeerConnector> PeerConnector__p;
    class Assigner;
    typedef boost::shared_ptr<Assigner> Assigner__p;
    
    class PeerConnection;
    typedef boost::shared_ptr<PeerConnection> PeerConnection__p;

    class DownloadDriver;
    typedef boost::shared_ptr<DownloadDriver> DownloadDriver__p;

    struct SerialStaticInfo
    {
        uint32_t ip_pool_size;
        uint32_t connected_count;
        uint32_t p2p_speed;
        uint32_t connect_send_count;
        uint32_t connect_err_full_count;
        uint32_t connect_err_other_count;

        SerialStaticInfo() { memset(this, 0, sizeof(SerialStaticInfo)); }
    };

    class P2PDownloader
        : public VodDownloader
        , public IP2PControlTarget
        , public boost::enable_shared_from_this<P2PDownloader>
#ifdef DUMP_OBJECT
        , public count_object_allocate<P2PDownloader>
#endif
    {
        friend class Assigner;
    public:
        typedef boost::shared_ptr<P2PDownloader> p;
        static p create(const RID& rid) { return p(new P2PDownloader(rid)); }
        virtual ~P2PDownloader();
    public:

        virtual void Start();
        virtual void Stop();

        virtual bool IsDownloading();
        virtual void PutPieceTask(const std::deque<protocol::PieceInfoEx> & piece_info_ex_s, DownloadDriver__p downloader_driver);
        virtual bool IsConnected();
        virtual bool GetUrlInfo(protocol::UrlInfo& url_info);
        virtual bool CanDownloadPiece(const protocol::PieceInfo& piece_info);
        virtual bool IsP2PDownloader() { return true; }
        virtual bool IsSupportRange() { return true; }
        virtual void StopPausing() { is_p2p_pausing_ = false; }
        virtual void SetPausing() { is_p2p_pausing_ = true; }
        virtual bool IsPausing() {return is_p2p_pausing_;}
        virtual bool HasPieceTask() const { return piece_tasks_.size() != 0; }


        void AttachDownloadDriver(DownloadDriver__p download_driver);
        void DettachDownloadDriver(DownloadDriver__p download_driver);

        void InitPeerConnection();
        void KickPeerConnection();
        void AddPeer(PeerConnection__p peer_connection);
        void DelPeer(PeerConnection__p peer_connection);

        template <typename PacketType>
        void DoSendPacket(PacketType const & packet,
            boost::uint16_t dest_protocol_version);

        void AddCandidatePeers(std::vector<protocol::CandidatePeerInfo> peers);

        void NoticeSubPiece(const protocol::SubPieceInfo& sub_piece);
        bool HasSubPiece(const protocol::SubPieceInfo& sub_piece);

        bool IsDownloadInitialization() {return start_time_counter_.elapsed() <= p2sp::P2SPConfigs::P2P_DOWNLOAD_INIT_TIMEOUT;}
        bool IsConnected(const boost::asio::ip::udp::endpoint& end_point) const;
        bool IsConnected(const Guid& peer_guid) const;
        boost::shared_ptr<storage::Instance> GetInstance() const { return instance_; }
        void SetInstance(boost::shared_ptr<storage::Instance> inst) { instance_ = inst; }
        statistic::P2PDownloaderStatistic::p GetStatistic() const { return statistic_; }

        bool HasPeer(Guid rid) {return peers_.find(rid) != peers_.end();}
        std::map<Guid, PeerConnection__p> GetPeers(){return peers_;}
        IpPool__p GetIpPool() {return ippool_;}
        Exchanger__p GetExchanger() {return exchanger_;}
        Assigner__p GetAssigner() {return assigner_;}

        virtual void SetSpeedLimitInKBps(boost::int32_t speed_limit_in_KBps);
        virtual statistic::SPEED_INFO GetSpeedInfo();
        virtual statistic::SPEED_INFO_EX GetSpeedInfoEx();
        uint32_t CalcConnectedFullBlockPeerCount();
        uint32_t CalcConnectedFullBlockActivePeerCount();
        uint32_t CalcConnectedAvailableBlockPeerCount();
        uint32_t GetConnectedFullBlockPeerCount() const { return connected_full_block_peer_count_; }
        uint32_t GetConnectedAvailableBlockPeerCount() const { return connected_available_block_peer_count_; }
        uint32_t CalcConnectedFullBlockAvgDownloadSpeed();
        bool ContainFullBlock(uint32_t count);
        uint32_t CalculateActivePeerCount();
        uint32_t GetActivePeerCount() const { return active_peer_count_; }

        protocol::PEER_COUNT_INFO GetPeerCountInfo() const;

        bool CanPreemptive(DownloadDriver__p download_driver_, const protocol::PieceInfoEx & piece);
        void OnPieceTimeout(DownloadDriver__p download_driver_, const protocol::PieceInfoEx & piece);
        void OnPieceRequest(const protocol::PieceInfo & piece);
        virtual boost::int32_t  GetPieceTaskNum() {return 0;}

    public:

        void OnP2PTimer(boost::uint32_t times);
        void OnUdpRecv(protocol::VodPeerPacket const & packet);

        void OnTimerElapsed(framework::timer::Timer * pointer);

        std::set<DownloadDriver__p> GetDownloadDrivers(){return download_driver_s_;}

        uint32_t GetHttpDownloadMaxSpeed();
        uint32_t GetHttpDownloadAvgSpeed();
        uint32_t GetP2PDownloadMaxSpeed();

        bool IsHttpSupportRange();

        uint32_t GetConnectedElapsedTime() const { return can_connect_ ? connected_time_counter_.elapsed() : 0; }

        uint32_t GetDownloadStatus();

        uint32_t GetTotalWindowSize() const;

        bool IsHttpBad() const { return is_http_bad_; }
        bool NeedIncreaseWindowSize();

        bool IsOpenService() const { return is_openservice_; }
        void SetIsOpenService(bool openservie) { is_openservice_ = openservie; }

        P2PDwonloadMode GetDownloadMode() const { return dl_mode_;}
        boost::uint32_t GetRTTPlus();

        boost::uint32_t GetSpeedLimitRestCount();
        boost::int32_t GetDownloadPriority() const {return download_priority_;}

        bool NeedKickPeerConnection();

        boost::uint32_t GetDownloadingTimeInSeconds() const {return downloading_time_in_seconds_;}

        boost::uint32_t GetConnectFullTimeInSeconds() const {return seconds_elapsed_until_connection_full_;}
    public:
        //////////////////////////////////////////////////////////////////////////
        // IP2PControlTarget

        virtual void Pause();
        virtual void Resume();
        virtual void SetAssignPeerCountLimit(uint32_t assign_peer_count_limit);  // 0: no limit
        virtual void SetDownloadMode(IP2PControlTarget::P2PDwonloadMode mode);
        virtual void SetDownloadPriority(boost::int32_t prioriy);
        virtual void NoticeHttpBad(bool is_http_bad);

        virtual boost::uint32_t GetSecondDownloadSpeed();
        virtual boost::uint32_t GetCurrentDownloadSpeed();
        virtual boost::uint32_t GetMinuteDownloadSpeed();
        virtual boost::uint32_t GetRecentDownloadSpeed();
        virtual boost::uint32_t GetPooledPeersCount();
        virtual boost::uint32_t GetConnectedPeersCount();
        virtual boost::uint32_t GetFullBlockPeersCount();
        virtual boost::uint32_t GetFullBlockActivePeersCount();
        virtual boost::uint32_t GetActivePeersCount();
        virtual boost::uint32_t GetAvailableBlockPeerCount();
        virtual boost::uint16_t GetNonConsistentSize() { return non_consistent_size_;}
        virtual boost::uint32_t GetMaxConnectCount() {return p2p_max_connect_count_;}
        virtual RID GetRid() {return rid_;}
        virtual void GetCandidatePeerInfos(std::vector<protocol::CandidatePeerInfo> &candidate_peers);
        virtual bool IsLive() {return false;}

    public:

        uint32_t GetDataRate();

        bool IsPlayByRID();

        // Assigner驱动的检查piece的完成情况
        void CheckPieceComplete();

        // 设置共享内存中的每个连接经过上轮剩余的分配到的Subpiece个数
        void SetAssignedLeftSubPieceCount();

        //
        void KeepConnectionAlive();

        void AddRequestingSubpiece(const protocol::SubPieceInfo & subpiece_info,
            boost::uint32_t timeout, PeerConnection__p peer_connection);

        boost::uint32_t GetAvgConnectRTT() const;

    private:
        void DoList();

    private:

        boost::shared_ptr<storage::Instance> instance_;
        boost::uint32_t block_size_;
        std::map<Guid, PeerConnection__p> peers_;                                // 连接上的peers

        IpPool__p ippool_;
        Exchanger__p exchanger_;
        PeerConnector__p connector_;
        Assigner__p assigner_;
        p2sp::SubPieceRequestManager subpiece_request_manager_;
        statistic::P2PDownloaderStatistic::p statistic_;

        RID rid_;
        std::set<DownloadDriver__p> download_driver_s_;                         // 注册的DownloadDriver
        std::multimap<protocol::PieceInfoEx, DownloadDriver__p> piece_tasks_;    // 收到的Piece请求任务
        // 记录第block_index * 16 + piece_index片piece的第subpiece_index片subpiece是否已经存在
        std::map<protocol::PieceInfo, std::bitset<storage::subpiece_num_per_piece_g_> > piece_bitmaps_;
        framework::timer::OnceTimer once_timer_;
        //
        bool is_connected_;                                        // 记录是否已经连接
        framework::timer::TickCounter start_time_counter_;
        bool can_connect_;
        framework::timer::TickCounter connected_time_counter_;
        volatile bool set_pausing_;
        uint32_t connected_full_block_peer_count_;                // 已经连上的Peer中有多少是FullBlock的
        uint32_t connected_available_block_peer_count_;           // 已经连上的Peer中有多少个是拥有当前Block的(预分配队列中的第一个Piece所在的Block)
        // Subpiece
        protocol::SubPieceInfo last_downloaded_subpiece_;
        // P2P Check
        bool is_checking_;
        bool checked_;
//        framework::timer::OnceTimer checking_timer_;
        // pausing
        bool is_p2p_pausing_;
        // video data rate
        uint32_t data_rate_;
        // p2p_download_max_speed_
        uint32_t p2p_download_max_speed_;
        // active peer count
        uint32_t active_peer_count_;
        // p2p下载的距离，指当前下载的最后一片piece和第一片piece之间的piece数
        uint16_t non_consistent_size_;
        uint32_t connected_full_block_active_peer_count_;
        volatile bool is_http_bad_;
        boost::int32_t p2p_max_connect_count_;

        // download speed limiter
        DownloadSpeedLimiter download_speed_limiter_;
        uint32_t last_trans_id_;

        // Last RequestSubpieceTransID
        uint32_t last_request_subpiece_trans_id_;

        //
        bool is_openservice_;
        P2PDwonloadMode dl_mode_;
        boost::int32_t download_priority_;

        // List相关
        framework::timer::TickCounter last_dolist_time_;
        uint32_t dolist_count_;

        boost::uint32_t downloading_time_in_seconds_;

        bool is_connect_full_;
        boost::uint32_t seconds_elapsed_until_connection_full_;

    private:
        P2PDownloader(const RID& rid);
    };

    template <typename PacketType>
    void P2PDownloader::DoSendPacket(PacketType const & packet,
        boost::uint16_t dest_protocol_version)
    {
        if (is_running_ == false) {
            return;
        }

        if (packet.Action == protocol::RequestSubPiecePacket::Action)
        {
            protocol::RequestSubPiecePacket const & request_subpiece_packet = (protocol::RequestSubPiecePacket const &)packet;

            download_speed_limiter_.DoRequestSubPiece(shared_from_this(), request_subpiece_packet
                , dest_protocol_version);
        }
        else if (packet.Action == protocol::RequestSubPiecePacketOld::Action)
        {
            protocol::RequestSubPiecePacketOld const & request_subpiece_packet = (protocol::RequestSubPiecePacketOld const &)packet;

            download_speed_limiter_.DoRequestSubPiece(shared_from_this(), request_subpiece_packet
                , dest_protocol_version);
        }
        else
        {
            statistic_->SubmitUploadedBytes(packet.length());
            p2sp::AppModule::Inst()->DoSendPacket(packet, dest_protocol_version);
        }
    }

    inline bool P2PDownloader::IsDownloading()
    {
        if (is_running_ == false)
            return false;
        return piece_tasks_.size() > 0;
    }

    inline uint32_t P2PDownloader::GetP2PDownloadMaxSpeed()
    {
        if (is_running_ == false)
            return 0;
        return p2p_download_max_speed_;
    }

    inline void P2PDownloader::AddPeer(PeerConnection__p peer_connection)
    {
        if (is_running_ == false) return;
        if (peers_.find(peer_connection->GetGuid()) == peers_.end())
        {
            peers_[peer_connection->GetGuid()] = peer_connection;
        }
    }

    inline void P2PDownloader::DelPeer(PeerConnection__p peer_connection)
    {
        if (is_running_ == false || !peer_connection) return;
        if (peers_.find(peer_connection->GetGuid()) != peers_.end())
        {
            peer_connection->Stop();
            peers_.erase(peer_connection->GetGuid());
        }
    }

    inline void P2PDownloader::SetDownloadPriority(boost::int32_t prioriy)
    {
        //DebugLog("SetDownloadPriority:%d", prioriy);
        download_priority_ = prioriy;
    }
}
#endif  // _P2SP_P2P_P2P_DOWNLOADER_H_
