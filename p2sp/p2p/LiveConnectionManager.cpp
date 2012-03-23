#include "Common.h"
#include "p2sp/p2p/LiveConnectionManager.h"
#include "p2sp/p2p/LivePeerConnection.h"

namespace p2sp
{
    void LiveConnectionManger::Stop()
    {
        for (std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::iterator iter = peers_.begin();
            iter != peers_.end(); ++iter)
        {
            iter->second->Stop();
        }

        peers_.clear();       
    }

    void LiveConnectionManger::AddPeer(LivePeerConnection__p peer_connection)
    {
        if (peers_.find(peer_connection->GetEndpoint()) == peers_.end())
        {
            peers_[peer_connection->GetEndpoint()] = peer_connection;
            if (peer_connection->GetConnectType() == protocol::CONNECT_LIVE_UDPSERVER)
            {
                ++connected_udpserver_count_;
            }
        }
    }

    LivePeerConnection__p LiveConnectionManger::DelPeer(const boost::asio::ip::udp::endpoint & endpoint)
    {
        std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::iterator
            iter = peers_.find(endpoint);

        if (iter == peers_.end())
        {
            return LivePeerConnection__p();
        }

        if (iter->second->GetConnectType() == protocol::CONNECT_LIVE_UDPSERVER)
        {
            assert(connected_udpserver_count_ > 0);
            --connected_udpserver_count_;
        }

        iter->second->Stop();

        LivePeerConnection__p peer_to_remove = iter->second;
        peers_.erase(endpoint);
        return peer_to_remove;
    }

    bool LiveConnectionManger::HasPeer(const boost::asio::ip::udp::endpoint & end_point) const
    {
        return peers_.find(end_point) != peers_.end();
    }

    bool LiveConnectionManger::IsLivePeer(const boost::asio::ip::udp::endpoint & end_point) const
    {
        std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::const_iterator
            iter = peers_.find(end_point);

        if (iter != peers_.end())
        {
            return iter->second->GetConnectType() == protocol::CONNECT_LIVE_PEER;
        }

        return false;
    }

    bool LiveConnectionManger::IsLiveUdpServer(const boost::asio::ip::udp::endpoint & end_point) const
    {
        std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::const_iterator
            iter = peers_.find(end_point);

        if (iter != peers_.end())
        {
            return iter->second->GetConnectType() == protocol::CONNECT_LIVE_UDPSERVER;
        }

        return false;
    }

    void LiveConnectionManger::OnP2PTimer(boost::uint32_t times)
    {
        for (std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::iterator iter = peers_.begin();
            iter != peers_.end(); ++iter)
        {
            iter->second->OnP2PTimer(times);
        }
    }

    void LiveConnectionManger::OnAnnouncePacket(const protocol::LiveAnnouncePacket & packet)
    {
        if (peers_.find(packet.end_point) != peers_.end())
        {
            LivePeerConnection::p peer_connection = peers_.find(packet.end_point)->second;
            peer_connection->OnAnnounce(packet);
        }
    }

    void LiveConnectionManger::OnErrorPacket(const protocol::ErrorPacket & packet)
    {
        if (peers_.find(packet.end_point) != peers_.end())
        {
            if (packet.error_code_ == protocol::ErrorPacket::PPV_ANNOUCE_NO_RESOURCEID ||
                packet.error_code_ == protocol::ErrorPacket::PPV_SUBPIECE_NO_RESOURCEID)
            {
                // 正在Announce 或则 请求Subpiece
                // 对方没有该资源 或者 我被T了
                DelPeer(packet.end_point);
            }
        }
    }

    void LiveConnectionManger::OnPeerInfoPacket(const protocol::PeerInfoPacket & packet)
    {
        if (peers_.find(packet.end_point) != peers_.end())
        {
            const protocol::PeerInfoPacket peer_info_packet = (const protocol::PeerInfoPacket &)packet;

            statistic::PEER_INFO peer_info(peer_info_packet.peer_info_.download_connected_count_, peer_info_packet.peer_info_.upload_connected_count_,
                peer_info_packet.peer_info_.upload_speed_, peer_info_packet.peer_info_.max_upload_speed_, peer_info_packet.peer_info_.rest_playable_time_,
                peer_info_packet.peer_info_.lost_rate_, peer_info_packet.peer_info_.redundancy_rate_);

            peers_[packet.end_point]->UpdatePeerInfo(peer_info);
        }
    }

    void LiveConnectionManger::EliminateElapsedBlockBitMap(boost::uint32_t block_id)
    {
        for (std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::iterator peer_iter = peers_.begin();
            peer_iter != peers_.end(); ++peer_iter)
        {
            peer_iter->second->EliminateElapsedBlockBitMap(block_id);
        }
    }

    bool LiveConnectionManger::IsAheadOfMostPeers() const
    {
        boost::uint32_t large_bitmap_peer_count = 0;
        for (std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::const_iterator iter = peers_.begin();
            iter != peers_.end(); ++iter)
        {
            if (iter->second->GetConnectType() == protocol::CONNECT_LIVE_UDPSERVER)
            {
                continue;
            }

            if (iter->second->GetBlockBitmapSize() > 1)
            {
                ++large_bitmap_peer_count;

                if (large_bitmap_peer_count >= 2)
                {
                    return false;
                }
            }
        }

        return true;
    }

    boost::uint32_t LiveConnectionManger::GetDownloadablePeersCount() const
    {
        boost::uint32_t count = 0;
        for(std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::const_iterator iter = peers_.begin();
            iter != peers_.end();
            iter++)
        {
            LivePeerConnection__p conn = iter->second;
            if (!conn->IsBlockBitmapEmpty())
            {
                count++;
            }
        }

        return count;
    }

    boost::uint32_t LiveConnectionManger::GetReverseOrderSubPiecePacketCount() const
    {
        boost::uint32_t reverse_order_packet_count = 0;

        for (std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::const_iterator iter = peers_.begin();
            iter != peers_.end(); ++iter)
        {
            reverse_order_packet_count += iter->second->GetReverseOrderSubPiecePacketCount();
        }

        return reverse_order_packet_count;
    }

    boost::uint32_t LiveConnectionManger::GetTotalReceivedSubPiecePacketCount() const
    {
        boost::uint32_t total_received_packet_count = 0;

        for (std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::const_iterator iter = peers_.begin();
            iter != peers_.end(); ++iter)
        {
            total_received_packet_count += iter->second->GetTotalReceivedSubPiecePacketCount();
        }

        return total_received_packet_count;
    }

    boost::uint32_t LiveConnectionManger::GetMinFirstBlockID() const
    {
        boost::uint32_t min_first_block_id = std::numeric_limits<uint32_t>::max();

        for (std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::const_iterator iter = peers_.begin();
            iter != peers_.end(); ++iter)
        {
            if (min_first_block_id > iter->second->GetPeerConnectionInfo().FirstLiveBlockId &&
                iter->second->GetPeerConnectionInfo().FirstLiveBlockId != 0)
            {
                min_first_block_id = iter->second->GetPeerConnectionInfo().FirstLiveBlockId;
            }
        }

        if (min_first_block_id == std::numeric_limits<uint32_t>::max())
        {
            min_first_block_id = 0;
        }

        return min_first_block_id;
    }

    boost::uint32_t LiveConnectionManger::GetRequestingCount() const
    {
        boost::uint32_t requesting_count = 0;

        for (std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::const_iterator iter = peers_.begin();
            iter != peers_.end(); ++iter)
        {
            requesting_count += iter->second->GetPeerConnectionInfo().Requesting_Count;
        }

        return requesting_count;
    }

    void LiveConnectionManger::GetUdpServerEndpoints(std::set<boost::asio::ip::udp::endpoint> & endpoints)
    {
        for (std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::iterator 
            iter = peers_.begin(); iter != peers_.end(); ++iter)
        {
            if (iter->second->GetConnectType() == protocol::CONNECT_LIVE_UDPSERVER)
            {
                endpoints.insert(iter->first);
            }
        }
    }

    void LiveConnectionManger::GetNoResponsePeers(
        std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p> & no_response_peers)
    {
        for (std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::iterator 
            iter = peers_.begin(); iter != peers_.end(); ++iter)
        {
            if (iter->second->LongTimeNoResponse())
            {
                no_response_peers.insert(std::make_pair(iter->first, iter->second));
            }
        }
    }

    void LiveConnectionManger::GetKickMap(
        std::multimap<KickLiveConnectionIndicator, LivePeerConnection::p> & kick_map)
    {
        for (std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::iterator 
            iter = peers_.begin(); iter != peers_.end(); ++iter)
        {
            if (iter->second->GetConnectedTimeInMillseconds() >= 15000)
            {
                // only kick peers which are connected longer than 15 seconds
                kick_map.insert(std::make_pair(KickLiveConnectionIndicator(iter->second), iter->second));
            }
        }
        
    }

    void LiveConnectionManger::GetCandidatePeerInfosBasedOnUploadAbility(std::set<protocol::CandidatePeerInfo> & selected_peers)
    {
        std::vector<boost::shared_ptr<LivePeerConnection> > large_upload_ability_peers;
        for (std::map<boost::asio::ip::udp::endpoint, boost::shared_ptr<LivePeerConnection> >::const_iterator iter = peers_.begin();
            iter != peers_.end(); ++iter)
        {
            if (iter->second->GetPeerConnectionInfo().RealTimePeerInfo.max_upload_speed_ >=
                iter->second->GetPeerConnectionInfo().RealTimePeerInfo.mine_upload_speed_ + live_exchange_large_upload_ability_delim_)
            {
                large_upload_ability_peers.push_back(iter->second);
            }
        }

        if (large_upload_ability_peers.size() > live_exchange_large_upload_ability_max_count_)
        {
            std::sort(large_upload_ability_peers.begin(), large_upload_ability_peers.end(), &LiveConnectionManger::CompareBasedOnUploadAbility);
        }

        SelectPeers(selected_peers, large_upload_ability_peers, live_exchange_large_upload_ability_max_count_);
    }

    void LiveConnectionManger::GetCandidatePeerInfosBasedOnUploadSpeed(std::set<protocol::CandidatePeerInfo> & selected_peers)
    {
        std::vector<boost::shared_ptr<LivePeerConnection> > large_upload_peers;

        for (std::map<boost::asio::ip::udp::endpoint, boost::shared_ptr<LivePeerConnection> >::const_iterator iter = peers_.begin();
            iter != peers_.end(); ++iter)
        {
            if (iter->second->GetSpeedInfoEx().RecentDownloadSpeed >= live_exchange_large_upload_to_me_delim_)
            {
                large_upload_peers.push_back(iter->second);
            }
        }

        if (large_upload_peers.size() > live_exchange_large_upload_to_me_max_count_)
        {
            std::sort(large_upload_peers.begin(), large_upload_peers.end(), &LiveConnectionManger::CompareBasedOnUploadSpeed);
        }

        SelectPeers(selected_peers, large_upload_peers, live_exchange_large_upload_to_me_max_count_);
    }

    void LiveConnectionManger::SelectPeers(std::set<protocol::CandidatePeerInfo> & selected_peers,
        const std::vector<boost::shared_ptr<LivePeerConnection> > & sorted_peers, boost::uint32_t to_select_peers_count)
    {
        boost::uint32_t selected_peers_count = 0;
        for (size_t i = 0; i < sorted_peers.size(); ++i)
        {
            if (selected_peers_count == to_select_peers_count)
            {
                break;
            }

            if (selected_peers.find(sorted_peers[i]->GetCandidatePeerInfo()) == selected_peers.end())
            {
                ++selected_peers_count;
                selected_peers.insert(sorted_peers[i]->GetCandidatePeerInfo());
            }
        }
    }

    bool LiveConnectionManger::CompareBasedOnUploadAbility(boost::shared_ptr<LivePeerConnection> const & lhs,
        boost::shared_ptr<LivePeerConnection> const & rhs)
    {
        boost::int32_t lhs_upload_ability = lhs->GetPeerConnectionInfo().RealTimePeerInfo.max_upload_speed_
            - lhs->GetPeerConnectionInfo().RealTimePeerInfo.mine_upload_speed_;

        boost::int32_t rhs_upload_ability = rhs->GetPeerConnectionInfo().RealTimePeerInfo.max_upload_speed_
            - rhs->GetPeerConnectionInfo().RealTimePeerInfo.mine_upload_speed_;

        if (lhs_upload_ability != rhs_upload_ability)
        {
            return lhs_upload_ability > rhs_upload_ability;
        }

        return lhs < rhs;
    }

    bool LiveConnectionManger::CompareBasedOnUploadSpeed(boost::shared_ptr<LivePeerConnection> const & lhs,
        boost::shared_ptr<LivePeerConnection> const & rhs)
    {
        if (lhs->GetSpeedInfoEx().RecentDownloadSpeed != rhs->GetSpeedInfoEx().RecentDownloadSpeed)
        {
            return lhs->GetSpeedInfoEx().RecentDownloadSpeed > rhs->GetSpeedInfoEx().RecentDownloadSpeed;
        }

        return lhs < rhs;
    }

    bool LiveConnectionManger::IsFromUdpServer(const boost::asio::ip::udp::endpoint & end_point)
    {
        std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::iterator peer = peers_.find(end_point);
        if (peer != peers_.end())
        {
            LivePeerConnection::p peer_connection = peer->second;
            return peer_connection->IsUdpServer();
        }
        else
        {
            return false;
        }
    }

    void LiveConnectionManger::ClearSubPiecesRequestdToUdpServer()
    {
        for (std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::iterator iter = peers_.begin();
            iter != peers_.end();
            ++iter)
        {
            iter->second->ClearSubPiecesRequestedToUdpServer();
        }
    }

    boost::uint32_t LiveConnectionManger::GetSubPieceRequestedToUdpServerCount() const
    {
        std::set<protocol::LiveSubPieceInfo> subpiece_requested_to_udpserver;
        for (std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::const_iterator iter = peers_.begin();
            iter != peers_.end();
            ++iter)
        {
            iter->second->GetSubPieceRequestedToUdpServer(subpiece_requested_to_udpserver);
        }

        return subpiece_requested_to_udpserver.size();
    }
}