//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _PIECE_NODE_H
#define _PIECE_NODE_H

namespace storage
{
    enum NodeState
    {
        NDST_NONE,
        NDST_DISK,
        NDST_MEM,
        NDST_READING,
        NDST_SAVING,
        NDST_ALL,
    };

    class Resource;

    //////////////////////////////////////////////////////////////////////////
    //     SubPieceNode
    class SubPieceNode
#ifdef DUMP_OBJECT
        : public count_object_allocate<SubPieceNode>
#endif
    {
    public:
        typedef boost::shared_ptr<SubPieceNode> p;
        SubPieceNode()
        {
            state_ = NDST_NONE;
            subpiece_ = protocol::SubPieceBuffer();
        }

        explicit SubPieceNode(NodeState state,
            protocol::SubPieceBuffer buf = protocol::SubPieceBuffer()) : state_(state), subpiece_(buf){}

        ~SubPieceNode() {
            state_ = NDST_NONE;
        }
    public:
        NodeState state_;
        protocol::SubPieceBuffer subpiece_;
    };

    //////////////////////////////////////////////////////////////////////////
    //     PieceNode
    class PieceNode: public boost::noncopyable
#ifdef DUMP_OBJECT
        , public count_object_allocate<PieceNode>
#endif
    {
    public:
        typedef boost::shared_ptr<PieceNode> p;
        friend std::ostream& operator << (std::ostream& os, const PieceNode& info);

    private:
        PieceNode(const protocol::PieceInfo & info, boost::uint16_t subpiece_num, bool is_full_disk);  // 1-128(subpiece_num_per_piece_g_)
        PieceNode(const protocol::PieceInfo & info, boost::dynamic_bitset<uint32_t> &subpiece_set, uint32_t bitcount);

    public:
        static PieceNode::p Create(const protocol::PieceInfo & info, uint32_t subpiece_num, bool is_full_disk = false);
        static bool Parse(const protocol::PieceInfo & info, base::AppBuffer piece_buf, PieceNode::p &piece_node);
        static base::AppBuffer NullPieceToBuffer() {return base::AppBuffer(4, 0);}

        base::AppBuffer ToBuffer();
        bool AddSubPiece(uint32_t index, SubPieceNode::p subpiece_node);
        bool LoadSubPieceBuffer(uint32_t index, protocol::SubPieceBuffer buf);
        bool HasSubPiece(uint32_t index) const;  // 0-127(subpiece_num_per_piece_g_-1);
        bool HasSubPieceInMem(uint32_t index) const;
        bool SetSubPieceReading(uint32_t index);
        protocol::SubPieceBuffer GetSubPiece(uint32_t subpiece_index) const;

        bool GetNextNullSubPiece(uint32_t start, uint32_t &next_index) const;
        uint32_t GetCurrNullSubPieceCount() const{return subpiece_set_.size() - subpiece_set_.count();}
        uint32_t GetCurrSubPieceCount() const{return subpiece_set_.count();}
        uint32_t GetSize()  const{return subpiece_set_.size();}
        bool IsFull()  const{return subpiece_set_.count() == subpiece_set_.size();}
        bool IsEmpty()  const{return subpiece_set_.count() == 0;}
        bool IsSaving() const;
        NodeState SubPieceState(uint16_t idx) const {
            return subpieces_[idx]->state_;
        }

        void ClearBlockMemCache(uint16_t play_subpiece_index);
        void OnWriteFinish();
        void GetBufferForSave(std::map<protocol::SubPieceInfo, protocol::SubPieceBuffer> & buffer_set) const;

    private:
        static bool CheckBuf(base::AppBuffer inbuf, uint32_t &bitcount, uint16_t &bitbuffersize);

    public:
        static PieceNode::p null_piece_;

    private:
        boost::dynamic_bitset<uint32_t> subpiece_set_;
        std::vector<SubPieceNode::p> subpieces_;
        protocol::PieceInfo info_;
    };
}

#endif
