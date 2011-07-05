//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// PeerHelper.h

#ifndef _P2SP_P2P_PEER_HELPER_H_
#define _P2SP_P2P_PEER_HELPER_H_

namespace p2sp
{
    class PeerHelper
    {
    public:
        static bool IsPeerFromSameSubnet(const protocol::CandidatePeerInfo& peer_info);
    };
}

#endif  // _P2SP_P2P_UPLOAD_MANAGER_H_
