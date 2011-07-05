#ifndef _LIVE_SUBPIECE_REQUEST_MANAGER_H_
#define _LIVE_SUBPIECE_REQUEST_MANAGER_H_

#include "protocol/LivePeerPacket.h"

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
        static p create(uint32_t timeout, LivePeerConnection__p peer_connection)
        {
            return p(new LiveSubPieceRequestTask(timeout, peer_connection));
        }

        bool IsTimeout()
        {
            return request_time_elapse_ > timeout_;
        }

        framework::timer::TickCounter::count_value_type GetTimeElapsed() const 
        {
            return request_time_elapse_;
        }

    public:
        uint32_t request_time_elapse_;
        uint32_t timeout_;
        LivePeerConnection__p peer_connection_;

    private:
    private:
        LiveSubPieceRequestTask(uint32_t timeout, LivePeerConnection__p peer_connection)
            : request_time_elapse_(0)
            , timeout_(timeout)
            , peer_connection_(peer_connection)
        {
        }
    };

    class LiveSubPieceRequestManager
#ifdef DUMP_OBJECT
        : public count_object_allocate<LiveSubPieceRequestManager>
#endif
    {
    public:
        LiveSubPieceRequestManager()
            : total_unused_subpiece_count_(0)
            , total_received_subpiece_count_(0)
            , total_p2p_data_bytes_(0)
        {

        }
        // 操作
        void Start(LiveP2PDownloader__p p2p_downloader);
        void Add(const protocol::LiveSubPieceInfo & subpiece_info, boost::uint32_t timeout, LivePeerConnection__p peer_connection);
        // 消息
        void OnSubPiece(const protocol::LiveSubPiecePacket & packet);
        // 每秒执行一次
        void OnP2PTimer(uint32_t times);

        bool IsRequesting(const protocol::LiveSubPieceInfo& subpiece_info) const;

        // for statistic
        boost::uint32_t GetTotalUnusedSubPieceCount() const;
        boost::uint32_t GetTotalRecievedSubPieceCount() const;
        boost::uint32_t GetTotalP2PDataBytes() const;

    private:
        void CheckExternalTimeout();
    private:
        // 模块
        LiveP2PDownloader__p p2p_downloader_;
        // 变量
        std::multimap<protocol::LiveSubPieceInfo, LiveSubPieceRequestTask::p> request_tasks_;
        
        // for statistic
        boost::uint32_t total_unused_subpiece_count_;  // 收到的所有的subpiece个数（包括冗余的）
        boost::uint32_t total_received_subpiece_count_;  // 收到的subpiece个数（不包括冗余的）
        boost::uint32_t total_p2p_data_bytes_;  // P2P总下载
    };
}
#endif