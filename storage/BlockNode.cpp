//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "storage/storage_base.h"
#include "storage/Resource.h"
#include "storage/StorageThread.h"
#include "storage/PieceNode.h"
#include "storage/BlockNode.h"
#include "base/util.h"

#include <iomanip>

namespace storage
{
    using protocol::SubPieceBuffer;
    using protocol::SubPieceContent;
    using base::util::memcpy2;
    FRAMEWORK_LOGGER_DECLARE_MODULE("node");

    BlockNode::BlockNode(uint32_t index, uint32_t subpiece_num, bool is_full_disk)
        : index_(index), need_write_(false)
    {
        assert(subpiece_num>0);
        total_subpiece_count_ = subpiece_num;
        last_piece_capacity_ = (subpiece_num - 1) % subpiece_num_per_piece_g_ + 1;
        uint32_t piece_count = (subpiece_num + subpiece_num_per_piece_g_ - 1) / subpiece_num_per_piece_g_;
        piece_nodes_.resize(piece_count, PieceNode::p());

        if (!is_full_disk)
        {
            curr_subpiece_count_ = 0;
        }
        else
        {
            curr_subpiece_count_ = total_subpiece_count_;
            uint32_t piece_capacity = 0;
            for (uint32_t pidx = 0; pidx < piece_count; ++pidx)
            {
                piece_capacity = (pidx == piece_count-1) ? last_piece_capacity_ : subpiece_num_per_piece_g_;
                piece_nodes_[pidx] = PieceNode::Create(protocol::PieceInfo(index, pidx), piece_capacity, true);
            }
        }
        access_counter_.start();
    }

    BlockNode::p BlockNode::Create(uint32_t index, uint32_t subpiece_num, bool is_full_disk)
    {
        p pointer;
        pointer = p(new BlockNode(index, subpiece_num, is_full_disk));
        assert(pointer);
        return pointer;
    }

    bool BlockNode::Parse(uint32_t index, base::AppBuffer const & blockinfo_buf, BlockNode::p & block_node)
    {
        uint32_t total_subpiece_count = 0;
        uint32_t pieces_buffer_size = 0;
        if (!CheckBuf(blockinfo_buf, total_subpiece_count, pieces_buffer_size))
        {
            return false;
        }

        if (pieces_buffer_size == 0)
        {
            block_node = BlockNode::p();
            return true;
        }

        boost::uint8_t* buf = blockinfo_buf.Data() + 4;
        uint32_t offset = 0;

        uint32_t piecebuflen;
        boost::uint16_t piece_index;

        BlockNode::p pointer = BlockNode::p(new BlockNode(index, total_subpiece_count, false));
        std::set<uint32_t> empty_pieces;

        while (offset < pieces_buffer_size)
        {
            if (offset + 6 > pieces_buffer_size)
            {
                assert(false);
                return false;
            }

            // read piece_index @herain
            memcpy2((void*)(&piece_index), sizeof(piece_index), buf + offset, sizeof(boost::uint16_t));
            offset += 2;

            if (piece_index > pointer->piece_nodes_.size())
            {
                assert(false);
                return false;
            }

            // read PieceNode buffer length @herain
            memcpy2((void*)(&piecebuflen), sizeof(piecebuflen), buf + offset, sizeof(uint32_t));
            offset += 4;

            if (piecebuflen == 0)
            {
                assert(false);
                return false;
            }

            if (offset + piecebuflen > pieces_buffer_size)
            {
                assert(false);
                return false;
            }

            base::AppBuffer piecebuf(buf + offset, piecebuflen);
            offset += piecebuflen;

            PieceNode::p piece_node;
            if (!PieceNode::Parse(protocol::PieceInfo(index, piece_index), piecebuf, piece_node))
            {
                assert(false);
                return false;
            }
            pointer->piece_nodes_[piece_index] = piece_node;
            if (!piece_node)
            {
                empty_pieces.insert(piece_index);
            }
        }

        assert(offset == pieces_buffer_size);

        // calculate cur_subpiece_count_ @herain
        uint32_t curr_null_subpiece_count = 0;
        for (uint32_t pidx = 0; pidx < pointer->piece_nodes_.size(); ++pidx)
        {
            if (pointer->piece_nodes_[pidx])
            {
                curr_null_subpiece_count += pointer->piece_nodes_[pidx]->GetCurrNullSubPieceCount();
            }
            else if (empty_pieces.find(pidx) != empty_pieces.end())
            {
                // lost entire piece, null_subpiece_count == piece_capacity @herain
                if (pidx == (pointer->total_subpiece_count_ - 1) / subpiece_num_per_piece_g_)
                {
                    curr_null_subpiece_count += pointer->last_piece_capacity_;
                }
                else
                {
                    curr_null_subpiece_count += subpiece_num_per_piece_g_;
                }
            }
            else
            {
                // have entire piece, null_subpiece_count == 0 @herain
                if (pidx == (pointer->total_subpiece_count_ - 1) / subpiece_num_per_piece_g_)
                {
                    pointer->piece_nodes_[pidx] = PieceNode::Create(protocol::PieceInfo(index, pidx), pointer->last_piece_capacity_, true);
                }
                else
                {
                    pointer->piece_nodes_[pidx] = PieceNode::Create(protocol::PieceInfo(index, pidx), subpiece_num_per_piece_g_, true);
                }
            }
        }
        pointer->curr_subpiece_count_ = pointer->total_subpiece_count_ - curr_null_subpiece_count;

        block_node = pointer;
        return true;
    }

    /**
    *  BlockNode SubPieceBuffer output format(Hex) :
    *  xx xx xx xx              xx xx                  xx xx xx xx xx       xx xx                  xx xx xx xx xx  ...repeat...
    *          /||\                       /||\                      /||\                       /||\                             /||\
    *            ||                          ||                         ||                          ||                                ||
    *  --------------------   ----------------  ------------------    -----------------  ------------------
    *  | subpiece_cout |  | piece_index | | piece_buffer |    | piece_index |  | piece_buffer |
    *  --------------------  -----------------  -----------------     -----------------  ------------------
    */
    base::AppBuffer BlockNode::ToBuffer()
    {
        uint32_t tmp_buffer_size = 2 * 1024 * 32;
        uint32_t buf_len = 0;

        boost::uint8_t *tmp = new boost::uint8_t[tmp_buffer_size];

        // write subpiece count @herain
        memcpy2((void*)(tmp + buf_len), tmp_buffer_size - buf_len, (void*)(&total_subpiece_count_), sizeof(uint32_t));
        buf_len += 4;

        std::vector<PieceNode::p>::const_iterator pieceit = piece_nodes_.begin();
        for (uint16_t pidx = 0; pieceit != piece_nodes_.end(); ++pieceit, ++pidx)
        {
            // write piece_index @herain
            memcpy2((void*)(tmp + buf_len), tmp_buffer_size - buf_len, (void*)(&pidx), sizeof(boost::uint16_t));
            buf_len += 2;
            // get PieceNode buffer @herain
            base::AppBuffer piecebuf;
            if (!piece_nodes_[pidx])
            {
                piecebuf = PieceNode::NullPieceToBuffer();
            }
            else
            {
                piecebuf = piece_nodes_[pidx]->ToBuffer();
            }

            // reallocate buffer when encounter buffer space shortage @herain
            if (buf_len + piecebuf.Length() + 1024 > tmp_buffer_size)
            {
                tmp_buffer_size = tmp_buffer_size * 2;
                boost::uint8_t *tmp2 = tmp;
                tmp = new boost::uint8_t[tmp_buffer_size];
                memcpy2(tmp, tmp_buffer_size, tmp2, buf_len);
                delete[] tmp2;
            }

            // write PieceNode buffer Length @herain
            uint32_t piecebuf_len = piecebuf.Length();
            memcpy2((void*)(tmp + buf_len), tmp_buffer_size - buf_len, (void*)(&piecebuf_len), sizeof(uint32_t));
            buf_len += 4;

            // write PieceNode buffer content @herain
            memcpy2((tmp + buf_len), tmp_buffer_size - buf_len, piecebuf.Data(), piecebuf.Length());
            buf_len += piecebuf.Length();
        }
        base::AppBuffer outbuf(tmp, buf_len);

        delete[] tmp;
        return outbuf;
    }

    bool BlockNode::AddSubPiece(uint32_t index, SubPieceNode::p node)
    {
        assert(index <= total_subpiece_count_ -1);
        if (!need_write_)
            need_write_ = true;

        uint32_t piece_index = index / subpiece_num_per_piece_g_;
        uint16_t subpiece_index = index % subpiece_num_per_piece_g_;
        if (!piece_nodes_[piece_index])
        {
            uint32_t piece_capacity;
            if (piece_index != piece_nodes_.size() - 1)
                piece_capacity = subpiece_num_per_piece_g_;
            else
                piece_capacity = last_piece_capacity_;
            piece_nodes_[piece_index] = PieceNode::Create(protocol::PieceInfo(index_, piece_index), piece_capacity);
        }
        if (piece_nodes_[piece_index]->AddSubPiece(subpiece_index, node))
        {
            ++curr_subpiece_count_;
            access_counter_.reset();
            STORAGE_DEBUG_LOG("AddSubPiece [" << index_ << "|" << piece_index << "|" << subpiece_index << "]");
            STORAGE_DEBUG_LOG("curr_subpiece_count_=" << curr_subpiece_count_ << ", need_write=" << need_write_);
            return true;
        }
        else
        {
            return false;
        }
    }

    bool BlockNode::LoadSubPieceBuffer(uint32_t index, protocol::SubPieceBuffer buf)
    {
        assert(index <= total_subpiece_count_-1);
        uint32_t piece_index = index / subpiece_num_per_piece_g_;
        uint16_t subpiece_index = index % subpiece_num_per_piece_g_;
        assert(piece_nodes_[piece_index]);
        if (!piece_nodes_[piece_index])
            return false;
        if (piece_nodes_[piece_index]->LoadSubPieceBuffer(subpiece_index, buf))
        {
            access_counter_.reset();
            return true;
        }
        else
        {
            return false;
        }
    }

    bool BlockNode::HasPiece (const uint32_t piece_index) const
    {
        if (piece_index < piece_nodes_.size())
        {
            if (!piece_nodes_[piece_index])
                return false;
            else
                return piece_nodes_[piece_index]->IsFull();
        }
        else
        {
            assert(false);
            return false;
        }
    }

    bool BlockNode::HasSubPiece(const uint32_t index) const
    {
        uint32_t piece_index = index / subpiece_num_per_piece_g_;
        if (!piece_nodes_[piece_index])
        {
            return false;
        }
        return piece_nodes_[piece_index]->HasSubPiece(index % subpiece_num_per_piece_g_);
    }

    bool BlockNode::HasSubPieceInMem(const uint32_t index) const
    {
        assert(index <= total_subpiece_count_-1);
        uint32_t piece_index = index / subpiece_num_per_piece_g_;

        if (!piece_nodes_[piece_index])
        {
            STORAGE_DEBUG_LOG("PieceNode[" << piece_index << "] not exist");
            return false;
        }
        return piece_nodes_[piece_index]->HasSubPieceInMem(index % subpiece_num_per_piece_g_);
    }

    bool BlockNode::SetSubPieceReading(const uint32_t index)
    {
        assert(index <= total_subpiece_count_-1);
        uint32_t piece_index = index / subpiece_num_per_piece_g_;
        if (!piece_nodes_[piece_index])
        {
            assert(false);
            return false;
        }
        return piece_nodes_[piece_index]->SetSubPieceReading(index % subpiece_num_per_piece_g_);
    }

    protocol::SubPieceBuffer BlockNode::GetSubPiece(uint32_t subpiece_index)
    {
        assert(subpiece_index <= total_subpiece_count_ - 1);
        uint32_t piece_index = subpiece_index / subpiece_num_per_piece_g_;

        if (!piece_nodes_[piece_index])
        {
            return protocol::SubPieceBuffer();
        }

        protocol::SubPieceBuffer buf = piece_nodes_[piece_index]->GetSubPiece(subpiece_index % subpiece_num_per_piece_g_);
        if (buf.IsValid(SUB_PIECE_SIZE))
        {
            access_counter_.reset();
            return buf;
        }
        else
            return protocol::SubPieceBuffer();
    }

    bool BlockNode::GetNextNullSubPiece(const uint32_t start, uint32_t& subpiece_for_download) const
    {
        uint32_t start_piece_index = start / subpiece_num_per_piece_g_;

        for (uint32_t pidx = start_piece_index; pidx < piece_nodes_.size(); ++pidx)
        {
            if (!piece_nodes_[pidx])
            {
                if (pidx == start_piece_index)
                    subpiece_for_download = start;
                else
                    subpiece_for_download = pidx * subpiece_num_per_piece_g_;
                return true;
            }

            if (piece_nodes_[pidx]->IsFull())
            {
                continue;
            }
            else
            {
                // start subpiece index in piece pidx@herain
                uint32_t startindex;
                if (start_piece_index == pidx)
                    startindex = start % subpiece_num_per_piece_g_;
                else
                    startindex = 0;

                if (piece_nodes_[pidx]->GetNextNullSubPiece(startindex, subpiece_for_download))
                {
                    subpiece_for_download += pidx * subpiece_num_per_piece_g_;
                    return true;
                }
            }
        }
        return false;
    }

    bool BlockNode::CheckBuf(base::AppBuffer blockinfo_buf, uint32_t &max_num_subpiece, uint32_t &pieces_buffer_size)
    {
        if (blockinfo_buf.Length() < 4)
        {
            return false;
        }

        // read subpiece  count
        memcpy2((void*)(&max_num_subpiece), sizeof(max_num_subpiece), (void*)blockinfo_buf.Data(), sizeof(uint32_t));

        if (max_num_subpiece == 0)
        {
            // empty block, 4 bytes zero placeholder @herain
            assert(blockinfo_buf.Length() == 4);
            pieces_buffer_size = 0;
        }
        else
            pieces_buffer_size = blockinfo_buf.Length() - 4;

        return true;
    }

#ifdef DISK_MODE
    void BlockNode::WriteToResource(Resource::p resource_p)
    {
        if (!need_write_)
        {
            STORAGE_DEBUG_LOG("need_write_ be false!");
            return;
        }

        need_write_ = false;
        STORAGE_DEBUG_LOG("set need_write_ false!");
        if (!IsFull())
        {
            for (uint32_t pidx = 0; pidx < piece_nodes_.size(); ++pidx)
            {
                if (!piece_nodes_[pidx])
                    continue;

                piece_nodes_[pidx]->WriteToResource(resource_p);
            }
            StorageThread::Post(boost::bind(&Resource::SecSaveResourceFileInfo, resource_p));
            STORAGE_DEBUG_LOG("will post to SecSaveResourceFileInfo");
        }
        else
        {
            assert(false);
        }
    }
#endif

    void BlockNode::ClearBlockMemCache(uint32_t play_subpiece_index)
    {
        access_counter_.reset();
        uint32_t play_piece = play_subpiece_index / subpiece_num_per_piece_g_;
        uint16_t play_subpiece = play_subpiece_index % subpiece_num_per_piece_g_;
        STORAGE_DEBUG_LOG("ClearBlockMemCache play_subpiece_index:" << play_subpiece_index);

        for (uint32_t pidx = 0; pidx <= play_piece && pidx < piece_nodes_.size(); ++pidx)
        {
            if (!piece_nodes_[pidx])
                continue;

            // 非磁盘模式ClearBlockMemCache会造成数据丢失 @herain
#ifndef DISK_MODE
            uint16_t old_subpiece_count = piece_nodes_[pidx]->GetCurrSubPieceCount();
#endif
            if (pidx == play_piece)
                piece_nodes_[pidx]->ClearBlockMemCache(play_subpiece);
            else
                piece_nodes_[pidx]->ClearBlockMemCache(subpiece_num_per_piece_g_);

#ifndef DISK_MODE
            uint16_t new_subpiece_count = piece_nodes_[pidx]->GetCurrSubPieceCount();
            curr_subpiece_count_ -= old_subpiece_count - new_subpiece_count;
#endif
        }
    }

    void BlockNode::OnWriteFinish()
    {
        for (uint32_t pidx = 0; pidx < piece_nodes_.size(); ++pidx)
        {
            if (!piece_nodes_[pidx])
                continue;

            piece_nodes_[pidx]->OnWriteFinish();
        }
    }

    void BlockNode::OnWriteSubPieceFinish(uint32_t index_)
    {
        uint32_t piece_index = index_ / subpiece_num_per_piece_g_;
        uint32_t subpiece_index = index_ % subpiece_num_per_piece_g_;
        if (piece_nodes_[piece_index])
        {
            piece_nodes_[piece_index]->OnWriteSubPieceFinish(subpiece_index);
            return;
        }
        assert(false);
    }

    void BlockNode::GetBufferForSave(std::map<protocol::SubPieceInfo, protocol::SubPieceBuffer> & buffer_set)
    {
        if (!need_write_)
        {
            STORAGE_DEBUG_LOG("need_write_ false! return.");
            return;
        }

        need_write_ = false;
        STORAGE_DEBUG_LOG("set need_write_ false.");
        for (uint32_t pidx = 0; pidx < piece_nodes_.size(); ++pidx)
        {
            if (!piece_nodes_[pidx])
                continue;

            piece_nodes_[pidx]->GetBufferForSave(buffer_set);
        }
    }

    bool BlockNode::IsSaving() const
    {
        for (boost::uint16_t pidx = 0; pidx < piece_nodes_.size(); ++pidx)
        {
            if (piece_nodes_[pidx] && piece_nodes_[pidx]->IsSaving())
                return true;
        }
        return false;
    }

    std::ostream& operator << (std::ostream& os, const BlockNode& node)
    {
        std::vector<PieceNode::p>::const_iterator iter = node.piece_nodes_.begin();
        for (uint32_t pidx = 0; iter != node.piece_nodes_.end(); ++pidx, ++iter)
        {
            os << std::setw(6) << pidx << " ";
            if (node.piece_nodes_[pidx])
            {
                os << *(node.piece_nodes_[pidx]);
            }
            os << std::endl;
        }
        return os;
    }

}
