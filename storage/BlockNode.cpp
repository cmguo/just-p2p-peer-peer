//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "storage/storage_base.h"
#include "storage/Resource.h"
#include "storage/BlockNode.h"
#include "base/util.h"

#include <iomanip>

namespace storage
{
    using base::util::memcpy2;
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_node = log4cplus::Logger::getInstance("[block_node]");
#endif

    BlockNode::BlockNode(uint32_t index, uint32_t subpiece_num, bool is_full_disk)
        : index_(index), need_write_(false)
    {
        assert(subpiece_num>0);
        total_subpiece_count_ = subpiece_num;
        last_piece_capacity_ = (subpiece_num - 1) % subpiece_num_per_piece_g_ + 1;
        uint32_t piece_count = (subpiece_num + subpiece_num_per_piece_g_ - 1) / subpiece_num_per_piece_g_;
        piece_count_.resize(piece_count, 0);

        if (!is_full_disk)
        {
            node_state_ = BlockNode::EMPTY;
        }
        else
        {
            node_state_ = BlockNode::DISK;
            uint32_t piece_capacity = 0;
            for (uint32_t pidx = 0; pidx < piece_count; ++pidx)
            {
                piece_capacity = (pidx == piece_count-1) ? last_piece_capacity_ : subpiece_num_per_piece_g_;
                piece_count_[pidx] = piece_capacity;
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

            if (piece_index > pointer->piece_count_.size())
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

            uint32_t bitcount = 0;
            uint32_t bitbuffersize = 0;
            if (false == CheckBuf(piecebuf, bitcount, bitbuffersize))
            {
                return false;
            }

            if (bitbuffersize == 0)
            {
               break;
            }

            // construct subpiece_set_ @herain
            boost::dynamic_bitset<uint32_t> subpiece_set;
            for (uint8_t i = 0; i < bitbuffersize / sizeof(uint32_t); ++i)
            {
                uint32_t v;
                memcpy2(&v, sizeof(v), piecebuf.Data() + 4 + i*sizeof(uint32_t), sizeof(uint32_t));
                subpiece_set.append(v);
            }
            subpiece_set.resize(bitcount, true);
            subpiece_set.flip();

            if (subpiece_set.count() < bitcount)
            {
                break;
            }

            pointer->piece_count_[piece_index] = bitcount;
        }

        if (offset < pieces_buffer_size)
        {
            // 旧版本存储模式，在block不满的情况下存储，在这里全部舍弃
            pointer.reset();
        }
        else
        {
            pointer->node_state_ = BlockNode::DISK;
        }

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

        for (uint16_t pidx = 0; pidx < piece_count_.size(); pidx++)
        {
            // write piece_index @herain
            memcpy2((void*)(tmp + buf_len), tmp_buffer_size - buf_len, (void*)(&pidx), sizeof(boost::uint16_t));
            buf_len += 2;
            // get PieceNode buffer @herain
            base::AppBuffer piecebuf;
            if (IsBlockSavedOnDisk())
            {
                boost::dynamic_bitset<uint32_t> subpieces_set(piece_count_[pidx], 0);
                // buffer content contains subpiece count and blocks in subpiece_set_ @herain
                uint32_t buf_len = sizeof(uint32_t) + subpieces_set.num_blocks() * sizeof(uint32_t);
                piecebuf.Length(buf_len);
                uint32_t total_subpiece_count = subpieces_set.size();
                memcpy2(piecebuf.Data(), piecebuf.Length(), &total_subpiece_count, sizeof(uint32_t));  // write subpiece count @herain
                // 为了和1.5的内核Cfg文件格式保持一致
                // 1.5内核中使用0表示存在，1表示不存在。
                boost::to_block_range(subpieces_set, (uint32_t*)(piecebuf.Data() + 4));     // write blocks in subpiece_set_ @herain
            }
            else
            {
                piecebuf = base::AppBuffer(4, 0);
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

    bool BlockNode::AddSubpiece(uint32_t index, protocol::SubPieceBuffer buf)
    {
        assert(index <= total_subpiece_count_ -1);

        if (node_state_ == BlockNode::EMPTY)
        {
            node_state_ = BlockNode::DOWNLOADING;
        }

        if (subpieces_.find(index) == subpieces_.end())
        {
            subpieces_[index] = buf;
            access_counter_.reset();

            uint32_t piece_index = index / subpiece_num_per_piece_g_;
            piece_count_[piece_index]++;

            if (IsFull() && !need_write_)
            {
                need_write_ = true;
                node_state_ = BlockNode::MEM;
            }

            return true;
        }

        return false;
    }

    bool BlockNode::LoadSubPieceBuffer(uint32_t index, protocol::SubPieceBuffer buf)
    {
        assert(node_state_ == BlockNode::READING);

        if (subpieces_.find(index) == subpieces_.end())
        {
            subpieces_[index] = buf;
            access_counter_.reset();

            if (subpieces_.size() == total_subpiece_count_)
            {
                node_state_ = BlockNode::ALL;
            }
            return true;
        }
        else
        {
            return false;
        }
    }

    bool BlockNode::HasPiece (const uint32_t piece_index) const
    {
        if (piece_index < piece_count_.size())
        {
            return IsPieceFull(piece_index);
        }
        else
        {
            assert(false);
            return false;
        }
    }

    bool BlockNode::HasSubPiece(const uint32_t index) const
    {
        if (IsBlockSavedOnDisk())
            return true;
        return subpieces_.find(index) != subpieces_.end();
    }

    bool BlockNode::HasSubPieceInMem(const uint32_t index) const
    {
        assert(index <= total_subpiece_count_ - 1);
        return subpieces_.find(index) != subpieces_.end();
    }

    bool BlockNode::SetBlockReading()
    {
        if (node_state_ == BlockNode::DISK)
        {
            node_state_ = BlockNode::READING;
            return true;
        }
        else
        {
            return false;
        }
    }

    protocol::SubPieceBuffer BlockNode::GetSubPiece(uint32_t subpiece_index)
    {
        assert(subpiece_index <= total_subpiece_count_ - 1);
        if (subpieces_.find(subpiece_index) != subpieces_.end())
        {
            protocol::SubPieceBuffer buf = subpieces_[subpiece_index];
            if (buf.IsValid(SUB_PIECE_SIZE))
            {
                access_counter_.reset();
                return buf;
            }
        }

        return protocol::SubPieceBuffer();
    }

    bool BlockNode::GetNextNullSubPiece(const uint32_t start, uint32_t& subpiece_for_download) const
    {
        uint32_t start_piece_index = start / subpiece_num_per_piece_g_;

        for (uint32_t pidx = start_piece_index; pidx < piece_count_.size(); ++pidx)
        {
            if (piece_count_[pidx] == 0)
            {
                if (pidx == start_piece_index)
                    subpiece_for_download = start;
                else
                    subpiece_for_download = pidx * subpiece_num_per_piece_g_;
                return true;
            }

            if (IsPieceFull(pidx))
            {
                continue;
            }
            else
            {
                // start subpiece index in piece pidx@herain
                uint32_t startindex;
                if (start_piece_index == pidx)
                    startindex = start;
                else
                    startindex = pidx * subpiece_num_per_piece_g_;

                for (uint16_t sidx = (uint16_t)startindex; sidx < total_subpiece_count_; ++sidx)
                {
                    if (subpieces_.find(sidx) == subpieces_.end())
                    {
                        subpiece_for_download = sidx;
                        return true;
                    }
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

    void BlockNode::ClearBlockMemCache(uint32_t play_subpiece_index)
    {
        access_counter_.reset();
        uint16_t play_subpiece = play_subpiece_index % subpiece_num_per_piece_g_;
        LOG4CPLUS_DEBUG_LOG(logger_node, "ClearBlockMemCache play_subpiece_index:" << play_subpiece_index);

        for (uint32_t sidx = 0; sidx <= play_subpiece_index && sidx < subpieces_.size(); ++sidx)
        {
            if (subpieces_.find(sidx) == subpieces_.end())
                continue;

            subpieces_.erase(sidx);
            if (!IsBlockSavedOnDisk())
            {
                uint32_t play_piece = sidx / subpiece_num_per_piece_g_;
                piece_count_[play_piece]--;
            }
        }

        if (node_state_ == BlockNode::ALL)
        {
            node_state_ = BlockNode::DISK;
        }
    }

    void BlockNode::OnWriteFinish()
    {
        if (node_state_ == BlockNode::SAVING)
        {
            subpieces_.clear();
            node_state_ = BlockNode::DISK;
        }
    }

    void BlockNode::GetBufferForSave(std::map<protocol::SubPieceInfo, protocol::SubPieceBuffer> & buffer_set)
    {
        if (!need_write_)
        {
            LOG4CPLUS_DEBUG_LOG(logger_node, "need_write_ false! return.");
            return;
        }

        need_write_ = false;
        LOG4CPLUS_DEBUG_LOG(logger_node, "set need_write_ false.");

        switch(node_state_)
        {
        case BlockNode::ALL:
            node_state_ = BlockNode::DISK;
            subpieces_.clear();
            break;
        case BlockNode::MEM:
            node_state_ = SAVING;
            for (uint32_t sidx = 0; sidx < subpieces_.size(); sidx++)
            {
                assert(subpieces_.find(sidx) != subpieces_.end());
                buffer_set.insert(std::make_pair(protocol::SubPieceInfo(index_, sidx), subpieces_[sidx]));
            }
            break;
        default:
            break;
        }
    }

    bool BlockNode::IsSaving() const
    {
        return node_state_ == SAVING;
    }

    bool BlockNode::IsPieceFull(uint32_t piece_index) const
    {
        assert(piece_index < piece_count_.size());
        if (piece_index == piece_count_.size() - 1)
            return piece_count_[piece_index] == last_piece_capacity_;
        else
            return piece_count_[piece_index] == subpiece_num_per_piece_g_;
    }

    std::ostream& operator << (std::ostream& os, const BlockNode& node)
    {
        for (uint32_t pidx = 0; pidx < node.piece_count_.size(); pidx++)
        {
            os << std::setw(6) << pidx << " ";
            if (node.piece_count_[pidx] != 0)
            {
                uint32_t start_postion = pidx * subpiece_num_per_piece_g_;
                uint32_t end_position  = (pidx == node.piece_count_.size() - 1) ? node.last_piece_capacity_ : subpiece_num_per_piece_g_;
                for (uint32_t sidx = start_postion; sidx < end_position; sidx++)
                {
                    if (node.subpieces_.find(sidx) != node.subpieces_.end())
                    {
                        os << "1";
                    }
                    else
                    {
                        os << "0";
                    }
                }
            }

            os << std::endl;
        }
        
        return os;
    }
}
