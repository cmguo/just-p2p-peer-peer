//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "storage/storage_base.h"
#include "storage/PieceNode.h"
#include "storage/StorageThread.h"
#include "storage/Resource.h"
#include "base/util.h"

namespace storage
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_node = log4cplus::Logger::getInstance("[piece_node]");
#endif
    using protocol::SubPieceBuffer;
    using protocol::SubPieceContent;
    using base::util::memcpy2;

    // normal construct method, no subpiece in piece now @herain
    PieceNode::PieceNode(const protocol::PieceInfo & info, boost::uint16_t subpiece_num, bool is_full_disk)
        : subpiece_set_(subpiece_num), info_(info)
    {
        subpieces_.resize(subpiece_num, SubPieceNode::p());
        if (is_full_disk)
        {
            subpiece_set_.set();
            for (uint16_t sidx = 0; sidx < subpiece_num; ++sidx)
            {
                subpieces_[sidx] = SubPieceNode::p(new SubPieceNode(NDST_DISK));
            }
        }
    }

    // construct from config file @herain
    PieceNode::PieceNode(const protocol::PieceInfo & info, boost::dynamic_bitset<uint32_t> &subpiece_set
        , uint32_t bitcount)
        : info_(info), subpiece_set_(subpiece_set)
    {
        // construct subpieces_ @herain
        subpieces_.resize(bitcount, SubPieceNode::p());
        for (uint16_t sidx = 0; sidx < subpiece_set_.size(); ++sidx)
        {
            if (subpiece_set_.test(sidx))
            {
                subpieces_[sidx] = SubPieceNode::p(new SubPieceNode(NDST_DISK));
            }
        }
    }

    PieceNode::p PieceNode::Create(const protocol::PieceInfo & info, uint32_t subpiece_num, bool is_full_disk)
    {
        return p(new PieceNode(info, subpiece_num, is_full_disk));
    }

    bool PieceNode::Parse(const protocol::PieceInfo & info, base::AppBuffer piece_buf, PieceNode::p & piece_node)
    {
        uint32_t bitcount = 0;
        uint16_t bitbuffersize = 0;
        if (false == CheckBuf(piece_buf, bitcount, bitbuffersize))
        {
            return false;
        }

        if (bitbuffersize == 0)
        {
            piece_node = PieceNode::p();
            return true;
        }

        // construct subpiece_set_ @herain
        boost::dynamic_bitset<uint32_t> subpiece_set;
        for (uint8_t i = 0; i < bitbuffersize / sizeof(uint32_t); ++i)
        {
            uint32_t v;
            memcpy2(&v, sizeof(v), piece_buf.Data() + 4 + i*sizeof(uint32_t), sizeof(uint32_t));
            subpiece_set.append(v);
        }
        subpiece_set.resize(bitcount, true);
        subpiece_set.flip();

        piece_node = PieceNode::p(new PieceNode(info, subpiece_set, bitcount));
        return true;
    }

    bool PieceNode::CheckBuf(base::AppBuffer inbuf, uint32_t &bitcount, uint16_t &bitbuffersize)
    {
        if (inbuf.Length() < 4)
            return false;

        // read bitcount @herain
        memcpy2((void*)(&bitcount), sizeof(bitcount), (void*)inbuf.Data(), sizeof(uint32_t));
        if (bitcount == 0)
        {
            // empty piece, 4 bytes zero placeholder @herain
            assert(inbuf.Length() == 4);
            bitbuffersize = 0;
            return true;
        }
        else
        {
            if (bitcount > subpiece_num_per_piece_g_)
            {
                assert(false);
                return false;
            }

            bitbuffersize = inbuf.Length() - 4;
            if (bitbuffersize % sizeof(uint32_t) != 0)
            {
                assert(false);
                return false;
            }

            return true;
        }
    }

    // save subpieces, which state is  NDST_FULL_DISK, bitmap @herain
    base::AppBuffer PieceNode::ToBuffer()
    {
        // construct subpiece_set in disk @herain
        boost::dynamic_bitset<uint32_t> subpiece_set_disk(subpiece_set_);
        for (uint16_t sidx = 0; sidx < subpieces_.size(); ++sidx)
        {
            if (!subpieces_[sidx] || (subpieces_[sidx]->state_ != NDST_DISK &&
                subpieces_[sidx]->state_ != NDST_READING &&
                subpieces_[sidx]->state_ != NDST_ALL))
            {
                subpiece_set_disk.reset(sidx);
            }
        }

        // buffer content contains subpiece count and blocks in subpiece_set_ @herain
        uint32_t buf_len = sizeof(uint32_t) + subpiece_set_disk.num_blocks() * sizeof(uint32_t);
        base::AppBuffer outbuf(buf_len);
        uint32_t total_subpiece_count = subpiece_set_disk.size();
        memcpy2(outbuf.Data(), outbuf.Length(), &total_subpiece_count, sizeof(uint32_t));  // write subpiece count @herain
        // 为了和1.5的内核Cfg文件格式保持一致
        // 1.5内核中使用0表示存在，1表示不存在。与2.0内核的规则相反，因此这里需要将位图翻转。
        subpiece_set_disk.flip();
        boost::to_block_range(subpiece_set_disk, (uint32_t*)(outbuf.Data() + 4));     // write blocks in subpiece_set_ @herain
        return outbuf;
    }

    bool PieceNode::AddSubPiece(uint32_t index, SubPieceNode::p subpiece_node)
    {
        assert(index<subpiece_set_.size());

        if (!subpiece_node || !subpiece_node->subpiece_.IsValid(SUB_PIECE_SIZE) || subpiece_node->state_ != NDST_MEM)
        {
            assert(false);
            return false;
        }

        if (subpiece_set_.test(index))
        {
            LOG4CPLUS_INFO_LOG(logger_node, "AddSubPieceInfo Again! subpiece[" << info_.block_index_ << "|" 
                << info_.piece_index_ << "|" << index << "]");
            return false;
        }

        if (subpieces_[index])
        {
            LOG4CPLUS_INFO_LOG(logger_node, "SubPieceNode state_ not identical! subpiece[" << info_.block_index_ 
                << "|" << info_.piece_index_ << "|" << index << "]");
            assert(false);
            return false;
        }

        subpiece_set_.set(index);
        subpieces_[index] = subpiece_node;
        LOG4CPLUS_INFO_LOG(logger_node, "AddSubPiece subpiece_index=" << index << ", length=" 
            << subpiece_node->subpiece_.Length() << ", address@" << (void*)subpiece_node->subpiece_.Data());
        return true;
    }

    bool PieceNode::LoadSubPieceBuffer(uint32_t index, protocol::SubPieceBuffer buf)
    {
        assert(index<subpiece_set_.size());

        if (!buf.GetSubPieceBuffer())
        {
            LOG4CPLUS_INFO_LOG(logger_node, "LoadSubPieceBuffer subpieces_[" << info_.block_index_ << "|" 
                << info_.piece_index_ << "|" << index << "] with illegal SubPieceBuffer");
            assert(false);
            return false;
        }

        if (!subpiece_set_.test(index) || !subpieces_[index])
        {
            LOG4CPLUS_INFO_LOG(logger_node, "LoadSubPieceBuffer subpieces_[" << info_.block_index_ << "|" 
                << info_.piece_index_ << "|" << index << "] not exist");
            assert(false);
            return false;
        }
        // assert(subpieces_[index]->state_ == NDST_READING);
        if (subpieces_[index]->state_ == NDST_READING)
        {
            LOG4CPLUS_INFO_LOG(logger_node, "LoadSubPieceBuffer subpieces_[" << info_.block_index_ << "|" 
                << info_.piece_index_ << "|" << index << "] succed, to address@" << (void*)buf.Data());
            subpieces_[index]->subpiece_ = buf;
            subpieces_[index]->state_ = NDST_ALL;
            return true;
        }
        else
        {
            LOG4CPLUS_INFO_LOG(logger_node, "LoadSubPiece subpieces_[" << info_.block_index_ << "|" 
                << info_.piece_index_ << "|" << index << "] Failed. state:" << subpieces_[index]->state_ 
                << " PieceNode:@" << this);
        }
        return false;
    }

    bool PieceNode::HasSubPiece(uint32_t index) const  // 0-127(subpiece_num_per_piece_g_-1)
    {
        if (index >= subpiece_set_.size())
        {
            return false;
        }
        return subpiece_set_.test(index);
    }

    bool PieceNode::HasSubPieceInMem(uint32_t index) const
    {
        assert(index < subpiece_set_.size());
        if (index >= subpiece_set_.size())
        {
            return false;
        }
        if (subpiece_set_.test(index) && subpieces_[index])
        {
            if (subpieces_[index]->state_ == NDST_MEM || subpieces_[index]->state_ == NDST_ALL
                || subpieces_[index]->state_ == NDST_SAVING)
            {
                assert(subpieces_[index]->subpiece_.IsValid(SUB_PIECE_SIZE));
                return subpieces_[index]->subpiece_.IsValid(SUB_PIECE_SIZE);
            }
        }
        return false;
    }

    bool PieceNode::SetSubPieceReading(uint32_t index)
    {
        assert(index < subpiece_set_.size());
        if (index >= subpiece_set_.size())
        {
            assert(false);
            return false;
        }
        if (!subpiece_set_.test(index) || !subpieces_[index])
        {
            assert(false);
            return false;
        }

        if (subpieces_[index]->state_ == NDST_DISK)
        {
            LOG4CPLUS_INFO_LOG(logger_node, "SetSubPieceReading subpieces_[" << info_.block_index_ << "|" 
                << info_.piece_index_ << "|" << index << "] NDST_READING. PieceNode:@" << this);
            subpieces_[index]->state_ = NDST_READING;
            return true;
        }
        else if (subpieces_[index]->state_ == NDST_READING)
        {
            return false;
        }
        else
        {
            // assert(false);
            return false;
        }
    }

    protocol::SubPieceBuffer PieceNode::GetSubPiece(uint32_t subpiece_index) const
    {
        assert(subpiece_index < subpiece_set_.size());
        if (subpiece_index >= subpiece_set_.size())
        {
            return protocol::SubPieceBuffer();
        }

        if (subpiece_set_.test(subpiece_index))
        {
            assert(subpieces_[subpiece_index]);
            if (subpieces_[subpiece_index] && (subpieces_[subpiece_index]->state_ == NDST_MEM ||
                subpieces_[subpiece_index]->state_ == NDST_ALL))
            {
                assert(subpieces_[subpiece_index]->subpiece_.IsValid(SUB_PIECE_SIZE));
                return subpieces_[subpiece_index]->subpiece_;
            }
            else if (subpieces_[subpiece_index] && subpieces_[subpiece_index]->state_ == NDST_SAVING)
            {
                assert(subpieces_[subpiece_index]->subpiece_.IsValid(SUB_PIECE_SIZE));
                // assert(false);
                return subpieces_[subpiece_index]->subpiece_;
            }
        }
        return protocol::SubPieceBuffer();
    }

    bool PieceNode::GetNextNullSubPiece(uint32_t start, uint32_t &subpiece_for_download) const
    {
        assert(start < subpiece_set_.size());
        for (uint16_t sidx = (uint16_t)start; sidx < subpiece_set_.size(); ++sidx)
        {
            if (!subpiece_set_.test(sidx))
            {
                subpiece_for_download = sidx;
                return true;
            }
        }
        return false;
    }

#ifdef DISK_MODE
    void PieceNode::WriteToResource(Resource::p resource_p_)
    {

        for (boost::uint16_t sidx = 0; sidx < subpiece_set_.size(); ++sidx)
        {
            if (!subpiece_set_.test(sidx))
            {
                continue;
            }
            assert(subpieces_[sidx]);
            if (subpieces_[sidx] && subpieces_[sidx]->state_ == NDST_MEM)
            {
                protocol::SubPieceInfo subpiece(info_.block_index_, info_.piece_index_*subpiece_num_per_piece_g_ + sidx);
                StorageThread::Post(boost::bind(&Resource::ThreadSecWriteSubPiece, resource_p_,
                    subpiece, new protocol::SubPieceBuffer(subpieces_[sidx]->subpiece_), true));
                LOG4CPLUS_DEBUG_LOG(logger_node, "will post to ThreadSecWriteSubPiece: subpiece:" << subpiece);
                subpieces_[sidx]->state_ = NDST_SAVING;
            }
        }
    }
#endif

    void PieceNode::ClearBlockMemCache(uint16_t play_subpiece_index)
    {
        uint16_t max_subpiece_index = std::min(play_subpiece_index, (uint16_t)subpieces_.size());
        for (boost::uint16_t sidx = 0; sidx < max_subpiece_index; ++sidx)
        {
            if (!subpiece_set_.test(sidx))
            {
                continue;
            }
            assert(subpieces_[sidx]);
#ifdef DISK_MODE
            if (subpieces_[sidx] && subpieces_[sidx]->state_ == NDST_ALL)
            {
                LOG4CPLUS_INFO_LOG(logger_node, "subpieces_[" << info_.block_index_ << "|" << info_.piece_index_ 
                    << "|" << sidx << "] cleared.");
                subpieces_[sidx]->subpiece_ = protocol::SubPieceBuffer();
                subpieces_[sidx]->state_ = NDST_DISK;
            }
#else
            if (subpieces_[sidx] && subpieces_[sidx]->state_ == NDST_MEM)
            {
                LOG4CPLUS_INFO_LOG(logger_node, "subpieces_[" << info_.block_index_ << "|" << info_.piece_index_ 
                    << "|" << sidx << "] cleared.");
                subpieces_[sidx].reset();
                subpiece_set_.reset(sidx);
            }
#endif
        }
    }

    void PieceNode::OnWriteFinish()
    {
        for (boost::uint16_t sidx = 0; sidx < subpiece_set_.size(); ++sidx)
        {
            if (!subpiece_set_.test(sidx) || !subpieces_[sidx])
            {
                assert(false);
                continue;
            }
            if (subpieces_[sidx]->state_ == NDST_SAVING)
            {
                subpieces_[sidx]->subpiece_ = protocol::SubPieceBuffer();
                subpieces_[sidx]->state_ = NDST_DISK;
            }
        }
    }

    void PieceNode::OnWriteSubPieceFinish(uint32_t subpiece_index)
    {
        if (subpieces_[subpiece_index] && subpieces_[subpiece_index]->state_ == NDST_SAVING)
        {
            subpieces_[subpiece_index]->subpiece_ = protocol::SubPieceBuffer();
            subpieces_[subpiece_index]->state_ = NDST_DISK;
            return;
        }
        assert(false);
    }

    void PieceNode::GetBufferForSave(std::map<protocol::SubPieceInfo, protocol::SubPieceBuffer> & buffer_set) const
    {
        for (boost::uint16_t sidx = 0; sidx < subpiece_set_.size(); ++sidx)
        {
            if (!subpiece_set_.test(sidx))
            {
                continue;
            }
            assert(subpieces_[sidx]);
            if (subpieces_[sidx])
            {
                switch (subpieces_[sidx]->state_)
                {
                case NDST_MEM:
                    assert(subpieces_[sidx]->subpiece_.IsValid(SUB_PIECE_SIZE));
                    buffer_set.insert(std::make_pair(protocol::SubPieceInfo(info_.block_index_, info_.piece_index_*subpiece_num_per_piece_g_ + sidx), subpieces_[sidx]->subpiece_));
                    subpieces_[sidx]->state_ = NDST_SAVING;
                    break;
                case NDST_ALL:
                    subpieces_[sidx]->subpiece_ = protocol::SubPieceBuffer();
                    subpieces_[sidx]->state_ = NDST_DISK;
                    break;
                default:
                    break;
                }
            }
        }
    }

    bool PieceNode::IsSaving() const
    {
        for (boost::uint16_t sidx = 0; sidx < subpiece_set_.size(); ++sidx)
        {
            if (subpieces_[sidx] && subpieces_[sidx]->state_ == NDST_SAVING)
                return true;
        }
        return false;
    }

    std::ostream& operator << (std::ostream& os, const PieceNode& info)
    {
        string subpieces;
        boost::to_string(info.subpiece_set_, subpieces);
        return os << subpieces;
    }
}
