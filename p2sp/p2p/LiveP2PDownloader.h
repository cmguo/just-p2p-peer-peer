#ifndef _LIVE_P2PDOWNLOADER_H_
#define _LIVE_P2PDOWNLOADER_H_

#include "p2sp/download/Downloader.h"
#include "p2sp/download/SwitchControllerInterface.h"
#include "p2sp/p2p/LiveAssigner.h"
#include "p2sp/p2p/LivePeerConnection.h"
#include "p2sp/p2p/LiveSubPieceRequestManager.h"
#include "p2sp/p2p/IpPool.h"
#include "p2sp/p2p/Exchanger.h"
#include "p2sp/p2p/PeerConnector.h"
#include "storage/LiveInstance.h"


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

    class LivePeerConnection;
    typedef boost::shared_ptr<LivePeerConnection> LivePeerConnection__p;

    class LiveP2PDownloader;
    typedef boost::shared_ptr<LiveP2PDownloader> LiveP2PDownloader__p;

    class LiveDownloadDriver;

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
        static p Create(const RID& rid, storage::LiveInstance__p live_instance)
        {
            return p(new LiveP2PDownloader(rid, live_instance));
        }

    private:
        LiveP2PDownloader(const RID & rid, storage::LiveInstance__p live_instance);

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

        // IP2PControlTarget
        virtual void SetAssignPeerCountLimit(uint32_t assign_peer_count_limit); // 0: no limit
        virtual void NoticeHttpBad(bool is_http_bad);
        virtual void SetDownloadMode(P2PDwonloadMode mode);
        virtual void SetMaxConnectCount(boost::int32_t max_connect_count);

        virtual uint32_t GetPooledPeersCount();
        virtual uint32_t GetConnectedPeersCount();
        virtual uint32_t GetFullBlockPeersCount();
        virtual uint32_t GetFullBlockActivePeersCount();
        virtual uint32_t GetActivePeersCount();
        virtual uint32_t GetAvailableBlockPeerCount();

        virtual uint16_t GetNonConsistentSize() {return 0;}
        virtual void SetDownloadPriority(boost::int32_t) {}
        virtual uint32_t GetMaxConnectCount();
        virtual bool IsLive() {return true;}

    public:
        void Start();
        void AddCandidatePeers(std::vector<protocol::CandidatePeerInfo> peers);
        void OnP2PTimer(boost::uint32_t times);
        void DoRequestSubPiece() {};
        void AddBlockCount(boost::uint32_t);
        void AddBlockId(boost::uint32_t);
        boost::int32_t  GetBlockTaskNum();
        bool HasBlockTask() const;
        RID GetRid();

        statistic::SPEED_INFO GetSpeedInfo();
        statistic::SPEED_INFO_EX GetSpeedInfoEx();
        statistic::SPEED_INFO GetSubPieceSpeedInfo();
        statistic::SPEED_INFO_EX GetSubPieceSpeedInfoEx();

        template <typename PacketType>
        void DoSendPacket(PacketType const & packet)
        {
            if (is_running_ == false) {
                return;
            }

            p2sp::AppModule::Inst()->DoSendPacket(packet, protocol::PEER_VERSION);
        }

        void GetCandidatePeerInfos(std::vector<protocol::CandidatePeerInfo> &candidate_peers);

        void InitPeerConnection();
        void KickPeerConnection();

        void AttachDownloadDriver(LiveDownloadDriver__p download_driver);
        void DetachDownloadDriver(LiveDownloadDriver__p download_driver);

        const std::set<LiveDownloadDriver__p> & GetDownloadDriverSet()
        {
            return download_driver_s_;
        }

        void OnUdpRecv(protocol::Packet const & packet);

        void AddPeer(LivePeerConnection__p peer_connection);
        void DelPeer(LivePeerConnection__p peer_connection);
        const std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p> & GetPeers()
        {
            return peers_;
        }

        bool HasPeer(boost::asio::ip::udp::endpoint end_point)
        {
            return peers_.find(end_point) != peers_.end();
        }

        bool HasSubPiece(const protocol::LiveSubPieceInfo & sub_piece);

        void SetBlockCountMap(boost::uint32_t block_id, std::vector<boost::uint16_t> subpiece_count);

        std::list<protocol::LiveSubPieceInfo> & GetBlockTasks();

        storage::LiveInstance__p GetInstance()
        {
            return live_instance_;
        }

        bool HasSubPieceCount(boost::uint32_t piece_id);
        boost::uint16_t GetSubPieceCount(boost::uint32_t block_id);
        const map<uint32_t, uint16_t> & GetSubPieceCountMap();

        bool IsRequesting(const protocol::LiveSubPieceInfo & subpiece_info);
        void AddRequestingSubpiece(const protocol::LiveSubPieceInfo & subpiece_info,
            boost::uint32_t timeout, LivePeerConnection__p peer_connection);

        const std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p> & GetPeerConnectionInfo() const;

        // 传说中一代直播的经典算法
        storage::LivePosition Get75PercentPointInBitmap();

        // for statistic
        void SubmitAllRequestSubPieceCount(boost::uint16_t request_sub_piece_count);
        void SubmitRequestSubPieceCount(boost::uint16_t request_sub_piece_count);
        boost::uint32_t GetTotalUnusedSubPieceCount() const;
        boost::uint32_t GetTotalAllRequestSubPieceCount() const;
        boost::uint32_t GetTotalRecievedSubPieceCount() const;
        boost::uint32_t GetTotalRequestSubPieceCount() const;
        boost::uint32_t GetTotalP2PDataBytes() const;
        boost::uint32_t GetMinRestTimeInSeconds() const;
        boost::uint32_t GetTotalUdpServerDataBytes() const;

    private:
        void CheckBlockComplete();
        void DoList();

    public:
        bool is_running_;
        RID rid_;
        LiveAssigner live_assigner_;
        IpPool__p ippool_;
        Exchanger__p exchanger_;
        PeerConnector__p connector_;
        std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p> peers_;
        bool is_p2p_pausing_;
        boost::uint32_t p2p_max_connect_count_;
        std::set<LiveDownloadDriver__p> download_driver_s_;

        storage::LiveInstance__p live_instance_;

        std::list<protocol::LiveSubPieceInfo> block_tasks_;
        map<uint32_t, uint16_t> block_count_map_;

        LiveSubPieceRequestManager live_subpiece_request_manager_;

        framework::timer::TickCounter last_dolist_time_;
        boost::uint32_t dolist_time_interval_;

    private:
        // for statistic
        statistic::SpeedInfoStatistic p2p_speed_info_;
        statistic::SpeedInfoStatistic p2p_subpiece_speed_info_;
        boost::uint32_t total_all_request_subpiece_count_;  // 请求的所有subpiece个数（包括冗余的）
        boost::uint32_t total_request_subpiece_count_;  // 请求的subpiece个数（不包括冗余的）

    };

    inline statistic::SPEED_INFO LiveP2PDownloader::GetSpeedInfo()
    {
        return p2p_speed_info_.GetSpeedInfo();
    }

    inline statistic::SPEED_INFO LiveP2PDownloader::GetSubPieceSpeedInfo()
    {
        return p2p_subpiece_speed_info_.GetSpeedInfo();
    }

    inline uint32_t LiveP2PDownloader::GetConnectedPeersCount()
    {
        return peers_.size();
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
        return live_subpiece_request_manager_.GetTotalUnusedSubPieceCount();
    }

    inline boost::uint32_t LiveP2PDownloader::GetTotalAllRequestSubPieceCount() const
    {
        return total_all_request_subpiece_count_;
    }

    inline boost::uint32_t LiveP2PDownloader::GetTotalRecievedSubPieceCount() const
    {
        return live_subpiece_request_manager_.GetTotalRecievedSubPieceCount();
    }

    inline boost::uint32_t LiveP2PDownloader::GetTotalRequestSubPieceCount() const
    {
        return total_request_subpiece_count_;
    }

    inline boost::uint32_t LiveP2PDownloader::GetTotalP2PDataBytes() const
    {
        return live_subpiece_request_manager_.GetTotalP2PDataBytes();
    }

    inline boost::uint32_t LiveP2PDownloader::GetTotalUdpServerDataBytes() const
    {
        return live_subpiece_request_manager_.GetTotalUdpServerDataBytes();
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
