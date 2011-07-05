//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _MEMORY_QUOTA_H
#define _MEMORY_QUOTA_H

namespace storage
{
    struct MemoryQuota
    {
        MemoryQuota(size_t aQuota)
            :quota(aQuota)
        {
        }

        size_t quota;
    };
}

#endif  //_MEMORY_QUOTA_H
