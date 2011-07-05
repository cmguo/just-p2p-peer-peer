//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// Exchanger.h

#ifndef _P2SP_P2P_EXCHANGER_H_
#define _P2SP_P2P_EXCHANGER_H_

#include <protocol/PeerPacket.h>
#include "p2sp/download/SwitchControllerInterface.h"

namespace p2sp
{
    class IP2PControlTarget;

    class IpPool;
    typedef boost::shared_ptr<IpPool> IpPool__p;

    class Exchanger
        : public boost::noncopyable
        , public boost::enable_shared_from_this<Exchanger>
#ifdef DUMP_OBJECT
        , public count_object_allocate<Exchanger>
#endif
    {
    public:
        typedef boost::shared_ptr<Exchanger> p;
        static p create(IP2PControlTarget::p p2p_downloader, IpPool__p ip_pool) { return p(new Exchanger(p2p_downloader, ip_pool)); }
    public:
        // 启停
        void Start();
        void Stop();
        // 消息
        void OnP2PTimer(uint32_t times);
        void OnPeerExchangePacket(const protocol::PeerExchangePacket & packet);
    public:
        void DoPeerExchange(protocol::CandidatePeerInfo candidate_peerinfo) const;
    private:
        // 变量
        IP2PControlTarget::p p2p_downloader_;
        // 状态
        bool is_running_;
        IpPool__p ip_pool_;
    private:
        // 构造
        Exchanger(IP2PControlTarget::p p2p_downloader, IpPool__p ip_pool)
            : p2p_downloader_(p2p_downloader)
            , is_running_(false)
            , ip_pool_(ip_pool)
        {}
    };
}
#endif  // _P2SP_P2P_EXCHANGER_H_
