#include "Common.h"
#include "LiveSubPieceCountManager.h"

namespace p2sp
{
    LiveSubPieceCountManager::LiveSubPieceCountManager(boost::uint32_t live_interval)
        : live_interval_(live_interval)
    {

    }

    void LiveSubPieceCountManager::SetSubPieceCountMap(boost::uint32_t block_id, 
        const std::vector<boost::uint16_t> & subpiece_count)
    {
        for (boost::uint32_t i=0; i<subpiece_count.size(); i++)
        {
            subpiece_count_map_.insert(std::make_pair(block_id+i*live_interval_, subpiece_count[i]));
        }
    }

    void LiveSubPieceCountManager::EliminateElapsedSubPieceCountMap(boost::uint32_t block_id)
    {
        for (map<uint32_t, uint16_t>::iterator iter = subpiece_count_map_.begin(); 
            iter != subpiece_count_map_.end(); )
        {
            if (iter->first < block_id)
            {
                subpiece_count_map_.erase(iter++);
            }
            else
            {
                break;
            }
        }
    }

    bool LiveSubPieceCountManager::HasSubPieceCount(boost::uint32_t block_id) const
    {
        return subpiece_count_map_.find(block_id) != subpiece_count_map_.end();
    }

    boost::uint16_t LiveSubPieceCountManager::GetSubPieceCount(boost::uint32_t block_id) const
    {
        map<uint32_t, uint16_t>::const_iterator iter = subpiece_count_map_.find(block_id);
        if (iter != subpiece_count_map_.end())
        {
            return iter->second;
        }

        return 0;
    }
}