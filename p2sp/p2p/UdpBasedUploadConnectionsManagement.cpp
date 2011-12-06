//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "p2sp/AppModule.h"
#include "p2sp/p2p/P2PModule.h"
#include "p2sp/p2p/PeerHelper.h"
#include "p2sp/p2p/UdpBasedUploadConnectionsManagement.h"


namespace p2sp
{
    boost::uint32_t UdpBasedUploadConnectionsManagement::GetCurrentUploadSpeed() const
    {
        boost::uint32_t current_upload_speed = 0;
        std::map<boost::asio::ip::udp::endpoint, std::pair<boost::uint32_t, framework::timer::TickCounter> >::const_iterator iter = uploading_peers_speed_.begin();
        for (; iter != uploading_peers_speed_.end(); ++iter)
        {
            if (iter->second.second.elapsed() < 5000)
            {
                if (!IsPeerFromSameSubnet(iter->first))
                {
                    current_upload_speed += iter->second.first;
                }
            }
        }

        return current_upload_speed;
    }

    bool UdpBasedUploadConnectionsManagement::KickABadUploadConnection(const PEER_UPLOAD_INFO & potential_new_peer_info)
    {
        boost::int32_t max_speed_limit_in_KBps = P2PModule::Inst()->GetUploadSpeedLimitInKBps();
        boost::uint32_t now_upload_data_speed = statistic::StatisticModule::Inst()->GetUploadDataSpeed();

        for (std::set<boost::asio::ip::udp::endpoint>::iterator it = accept_uploading_peers_.begin(); 
            it != accept_uploading_peers_.end(); 
            ++it)
        {
            std::map<boost::asio::ip::udp::endpoint, PEER_UPLOAD_INFO>::iterator iter = accept_connecting_peers_.find(*it);

            if (iter != accept_connecting_peers_.end())
            {
                if (iter->second.last_data_time.elapsed() >= 3 * 1000)
                {
                    uploading_peers_speed_.erase(*it);
                    accept_uploading_peers_.erase(it);
                    return true;
                }
                else if (true == potential_new_peer_info.is_open_service && false == iter->second.is_open_service &&
                    (boost::int32_t)now_upload_data_speed + 5 * 1024 > max_speed_limit_in_KBps * 1024)
                {
                    uploading_peers_speed_.erase(*it);
                    accept_uploading_peers_.erase(it);
                    return true;
                }
            }
            else
            {
                assert(false);
            }
        }

        return false;
    }

    bool UdpBasedUploadConnectionsManagement::AcceptsNewUploadConnection(boost::asio::ip::udp::endpoint const& end_point)
    {
        boost::int32_t max_speed_limit_in_KBps = P2PModule::Inst()->GetUploadSpeedLimitInKBps();
        boost::int32_t max_upload_peers = P2PModule::Inst()->GetMaxUploadLimitSize();

        if (max_speed_limit_in_KBps <= -1)
        {
            //no limit
            return true;
        }
        
        if (max_upload_peers == 0)
        {
            return false;
        }
        
        if ((boost::uint32_t)max_upload_peers > accept_uploading_peers_.size())
        {
            DebugLog("UPLOAD: upload peers count %d is smaller than the limit %d. New upload connection is accepted.", accept_uploading_peers_.size(), max_upload_peers);
            return true;
        }
        
        assert(accept_connecting_peers_.find(end_point) != accept_connecting_peers_.end());
        const PEER_UPLOAD_INFO& candidate_peer_upload_info = accept_connecting_peers_[end_point];
        if (KickABadUploadConnection(candidate_peer_upload_info))
        {
            DebugLog("UPLOAD: After kicking a bad upload connection, the new upload connection is accepted.");
            return true;
        }

        boost::uint32_t now_upload_data_speed = statistic::StatisticModule::Inst()->GetUploadDataSpeed();

        //we are flexible here, as long as it's not close to the upload limit
        if (accept_uploading_peers_.size() <= (boost::uint32_t)max_upload_peers + 3 && 
            now_upload_data_speed + 5 * 1024 < (boost::uint32_t)max_speed_limit_in_KBps * 1024)
        {
            DebugLog("UPLOAD: Upload speed is not close to the speed limit yet, upload connection is accepted.");
            return true;
        }
        
        //if we are really important to this peer, try harder
        if (candidate_peer_upload_info.ip_pool_size > 0 && candidate_peer_upload_info.ip_pool_size <= 30)
        {
            if (accept_uploading_peers_.size() <= (boost::uint32_t)max_upload_peers + 3)
            {
                DebugLog("UPLOAD: Peer IP Pool size is small and the upload connection is accepted.");
                return true;
            }
        }
        
        //failing all attempts
        return false;
    }

    bool UdpBasedUploadConnectionsManagement::AcceptsNewConnection(size_t ip_pool_size) const
    {
        boost::int32_t max_speed_limit_in_KBps = P2PModule::Inst()->GetUploadSpeedLimitInKBps();
        boost::int32_t max_connect_peers = P2PModule::Inst()->GetMaxConnectLimitSize();

        if (max_speed_limit_in_KBps <= -1)
        {
            //no limit
            return true;
        }

        if (max_connect_peers == 0)
        {
            return false;
        }

        boost::uint32_t now_upload_data_speed = statistic::StatisticModule::Inst()->GetUploadDataSpeed();
        if (static_cast<boost::uint32_t>(max_connect_peers) > accept_connecting_peers_.size())
        {
            DebugLog("UPLOAD: connected peers count %d is smaller than the limit %d. New connection is accepted.", accept_connecting_peers_.size(), max_connect_peers);
            return true;
        }

        if (ip_pool_size > 0 && ip_pool_size <= 40 &&
            static_cast<boost::uint32_t>(max_connect_peers) + 3 > accept_connecting_peers_.size())
        {
            DebugLog("UPLOAD: Peer IP Pool size is small and the connection is accepted.");
            return true;
        }

        if (static_cast<boost::uint32_t>(max_speed_limit_in_KBps) * 1024 >= now_upload_data_speed + 5 * 1024)
        {
            DebugLog("UPLOAD: Upload speed %d is not close to the limit %d yet, and the connection is accepted.", now_upload_data_speed, max_speed_limit_in_KBps);
            return true;
        }

        if (UploadingToNonOpenServicePeer())
        {
            return true;
        }

        return false;
    }

    void UdpBasedUploadConnectionsManagement::KickTimedOutConnections(std::set<boost::asio::ip::udp::endpoint> & kicked_endpoints)
    {
        for (std::map<boost::asio::ip::udp::endpoint, PEER_UPLOAD_INFO>::iterator 
            iter = accept_connecting_peers_.begin(); iter != accept_connecting_peers_.end();)
        {
            if (iter->second.last_talk_time.elapsed() >= 10*1000)
            {
                kicked_endpoints.insert(iter->first);

                KickConnection((iter++)->first);
            }
            else
            {
                ++iter;
            }
        }
    }

    void UdpBasedUploadConnectionsManagement::KickAllConnections()
    {
        accept_connecting_peers_.clear();
        accept_uploading_peers_.clear();
        uploading_peers_speed_.clear();
    }

    void UdpBasedUploadConnectionsManagement::KickBadConnections(size_t desirable_upload_speed, size_t new_peer_protection_time_in_seconds)
    {
        boost::int32_t max_speed_limit_in_KBps = P2PModule::Inst()->GetUploadSpeedLimitInKBps();
        boost::int32_t max_connect_peers = UploadModule::Inst()->GetMaxConnectLimitSize();

        if (max_connect_peers < 0 || max_speed_limit_in_KBps < 0)
        {
            return;
        }

        // 如果连接数超过了最大允许的值，则按照速度由小到大首先踢掉连接
        // 如果刚连接上还没超过保护时间，则不踢
        std::multimap<boost::uint32_t, boost::asio::ip::udp::endpoint> kick_upload_connection_map;
        boost::int32_t need_kick_count = 0;  // 需要踢掉的连接数
        if (accept_connecting_peers_.size() > (boost::uint32_t)max_connect_peers)
        {
            need_kick_count = accept_connecting_peers_.size() - max_connect_peers;
            for (std::map<boost::asio::ip::udp::endpoint, PEER_UPLOAD_INFO>::iterator
                iter = accept_connecting_peers_.begin(); iter != accept_connecting_peers_.end(); ++iter)
            {
                kick_upload_connection_map.insert(std::make_pair(iter->second.speed_info.GetSpeedInfo().NowUploadSpeed, iter->first));
            }
        }

        // 已经踢掉的连接个数
        std::multimap<boost::uint32_t, boost::asio::ip::udp::endpoint>::iterator iter_kick = kick_upload_connection_map.begin();
        for (boost::int32_t kicked_count = 0; kicked_count < need_kick_count;)
        {
            if (iter_kick == kick_upload_connection_map.end() || iter_kick->first >= desirable_upload_speed)
            {
                break;
            }

            const boost::asio::ip::udp::endpoint & ep = iter_kick->second;
            if (accept_connecting_peers_[ep].connected_time.elapsed() >= new_peer_protection_time_in_seconds * 1000)
            {
                KickConnection(ep);

                ++kicked_count;
            }

            ++iter_kick;
        }
    }

    bool UdpBasedUploadConnectionsManagement::IsPeerFromSameSubnet(const boost::asio::ip::udp::endpoint & peer_endpoint) const
    {
        std::map<boost::asio::ip::udp::endpoint, PEER_UPLOAD_INFO>::const_iterator peer_upload_info_iter = 
            accept_connecting_peers_.find(peer_endpoint);

        if (peer_upload_info_iter != accept_connecting_peers_.end())
        {
            return PeerHelper::IsPeerFromSameSubnet(peer_upload_info_iter->second.peer_info);
        }

        return false;
    }

    bool UdpBasedUploadConnectionsManagement::UploadingToNonOpenServicePeer() const
    {
        for (std::set<boost::asio::ip::udp::endpoint>::const_iterator upload_iter = accept_uploading_peers_.begin(); 
            upload_iter != accept_uploading_peers_.end(); 
            ++upload_iter)
        {
            const boost::asio::ip::udp::endpoint& ep = *upload_iter;

            std::map<boost::asio::ip::udp::endpoint, PEER_UPLOAD_INFO>::const_iterator iter = accept_connecting_peers_.find(ep);

            if (iter != accept_connecting_peers_.end())
            {
                if (false == iter->second.is_live && 
                    false == iter->second.is_open_service)
                {
                    return true;
                }
            }
        }

        return false;
    }

    void UdpBasedUploadConnectionsManagement::GetUploadingPeersExcludeSameSubnet(std::set<boost::asio::ip::address> & uploading_peers) const
    {
        for (std::set<boost::asio::ip::udp::endpoint>::const_iterator iter = accept_uploading_peers_.begin();
            iter != accept_uploading_peers_.end(); ++iter)
        {
            if (!IsPeerFromSameSubnet(*iter))
            {
                uploading_peers.insert(iter->address());
            }
        }
    }

    boost::uint32_t UdpBasedUploadConnectionsManagement::GetAcceptUploadingPeersCount() const
    {
        return accept_uploading_peers_.size();
    }

    void UdpBasedUploadConnectionsManagement::KickConnection(const boost::asio::ip::udp::endpoint kick_endpoint)
    {
        accept_connecting_peers_.erase(kick_endpoint);
        accept_uploading_peers_.erase(kick_endpoint);
        uploading_peers_speed_.erase(kick_endpoint);
    }
}