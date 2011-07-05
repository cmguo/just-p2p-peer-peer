#ifndef _CONNECTING_PEER_BASE_H_
#define _CONNECTING_PEER_BASE_H_

namespace p2sp
{
    const uint32_t p2p_timeout = 4*1000;

    class ConnectingPeer
        : public boost::noncopyable
        , public boost::enable_shared_from_this<ConnectingPeer>
    #ifdef DUMP_OBJECT
        , public count_object_allocate<ConnectingPeer>
    #endif
    {
    public:

        typedef boost::shared_ptr<ConnectingPeer> p;
        static p create(const protocol::CandidatePeerInfo& candidate_peer_info)
        {
            return p(new ConnectingPeer(candidate_peer_info));
        }

        inline bool IsTimeOut() { /*return false; */return last_connecting_time_.elapsed() > p2p_timeout; }

        protocol::CandidatePeerInfo candidate_peer_info_;
    private:
        // 上次发起连接的时间
        framework::timer::TickCounter last_connecting_time_;
        ConnectingPeer(const protocol::CandidatePeerInfo& candidate_peer_info)
            : candidate_peer_info_(candidate_peer_info)
        {
            last_connecting_time_.reset();
        }
    };
}

#endif