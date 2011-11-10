#ifndef UPLOAD_STRUCT_H
#define UPLOAD_STRUCT_H

#include "statistic/SpeedInfoStatistic.h"

namespace p2sp
{
    struct PEER_UPLOAD_INFO
    {
        static const uint32_t TRANS_ID_SIZE = 5;

        framework::timer::TickCounter last_talk_time;
        framework::timer::TickCounter last_data_time;
        uint32_t ip_pool_size;
        // uint32_t last_data_trans_id;
        uint32_t last_data_trans_ids[TRANS_ID_SIZE];
        Guid peer_guid;
        bool is_open_service;
        RID resource_id;
        protocol::CandidatePeerInfo peer_info;
        bool is_live;
        statistic::SpeedInfoStatistic speed_info;
        framework::timer::TickCounter connected_time;

        PEER_UPLOAD_INFO()
            : last_talk_time(0)
            , last_data_time(0)
            , ip_pool_size(0)
            , is_open_service(false)
            , is_live(false)
        {
            memset(last_data_trans_ids, 0, sizeof(last_data_trans_ids));
        }

        bool IsInLastDataTransIDs(uint32_t trans_id)
        {
            for (uint32_t i = 0; i < TRANS_ID_SIZE; ++i)
            {
                if (last_data_trans_ids[i] == trans_id)
                {
                    return true;
                }
            }
            return false;
        }

        bool UpdateLastDataTransID(uint32_t trans_id)
        {
            uint32_t min_pos = 0;
            for (uint32_t i = 0; i < TRANS_ID_SIZE; ++i)
            {
                if (last_data_trans_ids[i] == trans_id)
                {
                    return false;
                }
                if (last_data_trans_ids[i] < last_data_trans_ids[min_pos])
                {
                    min_pos = i;
                }
            }
            // check
            if (last_data_trans_ids[min_pos] < trans_id)
            {
                last_data_trans_ids[min_pos] = trans_id;
                return true;
            }
            return false;
        }
    };

    struct RBIndex
    {
        RBIndex(const RID & rid, uint32_t block_index)
            : rid(rid)
            , block_index(block_index)
        {

        }
        RID rid;
        uint32_t block_index;

        bool operator == (const RBIndex& other) const
        {
            return block_index == other.block_index && rid == other.rid;
        }
        bool operator<(const RBIndex& other) const
        {
            if (rid != other.rid)
            {
                return rid < other.rid;
            }
            return block_index < other.block_index;
        }
        bool operator <= (const RBIndex& other) const
        {
            if (rid != other.rid)
            {
                return rid < other.rid;
            }
            return block_index <= other.block_index;
        }
    };

    struct RidBlock
    {
        RidBlock(const RBIndex & index, const base::AppBuffer & buf)
            : index(index)
            , buf(buf)
        {
            touch_time_counter_.reset();
        }
        RBIndex index;
        base::AppBuffer buf;
        framework::timer::TickCounter touch_time_counter_;

        bool operator<(const RidBlock& other) { return index < other.index; }
        bool operator == (const RidBlock& other) { return index == other.index; }
        bool operator <= (const RidBlock& other) { return index <= other.index; }
    };
}

#endif
