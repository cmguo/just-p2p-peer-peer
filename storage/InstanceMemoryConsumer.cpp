//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "InstanceMemoryConsumer.h"
#include "LiveInstanceMemoryConsumer.h"
#include "Instance.h"

namespace storage
{
    MemoryUsageDescription InstanceMemoryConsumer::GetMemoryUsage() const
    {
        const uint32_t MostWantedBlocksScoreBar = 3;

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
                if (iter->second >= MostWantedBlocksScoreBar)
                {
                    ++min_blocks;
                }
            }
        }

        MemoryUsageDescription::MemoryUsagePriority priority = MemoryUsageDescription::LOW;
        if (desirable_blocks > 0)
        {
            priority = MemoryUsageDescription::NORMAL;
        }

        return MemoryUsageDescription(
            min_blocks*instance_->GetBlockSize(), 
            desirable_blocks*instance_->GetBlockSize(), 
            priority);
    }

    void InstanceMemoryConsumer::SetMemoryQuota(const MemoryQuota & quota)
    {
        std::map<uint32_t, size_t> blocks_importance_score;
        BuildBlocksImportanceScore(blocks_importance_score);

        //先删掉得分为0或压根没记录在block_importance_score中的block。
        std::multimap<size_t, uint32_t> score_to_block_id;
        for (uint32_t block_id = 0; block_id < instance_->GetBlockCount(); ++block_id)
        {
            std::map<uint32_t, size_t>::const_iterator iter = blocks_importance_score.find(block_id);
            if (iter != blocks_importance_score.end() && iter->second > 0)
            {
                score_to_block_id.insert(std::make_pair(iter->second, iter->first));
            }
            else
            {
                //从crash mini-dump记录看来，存在subpiece_manager_非空&&resource_p_为空&&resource_p->block_nodes已满的情况。
                //这里加上asertion帮助我们找到这种状况
                assert(instance_->resource_p_);
                if (instance_->subpiece_manager_ && instance_->resource_p_)
                {
                    if (instance_->subpiece_manager_->IsBlockDataInMemCache(block_id))
                    {
                        instance_->subpiece_manager_->RemoveBlockDataFromMemCache(instance_->resource_p_, block_id);
                    }
                }
            }
        }
        
        size_t blocks_quota = GetBlocksQuota(quota);
        if (score_to_block_id.size() < blocks_quota)
        {
            //根据block的得分从小到大把block的中占用memory的数据部分给拿掉
            size_t blocks_to_remove = blocks_quota - score_to_block_id.size();
            for(std::multimap<size_t, uint32_t>::const_iterator iter = score_to_block_id.begin();
                iter != score_to_block_id.end() && blocks_to_remove > 0;
                ++iter)
            {
                assert(instance_->resource_p_);
                if (instance_->subpiece_manager_ && instance_->resource_p_)
                {
                    assert(instance_->subpiece_manager_->IsBlockDataInMemCache(iter->second));
                    instance_->subpiece_manager_->RemoveBlockDataFromMemCache(instance_->resource_p_, iter->second);
                    --blocks_to_remove;
                }
            }
        }
    }

    size_t InstanceMemoryConsumer::GetBlocksQuota(const MemoryQuota& quota) const
    {
        if (quota.quota == 0)
        {
            return 0;
        }

        size_t block_size = instance_->GetBlockSize();
        if (block_size == 0)
        {
            return 0;
        }

        return (quota.quota + block_size - 1)/block_size;
    }

    void InstanceMemoryConsumer::GetSortedPlayPoints(std::vector<uint32_t>& sorted_play_points) const
    {
        sorted_play_points.clear();

        for(std::set<IDownloadDriver::p>::const_iterator iter = instance_->download_driver_s_.begin();
            iter != instance_->download_driver_s_.end();
            ++iter)
        {
            uint32_t position = (*iter)->GetPlayingPosition();
            protocol::PieceInfo piece_info;
            if (instance_->subpiece_manager_->PosToPieceInfo(position, piece_info))
            {
                sorted_play_points.push_back(piece_info.block_index_);
            }
        }

        std::sort(sorted_play_points.begin(), sorted_play_points.end());
    }

    size_t InstanceMemoryConsumer::GetPlayPointsRelevanceScore(uint32_t block_id, const std::vector<uint32_t>& sorted_play_points) const
    {
        const int play_point_relevance_score_base = 5;
        const int PlayPointRelevantBlocks = 10;

        size_t play_point_relevance_score = 0;
        int most_relevant_play_position_index = LiveInstanceMemoryConsumer::FindMostRelevantPlayPointIndex(sorted_play_points, block_id);
        if (most_relevant_play_position_index >= 0)
        {
            //>=0时，表示该block对第most_relevant_play_position_index个播放点最重要，
            //同时对其左边的播放点可能是重要的，而对其右边的播放点一定是不重要的。
            assert(sorted_play_points[most_relevant_play_position_index] <= block_id);

            for (int i = most_relevant_play_position_index; i >= 0; --i)
            {
                size_t distance_to_current_play_point = block_id - sorted_play_points[i];

                //确保播放点以后(右边)的block的得分>=1，这样它们就轻易不会被删掉。
                size_t relevance_score_to_current_play_point = GetDistanceScore(
                    distance_to_current_play_point, 
                    PlayPointRelevantBlocks, 
                    play_point_relevance_score_base);

                play_point_relevance_score += relevance_score_to_current_play_point;
            }
        }

        return play_point_relevance_score;
    }

    size_t InstanceMemoryConsumer::GetPotentialDiskWriteScore(uint32_t block_id) const
    {
        if (instance_->subpiece_manager_->DownloadingBlock(block_id))
        {
            return 1;
        }

        return 0;
    }

    void InstanceMemoryConsumer::BuildBlocksImportanceScore(std::map<uint32_t, size_t>& blocks_importance_score) const
    {
        blocks_importance_score.clear();

        vector<uint32_t> sorted_play_points;
        GetSortedPlayPoints(sorted_play_points);

        for(uint32_t block_id = 0; block_id < instance_->GetBlockCount(); ++block_id)
        {
            if (instance_->subpiece_manager_->IsBlockDataInMemCache(block_id))
            {
                size_t play_point_relevance_score = GetPlayPointsRelevanceScore(block_id, sorted_play_points);
                size_t potential_disk_write_score = GetPotentialDiskWriteScore(block_id);
                
                blocks_importance_score[block_id] = play_point_relevance_score + potential_disk_write_score;
            }
        }
    }
}

