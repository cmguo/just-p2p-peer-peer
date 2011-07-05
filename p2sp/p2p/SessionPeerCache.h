//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// SessionPeerCache.h

#ifndef _P2SP_P2P_SESSION_PEER_CACHE_H_
#define _P2SP_P2P_SESSION_PEER_CACHE_H_
#include "struct/Base.h"

namespace p2sp
{

    struct SESSION_PEER_INFO
    {
        protocol::CandidatePeerInfo m_candidate_peer_info;
        time_t m_time;
        boost::uint32_t  m_rtt;
        boost::uint32_t  m_window_size;
        boost::uint32_t  m_request_size;
        boost::uint32_t  m_avg_delt_time;
    };

    class SessionPeerCache
        : public boost::noncopyable
        , public boost::enable_shared_from_this<SessionPeerCache>
#ifdef DUMP_OBJECT
        , public count_object_allocate<SessionPeerCache>
#endif
    {
    public:
        typedef boost::shared_ptr<SessionPeerCache> p;
        static p create()
        {
            return SessionPeerCache::p(new SessionPeerCache());
        }

    public:
        bool IsHit(const string& session_id, boost::asio::ip::udp::endpoint end_point);
        SESSION_PEER_INFO GetSessionPeerInfo(const string& session_id, boost::asio::ip::udp::endpoint end_point);
        bool QuerySessionPeers(const string& session_id, std::vector<protocol::CandidatePeerInfo>& peers);
        void AddSessionPeer(const string& session_id, boost::asio::ip::udp::endpoint end_point, SESSION_PEER_INFO session_peer_info);
        void DelSessionPeer(const string& session_id, boost::asio::ip::udp::endpoint end_point);
        void ExpireCache();

    private:
        SessionPeerCache();

        typedef std::map<boost::asio::ip::udp::endpoint, SESSION_PEER_INFO> TimePeerMap;
        typedef std::map<string, TimePeerMap> SessionPeerMap;

        SessionPeerMap m_SessionPeerMap;
    };
}

#endif  // _P2SP_P2P_SESSION_PEER_CACHE_H_

