//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _MEMORY_CONSUMPTION_MONITOR_H
#define _MEMORY_CONSUMPTION_MONITOR_H

#include "MemoryQuotaAssigner.h"

namespace storage
{
    class MemoryConsumptionMonitor
    {
        typedef boost::shared_ptr<IMemoryConsumer> ConsumerPointer;
    public:
        MemoryConsumptionMonitor(boost::uint32_t overall_quota_in_bytes)
            : overall_quota_in_bytes_(overall_quota_in_bytes)
        {
        }

        void Add(boost::shared_ptr<IMemoryConsumer> consumer)
        {
            memory_consumers_.push_back(consumer);
        }

        void AssignQuota()
        {
            MemoryQuotaAssigner assigner(memory_consumers_);
            assigner.AssignQuota(overall_quota_in_bytes_);
        }

    private:
        std::vector<ConsumerPointer> memory_consumers_;
        boost::uint32_t overall_quota_in_bytes_;
    };
}

#endif  //_MEMORY_CONSUMPTION_MONITOR_H
