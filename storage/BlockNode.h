//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _BLOCKNODE_H_
#define _BLOCKNODE_H_

namespace storage
{
#ifdef PEER_PC_CLIENT
    #define BLOCK_ACCESS_TIMEOUT 20
#else
    #define BLOCK_ACCESS_TIMEOUT 0
#endif

    class Resource;
    class SubPieceInfo;
    class SubPieceBuffer;

    //////////////////////////////////////////////////////////////////////////
    //     BlockNode
    class BlockNode: boost::noncopyable
#ifdef DUMP_OBJECT
        , public count_object_allocate<BlockNode>
#endif
    {
    public:
        enum BlockNodeState
        {
            EMPTY,
            DOWNLOADING,
            MEM,
            SAVING,
            DISK,
            READING,
            ALL
        };

        typedef boost::shared_ptr<BlockNode> p;
        friend std::ostream& operator << (std::ostream& os, const BlockNode& node);
    private:
        BlockNode(uint32_t index, uint32_t subpiece_num, bool is_full_disk);
    public:
        static BlockNode::p Create(uint32_t index, uint32_t subpiece_num, bool is_full_disk = false);

        // serialization related function @herain
        static bool Parse(uint32_t index, base::AppBuffer const & blockinfo_buf, BlockNode::p & block_node);
        static base::AppBuffer NullBlockToBuffer(){return base::AppBuffer(4, 0);}
        base::AppBuffer ToBuffer();
    private:
        static bool CheckBuf(base::AppBuffer inbuf, uint32_t &max_num_subpiece, uint32_t &pieces_buffer_size);
        bool IsPieceFull(uint32_t piece_index) const;
        bool IsBlockSavedOnDisk() const { return node_state_ == BlockNode::ALL || node_state_ == BlockNode::DISK || node_state_ == READING;}

    public:
        // subpiece management function @herain
        bool AddSubpiece(uint32_t index, protocol::SubPieceBuffer buf);
        bool LoadSubPieceBuffer(uint32_t index, protocol::SubPieceBuffer buf);
        bool HasSubPiece(const uint32_t subpiece_index) const;
        bool HasSubPieceInMem(const uint32_t subpiece_index) const;
        bool SetBlockReading();
        protocol::SubPieceBuffer GetSubPiece(uint32_t subpiece_index);

        // property query
        bool HasPiece (const uint32_t piece_index) const;
        uint32_t GetCurrNullSubPieceCount() const
        {
            return total_subpiece_count_ - subpieces_.size();
        }

        bool GetNextNullSubPiece(const uint32_t sub_subpiece_index, uint32_t& subpiece_for_download) const;
        uint32_t GetCurrSubPieceCount() const
        {
            return subpieces_.size();
        }

        bool IsFull() const
        {
            return subpieces_.size() == total_subpiece_count_ || IsBlockSavedOnDisk();
        }

        bool IsEmpty()  const {return subpieces_.size() == 0 && !IsBlockSavedOnDisk();}
        bool IsSaving() const;
        bool NeedWrite() const {return need_write_;}
        bool IsAccessTimeout() const {return access_counter_.elapsed() > BLOCK_ACCESS_TIMEOUT*1000;}

        void ClearBlockMemCache(uint32_t play_subpiece_index);
        void OnWriteFinish();
        void GetBufferForSave (std::map<protocol::SubPieceInfo, protocol::SubPieceBuffer> & buffer_set);

        uint32_t GetTotalSubpieceCountInBlock()
        {
            return total_subpiece_count_;
        }

    private:
        std::map<uint32_t, protocol::SubPieceBuffer> subpieces_;
        std::vector<uint32_t> piece_count_;
        BlockNodeState node_state_;
        uint32_t total_subpiece_count_;
        uint32_t last_piece_capacity_;
        uint32_t index_;
        bool need_write_;
        framework::timer::TickCounter access_counter_;
    };
}

#endif

