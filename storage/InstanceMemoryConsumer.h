//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _INSTANCE_MEMORY_CONSUMER_H
#define _INSTANCE_MEMORY_CONSUMER_H

#include "MemoryConsumer.h"

namespace storage
{
    class Instance;

    class InstanceMemoryConsumer:public IMemoryConsumer
    {
    public:
        InstanceMemoryConsumer(boost::shared_ptr<Instance> instance)
            :instance_(instance)
        {
        }

        MemoryUsageDescription GetMemoryUsage() const;
        void SetMemoryQuota(const MemoryQuota & quota);

    private:
        void BuildBlocksImportanceScore(std::map<uint32_t, size_t>& blocks_importance_score) const;
        void GetSortedPlayPoints(std::vector<uint32_t>& sorted_play_points) const;
        size_t GetBlocksQuota(const MemoryQuota& quota) const;
        size_t GetPlayPointsRelevanceScore(uint32_t block_id, const std::vector<uint32_t>& sorted_play_points) const;
        size_t GetPotentialDiskWriteScore(uint32_t block_id) const;

        static size_t GetDistanceScore(size_t distance, size_t effective_range, size_t score_base)
        {
            assert(effective_range > 0);
            assert(score_base > 0);

            if (distance >= effective_range)
            {
                return 1;
            }

            return std::max<size_t>(1, (score_base*(effective_range - distance))/effective_range);
        }

    private:
        boost::shared_ptr<Instance> instance_;
    };
}

#endif  //_INSTANCE_MEMORY_CONSUMER_H
