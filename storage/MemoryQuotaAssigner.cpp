//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "MemoryQuotaAssigner.h"

namespace storage
{
    void MemoryQuotaAssigner::AssignQuota(uint32_t overall_bytes)
    {
        quota_left_ = overall_bytes;

        //mapping<priority, list<ConsumerEntry> >
        BuildConsumptionEntries();

        //if any remaining quota left, assign them to each consumer, in the order of priority (high -> left).
        //in case we cannot make everyone of the same priority happy, the principle is to be fair.
        AssignQuotaUpToDesirableBytes();

        //to actually tell each consumer their assigned quota
        for (PriorityToConsumptionEntryMapping::iterator map_iter = consumption_entries_.begin();
            map_iter != consumption_entries_.end();
            ++map_iter)
        {
            for(std::list<MemoryConsumptionEntry>::iterator entry_iter = map_iter->second.begin();
                entry_iter != map_iter->second.end();
                ++entry_iter)
            {
                entry_iter->consumer_->SetMemoryQuota(entry_iter->quota_);
            }
        }
    }

    //those whose usage priority == NONE are excluded
    void MemoryQuotaAssigner::BuildConsumptionEntries()
    {
        consumption_entries_.clear();

        for (std::vector<ConsumerPointer>::iterator iter = memory_consumers_.begin();
            iter != memory_consumers_.end();
            ++iter)
        {
            MemoryConsumptionEntry entry(*iter, (*iter)->GetMemoryUsage());

            if (entry.usage_.GetPriority() == MemoryUsageDescription::NONE)
            {
                assert(entry.usage_.GetMinimumSize() == 0);
                assert(entry.usage_.GetDesirableSize() == 0);

                (*iter)->SetMemoryQuota(MemoryQuota(0));
                continue;
            }

            //make sure at least everyone gets the minimum size
            entry.quota_.quota = entry.usage_.GetMinimumSize();

            quota_left_ = (quota_left_ > entry.usage_.GetMinimumSize()) ? (quota_left_ - entry.usage_.GetMinimumSize()) : 0;

            consumption_entries_[entry.usage_.GetPriority()].push_back(entry);
        }
    }

    void MemoryQuotaAssigner::AssignQuotaUpToDesirableBytes()
    {
        if (quota_left_ == 0)
        {
            return;
        }

        //iterate from highest priority to lowest priority
        for (PriorityToConsumptionEntryMapping::reverse_iterator map_iter = consumption_entries_.rbegin();
            map_iter != consumption_entries_.rend();
            ++map_iter)
        {
            //1. for a given priority, get the sum of additional bytes needed
            uint32_t additional_bytes_needed(0);
            for(std::list<MemoryConsumptionEntry>::iterator entry_iter = map_iter->second.begin();
                entry_iter != map_iter->second.end();
                ++entry_iter)
            {
                assert(entry_iter->usage_.GetMinimumSize() <= entry_iter->usage_.GetDesirableSize());
                additional_bytes_needed += (entry_iter->usage_.GetDesirableSize() - entry_iter->usage_.GetMinimumSize());
            }

            //2. figure out if we can meet the requirement of everyone (of current priority) 
            assert(quota_left_ > 0);
            double ratio = 1.00000001f;
            if (additional_bytes_needed > quota_left_)
            {
                ratio = static_cast<double>(quota_left_)/additional_bytes_needed;
                quota_left_ = 0;
            }
            else
            {
                quota_left_ -= additional_bytes_needed;
            }

            //3. assign additional quota 
            for(std::list<MemoryConsumptionEntry>::iterator entry_iter = map_iter->second.begin();
                entry_iter != map_iter->second.end();
                ++entry_iter)
            {
                assert(entry_iter->usage_.GetMinimumSize() <= entry_iter->usage_.GetDesirableSize());
                entry_iter->quota_.quota += static_cast<uint32_t>(ratio*(entry_iter->usage_.GetDesirableSize() - entry_iter->usage_.GetMinimumSize()));
            }

            if (quota_left_ == 0)
            {
                break;
            }
        }
    }
}
