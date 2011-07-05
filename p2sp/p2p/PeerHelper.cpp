//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/AppModule.h"
#include "p2sp/p2p/PeerHelper.h"

namespace p2sp
{
    bool PeerHelper::IsPeerFromSameSubnet(const protocol::CandidatePeerInfo& peer_info)
    {
        return AppModule::Inst()->GetCandidatePeerInfo().FromSameSubnet(peer_info);
    }
}
