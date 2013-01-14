//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _MEMORY_QUOTA_ASSIGNER_H
#define _MEMORY_QUOTA_ASSIGNER_H

#include "MemoryQuota.h"
#include "MemoryUsageDescription.h"
#include "MemoryConsumer.h"

namespace storage
{
    typedef boost::shared_ptr<IMemoryConsumer> ConsumerPointer;

    class MemoryQuotaAssigner
    {
        struct MemoryConsumptionEntry
        {
            MemoryConsumptionEntry(ConsumerPointer consumer, MemoryUsageDescription usage)
                :consumer_(consumer), usage_(usage), quota_(0)
            {
            }

            ConsumerPointer consumer_;
            MemoryUsageDescription usage_;
            MemoryQuota quota_;
        };

        typedef std::map<MemoryUsageDescription::MemoryUsagePriority, std::list<MemoryConsumptionEntry> > PriorityToConsumptionEntryMapping;

        PriorityToConsumptionEntryMapping consumption_entries_;
        std::vector<ConsumerPointer>& memory_consumers_;
        boost::uint32_t quota_left_;

    public:
        MemoryQuotaAssigner(std::vector<ConsumerPointer>& consumers)
            :memory_consumers_(consumers), quota_left_(0)
        {
        }

        void AssignQuota(boost::uint32_t overall_bytes);

    private:
        //those whose usage priority == NONE are excluded
        void BuildConsumptionEntries();
        void AssignQuotaUpToDesirableBytes();
    };
}

#endif  //_MEMORY_QUOTA_ASSIGNER_H
