#ifndef _LIVE_SUBPIECE_REQUEST_MANAGER_H_
#define _LIVE_SUBPIECE_REQUEST_MANAGER_H_

#include "protocol/LivePeerPacket.h"
#include <framework/timer/Timer.h>
#include <framework/timer/TickCounter.h>

namespace p2sp
{
    class LiveP2PDownloader;
    typedef boost::shared_ptr<LiveP2PDownloader> LiveP2PDownloader__p;

    class LivePeerConnection;
    typedef boost::shared_ptr<LivePeerConnection> LivePeerConnection__p;

    class LiveSubPieceRequestTask
        : public boost::noncopyable
        , public boost::enable_shared_from_this<LiveSubPieceRequestTask>
#ifdef DUMP_OBJECT
        , public count_object_allocate<LiveSubPieceRequestTask>
#endif
    {
    public:
        typedef boost::shared_ptr<LiveSubPieceRequestTask> p;
        static p create(boost::uint32_t timeout, LivePeerConnection__p peer_connection, boost::uint32_t transaction_id)
        {
            return p(new LiveSubPieceRequestTask(timeout, peer_connection, transaction_id));
        }

        bool IsTimeout()
        {
            return request_time_counter_.elapsed() > timeout_;
        }

        boost::uint32_t GetTimeElapsed() const 
        {
            return request_time_counter_.elapsed();
        }

        boost::uint32_t GetTransactionId() const
        {
            return transaction_id_;
        }

    public:
        boost::uint32_t timeout_;
        LivePeerConnection__p peer_connection_;

    private:
        framework::timer::TickCounter request_time_counter_;
        boost::uint32_t transaction_id_;

    private:
        LiveSubPieceRequestTask(boost::uint32_t timeout, LivePeerConnection__p peer_connection, boost::uint32_t transaction_id)
            : timeout_(timeout)
            , peer_connection_(peer_connection)
            , transaction_id_(transaction_id)
        {
        }
    };

    class LiveSubPieceRequestManager
#ifdef DUMP_OBJECT
        : public count_object_allocate<LiveSubPieceRequestManager>
#endif
    {
    public:
        // 操作
        void Add(const protocol::LiveSubPieceInfo & subpiece_info, boost::uint32_t timeout, 
            LivePeerConnection__p peer_connection, boost::uint32_t transaction_id);

        // 消息
        void OnSubPiece(const protocol::LiveSubPiecePacket & packet);
        // 每秒执行一次
        void OnP2PTimer(boost::uint32_t times);

        bool IsRequesting(const protocol::LiveSubPieceInfo& subpiece_info) const;

        boost::uint32_t GetRequestingCount( const protocol::LiveSubPieceInfo & subpiece_info ) const;

    private:
        void CheckExternalTimeout();
        
    private:
        std::multimap<protocol::LiveSubPieceInfo, LiveSubPieceRequestTask::p> request_tasks_;
    };
}
#endif