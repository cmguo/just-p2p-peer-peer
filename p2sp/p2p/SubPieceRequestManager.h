//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// SubPieceRequestManager.h

#ifndef _P2SP_P2P_SUB_PIECE_REQUEST_MANAGER_H_
#define _P2SP_P2P_SUB_PIECE_REQUEST_MANAGER_H_

#ifdef BOOST_WINDOWS_API
#pragma once
#endif

#include "p2sp/p2p/PeerConnection.h"

namespace p2sp
{
    class SubPieceRequestTask
        : public boost::noncopyable
#ifdef DUMP_OBJECT
        , public count_object_allocate<SubPieceRequestTask>
#endif
    {
    public:
        SubPieceRequestTask(uint32_t timeout, boost::intrusive_ptr<ConnectionBase> peer_connection)
            : request_time_elapse_(0)
            , timeout_(timeout)
            , dead_(false)
            , peer_connection_(peer_connection)
        {

        }

        bool IsTimeOut() {return request_time_elapse_ > timeout_;}
        framework::timer::TickCounter::count_value_type GetTimeElapsed() const { return request_time_elapse_; }

    public:
        uint32_t request_time_elapse_;
        uint32_t timeout_;
        bool dead_;
        boost::intrusive_ptr<ConnectionBase> peer_connection_;
    };

    class P2PDownloader;
    typedef boost::shared_ptr<P2PDownloader> P2PDownloader__p;

    class SubPieceRequestManager
    {
    public:
        SubPieceRequestManager()
            : is_running_(false)
        {

        }

        // 启停
        void Start(P2PDownloader__p p2p_downloader);
        void Stop();
        // 操作
        void Add(const protocol::SubPieceInfo& subpiece_info, boost::uint32_t timeout,
            boost::intrusive_ptr<ConnectionBase> peer_connection);
        void CheckExternalTimeout();
        // 消息
        void OnSubPiece(protocol::SubPiecePacket const & packet);
        void OnP2PTimer(uint32_t times);
        // 属性
        inline bool IsRequesting(const protocol::SubPieceInfo& subpiece_info) const;
        inline bool IsRequestingTimeout(const protocol::SubPieceInfo& subpiece_info, boost::uint32_t time_elapsed, boost::uint8_t percent = 9) const;
        uint32_t GetRequestingCount(const protocol::SubPieceInfo& subpiece_info, boost::uint32_t time_elapsed) const;
    private:
        // 模块
        P2PDownloader__p p2p_downloader_;
        // 变量
        std::multimap<protocol::SubPieceInfo, SubPieceRequestTask *> request_tasks_;
        // 状态
        volatile bool is_running_;

        boost::uint32_t block_size_;
    };

    inline bool SubPieceRequestManager::IsRequesting(const protocol::SubPieceInfo& subpiece_info) const
    {
        if (is_running_ == false) return false;

        // 找到返回true, 找不到返回flase
        std::multimap<protocol::SubPieceInfo, SubPieceRequestTask *>::const_iterator iter;
        iter = request_tasks_.find(subpiece_info);
        while (iter != request_tasks_.end() && iter->first == subpiece_info)
        {
            if (iter->second->dead_ == false)
            {
                return true;
            }
            ++iter;
        }
        return false;
    }

    // True：正在请求，且时间超过time_elapsed; 或者没有被请求;
    inline bool SubPieceRequestManager::IsRequestingTimeout(const protocol::SubPieceInfo& subpiece_info, boost::uint32_t time_elapsed, boost::uint8_t percent) const
    {
        if (false == is_running_) return false;

        // 所有task超时才算超时
        std::multimap<protocol::SubPieceInfo, SubPieceRequestTask *>::const_iterator it;
        for (it = request_tasks_.find(subpiece_info); it != request_tasks_.end() && it->first == subpiece_info; ++it)
        {
            SubPieceRequestTask * task = it->second;
            boost::intrusive_ptr<ConnectionBase> peer_conn = task->peer_connection_;
            if (peer_conn->GetConnectedTime() >= 3 * 1000 && peer_conn->GetSentCount() >= 5 && peer_conn->GetReceivedCount() == 0)
            {
                continue;
            }

            if (true == peer_conn->IsRunning())
            {
                boost::uint32_t elapsed = task->GetTimeElapsed();
                if (elapsed < task->timeout_ * percent / 10 && elapsed < time_elapsed)
                {
                    return false;
                }
            }
            else
            {
                boost::uint32_t elapsed = task->GetTimeElapsed();
                if (elapsed < task->timeout_ * percent / 10 * 8 / 10 && elapsed < time_elapsed)
                {
                    return false;
                }
            }
        }
        return true;
    }

}

#endif  // _P2SP_P2P_SUB_PIECE_REQUEST_MANAGER_H_
