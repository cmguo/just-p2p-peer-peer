//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// IpPool.h

#ifndef _P2SP_P2P_IPPOOL_H_
#define _P2SP_P2P_IPPOOL_H_

#include "p2sp/p2p/IpPoolIndex.h"

namespace p2sp
{
    class P2PDownloader;
    typedef boost::shared_ptr<P2PDownloader> P2PDownloader__p;

    struct IPPoolIndexUpdating;

    class IpPool
        : public boost::noncopyable
        , public boost::enable_shared_from_this<IpPool>
#ifdef DUMP_OBJECT
        , public count_object_allocate<IpPool>
#endif
    {
        friend struct IPPoolIndexUpdating;
    public:
        typedef boost::shared_ptr<IpPool> p;
        static p create() { return p(new IpPool()); }
    public:
        // 启停
        void Start();
        void Stop();
        // 操作
        void AddCandidatePeers(const std::vector<protocol::CandidatePeerInfo>& peers);
        bool GetForConnect(protocol::CandidatePeerInfo& peer);
        bool GetForExchange(protocol::CandidatePeerInfo& peer);
        uint32_t GetPeerCount() const { return candidate_peers_.size();}

        // 属性
        void OnConnect(const boost::asio::ip::udp::endpoint& end_point);
        void OnConnectFailed(const boost::asio::ip::udp::endpoint& end_point);
        void OnConnectSucced(const boost::asio::ip::udp::endpoint& end_point);
        void OnConnectTimeout(const boost::asio::ip::udp::endpoint& end_point);
        void OnDisConnect(const boost::asio::ip::udp::endpoint& end_point);

        boost::int32_t GetNotTriedPeerCount() const { return not_tried_peer_count_;}

    protected:
        void AddIndex(CandidatePeer::p peer);
        void DeleteIndex(CandidatePeer::p peer);
        void AddPeer(CandidatePeer::p peer);
        bool IsSelf(CandidatePeer::p peer);
    private:
        // 变量
        std::map<protocol::SocketAddr, CandidatePeer::p> candidate_peers_;
        std::map<ExchangeIndicator, CandidatePeer::p> exchange_index_;
        std::map<ConnectIndicator, CandidatePeer::p> connect_index_;
        std::map<ActiveIndicator, CandidatePeer::p> active_index_;

        // 状态
        volatile bool is_running_;

        boost::int32_t not_tried_peer_count_;
    private:
        // 构造
        IpPool() : is_running_(false) , not_tried_peer_count_(0) {}
    };
}
#endif  // _P2SP_P2P_IPPOOL_H_
