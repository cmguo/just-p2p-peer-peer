//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "storage_base.h"
#include "LiveCacheManager.h"

using namespace base;

namespace storage
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_live_cache = log4cplus::Logger::getInstance("[live_cache_manager]");
#endif

    bool LiveCacheManager::AddSubPiece(const protocol::LiveSubPieceInfo & subpiece, const protocol::LiveSubPieceBuffer & subpiece_buffer)
    {
        EnsureBlockExists(subpiece.GetBlockId());
        return block_nodes_[subpiece.GetBlockId()]->AddSubPiece(subpiece.GetSubPieceIndex(), subpiece_buffer);
    }

    bool LiveCacheManager::HasSubPiece(const protocol::LiveSubPieceInfo & subpiece) const
    {
        map<boost::uint32_t, LiveBlockNode::p>::const_iterator iter = block_nodes_.find(subpiece.GetBlockId());
        if (iter != block_nodes_.end())
        {
            return iter->second->HasSubPiece(subpiece.GetSubPieceIndex());
        }
        
        return false;
    }

    void LiveCacheManager::GetBlock(boost::uint32_t block_id, std::vector<protocol::LiveSubPieceBuffer> & subpiece_buffers) const
    {
        subpiece_buffers.clear();
        map<boost::uint32_t, LiveBlockNode::p>::const_iterator iter = block_nodes_.find(block_id);
        if (iter != block_nodes_.end())
        {
            LiveBlockNode::p target_block_node = iter->second;
            assert(target_block_node);

            target_block_node->GetBlock(subpiece_buffers);
        }
    }

    void LiveCacheManager::GetSubPiece(protocol::LiveSubPieceInfo subpiece_info, protocol::LiveSubPieceBuffer & subpiece_buffer) const
    {
        map<boost::uint32_t, LiveBlockNode::p>::const_iterator iter = block_nodes_.find(subpiece_info.GetBlockId());
        if (iter != block_nodes_.end())
        {
            iter->second->GetBuffer(subpiece_info.GetSubPieceIndex(), subpiece_buffer);
        }
        else
        {
            subpiece_buffer = protocol::LiveSubPieceBuffer();
        }
    }

    void LiveCacheManager::GetSubPieces(boost::uint32_t block_id, boost::uint16_t start_subpiece_index, boost::uint16_t last_subpiece_index, 
                                        std::vector<protocol::LiveSubPieceBuffer> & subpiece_buffers) const
    {
        assert(last_subpiece_index >= start_subpiece_index);

        subpiece_buffers.clear();

        if (last_subpiece_index < start_subpiece_index)
        {
            return;
        }

        map<boost::uint32_t, LiveBlockNode::p>::const_iterator iter = block_nodes_.find(block_id);
        if (iter != block_nodes_.end())
        {
            for(boost::uint16_t subpiece_index = start_subpiece_index; subpiece_index <= last_subpiece_index; ++subpiece_index)
            {
                protocol::LiveSubPieceBuffer subpiece;    
                iter->second->GetBuffer(subpiece_index, subpiece);

                //subpiece不该无数据，如果发生那意味着上层调用者所给的range [start, last]有问题。
                assert(subpiece);
                subpiece_buffers.push_back(subpiece);
            }
        }
        else
        {
            assert(false);
        }
    }

    bool LiveCacheManager::HasCompleteBlock(boost::uint32_t block_id) const
    {
        map<boost::uint32_t, LiveBlockNode::p>::const_iterator iter = block_nodes_.find(block_id);
        if (iter != block_nodes_.end())
        {
            return iter->second->IsComplete();
        }
        
        return false;
    }

    bool LiveCacheManager::IsPieceComplete(boost::uint32_t block_id, boost::uint16_t piece_index) const
    {
        map<boost::uint32_t, LiveBlockNode::p>::const_iterator iter = block_nodes_.find(block_id);
        if (iter != block_nodes_.end())
        {
            return iter->second->IsPieceComplete(piece_index);
        }

        return false;
    }

    bool LiveCacheManager::IsPieceValid(boost::uint32_t block_id, boost::uint16_t piece_index) const
    {
        map<boost::uint32_t, LiveBlockNode::p>::const_iterator iter = block_nodes_.find(block_id);
        if (iter != block_nodes_.end())
        {
            return iter->second->IsPieceValid(piece_index);
        }

        return false;
    }

    bool LiveCacheManager::IsBlockHeaderValid(boost::uint32_t block_id) const
    {
        map<boost::uint32_t, LiveBlockNode::p>::const_iterator iter = block_nodes_.find(block_id);
        if (iter != block_nodes_.end())
        {
            return iter->second->IsHeaderValid();
        }

        return false;
    }

    boost::uint32_t LiveCacheManager::GetBlockSizeInBytes(boost::uint32_t block_id) const
    {
        map<boost::uint32_t, LiveBlockNode::p>::const_iterator iter = block_nodes_.find(block_id);
        if (iter == block_nodes_.end())
        {
            return 0;
        }
        
        return iter->second->GetBlockSizeInBytes();
    }

    boost::uint32_t LiveCacheManager::GetSubPiecesCount(boost::uint32_t block_id) const
    {
        map<boost::uint32_t, LiveBlockNode::p>::const_iterator iter = block_nodes_.find(block_id);
        if (iter != block_nodes_.end())
        {
            return iter->second->GetSubPiecesCount();
        }

        return 0;
    }

    //Note - 有可能出现开始返回的missing subpiece的位置在后一次调用返回的missing subpiece之后
    //这是因为在piece validation不通过时，会发生DropPiece
    void LiveCacheManager::GetNextMissingSubPiece(boost::uint32_t start_block_id, protocol::LiveSubPieceInfo & missing_subpiece) const
    {
        boost::uint32_t block_id = start_block_id;
        while(true)
        {
            map<boost::uint32_t, LiveBlockNode::p>::const_iterator iter = block_nodes_.find(block_id);
            if (iter == block_nodes_.end())
            {
                missing_subpiece = protocol::LiveSubPieceInfo(block_id, 0);
                break;
            }

            if (!iter->second->IsComplete())
            {
                missing_subpiece = protocol::LiveSubPieceInfo(block_id, iter->second->GetNextMissingSubPiece());
                break;
            }

            block_id += GetLiveInterval();
        }
    }

    void LiveCacheManager::EnsureBlockExists(boost::uint32_t block_id)
    {
        if (block_nodes_.find(block_id) == block_nodes_.end())
        {
            LOG4CPLUS_DEBUG_LOG(logger_live_cache, "block_nodes_.size(): " << block_nodes_.size());
            
            LiveBlockNode::p node(new LiveBlockNode(block_id, rid_));
            block_nodes_.insert(std::make_pair(block_id, node));

            LOG4CPLUS_DEBUG_LOG(logger_live_cache, "new LiveBlockNode:" << block_id);
        }
    }

    boost::uint32_t LiveCacheManager::GetDataRate() const
    {
        if (block_nodes_.size() <= 0)
        {
            return 0;
        }

        boost::uint32_t data_rate = 0;

        for (map<boost::uint32_t, LiveBlockNode::p>::const_iterator iter = block_nodes_.begin();
            iter != block_nodes_.end(); 
            ++iter)
        {
            data_rate += iter->second->GetBlockSizeInBytes();
        }

        LOG4CPLUS_DEBUG_LOG(logger_live_cache, "cache memory = " << data_rate / 1024 << " KB\n");

        return data_rate /(block_nodes_.size() * GetLiveInterval());
    }

    boost::uint16_t LiveCacheManager::GetCacheSize() const
    {
        return block_nodes_.size();
    }

    boost::uint32_t LiveCacheManager::GetCacheFirstBlockId() const
    {
        if (block_nodes_.size() > 0)
        {
            return block_nodes_.begin()->first;
        }

        return 0;
    }

    boost::uint32_t LiveCacheManager::GetCacheLastBlockId() const
    {
        if (block_nodes_.size() > 0)
        {
            return block_nodes_.rbegin()->first;
        }

        return 0;
    }

    bool LiveCacheManager::RemoveBlock(boost::uint32_t block_id)
    {
        size_t elements_removed = block_nodes_.erase(block_id);
        return elements_removed > 0;
    }

    boost::uint32_t LiveCacheManager::GetChecksumFailedTimes() const
    {
        boost::uint32_t checksum_failed_times = 0;
        for (map<boost::uint32_t, LiveBlockNode::p>::const_iterator iter = block_nodes_.begin();
            iter != block_nodes_.end(); ++iter)
        {
            if (iter->second->GetChecksumFailedTimes() != 0)
            {
                block_checksum_failed_times_[iter->first] = iter->second->GetChecksumFailedTimes();
            }
        }
        for (map<boost::uint32_t, boost::uint32_t>::iterator iter = block_checksum_failed_times_.begin();
            iter != block_checksum_failed_times_.end(); ++iter)
        {
            checksum_failed_times += iter->second;
        }
        return checksum_failed_times;
    }

    boost::uint32_t LiveCacheManager::GetMissingSubPieceCount(boost::uint32_t block_id) const
    {
        std::map<boost::uint32_t, LiveBlockNode::p>::const_iterator iter = block_nodes_.find(block_id);

        if (iter == block_nodes_.end())
        {
            return 0;
        }

        return iter->second->GetMissingSubPieceCount();
    }

    boost::uint32_t LiveCacheManager::GetExistSubPieceCount(boost::uint32_t block_id) const
    {
        std::map<boost::uint32_t, LiveBlockNode::p>::const_iterator iter = block_nodes_.find(block_id);

        if (iter == block_nodes_.end())
        {
            return 0;
        }

        return iter->second->GetExistSubPieceCount();
    }
}
