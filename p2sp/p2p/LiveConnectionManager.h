#ifndef LIVE_CONNECTION_MANAGER_H
#define LIVE_CONNECTION_MANAGER_H

#include "p2sp/bootstrap/BootStrapGeneralConfig.h"
#include "p2sp/p2p/LivePeerConnection.h"

namespace p2sp
{
    class LivePeerConnection;
    typedef boost::shared_ptr<LivePeerConnection> LivePeerConnection__p;

    struct KickLiveConnectionIndicator
    {   
        uint32_t last_minute_speed_in_bytes_;
        uint32_t block_bitmap_empty_in_millseconds_;

        bool ShouldKick() const
        {
            return block_bitmap_empty_in_millseconds_ > 0 &&
                last_minute_speed_in_bytes_ == 0;
        }

        KickLiveConnectionIndicator(const LivePeerConnection__p & connection)
        {
            this->last_minute_speed_in_bytes_ = connection->GetSpeedInfo().MinuteDownloadSpeed;
            this->block_bitmap_empty_in_millseconds_ = connection->GetBitmapEmptyTimeInMillseconds();
        }
    };

    inline bool operator<(const KickLiveConnectionIndicator& x, const KickLiveConnectionIndicator& y)
    {
        if (x.block_bitmap_empty_in_millseconds_ > 0 &&
            y.block_bitmap_empty_in_millseconds_ == 0)
        {
            // x empty, y not empty
            return true;
        }

        if (x.block_bitmap_empty_in_millseconds_ == 0 &&
            y.block_bitmap_empty_in_millseconds_ > 0)
        {
            // x not empty, y empty
            return false;
        }

        if (x.block_bitmap_empty_in_millseconds_ > 0 &&
            y.block_bitmap_empty_in_millseconds_ > 0)
        {
            // both empty
            if (x.last_minute_speed_in_bytes_ == 0 &&
                y.last_minute_speed_in_bytes_ == 0)
            {
                // both no speed; compare empty time
                return x.block_bitmap_empty_in_millseconds_ > y.block_bitmap_empty_in_millseconds_;
            }

            // kick first if less speed
            return x.last_minute_speed_in_bytes_ < y.last_minute_speed_in_bytes_;
        }

        // both non empty; kick first if less speed
        return x.last_minute_speed_in_bytes_ < y.last_minute_speed_in_bytes_;
    }

    class LiveConnectionManger
    {
    public:
        LiveConnectionManger()
            : live_exchange_large_upload_ability_delim_(BootStrapGeneralConfig::Inst()->GetLiveExchangeLargeUploadAbilityDelim())
            , live_exchange_large_upload_ability_max_count_(BootStrapGeneralConfig::Inst()->GetLiveExchangeLargeUploadAbilityMaxCount())
            , live_exchange_large_upload_to_me_delim_(BootStrapGeneralConfig::Inst()->GetLiveExchangeLargeUploadToMeDelim())
            , live_exchange_large_upload_to_me_max_count_(BootStrapGeneralConfig::Inst()->GetLiveExchangeLargeUploadToMeMaxCount())
            , connected_udpserver_count_(0)
        {

        }

        void Stop();

        void AddPeer(LivePeerConnection__p peer_connection);
        void DelPeer(const boost::asio::ip::udp::endpoint & endpoint);

        bool HasPeer(const boost::asio::ip::udp::endpoint & end_point) const;
        bool IsLivePeer(const boost::asio::ip::udp::endpoint & end_point) const;
        bool IsLiveUdpServer(const boost::asio::ip::udp::endpoint & end_point) const;

        void OnP2PTimer(boost::uint32_t);

        void OnAnnouncePacket(const protocol::LiveAnnouncePacket & packet);
        void OnErrorPacket(const protocol::ErrorPacket & packet);
        void OnPeerInfoPacket(const protocol::PeerInfoPacket & packet);
        void OnCloseSessionPacket();

        void EliminateElapsedBlockBitMap(boost::uint32_t block_id);

        bool IsAheadOfMostPeers() const;

        boost::uint32_t GetConnectedUdpServerCount() const
        {
            return connected_udpserver_count_;
        }

        boost::uint32_t GetReverseOrderSubPiecePacketCount() const;
        boost::uint32_t GetTotalReceivedSubPiecePacketCount() const;
        boost::uint32_t GetMinFirstBlockID() const;
        boost::uint32_t GetDownloadablePeersCount() const;

        boost::uint32_t GetRequestingCount() const;

        inline uint32_t GetConnectedPeersCount() const
        {
            return peers_.size();
        }

        const std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p> & GetPeers()
        {
            return peers_;
        }

        void GetUdpServerEndpoints(std::set<boost::asio::ip::udp::endpoint> & endpoints);

        void GetNoResponsePeers(std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p> &
            no_response_peers);

        void GetKickMap(std::multimap<KickLiveConnectionIndicator, LivePeerConnection__p> & 
            kick_map);

        void GetCandidatePeerInfosBasedOnUploadAbility(std::set<protocol::CandidatePeerInfo> & selected_peers);
        void GetCandidatePeerInfosBasedOnUploadSpeed(std::set<protocol::CandidatePeerInfo> & selected_peers);

        static bool CompareBasedOnUploadAbility(boost::shared_ptr<LivePeerConnection> const & lhs,
            boost::shared_ptr<LivePeerConnection> const & rhs);

        static bool CompareBasedOnUploadSpeed(boost::shared_ptr<LivePeerConnection> const & lhs,
            boost::shared_ptr<LivePeerConnection> const & rhs);

    private:
        void SelectPeers(std::set<protocol::CandidatePeerInfo> & selected_peers,
            const std::vector<boost::shared_ptr<LivePeerConnection> > & sorted_peers, boost::uint32_t to_select_peers_count);

        boost::uint32_t live_exchange_large_upload_ability_delim_;
        boost::uint32_t live_exchange_large_upload_ability_max_count_;

        boost::uint32_t live_exchange_large_upload_to_me_delim_;
        boost::uint32_t live_exchange_large_upload_to_me_max_count_;

        boost::uint32_t connected_udpserver_count_;

        std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p> peers_;
    };
}

#endif