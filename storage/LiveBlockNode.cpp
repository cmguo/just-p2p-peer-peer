//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "storage_base.h"
#include "LiveBlockNode.h"
#include "protocol/CheckSum.h"

namespace storage
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("livestorage");

    bool LiveBlockNode::AddSubPiece(uint16_t subpiece_index, const protocol::LiveSubPieceBuffer& subpiece_buffer)
    {
        if (subpiece_index < subpieces_.size() && subpieces_[subpiece_index])
        {
            //不重复插入数据，但如果数据校验失败，会丢掉该数据(header subpiece 或 一个piece)
            STORAGE_DEBUG_LOG("SubPiece ("<< block_id_ << "|"<< subpiece_index << ") already exist!");
            return false;
        }

        if (subpiece_index >= subpieces_.size())
        {
            assert(!HasHeaderSubPiece());
            if (HasHeaderSubPiece())
            {
                STORAGE_DEBUG_LOG("AddSubPiece ("<< block_id_ << ", "<< subpiece_index << ") - subpiece_index is out of range(size = "<<subpieces_.size()<<")");
                return false;
            }

            subpieces_.resize(subpiece_index + 1);
            STORAGE_DEBUG_LOG("resize subpieces_ size to " << subpiece_index+1);
        }

        subpieces_[subpiece_index] = subpiece_buffer;

        STORAGE_DEBUG_LOG("Add SubPiece ("<< block_id_ << "|"<< subpiece_index << ")  succeed.");

        if (subpiece_index == 0)
        {
            CheckHeadSubPiece();
        }

        DropDataIfNecessary(subpiece_index);

#if ((defined _DEBUG || defined DEBUG) && (defined CHECK_DOWNLOADED_FILE))

        SetCurrentDirectory("W:\\");

        char file_name[20];
        unsigned char buf[1400];

        itoa(block_id_, file_name, 10);
        strcat(file_name, ".block");

        FILE *fp_;
        while(true)
        {
            bool ok = true;

            fp_=fopen(file_name,"rb");

            if (fp_ == NULL)
            {
                ok = false;
            }

            if (ok && fseek(fp_, subpiece_index*1400, SEEK_SET) != 0)
            {
                ok = false;
            }

            if (ok && fread(buf, subpieces_[subpiece_index].Length(), 1, fp_) != 1)
            {
                ok = false;
            }

            if (ok)
            {
                fclose(fp_);
                break; 
            }
            Sleep(100);
        }

        bool same = true;
        for (uint32_t i=0; i<subpieces_[subpiece_index].Length(); ++i)
        {
            unsigned char c = *(subpieces_[subpiece_index].GetSubPieceBuffer()->get_buffer() + i);

            if (buf[i] != c)
            {
                same = false;
                assert(false);
                break;
            }
        }

        IsPieceValid((subpiece_index-1)/16);

#endif

        return true;
    }

    bool LiveBlockNode::HasSubPiece(uint16_t index) const
    {
        if (index >= subpieces_.size())
        {
            return false;
        }

        return subpieces_[index];
    }

    void LiveBlockNode::GetBlock(std::vector<protocol::LiveSubPieceBuffer>& subpiece_buffers) const
    {
        assert(IsComplete());

        subpiece_buffers.clear();

        if (IsComplete())
        {
            for (uint16_t i = 0; i < subpieces_.size(); ++i)
            {
                subpiece_buffers.push_back(subpieces_[i]);
            }
        }
    }

    void LiveBlockNode::GetBuffer(uint16_t index, protocol::LiveSubPieceBuffer& subpiece_buffer) const
    {
        assert(index < subpieces_.size());
        subpiece_buffer = subpieces_[index];
    }

    uint16_t LiveBlockNode::GetNextMissingSubPiece() const
    {
        assert(!IsComplete());
        
        uint16_t index;
        for (index = 0; index < subpieces_.size(); ++index)
        {
            if (!subpieces_[index])
            {
                break;
            }
        }
        return index;
    }

    void LiveBlockNode::DropSubPiece(uint16_t subpiece_index)
    {
        if (subpiece_index < subpieces_.size())
        {
            subpieces_[subpiece_index] = protocol::LiveSubPieceBuffer();
        }
    }

    void LiveBlockNode::CheckHeadSubPiece()
    {
        assert(!header_subpiece_);

        assert(subpieces_[0]);
        assert(subpieces_[0].Length() == HeaderSubPiece::Constants::SubPieceSizeInBytes);
        assert(sizeof(HeaderSubPiece) <= HeaderSubPiece::Constants::SubPieceSizeInBytes);

        if (!subpieces_[0] || 
            subpieces_[0].Length() != HeaderSubPiece::Constants::SubPieceSizeInBytes)
        {
            DropSubPiece(0);
            return;
        }

        header_subpiece_ = HeaderSubPiece::Load(reinterpret_cast<char*>(subpieces_[0].Data()), subpieces_[0].Length());

        if (!IsHeaderValid())
        {
            header_subpiece_.reset();
            DropSubPiece(0);
            return;
        }

        // 获得piece length之后计算出subpiece的真实数目
        uint16_t real_subpieces_count = static_cast<uint16_t>( 1 + (header_subpiece_->GetDataLength() + LIVE_SUB_PIECE_SIZE - 1) / LIVE_SUB_PIECE_SIZE);
        STORAGE_DEBUG_LOG("data_length_=" << header_subpiece_->GetDataLength() << ", real_subpieces_count=" << real_subpieces_count);

        assert(subpieces_.size() <= real_subpieces_count);
        subpieces_.resize(real_subpieces_count);
    }

    void LiveBlockNode::DropDataIfNecessary(uint16_t new_arrival_subpiece_index)
    {
        if (new_arrival_subpiece_index == 0)
        {
            //当刚下好header subpiece时，得检查看在此之前已经下好的pieces中是否有校验有误的。
            if (HasHeaderSubPiece() && IsHeaderValid())
            {
                uint16_t pieces_count = GetPieceIndex(GetSubPiecesCount() -1) + 1;
                for(uint16_t piece_index = 0; piece_index < pieces_count; ++piece_index)
                {
                    if (IsPieceComplete(piece_index) && !IsPieceValid(piece_index))
                    {
                        DropPiece(piece_index);
                    }
                }
            }
        }
        else
        {
            if (HasHeaderSubPiece())
            {
                //一旦新下的subpiece使得其所在piece变完整，进行数据校验，如有需要则丢掉整个piece。
                uint16_t piece_index = GetPieceIndex(new_arrival_subpiece_index);
                if (IsPieceComplete(piece_index) && !IsPieceValid(piece_index))
                {
                    DropPiece(piece_index);
                }
            }
        }
    }

    void LiveBlockNode::DropPiece(uint16_t piece_index)
    {
        if (piece_index >= HeaderSubPiece::Constants::MaximumPiecesPerBlock)
        {
            assert(false);
            return;
        }

        uint32_t first_subpiece_index = LiveBlockNode::GetFirstSubPieceIndex(piece_index);
        if (first_subpiece_index >= subpieces_.size())
        {
            //there's no point to drop a piece if it's not even in existence.
            assert(false);
            return;
        }

        for(uint16_t subpiece_index = static_cast<uint16_t>(first_subpiece_index);
            subpiece_index < first_subpiece_index + HeaderSubPiece::Constants::SubPiecesPerPiece;
            ++subpiece_index)
        {
            if (subpiece_index >= subpieces_.size())
            {
                break;
            }

            DropSubPiece(subpiece_index);
        }
    }

    bool LiveBlockNode::IsPieceComplete(uint16_t piece_index) const
    {
        //check the hard limit
        if (piece_index >= HeaderSubPiece::Constants::MaximumPiecesPerBlock)
        {
            assert(false);
            return false;
        }

        //subpiece[0] is excluded, as it does not belong to any piece.
        uint32_t first_subpiece_index = LiveBlockNode::GetFirstSubPieceIndex(piece_index);
        if (first_subpiece_index >= subpieces_.size())
        {
            return false;
        }

        //the actual last_subpiece_index for this piece *MIGHT* be smaller than the below value, 
        //in case the piece is the last piece of the block
        uint32_t last_subpiece_index = first_subpiece_index + HeaderSubPiece::Constants::SubPiecesPerPiece - 1;
        if (last_subpiece_index >= subpieces_.size())
        {
            if (false == HasHeaderSubPiece())
            {
                //without the header, we can't tell if the subpiece size is reliable or not. 
                //Assume the specified piece is incomplete.
                return false;
            }
            
            last_subpiece_index = subpieces_.size() - 1;
        }
        
        for(uint32_t subpiece_index = first_subpiece_index; 
            subpiece_index <= last_subpiece_index; 
            ++subpiece_index)
        {
            if (!subpieces_[subpiece_index])
            {
                return false;
            }
        }

        return true;
    }

    bool LiveBlockNode::IsHeaderValid() const 
    {
        if (!block_header_validated_)
        {
            block_header_validated_ = HasHeaderSubPiece() && 
                                      header_subpiece_->IsValid() &&
                                      block_id_ == header_subpiece_->GetBlockId() &&
                                      rid_ == header_subpiece_->GetRID();
        }

        return block_header_validated_;
    }

    bool LiveBlockNode::IsPieceValid(uint16_t piece_index) const
    {
        if (piece_index >= HeaderSubPiece::Constants::MaximumPiecesPerBlock)
        {
            assert(false);
            return false;
        }

        if (validated_pieces_.find(piece_index) != validated_pieces_.end())
        {
            return true;
        }

        if (false == IsHeaderValid())
        {
            return false;
        }

        if (false == IsPieceComplete(piece_index))
        {
            return false;
        }

        uint32_t first_subpiece_index = LiveBlockNode::GetFirstSubPieceIndex(piece_index);
        assert(first_subpiece_index < subpieces_.size());

        uint32_t last_subpiece_index = first_subpiece_index + HeaderSubPiece::Constants::SubPiecesPerPiece - 1;
        if (last_subpiece_index >= subpieces_.size())
        {
            last_subpiece_index = subpieces_.size() - 1;
        }

        assert(last_subpiece_index >= first_subpiece_index);

        //memcpy的原因 - SubPieceContent::buffer_不是4字节对齐，不符合checksum算法的要求(checksum为了在box中也有好的性能而有此要求)。
        uint8_t buffer[HeaderSubPiece::Constants::SubPiecesPerPiece*HeaderSubPiece::Constants::SubPieceSizeInBytes];
        uint8_t* buffer_write_pos = buffer;

        for(uint32_t subpiece_index = first_subpiece_index;
            subpiece_index <= last_subpiece_index;
            ++subpiece_index)
        {
            assert(subpieces_[subpiece_index].Length() <= HeaderSubPiece::Constants::SubPieceSizeInBytes);

            base::util::memcpy2(
                buffer_write_pos, 
                HeaderSubPiece::Constants::SubPieceSizeInBytes, 
                subpieces_[subpiece_index].Data(), 
                subpieces_[subpiece_index].Length());

            buffer_write_pos += subpieces_[subpiece_index].Length();
        }

        uint32_t expected_checksum = header_subpiece_->GetPieceChecksum(piece_index);
        uint32_t actual_checksum = check_sum_new(boost::asio::const_buffers_1(buffer, buffer_write_pos - buffer));

        if (expected_checksum == actual_checksum)
        {
            validated_pieces_.insert(piece_index);
            return true;
        }
        ++checksum_failed_times_;
        STORAGE_DEBUG_LOG("Checksum Failed!!! block id = " << block_id_ << ", piece index = " << piece_index);
        return false;
    }

    boost::uint32_t LiveBlockNode::GetChecksumFailedTimes() const
    {
        return checksum_failed_times_;
    }

    bool LiveBlockNode::IsComplete() const 
    {
        if (!HasHeaderSubPiece())
        {
            return false;
        }

        for(size_t subpiece_index = 0; subpiece_index < subpieces_.size(); ++subpiece_index)
        {
            if (!subpieces_[subpiece_index])
            {
                return false;
            }
        }

        return true;
    }

    bool LiveBlockNode::IsEmpty() const 
    { 
        for(size_t subpiece_index = 0; subpiece_index < subpieces_.size(); ++subpiece_index)
        {
            if (subpieces_[subpiece_index])
            {
                return false;
            }
        }

        return true;
    }

    boost::uint32_t LiveBlockNode::GetMissingSubPieceCount() const
    {
        boost::uint32_t missing_subpiece_count = 0;

        for (size_t i = 0; i < subpieces_.size(); ++i)
        {
            if (!subpieces_[i])
            {
                ++missing_subpiece_count;
            }
        }

        return missing_subpiece_count;
    }

    boost::uint32_t LiveBlockNode::GetExistSubPieceCount() const
    {
        boost::uint32_t exist_subpiece_count = 0;

        for (size_t i = 0; i < subpieces_.size(); ++i)
        {
            if (subpieces_[i])
            {
                ++exist_subpiece_count;
            }
        }

        return exist_subpiece_count;
    }
}
