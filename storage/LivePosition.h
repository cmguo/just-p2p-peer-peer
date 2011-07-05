//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _LIVE_POSITION_H_
#define _LIVE_POSITION_H_

namespace storage
{
    class LivePosition
#ifdef DUMP_OBJECT
        : public count_object_allocate<LivePosition>
#endif
    {
    public:
        LivePosition(uint32_t block_id = 0, boost::int16_t subpiece_index = 0)
            : position_(block_id, subpiece_index)
        {
        }

        uint32_t GetBlockId() const { return position_.GetBlockId(); }

        uint16_t GetSubPieceIndex() const { return position_.GetSubPieceIndex(); }

        void SetBlockId(uint32_t new_block_id)
        { 
            position_ = protocol::LiveSubPieceInfo(new_block_id, 0); 
        }

        void AdvanceSubPieceIndexTo(uint16_t new_subpiece_index)
        {
            assert(position_.GetSubPieceIndex() < new_subpiece_index);
            position_ = protocol::LiveSubPieceInfo(position_.GetBlockId(), new_subpiece_index);
        }

        operator const protocol::LiveSubPieceInfo&() const
        {
            return position_;
        }

        bool operator < (const LivePosition& live_position) const
        {
            return position_ < live_position.position_;
        }

    private:
        protocol::LiveSubPieceInfo position_;
    };
}

#endif //_LIVE_POSITION_H_