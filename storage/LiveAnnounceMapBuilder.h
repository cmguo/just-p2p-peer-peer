//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _LIVE_ANNOUNCE_MAP_BUILDER_H
#define _LIVE_ANNOUNCE_MAP_BUILDER_H

#include "LiveCacheManager.h"

namespace storage
{
    class LiveAnnouceMapBuilder
    {
    public:
        LiveAnnouceMapBuilder(LiveCacheManager& cache_manager, protocol::LiveAnnounceMap& announce_map)
            : cache_manager_(cache_manager), announce_map_(announce_map)
        {
            ResetState();
        }

        void Build(boost::uint32_t request_block_id);

    private:
        //announce map修改的粒度应该以block为单位 - 要么整个block的信息都添加进去，否则就整个block都忽略。
        bool TryAddBlockToAnnounceMap(boost::uint32_t request_block_id );

        void ResetState()
        {
            announce_map_.block_info_count_ = 0;
            announce_map_.live_interval_ = cache_manager_.GetLiveInterval();
            announce_map_.subpiece_map_.clear();
            announce_map_.subpiece_nos_.clear();

            additional_bits_beyond_the_last_byte_ = 0;
            accumulated_packet_size_ = 4  // sizeof(request_block_id_)
                + 2  // sizeof(block_info_count_)
                + 4  // StartPieceInfoID
                + 2  // BufferMapLength
                + 2;  // sizeof(live_interval_)
        }


        boost::uint32_t MaximumPacketSizeAllowed() const;

    private:
        LiveCacheManager& cache_manager_;
        protocol::LiveAnnounceMap& announce_map_;
        boost::uint32_t accumulated_packet_size_;
        boost::uint32_t additional_bits_beyond_the_last_byte_;
    };
}

#endif  //_LIVE_ANNOUNCE_MAP_BUILDER_H
