//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "SessionPeerCache.h"

using namespace p2sp;

#define SPC_DEBUG(msg) LOGX(__DEBUG, "P2P", msg)

FRAMEWORK_LOGGER_DECLARE_MODULE("P2P");

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
    SPC_DEBUG("GetSessionPeerInfo");
    if (m_SessionPeerMap[session_id].find(end_point) != m_SessionPeerMap[session_id].end())
    {
        // 命中
        SPC_DEBUG("GetSessionPeerInfo hit!");
        return m_SessionPeerMap[session_id][end_point];
    }
    return SESSION_PEER_INFO();
}

bool SessionPeerCache::QuerySessionPeers(const string& session_id, std::vector<protocol::CandidatePeerInfo>& peers)
{
    SPC_DEBUG("session_id = " << session_id);

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
            SPC_DEBUG("Peer AvgDelt = " << it->first << ", Endpoint = " << it->second);
            peers.push_back(it->second);
        }
        SPC_DEBUG("Found Peers = " << peers.size());
        return true;
    }
    else
    {
        SPC_DEBUG("No Such session_id");
        // Cache未命中
        return false;
    }
}

void SessionPeerCache::AddSessionPeer(const string& session_id, boost::asio::ip::udp::endpoint end_point, SESSION_PEER_INFO session_peer_info)
{
    SPC_DEBUG("AddSessionPeer session_id = " << session_id << ", peer_info = " << session_peer_info.m_candidate_peer_info);
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
                SPC_DEBUG("Update RTT  = " << session_peer_info.m_rtt);
                m_SessionPeerMap[session_id][end_point].m_rtt = session_peer_info.m_rtt;
            }

            // 更新window_size
            if (m_SessionPeerMap[session_id][end_point].m_window_size < session_peer_info.m_window_size)
            {
                SPC_DEBUG("Update window_size  = " << session_peer_info.m_request_size);
                m_SessionPeerMap[session_id][end_point].m_window_size = session_peer_info.m_window_size;
            }

            // 更新m_request_size
            if (m_SessionPeerMap[session_id][end_point].m_request_size < session_peer_info.m_request_size)
            {
                SPC_DEBUG("Update window_size  = " << session_peer_info.m_request_size);
                m_SessionPeerMap[session_id][end_point].m_request_size = session_peer_info.m_request_size;
            }

            // 更新avg_delt_time
            if (m_SessionPeerMap[session_id][end_point].m_avg_delt_time > session_peer_info.m_avg_delt_time)
            {
                SPC_DEBUG("Update avg_delt_time  = " << session_peer_info.m_avg_delt_time);
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
    SPC_DEBUG("DelSessionPeer session_id = " << session_id << ", endpoint = " << end_point);

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
    SPC_DEBUG("");
    time_t timenow = time(NULL);
    SessionPeerMap::iterator iter;
    for (iter = m_SessionPeerMap.begin(); iter != m_SessionPeerMap.end();)
    {
        SPC_DEBUG("SessionID = " << iter->first << ", CacheSize = " << iter->second.size());
        TimePeerMap::iterator i;
        for (i = iter->second.begin(); i != iter->second.end();)
        {
            boost::int32_t elapsed_time = (timenow - i->second.m_time);
            SPC_DEBUG("CheckPeer, ElapsedTime: " << elapsed_time << ", peer = " << i->second.m_candidate_peer_info);
            // 如果超过10分钟，则过期
            if (elapsed_time >= 600)
            {
                SPC_DEBUG("Peer Expired, ElapsedTime = " << elapsed_time);
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
            SPC_DEBUG("cache empty, session_id = " << iter->first);
            // 为空，删除iter.
            m_SessionPeerMap.erase(iter++);
        }
        else
        {
            ++iter;
        }
    }
}
