//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "LiveInstanceMemoryConsumer.h"
#include "p2sp/download/LiveDownloadDriver.h"

namespace storage
{
    MemoryUsageDescription LiveInstanceMemoryConsumer::GetMemoryUsage() const
    {
        std::map<uint32_t, size_t> blocks_importance_score;
        BuildBlocksImportanceScore(blocks_importance_score);

        size_t min_blocks = 0;
        size_t desirable_blocks = 0;

        for(std::map<uint32_t, size_t>::const_iterator iter = blocks_importance_score.begin();
            iter != blocks_importance_score.end();
            ++iter)
        {
            if (iter->second > 0)
            {
                ++desirable_blocks;

                if (iter->second >= MostWantedBlocksScoreBar )
                {
                    ++min_blocks;
                }
            }
        }

        MemoryUsageDescription::MemoryUsagePriority priority = MemoryUsageDescription::LOW;
        if (desirable_blocks > 0)
        {
            priority = min_blocks > 0 ? MemoryUsageDescription::HIGH : MemoryUsageDescription::NORMAL;
        }

        size_t average_block_size = GetAverageBlockSize();

        return MemoryUsageDescription(
                    min_blocks*average_block_size, 
                    desirable_blocks*average_block_size, 
                    priority);
    }

    void LiveInstanceMemoryConsumer::SetMemoryQuota(const MemoryQuota & quota)
    {
        size_t blocks_quota = GetBlocksQuota(quota);

        size_t current_blocks_count = live_instance_->GetCacheSize();

        if (current_blocks_count <= blocks_quota)
        {
            return;
        }

        std::map<uint32_t, size_t> blocks_importance_score;
        BuildBlocksImportanceScore(blocks_importance_score);

        for(uint32_t block_id = live_instance_->GetCacheFirstBlockId();
            block_id <= live_instance_->GetCacheLastBlockId();
            block_id += live_instance_->GetLiveInterval())
        {
            std::map<uint32_t, size_t>::iterator iter = blocks_importance_score.find(block_id);
            if (iter != blocks_importance_score.end())
            {
                //score
                if (iter->second == 0)
                {
                    blocks_importance_score.erase(iter);
                    live_instance_->cache_manager_.RemoveBlock(block_id);
                }
            }
            else
            {
                live_instance_->cache_manager_.RemoveBlock(block_id);
            }
        }
        
        if (live_instance_->cache_manager_.GetCacheSize() > blocks_quota)
        {
            size_t blocks_to_remove = live_instance_->cache_manager_.GetCacheSize() - blocks_quota;
            std::multimap<size_t, uint32_t> score_to_block_id;
            for(std::map<uint32_t, size_t>::const_iterator iter = blocks_importance_score.begin();
                iter != blocks_importance_score.end();
                ++iter)
            {
                score_to_block_id.insert(std::make_pair(iter->second, iter->first));
            }

            for(std::multimap<size_t, uint32_t>::const_iterator iter = score_to_block_id.begin();
                iter != score_to_block_id.end() && blocks_to_remove > 0;
                ++iter)
            {
                live_instance_->cache_manager_.RemoveBlock(iter->second);
                --blocks_to_remove;
            }
        }
    }

    // 给每个block的重要性进行打分，打分时综合考虑了该block对上传的重要性以及对播放重要性。
    // 采用离散的分值而不是[0-1]的浮点数，是为了减少浮点数计算
    // 对于上传重要性，分值为[0-5]，而对于播放重要性，其分值范围为[0-10]
    // 也就是说一个block的理论最大分值为n*10+5,其中n是播放点的个数
    // 目前认为分值>=MostWantedBlocksScoreBar(3)的block是不能淘汰的，
    // [1-2]的是尽可能保留，但不排除会被淘汰的，而分值为0的是随时可以淘汰的
    // 今后还有必要根据实际测试情况来对这些范围/阈值进行调整
    void LiveInstanceMemoryConsumer::BuildBlocksImportanceScore(std::map<uint32_t, size_t>& blocks_importance_score) const
    {
        blocks_importance_score.clear();

        if (live_instance_->GetCacheSize() == 0)
        {
            return;
        }

        LivePosition last_block = LivePosition(live_instance_->GetCacheLastBlockId());

        vector<uint32_t> sorted_play_points;
        GetSortedPlayPoints(sorted_play_points);

        const int upload_score_base = 5;
        const int play_point_relevance_score_base = 10;

        for(uint32_t block_id = live_instance_->GetCacheFirstBlockId();
            block_id <= live_instance_->GetCacheLastBlockId();
            block_id += live_instance_->GetLiveInterval())
        {
            assert(block_id <= last_block.GetBlockId());

            if (false == live_instance_->BlockExists(block_id))
            {
                continue;
            }

            //1. 根据该block到最后一个block的距离计算其作为待上传块的重要性得分[0-5]
            size_t distance_to_last_block = last_block.GetBlockId() - block_id;
            size_t upload_candidate_score = GetDistanceScore(distance_to_last_block, LiveInstance::ExpectedP2PCoverageInSeconds, upload_score_base);

            //2. 计算该block对每一个播放点的重要性得分之和。对于一个播放点Pi，
            // 我们认为只有落在[Pi, Pi+PlayPointRelevantRangeInSeconds]内的block才是对Pi重要的。
            // 换言之，一个block只会对落在[block-PlayPointRelevantRangeInSeconds, block]内的播放点才是重要的，
            // 下面就是找出这些播放点并计算block对这些播放点的重要性得分。
            size_t play_point_relevance_score = 0;
            int most_relevant_play_position_index = FindMostRelevantPlayPointIndex(sorted_play_points, block_id);
            //most_relevant_play_position_index == -1 意味着该block对所有播放点都不重要
            if (most_relevant_play_position_index >= 0)
            {
                //>=0时，表示该block对第most_relevant_play_position_index个播放点最重要，
                //同时对其左边的播放点可能是重要的，而对其右边的播放点一定是不重要的。
                assert(sorted_play_points[most_relevant_play_position_index] <= block_id);

                for (int i = most_relevant_play_position_index; i >= 0; --i)
                {
                    size_t distance_to_current_play_point = block_id - sorted_play_points[i];
                    size_t relevance_score_to_current_play_point = GetDistanceScore(
                        distance_to_current_play_point, 
                        PlayPointRelevantRangeInSeconds, 
                        play_point_relevance_score_base);

                    if (relevance_score_to_current_play_point == 0)
                    {
                        //再往左边的播放点就更不重要了，故跳过
                        break;
                    }

                    play_point_relevance_score += relevance_score_to_current_play_point;
                }
            }

            blocks_importance_score[block_id] = upload_candidate_score + play_point_relevance_score;
        }
    }

    size_t LiveInstanceMemoryConsumer::GetBlocksQuota(const MemoryQuota& quota) const
    {
        if (quota.quota == 0)
        {
            return 0;
        }

        size_t average_block_size = GetAverageBlockSize();
        if (average_block_size == 0)
        {
            return 0;
        }
        
        return (quota.quota + average_block_size - 1)/average_block_size;
    }

    size_t LiveInstanceMemoryConsumer::GetAverageBlockSize() const
    {
        if (live_instance_->GetCacheSize() == 0)
        {
            return 0;
        }

        size_t blocks = 0;
        size_t sum_of_blocks_size = 0;

        for(uint32_t block_id = live_instance_->GetCacheFirstBlockId();
            block_id <= live_instance_->GetCacheLastBlockId();
            block_id += live_instance_->GetLiveInterval())
        {
            size_t block_size = live_instance_->GetBlockSizeInBytes(block_id);
            if (block_size > 0)
            {
                sum_of_blocks_size += block_size;
                ++blocks;
            }
        }

        return blocks == 0 ? 0 : (sum_of_blocks_size/blocks);
    }

    void LiveInstanceMemoryConsumer::GetSortedPlayPoints(std::vector<uint32_t>& sorted_play_points) const
    {
        std::vector<LivePosition> play_points;
        live_instance_->GetAllPlayPoints(play_points);

        sorted_play_points.clear();
        for(size_t i = 0; i < play_points.size(); ++i)
        {
            sorted_play_points.push_back(play_points[i].GetBlockId());
        }

        std::sort(sorted_play_points.begin(), sorted_play_points.end());
    }
}