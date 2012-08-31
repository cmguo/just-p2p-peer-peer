//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "IpPoolIndex.h"
#include <framework/string/Md5.h>

namespace p2sp
{
    //这个变换是为了引入一定的随机因素，以避免IP Pool中连接时单纯地根据peer ip来排序，导致流量不均匀
    //这一问题在udpserver ip pool中尤为明显
    class SockAddressTransformer
    {
    public:
        SockAddressTransformer()
        {
            seed_ = rand();
        }

        const std::string Transform(const protocol::SocketAddr& socket_address)
        {
            std::ostringstream stream;
            stream<<socket_address.IP + seed_<<":"<<socket_address.Port;
            const std::string str = stream.str();
            
            framework::string::Md5 md5;
            md5.update(reinterpret_cast<const boost::uint8_t*>(str.c_str()), str.length());
            md5.final();

            return md5.to_string();
        }

    private:
        size_t seed_;
    };

    ConnectIndicator::ConnectIndicator(const CandidatePeer & candidate_peer)
    {
        //这里需要是static，以至于在一次PPAP声明周期内对同一peer的变换是具有可重复性的
        static SockAddressTransformer transformer;

        key_ = transformer.Transform(candidate_peer.GetKey());

        next_time_to_connect_ = candidate_peer.last_connect_time_ + candidate_peer.connect_protect_time_;
        is_connecting_ = candidate_peer.is_connecting_;
        is_connected_ = candidate_peer.is_connected_;
        last_active_time_ = candidate_peer.last_active_time_;
        tracker_priority_ = 255 - candidate_peer.TrackerPriority;
        should_use_firstly_ = candidate_peer.should_use_firstly_;
        peer_score_ = candidate_peer.peer_score_;
        is_udpserver_from_cdn_ = candidate_peer.is_udpserver_from_cdn_;
    }
}