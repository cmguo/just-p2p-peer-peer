//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _LIVE_INSTANCE_MEMORY_CONSUMER_H
#define _LIVE_INSTANCE_MEMORY_CONSUMER_H

#include "MemoryConsumer.h"
#include "LiveInstance.h"

namespace storage
{
    class LiveInstanceMemoryConsumer:public IMemoryConsumer
    {
        static const size_t PlayPointRelevantRangeInSeconds = 120;

        //所有importance_score >= 这个数值的blocks都算入min_blocks,也就是说它被认为不该在此时被淘汰
        static const size_t MostWantedBlocksScoreBar = 3;

    public:
        LiveInstanceMemoryConsumer(LiveInstance::p live_instance)
            :live_instance_(live_instance)
        {
        }

        MemoryUsageDescription GetMemoryUsage() const;
        void SetMemoryQuota(const MemoryQuota & quota);

        //前提是所传入的sorted_play_points已经由小到大进行排序
        static int FindMostRelevantPlayPointIndex(const std::vector<boost::uint32_t>& sorted_play_points, boost::uint32_t block_id)
        {
            for(int i = 0; i < static_cast<int>(sorted_play_points.size()); ++i)
            {
                if (sorted_play_points[i] > block_id)
                {
                    return i - 1;
                }
            }

            return sorted_play_points.size() - 1;
        }
		
    private:
        void BuildBlocksImportanceScore(std::map<boost::uint32_t, size_t>& blocks_importance_score) const;
        size_t GetBlocksQuota(const MemoryQuota& quota) const;
        size_t GetAverageBlockSize() const;
        void GetSortedPlayPoints(std::vector<boost::uint32_t>& sorted_play_points) const;

        //目前是线性地计算距离度，如有必要可以考虑更复杂的算法
        static size_t GetDistanceScore(size_t distance, size_t effective_range, size_t score_base)
        {
            assert(effective_range > 0);
            assert(score_base > 0);

            if (distance >= effective_range)
            {
                return 0;
            }

            return std::max<size_t>(1, (score_base*(effective_range - distance))/effective_range);
        }

    private:
        LiveInstance::p live_instance_;
    };
}

#endif  //_LIVE_INSTANCE_MEMORY_CONSUMER_H
