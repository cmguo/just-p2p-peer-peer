#ifndef _LIVE_PEERCONNECTION_H_
#define _LIVE_PEERCONNECTION_H_

#include <protocol/PeerPacket.h>
#include <protocol/LivePeerPacket.h>
#include <statistic/SpeedInfoStatistic.h>

namespace p2sp
{
    class LiveDownloadDriver;
    typedef boost::shared_ptr<LiveDownloadDriver> LiveDownloadDriver__p;

    class LivePeerConnection
        : public boost::enable_shared_from_this<LivePeerConnection>
#ifdef DUMP_OBJECT
        , public count_object_allocate<LivePeerConnection>
#endif
    {
    public:
        typedef boost::shared_ptr<LivePeerConnection> p;
        static p create(LiveP2PDownloader__p p2p_downloader, boost::uint8_t connect_type)
        {
            return p(new LivePeerConnection(p2p_downloader, connect_type));
        }

        void OnP2PTimer(boost::uint32_t times);

        void OnAnnounce(protocol::LiveAnnouncePacket const & packet);

        void Start(protocol::ConnectPacket const & reconnect_packet,
            const boost::asio::ip::udp::endpoint &end_point,
            const protocol::CandidatePeerInfo& peer_info);

        void Stop();

        const boost::asio::ip::udp::endpoint GetEndpoint() const
        {
            return end_point_;
        }

        void ClearTaskQueue();
        void AddAssignedSubPiece(const protocol::LiveSubPieceInfo & subpiece_info);
        void OnSubPiece(uint32_t subpiece_rtt, uint32_t buffer_length);
        void OnSubPieceTimeout();

        bool HasSubPiece(const protocol::LiveSubPieceInfo & subpiece);

        void EliminateBlockBitMap(uint32_t block_id);

        boost::uint32_t GetAvgDeltaTime() const;

        const statistic::P2P_CONNECTION_INFO & GetPeerConnectionInfo();

        statistic::SPEED_INFO GetSpeedInfo();

        uint32_t Get75PercentPointInBitmap(uint32_t live_interval);

        uint32_t GetSubPieceCountInBitmap(uint32_t block_id);

        const protocol::CandidatePeerInfo & GetCandidatePeerInfo() const {return candidate_peer_info_;}

        bool LongTimeNoAnnounceResponse();

        boost::uint8_t GetConnectType() const;

        bool IsBlockBitmapEmpty() const
        {
            return block_bitmap_.empty();
        }

        uint32_t GetConnectedTimeInMillseconds();

    private:
        // 构造
        LivePeerConnection(LiveP2PDownloader__p p2p_downloader, boost::uint8_t connect_type) 
            : p2p_downloader_(p2p_downloader)
            , window_size_(25)
            , requesting_count_(0)
            , rtt_max_(0)
            , rtt_avg_(3000)
            , avg_delta_time_(100)
            , is_running_(false)
            , no_announce_response_time_(0)
            , connect_type_(connect_type)
        {
            assert(connect_type < protocol::CONNECT_MAX);
            peer_connection_info_.ConnectType = connect_type;
        }

        void DoAnnounce();

        void UpdatePeerConnectionInfo();

        boost::uint32_t GetTimeoutAdjustment();

        void RequestTillFullWindow();

        void RequestSubPieces(uint32_t block_count, bool need_check = false);

    private:
        LiveP2PDownloader__p p2p_downloader_;
        boost::asio::ip::udp::endpoint end_point_;
        std::deque<protocol::LiveSubPieceInfo> task_queue_;
        // 标识对方的bitmap
        // block_bitmap_里面的dynamic_bitset的第一位是1，表示subpiece[0]存在
        // 如果第二位是1，则表示subpiece[1] - subpiece[16] 是否都存在，依此类推
        map<boost::uint32_t, boost::dynamic_bitset<boost::uint8_t> > block_bitmap_;

        boost::int32_t window_size_;
        boost::int32_t requesting_count_;

        boost::uint32_t rtt_max_;
        boost::uint32_t rtt_avg_;
        boost::uint32_t avg_delta_time_;

        // 用于计算avg_delta_time
        framework::timer::TickCounter recv_subpiece_time_counter_;

        // 用于计算多久没有收到 Announce
        boost::uint32_t no_announce_response_time_;

        statistic::SpeedInfoStatistic speed_info_;

        statistic::P2P_CONNECTION_INFO peer_connection_info_;

        protocol::CandidatePeerInfo candidate_peer_info_;

        volatile bool is_running_;

        boost::uint8_t connect_type_;
    };
}

#endif
