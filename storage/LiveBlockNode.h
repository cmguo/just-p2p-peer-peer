//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _LIVE_BLOCK_NODE_H
#define _LIVE_BLOCK_NODE_H

#include "HeaderSubPiece.h"

namespace storage
{
    class LiveBlockNode
        : public boost::noncopyable
        , public boost::enable_shared_from_this<LiveBlockNode>
#ifdef DUMP_OBJECT
        , public count_object_allocate<LiveBlockNode>
#endif
    {
    public:
        typedef boost::shared_ptr<LiveBlockNode> p;
        LiveBlockNode(uint32_t block_id, const RID& rid)
            : block_id_(block_id), rid_(rid), block_header_validated_(false)
            , checksum_failed_times_(0)
        {
        }

        bool AddSubPiece(uint16_t subpiece_index, const protocol::LiveSubPieceBuffer & buf);
        bool HasSubPiece(uint16_t subpiece_index) const;
        void GetBlock(std::vector<protocol::LiveSubPieceBuffer> & subpiece_buffers) const;
        void GetBuffer(boost::uint16_t subpiece_index, protocol::LiveSubPieceBuffer & subpiece_buffer) const;
        bool IsComplete() const;
        bool IsEmpty() const;
        bool IsHeaderValid() const;
        uint16_t GetNextMissingSubPiece() const;
        bool HasLength() const { return HasHeaderSubPiece(); }
        uint32_t GetBlockSizeInBytes() const { return HasHeaderSubPiece() ? header_subpiece_->GetDataLength() + LIVE_SUB_PIECE_SIZE : 0; }
        uint32_t GetSubPiecesCount() const { return HasHeaderSubPiece() ? subpieces_.size() : 0; }

        bool IsPieceComplete(uint16_t piece_index) const;
        bool IsPieceValid(uint16_t piece_index) const;

        static uint16_t GetPieceIndex(uint32_t subpiece_index)
        {
            //subpiece[0] is excluded
            assert(subpiece_index > 0);
            return (subpiece_index - 1)/HeaderSubPiece::Constants::SubPiecesPerPiece;
        }

        static uint32_t GetFirstSubPieceIndex(uint16_t piece_index)
        {
            return HeaderSubPiece::Constants::SubPiecesPerPiece * piece_index + 1;
        }

        boost::uint32_t GetChecksumFailedTimes() const;

        boost::uint32_t GetMissingSubPieceCount() const;
        boost::uint32_t GetExistSubPieceCount() const;

    private:
        void CheckHeadSubPiece();
        bool HasHeaderSubPiece() const { return header_subpiece_; }
        
        void DropDataIfNecessary(uint16_t new_arrival_subpiece_index);
        void DropPiece(uint16_t piece_index);
        void DropSubPiece(uint16_t subpiece_index);
    private:
        std::vector<protocol::LiveSubPieceBuffer> subpieces_;
        boost::shared_ptr<HeaderSubPiece> header_subpiece_;

        uint32_t block_id_;
        RID rid_;

        //如果为true，表明block header被验证过而且通过验证
        mutable bool block_header_validated_;
        mutable std::set<uint16_t> validated_pieces_;
        mutable boost::uint32_t checksum_failed_times_;
    };
}

#endif  //_LIVE_BLOCK_NODE_H
