//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/tracker/TrackerModule.h"
#include "p2sp/p2p/IpPool.h"
#include "p2sp/p2p/P2PDownloader.h"
#include "p2sp/AppModule.h"

#define IPPOOL_DEBUG(s)    LOG(__DEBUG, "ippool", s)
#define IPPOOL_INFO(s)    LOG(__INFO, "ippool", s)
#define IPPOOL_EVENT(s)    LOG(__EVENT, "ippool", s)
#define IPPOOL_WARN(s)    LOG(__WARN, "ippool", s)
#define IPPOOL_ERROR(s)    LOG(__ERROR, "ippool", s)


namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("ippool");
    //////////////////////////////////////////////////////////////////////////
    // 用于 智能 添加索引 和 删除索引所用
    struct IPPoolIndexUpdating
    {
    public:
        IPPoolIndexUpdating(CandidatePeer::p peer, IpPool::p ip_pool)
            : ip_pool_(ip_pool), peer_(peer)
        {
            assert(ip_pool_);
            assert(peer_);
            ip_pool_->DeleteIndex(peer_);

        }
        ~IPPoolIndexUpdating()
        {
            assert(ip_pool_);
            if (peer_)
            {
                ip_pool_->AddIndex(peer_);
            }
        }

        void Disable()
        {
            peer_.reset();
        }

    private:
        IpPool::p ip_pool_;
        CandidatePeer::p peer_;
    };

    void IpPool::Start()
    {
        if (is_running_ == true) return;

        IPPOOL_INFO("IpPool::Start");

        assert(candidate_peers_.size() == 0);

        is_running_ = true;

        srand((unsigned)time(NULL));
    }

    void IpPool::Stop()
    {
        if (is_running_ == false) return;

        IPPOOL_INFO("IpPool::Stop");

        // 清除内部所有的 protocol::CandidatePeerInfo 结构
        candidate_peers_.clear();

        is_running_ = false;
    }

    void IpPool::AddPeer(CandidatePeer::p peer)
    {
        if (is_running_ == false) return;
        assert(peer);
        assert(false == IsSelf(peer));

        protocol::SocketAddr key = peer->GetKey();

        // IPPOOL_INFO("UUP = " << *peer);

        if (black_list_.find(peer->GetKey()) !=  black_list_.end())
        {
            return;
        }

        std::map<protocol::SocketAddr, CandidatePeer::p>::iterator iter = candidate_peers_.find(key);
        if (iter == candidate_peers_.end())
        {
            IPPoolIndexUpdating indexUpdating(peer, shared_from_this());
            IPPOOL_INFO("IpPool::AddPeer because CandidatePeer " << *peer << " not found, so insert it");
            candidate_peers_.insert(std::make_pair(key, peer));
        }
        else
        {
            IPPOOL_INFO("IpPool::AddPeer because CandidatePeer " << *peer << " existed, so update it");
            CandidatePeer::p self_peer = iter->second;
            IPPoolIndexUpdating indexUpdating(self_peer, shared_from_this());
            self_peer->BeCopiedTo(peer);
        }
    }

    bool IpPool::IsSelf(CandidatePeer::p peer)
    {
        protocol::CandidatePeerInfo local_peer_info(AppModule::Inst()->GetCandidatePeerInfo());
        if (local_peer_info.IP == peer->IP &&
            local_peer_info.UdpPort == peer->UdpPort)
        {
            if (local_peer_info.DetectIP == 0)
                return true;
            if (local_peer_info.DetectIP == peer->DetectIP)
                return true;
        }

        return false;
    }

    void IpPool::AddCandidatePeers(const std::vector<protocol::CandidatePeerInfo>& peers)
    {
        if (is_running_ == false) return;

        // 遍历要添加的所有的 CandidatePeer
        for (std::vector<protocol::CandidatePeerInfo>::const_iterator iter = peers.begin(); iter != peers.end(); iter ++)
        {
            CandidatePeer::p peer = CandidatePeer::create(*iter);

            if (true == IsSelf(peer))
            {
                IPPOOL_INFO("IpPool::AddCandidatePeers can not add self peer " << *iter);
                continue;
            }
            // 将这些 CadidatePeerInfo 结构 添加到内部
            AddPeer(peer);
        }

        KickTrivialCandidatePeers();

        //DebugLog("IpPool::AddCandidatePeers not_tried_peer_count_:%d", not_tried_peer_count_);
    }

    void IpPool::KickTrivialCandidatePeers()
    {
        if (!desirable_pool_size_ ||
            candidate_peers_.empty())
        {
            return;
        }
            
        size_t peers_to_delete = 0;
        if (candidate_peers_.size() > desirable_pool_size_)
        {
            peers_to_delete = candidate_peers_.size() - desirable_pool_size_;
        }

        size_t peers_deleted = 0;
        size_t max_connection_attempts_allowed = desirable_pool_size_/this->candidate_peers_.size();

        for(std::map<ActiveIndicator, CandidatePeer::p>::iterator iter = active_index_.begin();
            iter != active_index_.end() && peers_deleted < peers_to_delete;)
        {
            CandidatePeer::p peer = iter->second;
            ++iter;

            if (!peer->is_connected_ && !peer->is_connecting_)
            {
                bool too_many_connection_attempts = peer->connections_attempted_ > max_connection_attempts_allowed;

                if (too_many_connection_attempts)
                {
                    assert(black_list_.find(peer->GetKey()) ==  black_list_.end());
                    black_list_.insert(peer->GetKey());
                }

                if (peers_deleted < peers_to_delete ||
                    too_many_connection_attempts)
                {
                    DeleteIndex(peer);
                    candidate_peers_.erase(peer->GetKey());
                    ++peers_deleted;
                }
            }
        }

        if (peers_deleted)
        {
            DebugLog("IpPool::KickTrivialCandidatePeers: kicked %d peers.", peers_deleted);
        }
    }

    bool IpPool::GetForConnect(protocol::CandidatePeerInfo& peer_info, bool is_udpserver)
    {
        if (is_running_ == false) return false;

        // 连接策略 实现
        if (connect_index_.size() == 0)
        {
            IPPOOL_WARN("IpPool::GetForConnect Failed! IpPoolSize = 0, CandidatePeer: " << peer_info);
            return false;
        }

        CandidatePeer::p peer = connect_index_.begin()->second;
        if (peer->CanConnect(is_udpserver) == false)
        {
            IPPOOL_WARN("IpPool::GetForConnect Failed! CanConnect = false, CandidatePeer: " << (*peer));
            return false;
        }

        peer_info = (protocol::CandidatePeerInfo)(*peer);
        IPPoolIndexUpdating indexUpdating(peer, shared_from_this());
        peer->last_connect_time_ = framework::timer::TickCounter::tick_count();

        return true;
    }

    bool IpPool::GetForExchange(protocol::CandidatePeerInfo& peer_info)
    {
        if (is_running_ == false) return false;

        // 洪范策略 实现
        if (exchange_index_.size() == 0)
            return false;

        CandidatePeer::p peer = exchange_index_.begin()->second;
        if (peer->CanExchange() == false)
        {
            IPPOOL_WARN("IpPool::GetForExchange Failed because CandidatePeer " << *peer << " can not exchange");
            return false;
        }

        peer_info = peer->GetCandidatePeerInfo();
        IPPoolIndexUpdating indexUpdating(peer, shared_from_this());
        peer->last_exchage_time_ = framework::timer::TickCounter::tick_count();

        return true;
    }

    void IpPool::OnConnect(const boost::asio::ip::udp::endpoint& end_point)
    {
        if (is_running_ == false) return;

        IPPOOL_EVENT("IpPool::OnConnect Endpoint = " << end_point);

        protocol::SocketAddr socket_addr(end_point.address().to_v4().to_ulong(), end_point.port());
        std::map<protocol::SocketAddr, CandidatePeer::p>::iterator iter = candidate_peers_.find(socket_addr);
        if (iter == candidate_peers_.end())
            return;

        CandidatePeer::p peer = iter->second;
        assert(peer);

        IPPoolIndexUpdating updating(peer, shared_from_this());
        peer->is_connecting_ = true;
        peer->is_connected_ = false;
        ++(peer->connections_attempted_);
        peer->OnConnectSucceed();
    }

    void IpPool::OnConnectFailed(const boost::asio::ip::udp::endpoint& end_point)
    {
        if (is_running_ == false) return;

        IPPOOL_EVENT("IpPool::OnConnectFailed Endpoint=" << end_point);

        protocol::SocketAddr socket_addr(end_point.address().to_v4().to_ulong(), end_point.port());
        std::map<protocol::SocketAddr, CandidatePeer::p>::iterator iter = candidate_peers_.find(socket_addr);
        if (iter == candidate_peers_.end())
            return;

        CandidatePeer::p peer = iter->second;
        assert(peer);

        IPPoolIndexUpdating updating(peer, shared_from_this());
        peer->is_connecting_ = false;
        peer->is_connected_ = false;
        peer->OnConnectFailed();
    }

    void IpPool::OnConnectTimeout(const boost::asio::ip::udp::endpoint& end_point)
    {
        if (is_running_ == false) return;

        IPPOOL_EVENT("IpPool::OnConnectTimeout Endpoint=" << end_point);

        protocol::SocketAddr socket_addr(end_point.address().to_v4().to_ulong(), end_point.port());
        std::map<protocol::SocketAddr, CandidatePeer::p>::iterator iter = candidate_peers_.find(socket_addr);
        if (iter == candidate_peers_.end())
            return;

        CandidatePeer::p peer = iter->second;
        assert(peer);

        IPPoolIndexUpdating updating(peer, shared_from_this());
        peer->is_connecting_ = false;
        peer->is_connected_ = false;
        peer->OnConnectTimeout();
    }
    void IpPool::OnConnectSucced(const boost::asio::ip::udp::endpoint& end_point)
    {
        if (is_running_ == false) return;

        IPPOOL_EVENT("IpPool::OnConnectSucced Endpoint=" << end_point);

        protocol::SocketAddr socket_addr(end_point.address().to_v4().to_ulong(), end_point.port());
        std::map<protocol::SocketAddr, CandidatePeer::p>::iterator iter = candidate_peers_.find(socket_addr);
        if (iter == candidate_peers_.end())
            return;

        CandidatePeer::p peer = iter->second;
        assert(peer);

        IPPoolIndexUpdating updating(peer, shared_from_this());
        peer->last_active_time_ = framework::timer::TickCounter::tick_count();
        peer->is_connecting_ = false;
        peer->is_connected_ = true;
    }

    void IpPool::OnDisConnect(const boost::asio::ip::udp::endpoint& end_point, bool is_active)
    {
        if (is_running_ == false) return;

        IPPOOL_EVENT("IpPool::OnDisConnect Endpoint=" << end_point);

        protocol::SocketAddr socket_addr(end_point.address().to_v4().to_ulong(), end_point.port());
        std::map<protocol::SocketAddr, CandidatePeer::p>::iterator iter = candidate_peers_.find(socket_addr);
        if (iter == candidate_peers_.end())
            return;

        CandidatePeer::p peer = iter->second;
        assert(peer);

        IPPoolIndexUpdating updating(peer, shared_from_this());
        peer->last_active_time_ = framework::timer::TickCounter::tick_count();

        // reset last_connect_time_ to avoid re-connecting the peer immediately		
        peer->last_connect_time_ = peer->last_active_time_;

        peer->is_connecting_ = false;
        peer->is_connected_ = false;

        if (is_active)
        {
            protocol::CloseSessionPacket packet(protocol::Packet::NewTransactionID(), 
                protocol::PEER_VERSION, end_point);

            AppModule::Inst()->DoSendPacket(packet, protocol::PEER_VERSION);
        }
    }

    void IpPool::AddIndex(CandidatePeer::p peer)
    {
        if (is_running_ == false) return;

        // IPPOOL_INFO("UPP = " << *peer);

        exchange_index_.insert(std::make_pair(peer->GetExchangeIndicator(), peer));
        connect_index_.insert(std::make_pair(peer->GetConnectIndicator(), peer));
        active_index_.insert(std::make_pair(peer->GetActiveIndicator(), peer));
    }

    void IpPool::DeleteIndex(CandidatePeer::p peer)
    {
        if (is_running_ == false) return;

        exchange_index_.erase(peer->GetExchangeIndicator());
        connect_index_.erase(peer->GetConnectIndicator());
        active_index_.erase(peer->GetActiveIndicator());
    }

    void IpPool::DisConnectAll()
    {
        for (std::map<protocol::SocketAddr, CandidatePeer::p>::iterator iter = candidate_peers_.begin();
            iter != candidate_peers_.end(); ++iter)
        {
            boost::asio::ip::udp::endpoint endpoint = framework::network::Endpoint(iter->first.IP, iter->first.Port);
            OnDisConnect(endpoint, true);
        }
    }

    boost::int32_t IpPool::GetNotTriedPeerCount() const 
    {
        boost::int32_t count(0);
        for (std::map<protocol::SocketAddr, CandidatePeer::p>::const_iterator iter = candidate_peers_.begin();
            iter != candidate_peers_.end(); ++iter)
        {
            if (iter->second->last_connect_time_ == 0)
            {
                ++count;
            }
        }

        return count;
    }
}