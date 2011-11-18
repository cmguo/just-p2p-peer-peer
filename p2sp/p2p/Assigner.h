//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// Assigner.h

#ifndef _P2SP_P2P_ASSIGNER_H_
#define _P2SP_P2P_ASSIGNER_H_

#include "p2sp/p2p/ConnectionBase.h"

namespace p2sp
{
    class P2PDownloader;
    typedef boost::shared_ptr<P2PDownloader> P2PDownloader__p;

    class ConnectionBase;
    typedef boost::shared_ptr<ConnectionBase> ConnectionBase__p;

    class Assigner
        : public boost::noncopyable
        , public boost::enable_shared_from_this<Assigner>
#ifdef DUMP_OBJECT
        , public count_object_allocate<Assigner>
#endif
    {
    public:
        typedef boost::shared_ptr<Assigner> p;
        static p create(P2PDownloader__p p2p_downloader) { return p(new Assigner(p2p_downloader)); }
    public:
        // 启停
        void Start();
        void Stop();
        // 消息
        void OnP2PTimer(uint32_t times);

        void SetAssignPeerCountLimit(boost::uint32_t assign_peer_count_limit)
        {
            assign_peer_count_limit_ = assign_peer_count_limit;
        }

        bool IsEndOfAssign() const
        {
            return is_end_of_file_;
        }

    private:
        // 根据已有的subpiece_cout和需要分配的capacity
        // 计算出还需要请求多少片piece才能到capacity
        void CalcSubpieceTillCapatity();

        // 计算当前piece_task中，可以提供分配subpiece数
        uint32_t CaclSubPieceAssignCount();

        // 计算并分配需要去下载的subpiece, 将这些subpiece加入subpiece_assign_map_
        void CaclSubPieceAssignMap();

        // 预分配peer的队列生成，分配顺序保存在peer_connection_recvtime_list_中
        void CaclPeerConnectionRecvTimeMap();

        // 预分配，将subpiece_assign_map_中的指定任务按照peer_connection_recvtime_list_的顺序依次分配
        void AssignerPeers();

    private:
        struct PEER_RECVTIME
        {
            PEER_RECVTIME(uint32_t recv_time_, ConnectionBase__p peer_)
                : recv_time(recv_time_), peer(peer_) {}
            PEER_RECVTIME(const PEER_RECVTIME & p1)
                : recv_time(p1.recv_time), peer(p1.peer){}
            uint32_t recv_time;
            ConnectionBase__p peer;
        };
        friend inline bool operator < (PEER_RECVTIME & p1, PEER_RECVTIME & p2);
        // 模块
        P2PDownloader__p p2p_downloader_;
        // 变量
        std::deque<protocol::SubPieceInfo> subpiece_assign_map_;
        std::list<PEER_RECVTIME> peer_connection_recvtime_list_;
        //
        volatile bool is_running_;
        uint32_t block_size_;
        uint32_t file_length_;

        uint32_t assign_peer_count_limit_;
        boost::uint16_t subpiece_count_;
        // statistic
        uint32_t total_assign_count_;
        uint32_t redundant_assign_count_;

        //
        bool is_end_of_file_;

    private:
        // 构造
        Assigner(P2PDownloader__p p2p_downloader)
            : p2p_downloader_(p2p_downloader)
            , is_running_(false)
            , assign_peer_count_limit_(0)
        {}
    };

    inline bool operator < (Assigner::PEER_RECVTIME & p1, Assigner::PEER_RECVTIME & p2)
    {
        return p1.recv_time < p2.recv_time;
    }
}
#endif  // _P2SP_P2P_ASSIGNER_H_
