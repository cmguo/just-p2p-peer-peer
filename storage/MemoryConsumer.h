//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _MEMORY_CONSUMER_H
#define _MEMORY_CONSUMER_H

#include "MemoryQuota.h"
#include "MemoryUsageDescription.h"

namespace storage
{
    struct IMemoryConsumer
    {
        virtual MemoryUsageDescription GetMemoryUsage() const = 0;
        virtual void SetMemoryQuota(const MemoryQuota& quota) = 0;
        virtual ~IMemoryConsumer() {}
    };
}

#endif  //_MEMORY_CONSUMER_H
