//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef UDP_BASED_UPLOAD_CONNECTIONS_MANAGEMENT_H
#define UDP_BASED_UPLOAD_CONNECTIONS_MANAGEMENT_H

#include "p2sp/p2p/UploadStruct.h"

namespace p2sp
{
    class UdpBasedUploadConnectionsManagement
    {
    public:
        boost::uint32_t GetCurrentUploadSpeed() const;
        bool AcceptsNewUploadConnection(boost::asio::ip::udp::endpoint const& end_point);
        bool AcceptsNewConnection(size_t ip_pool_size) const;

        void KickTimedOutConnections();
        void KickAllConnections();
        void KickBadConnections(size_t desirable_upload_speed, size_t new_peer_protection_time_in_seconds);

        void GetUploadingPeers(std::set<boost::asio::ip::address> & uploading_peers) const
        {
            for (std::set<boost::asio::ip::udp::endpoint>::const_iterator iter = accept_uploading_peers_.begin();
                iter != accept_uploading_peers_.end(); ++iter)
            {
                uploading_peers.insert(iter->address());
            }
        }
        
        bool IsPeerFromSameSubnet(const boost::asio::ip::udp::endpoint & peer_endpoint) const;

        bool IsPeerConnected(const boost::asio::ip::udp::endpoint & peer_endpoint) const
        {
            return accept_connecting_peers_.find(peer_endpoint) != accept_connecting_peers_.end();
        }

        bool IsPeerAcceptedForUpload(const boost::asio::ip::udp::endpoint & peer_endpoint) const
        {
            return accept_uploading_peers_.find(peer_endpoint) != accept_uploading_peers_.end();
        }

        void AddConnection(const boost::asio::ip::udp::endpoint & peer_endpoint)
        {
            assert(!IsPeerConnected(peer_endpoint));
            accept_connecting_peers_[peer_endpoint] = PEER_UPLOAD_INFO();
        }

        void AddUploadConnection(const boost::asio::ip::udp::endpoint & peer_endpoint)
        {
            assert(IsPeerConnected(peer_endpoint));
            assert(!IsPeerAcceptedForUpload(peer_endpoint));
            assert(uploading_peers_speed_.find(peer_endpoint) == uploading_peers_speed_.end());

            accept_uploading_peers_.insert(peer_endpoint);
            uploading_peers_speed_[peer_endpoint] = std::make_pair(0, framework::timer::TickCounter());
        }

        PEER_UPLOAD_INFO& GetPeerUploadInfo(const boost::asio::ip::udp::endpoint & peer_endpoint)
        {
            assert(IsPeerConnected(peer_endpoint));
            return accept_connecting_peers_[peer_endpoint];
        }

        bool TryAddUniqueTransactionId(const boost::asio::ip::udp::endpoint & peer_endpoint, uint32_t transaction_id)
        {
            if (GetPeerUploadInfo(peer_endpoint).IsInLastDataTransIDs(transaction_id))
            {
                return false;
            }
            
            GetPeerUploadInfo(peer_endpoint).UpdateLastDataTransID(transaction_id);
            return true;
        }

        void UpdateConnectionHeartbeat(const boost::asio::ip::udp::endpoint & peer_endpoint)
        {
            GetPeerUploadInfo(peer_endpoint).last_talk_time.reset();
        }

        void UpdateUploadHeartbeat(const boost::asio::ip::udp::endpoint & peer_endpoint)
        {
            GetPeerUploadInfo(peer_endpoint).last_data_time.reset();
        }

        void UpdatePeerUploadSpeed(const boost::asio::ip::udp::endpoint & peer_endpoint, boost::uint32_t current_speed)
        {
            assert(IsPeerAcceptedForUpload(peer_endpoint));

            std::pair<boost::uint32_t, framework::timer::TickCounter> & speed = uploading_peers_speed_[peer_endpoint];
            speed.first = current_speed;
            speed.second.reset();
        }

    private:
        bool KickABadUploadConnection(const PEER_UPLOAD_INFO & potential_new_peer_info);
        bool UploadingToNonOpenServicePeer() const;

    private:
        std::map<boost::asio::ip::udp::endpoint, PEER_UPLOAD_INFO> accept_connecting_peers_;
        std::set<boost::asio::ip::udp::endpoint> accept_uploading_peers_;
        std::map<boost::asio::ip::udp::endpoint, std::pair<boost::uint32_t, framework::timer::TickCounter> > uploading_peers_speed_;
    };
}

#endif //UDP_BASED_UPLOAD_CONNECTIONS_MANAGEMENT_H