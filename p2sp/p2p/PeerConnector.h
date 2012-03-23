//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// PeerConnector.h

#ifndef _P2SP_P2P_PEER_CONNECTOR_H_
#define _P2SP_P2P_PEER_CONNECTOR_H_

#include <protocol/PeerPacket.h>
#include "p2sp/p2p/ConnectingPeerBase.h"
#include "p2sp/download/SwitchControllerInterface.h"

namespace p2sp
{
    class IpPool;
    typedef boost::shared_ptr<IpPool> IpPool__p;

    class IP2PControlTarget;

    class ConnectingPeer;

    class IConnectTimeoutHandler
    {
    public:
        virtual void OnConnectTimeout(const boost::asio::ip::udp::endpoint& end_point) = 0;
        virtual ~IConnectTimeoutHandler(){}
    };

    class PeerConnector
        : public boost::noncopyable
        , public boost::enable_shared_from_this<PeerConnector>
    {
    public:
        typedef boost::shared_ptr<PeerConnector> p;
        static p create(IP2PControlTarget::p p2p_downloader, IpPool__p ippool)
        {
            return p(new PeerConnector(p2p_downloader, ippool));
        }
    public:
        // 启停
        void Start(boost::shared_ptr<IConnectTimeoutHandler> connect_timeout_handler = boost::shared_ptr<IConnectTimeoutHandler>());
        void Stop();
        // 操作
        void Connect(const protocol::CandidatePeerInfo& candidate_peer_info);
        void CheckConnectTimeout();
        // 消息
        void OnP2PTimer(uint32_t times);
        void OnReConectPacket(protocol::ConnectPacket const & packet);
        void OnErrorPacket(protocol::ErrorPacket const & packet);
        // 属性
        uint32_t GetConnectingPeerCount() const {return connecting_peers_.size();}
    private:
        bool FindConnectingPeerEndPoint(boost::asio::ip::udp::endpoint end_point);
        bool EraseConnectingPeer(boost::asio::ip::udp::endpoint end_point);
        ConnectingPeer::p GetConnectingPeer(boost::asio::ip::udp::endpoint end_point);
    private:
        // 模块
        IP2PControlTarget::p p2p_downloader_;
        IpPool__p ippool_;
        // 变量
        std::map<boost::asio::ip::udp::endpoint, ConnectingPeer::p> connecting_peers_;    // 正在发起连接的 Peers
        // 状态
        volatile bool is_running_;

        boost::shared_ptr<IConnectTimeoutHandler> connect_timeout_handler_;
    private:
        // 构造
        PeerConnector(IP2PControlTarget::p p2p_downloader, IpPool__p ippool)
            : p2p_downloader_(p2p_downloader)
            , ippool_(ippool)
            , is_running_(false)
        {}
    };
}

#endif  // _P2SP_P2P_PEER_CONNECTOR_H_
