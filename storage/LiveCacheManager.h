//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _LIVE_CACHE_MANAGER_H
#define _LIVE_CACHE_MANAGER_H

#include "LivePointTracker.h"
#include "LiveBlockNode.h"

namespace storage
{
    class LiveCacheManager
        : public boost::noncopyable
        , public boost::enable_shared_from_this<LiveCacheManager>
#ifdef DUMP_OBJECT
        , public count_object_allocate<LiveCacheManager>
#endif
    {
    public:
        LiveCacheManager(boost::uint16_t live_interval, const RID& rid)
            : live_point_tracker_(live_interval), rid_(rid)
        {
        };

        bool AddSubPiece(const protocol::LiveSubPieceInfo & subpiece, const protocol::LiveSubPieceBuffer & buf);
        bool HasSubPiece(const protocol::LiveSubPieceInfo & subpiece) const;

        bool IsEmpty() const { return block_nodes_.size() == 0; }
        bool IsBlockHeaderValid(uint32_t block_id) const;
        bool HasCompleteBlock(uint32_t block_id) const;
        bool IsPieceComplete(uint32_t block_id, uint16_t piece_index) const;
        bool IsPieceValid(uint32_t block_id, uint16_t piece_index) const;
        bool BlockExists(uint32_t block_id) const { return block_nodes_.find(block_id) != block_nodes_.end(); }

        void GetBlock(uint32_t block_id, std::vector<protocol::LiveSubPieceBuffer> & subpiece_buffers) const;
        void GetSubPiece(protocol::LiveSubPieceInfo subpiece_info, protocol::LiveSubPieceBuffer & subpiece_buffer) const;

        //返回range[start_subpiece_index, last_subpiece_index]
        void GetSubPieces(uint32_t block_id, uint16_t start_subpiece_index, uint16_t last_subpiece_index, std::vector<protocol::LiveSubPieceBuffer> & subpiece_buffers) const;

        uint32_t GetBlockSizeInBytes(uint32_t block_id) const;
        uint32_t GetSubPiecesCount(uint32_t block_id) const;

        void GetNextMissingSubPiece(uint32_t start_block_id, protocol::LiveSubPieceInfo & missing_subpiece) const;

        boost::uint32_t GetDataRate() const;
        boost::uint16_t GetCacheSize() const;
        boost::uint32_t GetCacheFirstBlockId() const;
        boost::uint32_t GetCacheLastBlockId() const;
        boost::uint16_t GetLiveInterval() const { return live_point_tracker_.GetLiveInterval(); }

        const LivePosition GetCurrentLivePoint() const
        {
            return live_point_tracker_.GetCurrentLivePoint();
        }

        void SetCurrentLivePoint(const LivePosition & live_point)
        {
            live_point_tracker_.SetCurrentLivePoint(live_point);
        }

        bool RemoveBlock(uint32_t block_id);

        const RID& GetRID() const { return rid_; }

        boost::uint32_t GetChecksumFailedTimes() const;

        boost::uint32_t GetMissingSubPieceCount(boost::uint32_t block_id) const;
        boost::uint32_t GetExistSubPieceCount(boost::uint32_t block_id) const;

    private:
        void EnsureBlockExists(uint32_t block_id);

    private:
        RID rid_;
        map<uint32_t, LiveBlockNode::p> block_nodes_;
        LivePointTracker live_point_tracker_;
        mutable map<uint32_t, uint32_t> block_checksum_failed_times_;
    };
}

#endif  //_LIVE_CACHE_MANAGER_H
