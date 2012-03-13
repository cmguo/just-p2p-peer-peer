#ifndef _LIVE_P2PDOWNLOADER_H_
#define _LIVE_P2PDOWNLOADER_H_

#include "p2sp/download/Downloader.h"
#include "p2sp/download/SwitchControllerInterface.h"
#include "p2sp/p2p/LiveAssigner.h"
#include "p2sp/p2p/LiveSubPieceRequestManager.h"
#include "p2sp/p2p/IpPool.h"
#include "p2sp/p2p/Exchanger.h"
#include "p2sp/p2p/PeerConnector.h"
#include "storage/LiveInstance.h"
#include "p2sp/bootstrap/BootStrapGeneralConfig.h"

#include "p2sp/p2p/LiveSubPieceCountManager.h"
#include "p2sp/p2p/LiveConnectionManager.h"

namespace storage
{
    class LiveInstance;
    typedef boost::shared_ptr<LiveInstance> LiveInstance__p;
}

namespace p2sp
{
    class IpPool;
    typedef boost::shared_ptr<IpPool> IpPool__p;

    class Exchanger;
    typedef boost::shared_ptr<Exchanger> Exchanger__p;

    class PeerConnector;
    typedef boost::shared_ptr<PeerConnector> PeerConnector__p;

    class LiveP2PDownloader;
    typedef boost::shared_ptr<LiveP2PDownloader> LiveP2PDownloader__p;

    class LiveDownloadDriver;

    enum LIVE_CONNECT_LEVEL
    {
        LOW = 0,
        MEDIUM = 1,
        HIGH = 2
    };

    class LiveP2PDownloader
        : public LiveDownloader
        , public IP2PControlTarget
        , public boost::enable_shared_from_this<LiveP2PDownloader>
#ifdef DUMP_OBJECT
        , public count_object_allocate<LiveP2PDownloader>
#endif
    {
    public:
        typedef boost::shared_ptr<LiveP2PDownloader> p;
        static p Create(const RID& rid, LiveDownloadDriver__p live_download_driver, storage::LiveInstance__p live_instance)
        {
            return p(new LiveP2PDownloader(rid, live_download_driver, live_instance));
        }

    private:
        LiveP2PDownloader(const RID & rid, LiveDownloadDriver__p live_download_driver, storage::LiveInstance__p live_instance);

    public:
        // 重载基类的函数
        // IControlTarget
        virtual void Pause();
        virtual void Resume();

        virtual boost::uint32_t GetSecondDownloadSpeed();
        virtual boost::uint32_t GetCurrentDownloadSpeed();
        virtual uint32_t GetMinuteDownloadSpeed();
        virtual uint32_t GetRecentDownloadSpeed(); // 20s
        virtual void SetSpeedLimitInKBps(boost::int32_t speed_limit_in_KBps);

        // Downloader
        virtual void Stop();
        virtual bool IsPausing();

        // LiveDownloader
        virtual void OnBlockTimeout(boost::uint32_t block_id);
        virtual void PutBlockTask(const protocol::LiveSubPieceInfo & live_block);

        virtual bool IsP2PDownloader() {return true;}

        // IP2PControlTarget
        virtual void SetDownloadMode(P2PDwonloadMode mode);
        virtual void SetMaxConnectCount(boost::int32_t max_connect_count);

        virtual uint32_t GetPooledPeersCount();
        virtual uint32_t GetConnectedPeersCount();
        virtual uint32_t GetFullBlockPeersCount();
        virtual uint32_t GetActivePeersCount();
        virtual uint32_t GetAvailableBlockPeerCount();

        virtual uint16_t GetNonConsistentSize() {return 0;}
        virtual void SetDownloadPriority(boost::int32_t) {}
        virtual uint32_t GetMaxConnectCount();
        virtual bool IsLive() {return true;}

    public:
        void Start();
        void AddCandidatePeers(std::vector<protocol::CandidatePeerInfo> peers, bool is_live_udpserver);
        void OnP2PTimer(boost::uint32_t times);
        RID GetRid();

        statistic::SPEED_INFO GetSpeedInfo();
        statistic::SPEED_INFO_EX GetSpeedInfoEx();
        statistic::SPEED_INFO GetSubPieceSpeedInfo();
        statistic::SPEED_INFO_EX GetSubPieceSpeedInfoEx();
        statistic::SPEED_INFO GetUdpServerSpeedInfo();
        statistic::SPEED_INFO_EX GetUdpServerSpeedInfoEx();
        statistic::SPEED_INFO GetUdpServerSubpieceSpeedInfo();
        statistic::SPEED_INFO_EX GetUdpServerSubpieceSpeedInfoEx();

        template <typename PacketType>
        void DoSendPacket(PacketType const & packet)
        {
            if (is_running_ == false) {
                return;
            }

            p2sp::AppModule::Inst()->DoSendPacket(packet, protocol::PEER_VERSION);
        }

        void GetCandidatePeerInfos(std::vector<protocol::CandidatePeerInfo> &candidate_peers);

        void InitPeerConnection(LIVE_CONNECT_LEVEL connect_level);
        void KickPeerConnection(LIVE_CONNECT_LEVEL connect_level);

        LiveDownloadDriver__p GetDownloadDriver()
        {
            return live_download_driver_;
        }

        boost::uint32_t GetRestTimeInSeconds() const;

        void OnUdpRecv(protocol::Packet const & packet);

        void OnUdpPacketStatistic(protocol::Packet const & packet);

        void AddPeer(LivePeerConnection__p peer_connection);
        void DelPeer(const boost::asio::ip::udp::endpoint & endpoint);

        const std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p> & GetPeers()
        {
            return live_connection_manager_.GetPeers();
        }

        bool HasPeer(const boost::asio::ip::udp::endpoint & end_point)
        {
            return live_connection_manager_.HasPeer(end_point);
        }

        bool HasSubPiece(const protocol::LiveSubPieceInfo & sub_piece);

        void SetBlockCountMap(boost::uint32_t block_id, std::vector<boost::uint16_t> subpiece_count);

        storage::LiveInstance__p GetInstance()
        {
            return live_instance_;
        }

        bool HasSubPieceCount(boost::uint32_t piece_id);
        boost::uint16_t GetSubPieceCount(boost::uint32_t block_id);

        bool IsRequesting(const protocol::LiveSubPieceInfo & subpiece_info);
        void AddRequestingSubpiece(const protocol::LiveSubPieceInfo & subpiece_info,
            boost::uint32_t timeout, LivePeerConnection__p peer_connection,
            uint32_t transaction_id);

        // for statistic
        void SubmitAllRequestSubPieceCount(boost::uint16_t request_sub_piece_count);
        void SubmitRequestSubPieceCount(boost::uint16_t request_sub_piece_count);
        boost::uint32_t GetTotalUnusedSubPieceCount() const;
        boost::uint32_t GetTotalAllRequestSubPieceCount() const;
        boost::uint32_t GetTotalRecievedSubPieceCount() const;
        boost::uint32_t GetTotalRequestSubPieceCount() const;
        boost::uint32_t GetTotalP2PDataBytes() const;
        boost::uint32_t GetTotalUdpServerDataBytes() const;

        void SubmitUdpServerDownloadBytes(boost::uint32_t bytes);

        boost::uint32_t GetTimesOfUseUdpServerBecauseOfUrgent() const;
        boost::uint32_t GetTimesOfUseUdpServerBecauseOfLargeUpload() const;
        boost::uint32_t GetTimeElapsedUseUdpServerBecauseOfUrgent() const;
        boost::uint32_t GetTimeElapsedUseUdpServerBecauseOfLargeUpload() const;
        boost::uint32_t GetDownloadBytesUseUdpServerBecauseOfUrgent() const;
        boost::uint32_t GetDownloadBytesUseUdpServerBecauseOfLargeUpload() const;

        boost::uint8_t GetLostRate() const;
        boost::uint8_t GetRedundancyRate() const;

        void CalcTimeOfUsingUdpServerWhenStop();

        boost::uint32_t GetTotalConnectPeersCount() const;

        uint32_t GetRequestingCount(const protocol::LiveSubPieceInfo & subpiece_info);

        boost::uint32_t GetReverseOrderSubPiecePacketCount() const;
        boost::uint32_t GetTotalReceivedSubPiecePacketCount() const;

        boost::uint32_t GetMinFirstBlockID() const;

        void OnBlockComplete(const protocol::LiveSubPieceInfo & live_block);

    private:
        void DoList();
        LIVE_CONNECT_LEVEL GetConnectLevel();
        void CheckShouldUseUdpServer();

        void DeleteAllUdpServer();
        bool IsAheadOfMostPeers() const;

        void SendPeerInfo();
        bool IsInUdpServerProtectTimeWhenStart();

        uint32_t GetDownloadablePeersCount() const;

        void GetCandidatePeerInfosBasedOnUploadAbility(std::set<protocol::CandidatePeerInfo> & candidate_peers);
        void GetCandidatePeerInfosBasedOnUploadSpeed(std::set<protocol::CandidatePeerInfo> & candidate_peers);

    public:
        bool is_running_;
        RID rid_;
        LiveAssigner live_assigner_;
        IpPool__p ippool_;
        Exchanger__p exchanger_;
        PeerConnector__p connector_;

        LiveConnectionManger live_connection_manager_;
        std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p> peers_;

        bool is_p2p_pausing_;
        boost::int32_t p2p_max_connect_count_;

        LiveDownloadDriver__p live_download_driver_;

        storage::LiveInstance__p live_instance_;

        LiveSubPieceRequestManager live_subpiece_request_manager_;

        framework::timer::TickCounter last_dolist_time_;
        boost::uint32_t dolist_time_interval_;

        framework::timer::TickCounter last_dolist_udpserver_time_;
        boost::uint32_t dolist_udpserver_time_interval_;
        boost::uint32_t dolist_udpserver_times_;

    private:
        // for statistic
        statistic::SpeedInfoStatistic p2p_speed_info_;
        statistic::SpeedInfoStatistic p2p_subpiece_speed_info_;
        boost::uint32_t total_all_request_subpiece_count_;  // 请求的所有subpiece个数（包括冗余的）
        boost::uint32_t total_request_subpiece_count_;  // 请求的subpiece个数（不包括冗余的）
        statistic::SpeedInfoStatistic udp_server_speed_info_;
        statistic::SpeedInfoStatistic udp_server_subpiece_speed_info_;

        IpPool__p udpserver_pool_;
        PeerConnector__p udpserver_connector_;
        boost::uint32_t connected_udpserver_count_;
        bool should_use_udpserver_;
        framework::timer::TickCounter use_udpserver_tick_counter_;

        enum UseUdpServerReason
        {
            NO_USE_UDPSERVER = 0,
            URGENT = 1,
            LARGE_UPLOAD = 2,
        }use_udpserver_reason_;

        boost::uint32_t times_of_use_udpserver_because_of_urgent_;
        boost::uint32_t times_of_use_udpserver_because_of_large_upload_;
        boost::uint32_t time_elapsed_use_udpserver_because_of_urgent_;
        boost::uint32_t time_elapsed_use_udpserver_because_of_large_upload_;
        boost::uint32_t download_bytes_use_udpserver_because_of_urgent_;
        boost::uint32_t download_bytes_use_udpserver_because_of_large_upload_;

        boost::uint32_t send_peer_info_packet_interval_in_second_;

        boost::uint32_t urgent_rest_playable_time_delim_;
        boost::uint32_t safe_rest_playable_time_delim_;
        boost::uint32_t safe_enough_rest_playable_time_delim_;
        boost::uint32_t using_udpserver_time_in_second_delim_;
        boost::uint32_t using_udpserver_time_at_least_when_large_upload_;
        boost::uint32_t use_udpserver_count_;

        bool should_connect_udpserver_;

        boost::uint32_t udpserver_protect_time_when_start_;
        bool should_use_bw_type_;

        boost::uint32_t total_connect_peers_count_;

        boost::uint32_t live_connect_low_normal_threshold_;
        boost::uint32_t live_connect_normal_high_threshold_;

        boost::uint32_t live_exchange_interval_in_second_;

        framework::timer::TickCounter live_exchange_tick_counter_;

        uint32_t default_connection_limit_;
        uint32_t live_extended_connections_;
        framework::timer::TickCounter urgent_tick_counter_;
        framework::timer::TickCounter safe_tick_counter_;

        LiveSubPieceCountManager live_subpiece_count_manager_;

        // for statistic
        boost::uint32_t total_unused_subpiece_count_;  // 收到的所有的subpiece个数（包括冗余的）
        boost::uint32_t total_received_subpiece_count_;  // 收到的subpiece个数（不包括冗余的）
        boost::uint32_t total_p2p_data_bytes_;  // P2P总下载
        boost::uint32_t total_udpserver_data_bytes_;  // 从UdpServer下载的字节数
    };

    inline statistic::SPEED_INFO LiveP2PDownloader::GetSpeedInfo()
    {
        return p2p_speed_info_.GetSpeedInfo();
    }

    inline statistic::SPEED_INFO LiveP2PDownloader::GetSubPieceSpeedInfo()
    {
        return p2p_subpiece_speed_info_.GetSpeedInfo();
    }

    inline statistic::SPEED_INFO LiveP2PDownloader::GetUdpServerSpeedInfo()
    {
        return udp_server_speed_info_.GetSpeedInfo();
    }

    inline statistic::SPEED_INFO LiveP2PDownloader::GetUdpServerSubpieceSpeedInfo()
    {
        return udp_server_subpiece_speed_info_.GetSpeedInfo();
    }

    inline uint32_t LiveP2PDownloader::GetConnectedPeersCount()
    {
        return live_connection_manager_.GetConnectedPeersCount();
    }

    inline uint32_t LiveP2PDownloader::GetPooledPeersCount()
    {
        if (false == is_running_)
        {
            return 0;
        }

        assert(ippool_);

        return ippool_->GetPeerCount();
    }

    inline boost::uint32_t LiveP2PDownloader::GetTotalUnusedSubPieceCount() const
    {
        return total_unused_subpiece_count_;
    }

    inline boost::uint32_t LiveP2PDownloader::GetTotalAllRequestSubPieceCount() const
    {
        return total_all_request_subpiece_count_;
    }

    inline boost::uint32_t LiveP2PDownloader::GetTotalRecievedSubPieceCount() const
    {
        return total_received_subpiece_count_;
    }

    inline boost::uint32_t LiveP2PDownloader::GetTotalRequestSubPieceCount() const
    {
        return total_request_subpiece_count_;
    }

    inline boost::uint32_t LiveP2PDownloader::GetTotalP2PDataBytes() const
    {
        return total_p2p_data_bytes_;
    }

    inline boost::uint32_t LiveP2PDownloader::GetTotalUdpServerDataBytes() const
    {
        return total_udpserver_data_bytes_;
    }

    inline void LiveP2PDownloader::SubmitAllRequestSubPieceCount(boost::uint16_t request_sub_piece_count)
    {
        total_all_request_subpiece_count_ += request_sub_piece_count;
    }

    inline void LiveP2PDownloader::SubmitRequestSubPieceCount(boost::uint16_t request_sub_piece_count)
    {
        total_request_subpiece_count_ += request_sub_piece_count;
    }
}
#endif
