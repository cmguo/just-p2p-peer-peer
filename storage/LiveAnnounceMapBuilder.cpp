//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "storage_base.h"
#include "IStorage.h"
#include "LiveInstance.h"
#include "LivePosition.h"
#include "LiveAnnounceMapBuilder.h"
#include <p2sp/download/LiveDownloadDriver.h>

using namespace base;
using namespace p2sp;

namespace storage
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_live_announce_map_builder = log4cplus::Logger::
        getInstance("[live_announce_map_builder]");
#endif

    void LiveAnnouceMapBuilder::Build(boost::uint32_t request_block_id)
    {
        ResetState();

        if (cache_manager_.IsEmpty())
        {
            return;
        }

        announce_map_.request_block_id_ = request_block_id;

        boost::uint32_t first_block_id = cache_manager_.GetCacheFirstBlockId();
        boost::uint32_t last_block_id = cache_manager_.GetCacheLastBlockId();

        if(request_block_id > last_block_id)
        {
            return;
        }

        if (request_block_id < first_block_id)
        {
            request_block_id = first_block_id;
        }

        while (request_block_id <= last_block_id)
        {
            if (cache_manager_.IsBlockHeaderValid(request_block_id))
            {
                if (false == TryAddBlockToAnnounceMap(request_block_id))
                {
                    //添加失败意味着累计的数据长度已经超出一个packet的大小，放弃本block以及之后的所有blocks
                    break;
                }
            }
            else if (announce_map_.block_info_count_ > 0)
            {
                //以避免中间出现block空洞
                break;
            }

            request_block_id += cache_manager_.GetLiveInterval();
        }

        assert(accumulated_packet_size_ <= MaximumPacketSizeAllowed());
        assert(announce_map_.block_info_count_ == announce_map_.subpiece_nos_.size());
        assert(announce_map_.block_info_count_ == announce_map_.subpiece_map_.size());

        LOG4CPLUS_DEBUG_LOG(logger_live_announce_map_builder, "announce_map - number of blocks gathered:" 
            << announce_map_.block_info_count_);
        return;
    }

    uint32_t LiveAnnouceMapBuilder::MaximumPacketSizeAllowed() const
    {
        if (additional_bits_beyond_the_last_byte_ > 0)
        {
            return LIVE_SUB_PIECE_SIZE - 1;
        }

        return LIVE_SUB_PIECE_SIZE;
    }

    //announce map修改的粒度应该以block为单位 - 要么整个block的信息都添加进去，否则就整个block都忽略。
    //返回值表明是否添加成功
    bool LiveAnnouceMapBuilder::TryAddBlockToAnnounceMap(boost::uint32_t request_block_id)
    {
        //此函数被调用的前提是该block有个已经过验证的头部，这隐含着 - 至少已经存在subpiece[0]
        assert(cache_manager_.IsBlockHeaderValid(request_block_id));

        boost::uint16_t subpieces_count = static_cast<boost::uint16_t>(
            (cache_manager_.GetBlockSizeInBytes(request_block_id) + LIVE_SUB_PIECE_SIZE - 1) / LIVE_SUB_PIECE_SIZE);

        assert(subpieces_count >= 1);

        boost::dynamic_bitset<boost::uint8_t> bit_set;

        //subpiece[0]应该总是存在的，
        assert(cache_manager_.HasSubPiece(protocol::LiveSubPieceInfo(request_block_id, 0)));
        bit_set.push_back(1);

        int num_of_pieces = (subpieces_count - 1 + protocol::SUBPIECE_COUNT_IN_ONE_CHECK - 1) / protocol::SUBPIECE_COUNT_IN_ONE_CHECK;
        for(int piece_index = 0; piece_index < num_of_pieces; ++piece_index)
        {
            bool eligible_for_upload = cache_manager_.IsPieceComplete(request_block_id, piece_index) &&
                cache_manager_.IsPieceValid(request_block_id, piece_index);

            bit_set.push_back(eligible_for_upload ? 1 : 0);
        }

        uint32_t new_packet_size = accumulated_packet_size_;
        new_packet_size += 2;
        new_packet_size += (subpieces_count + additional_bits_beyond_the_last_byte_) / 8;
        additional_bits_beyond_the_last_byte_ = (subpieces_count + additional_bits_beyond_the_last_byte_) % 8;

        if (new_packet_size > MaximumPacketSizeAllowed())
        {
            return false;
        }

        accumulated_packet_size_ = new_packet_size;
        ++announce_map_.block_info_count_;
        announce_map_.subpiece_nos_.insert(std::make_pair(request_block_id, subpieces_count));
        announce_map_.subpiece_map_.insert(make_pair(request_block_id, bit_set));

        return true;
    }
}
