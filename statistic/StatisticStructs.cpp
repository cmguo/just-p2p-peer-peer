//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "StatisticStructs.h"
#include "p2sp/AppModule.h"

namespace statistic
{
    STASTISTIC_INFO::STASTISTIC_INFO()
    {
        Clear();
        LocalPeerInfo.PeerVersion = protocol::PEER_VERSION;
        PeerVersion = p2sp::AppModule::GetKernelVersionInfo();
    }
}
