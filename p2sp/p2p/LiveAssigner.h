#ifndef _LIVE_ASSIGNER_H_
#define _LIVE_ASSIGNER_H_

namespace p2sp
{
    class LiveDownloadDriver;
    typedef boost::shared_ptr<LiveDownloadDriver> LiveDownloadDriver__p;

    class LiveP2PDownloader;
    typedef boost::shared_ptr<LiveP2PDownloader> LiveP2PDownloader__p;

    class LivePeerConnection;
    typedef boost::shared_ptr<LivePeerConnection> LivePeerConnection__p;

    class LiveAssigner
#ifdef DUMP_OBJECT
        : public count_object_allocate<LiveAssigner>
#endif
    {
    public:
        void Start(LiveP2PDownloader__p p2p_downloader);
        void OnP2PTimer(boost::uint32_t times, bool urgent);

    private:
        void CalcSubpieceTillCapacity();
        void CaclSubPieceAssignMap(bool urgent);
        void CaclPeerConnectionRecvTimeMap();
        void AssignerPeers();

    private:
        LiveP2PDownloader__p p2p_downloader_;

        std::deque<protocol::LiveSubPieceInfo> subpiece_assign_deque_;

        struct PEER_RECVTIME
        {
            PEER_RECVTIME(uint32_t recv_time_, LivePeerConnection__p peer_)
                : recv_time(recv_time_), peer(peer_) {}           
            PEER_RECVTIME(const PEER_RECVTIME & p1)
                : recv_time(p1.recv_time), peer(p1.peer){}
            uint32_t recv_time;
            LivePeerConnection__p peer;
        };
        friend inline bool operator < (PEER_RECVTIME & p1, PEER_RECVTIME & p2);

        std::list<PEER_RECVTIME> peer_connection_recvtime_list_;
    };

    inline bool operator < (LiveAssigner::PEER_RECVTIME & p1, LiveAssigner::PEER_RECVTIME & p2)
    {
        return p1.recv_time < p2.recv_time;
    }
}

#endif