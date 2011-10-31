//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "IpPoolIndex.h"

namespace p2sp
{
    ConnectIndicator::ConnectIndicator(const CandidatePeer & candidate_peer)
    {
        key_ = candidate_peer.GetKey();
        next_time_to_connect_ = candidate_peer.last_connect_time_ + candidate_peer.connect_protect_time_;
        is_connecting_ = candidate_peer.is_connecting_;
        is_connction_ = candidate_peer.is_connction_;
        tracker_priority_ = 255 - candidate_peer.TrackerPriority;
    }
}