//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "SessionPeerCache.h"

using namespace p2sp;

#ifdef LOG_ENABLE
static log4cplus::Logger logger_session_peer_cache = log4cplus::Logger::getInstance("[session_peer_cache]");
#endif

SessionPeerCache::SessionPeerCache()
{

}

bool SessionPeerCache::IsHit(const string& session_id, boost::asio::ip::udp::endpoint end_point)
{
    if (m_SessionPeerMap[session_id].find(end_point) != m_SessionPeerMap[session_id].end())
    {
        // 命中
        return true;
    }

    return false;
}

SESSION_PEER_INFO SessionPeerCache::GetSessionPeerInfo(const string& session_id, boost::asio::ip::udp::endpoint end_point)
{
    LOG4CPLUS_DEBUG_LOG(logger_session_peer_cache, "GetSessionPeerInfo");
    if (m_SessionPeerMap[session_id].find(end_point) != m_SessionPeerMap[session_id].end())
    {
        // 命中
        LOG4CPLUS_DEBUG_LOG(logger_session_peer_cache, "GetSessionPeerInfo hit!");
        return m_SessionPeerMap[session_id][end_point];
    }
    return SESSION_PEER_INFO();
}

bool SessionPeerCache::QuerySessionPeers(const string& session_id, std::vector<protocol::CandidatePeerInfo>& peers)
{
    LOG4CPLUS_DEBUG_LOG(logger_session_peer_cache, "session_id = " << session_id);

    peers.clear();

    if (session_id == "")
    {
        return false;
    }

    if (m_SessionPeerMap.find(session_id) != m_SessionPeerMap.end())
    {
        // Cache命中
        // TODO: 命中是否需要更新时间？让过期延长？但命中 != 连通……
        std::multimap<boost::uint32_t, protocol::CandidatePeerInfo> result_cache;
        TimePeerMap::reverse_iterator iter;
        for (iter = m_SessionPeerMap[session_id].rbegin(); iter != m_SessionPeerMap[session_id].rend(); iter++)
        {
            result_cache.insert(std::make_pair(iter->second.m_avg_delt_time, iter->second.m_candidate_peer_info));
        }
        for (std::multimap<uint32_t, protocol::CandidatePeerInfo>::iterator it = result_cache.begin();
            it != result_cache.end() && peers.size() <= 20; ++it)
        {
            LOG4CPLUS_DEBUG_LOG(logger_session_peer_cache, "Peer AvgDelt = " << it->first << ", Endpoint = " 
                << it->second);
            peers.push_back(it->second);
        }
        LOG4CPLUS_DEBUG_LOG(logger_session_peer_cache, "Found Peers = " << peers.size());
        return true;
    }
    else
    {
        LOG4CPLUS_DEBUG_LOG(logger_session_peer_cache, "No Such session_id");
        // Cache未命中
        return false;
    }
}

void SessionPeerCache::AddSessionPeer(const string& session_id, boost::asio::ip::udp::endpoint end_point, SESSION_PEER_INFO session_peer_info)
{
    LOG4CPLUS_DEBUG_LOG(logger_session_peer_cache, "AddSessionPeer session_id = " << session_id << ", peer_info = " 
        << session_peer_info.m_candidate_peer_info);
    if (session_id != "")
    {
        // 设置更高优先级
        session_peer_info.m_candidate_peer_info.UploadPriority = 1;

        session_peer_info.m_time = time(NULL);

        if (m_SessionPeerMap[session_id].find(end_point) != m_SessionPeerMap[session_id].end())
        {
            // 已经存在

            // 更新时间
            m_SessionPeerMap[session_id][end_point].m_time = session_peer_info.m_time;

            // 更新RTT
            if (m_SessionPeerMap[session_id][end_point].m_rtt > session_peer_info.m_rtt)
            {
                LOG4CPLUS_DEBUG_LOG(logger_session_peer_cache, "Update RTT  = " << session_peer_info.m_rtt);
                m_SessionPeerMap[session_id][end_point].m_rtt = session_peer_info.m_rtt;
            }

            // 更新window_size
            if (m_SessionPeerMap[session_id][end_point].m_window_size < session_peer_info.m_window_size)
            {
                LOG4CPLUS_DEBUG_LOG(logger_session_peer_cache, "Update window_size  = " 
                    << session_peer_info.m_request_size);
                m_SessionPeerMap[session_id][end_point].m_window_size = session_peer_info.m_window_size;
            }

            // 更新m_request_size
            if (m_SessionPeerMap[session_id][end_point].m_request_size < session_peer_info.m_request_size)
            {
                LOG4CPLUS_DEBUG_LOG(logger_session_peer_cache, "Update window_size  = " 
                    << session_peer_info.m_request_size);
                m_SessionPeerMap[session_id][end_point].m_request_size = session_peer_info.m_request_size;
            }

            // 更新avg_delt_time
            if (m_SessionPeerMap[session_id][end_point].m_avg_delt_time > session_peer_info.m_avg_delt_time)
            {
                LOG4CPLUS_DEBUG_LOG(logger_session_peer_cache, "Update avg_delt_time  = " 
                    << session_peer_info.m_avg_delt_time);
                m_SessionPeerMap[session_id][end_point].m_avg_delt_time = session_peer_info.m_avg_delt_time;
            }
        }
        else
        {
            // 加入缓存
            m_SessionPeerMap[session_id].insert(std::make_pair(end_point, session_peer_info));
        }
    }
}

void SessionPeerCache::DelSessionPeer(const string& session_id, boost::asio::ip::udp::endpoint end_point)
{
    LOG4CPLUS_DEBUG_LOG(logger_session_peer_cache, "DelSessionPeer session_id = " << session_id 
        << ", endpoint = " << end_point);

    TimePeerMap::iterator i;
    for (i = m_SessionPeerMap[session_id].begin(); i != m_SessionPeerMap[session_id].end();)
    {
        if (i->first == end_point)
        {
            // del
            m_SessionPeerMap[session_id].erase(i++);
        }
        else
        {
            ++i;
        }
    }
}

void SessionPeerCache::ExpireCache()
{
    LOG4CPLUS_DEBUG_LOG(logger_session_peer_cache, "");
    time_t timenow = time(NULL);
    SessionPeerMap::iterator iter;
    for (iter = m_SessionPeerMap.begin(); iter != m_SessionPeerMap.end();)
    {
        LOG4CPLUS_DEBUG_LOG(logger_session_peer_cache, "SessionID = " << iter->first << ", CacheSize = " 
            << iter->second.size());
        TimePeerMap::iterator i;
        for (i = iter->second.begin(); i != iter->second.end();)
        {
            boost::int32_t elapsed_time = (timenow - i->second.m_time);
            LOG4CPLUS_DEBUG_LOG(logger_session_peer_cache, "CheckPeer, ElapsedTime: " << elapsed_time << ", peer = " 
                << i->second.m_candidate_peer_info);
            // 如果超过10分钟，则过期
            if (elapsed_time >= 600)
            {
                LOG4CPLUS_DEBUG_LOG(logger_session_peer_cache, "Peer Expired, ElapsedTime = " << elapsed_time);
                iter->second.erase(i++);
            }
            else
            {
                ++i;
            }
        }

        // 判断顶层是否为空
        if (iter->second.empty())
        {
            LOG4CPLUS_DEBUG_LOG(logger_session_peer_cache, "cache empty, session_id = " << iter->first);
            // 为空，删除iter.
            m_SessionPeerMap.erase(iter++);
        }
        else
        {
            ++iter;
        }
    }
}
