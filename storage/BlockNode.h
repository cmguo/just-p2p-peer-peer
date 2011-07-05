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

    class PieceNode;
    class Resource;

    //////////////////////////////////////////////////////////////////////////
    //     BlockNode
    class BlockNode: boost::noncopyable
#ifdef DUMP_OBJECT
        , public count_object_allocate<BlockNode>
#endif
    {
    public:
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

    public:
        // subpiece management function @herain
        bool AddSubPiece(uint32_t index, SubPieceNode::p node);
        bool LoadSubPieceBuffer(uint32_t index, protocol::SubPieceBuffer buf);
        bool HasSubPiece(const uint32_t subpiece_index) const;
        bool HasSubPieceInMem(const uint32_t subpiece_index) const;
        bool SetSubPieceReading(const uint32_t index);
        protocol::SubPieceBuffer GetSubPiece(uint32_t subpiece_index);

        // property query
        bool HasPiece (const uint32_t piece_index) const;
        uint32_t GetCurrNullSubPieceCount() const
        {
            return total_subpiece_count_ - curr_subpiece_count_;
        }

        bool GetNextNullSubPiece(const uint32_t sub_subpiece_index, uint32_t& subpiece_for_download) const;
        uint32_t GetCurrSubPieceCount() const
        {
            return curr_subpiece_count_;
        }

        uint32_t GetSize() const
        {
            return piece_nodes_.size();
        }

        bool IsFull() const
        {
            return curr_subpiece_count_ == total_subpiece_count_;
        }

        bool IsEmpty()  const {return curr_subpiece_count_ == 0;}
        bool IsSaving() const;
        bool NeedWrite() const {return need_write_;}
        bool IsAccessTimeout() const {return access_counter_.elapsed() > BLOCK_ACCESS_TIMEOUT*1000;}
#ifdef DISK_MODE
        void WriteToResource(boost::shared_ptr<Resource> resource_p_);
#endif
        void ClearBlockMemCache(uint32_t play_subpiece_index);
        void OnWriteFinish();
        void OnWriteSubPieceFinish(uint32_t subpiece_info);
        void GetBufferForSave (std::map<protocol::SubPieceInfo, protocol::SubPieceBuffer> & buffer_set);

    private:
        std::vector<PieceNode::p> piece_nodes_;
        uint32_t total_subpiece_count_;
        uint32_t curr_subpiece_count_;
        uint32_t last_piece_capacity_;
        uint32_t index_;
        bool need_write_;
        framework::timer::TickCounter access_counter_;
    };
}

#endif

