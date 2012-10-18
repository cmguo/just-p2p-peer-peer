//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "storage/storage_base.h"
#include "storage/HmacMd5.h"
#include "storage/StorageThread.h"
#include "SubPieceManager.h"
#include "base/util.h"
#include "p2sp/bootstrap/BootStrapGeneralConfig.h"

namespace storage
{
    using base::util::memcpy2;
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_subpiecemanager = log4cplus::Logger::getInstance("[subpiece_manager]");
#endif
    //////////////////////////////////////////////////////////////////////////

    SubPieceManager::p SubPieceManager::Create(uint32_t file_length, bool b_full_file)
    {
        protocol::RidInfo rid_info;
        rid_info.InitByFileLength(file_length);
        return SubPieceManager::p(new SubPieceManager(rid_info, b_full_file));
    }

    SubPieceManager::p SubPieceManager::Create(const protocol::RidInfo& rid_info, bool b_full_file)
    {
        return SubPieceManager::p(new SubPieceManager(rid_info, b_full_file));
    }

    SubPieceManager::SubPieceManager(const protocol::RidInfo& rid_info,
        bool b_full_file)
    {
        rid_info_ = rid_info;
        for (uint32_t i = rid_info_.block_md5_s_.size(); i < rid_info_.GetBlockCount(); i++)
        {
            rid_info_.block_md5_s_.push_back(MD5());
        }
        total_subpiece_count_ = (rid_info_.GetFileLength() + SUBPIECE_SIZE - 1) / SUBPIECE_SIZE;
        last_subpiece_size_ = (rid_info_.GetFileLength() - 1) % SUBPIECE_SIZE + 1;
        last_subpiece_info_.subpiece_index_ = ((rid_info_.GetFileLength() - 1) % rid_info_.GetBlockSize()) / bytes_num_per_subpiece_g_;
        last_subpiece_info_.block_index_ = rid_info_.GetBlockCount() - 1;
        last_block_capacity_ = last_subpiece_info_.subpiece_index_ + 1;

        // blocks
        blocks_.resize(rid_info_.GetBlockCount(), BlockNode::p());
        block_bit_map_ = protocol::BlockMap::Create(rid_info_.GetBlockCount());
        block_bit_map_->SetAll(b_full_file);
        blocks_count_ = b_full_file ? rid_info_.GetBlockCount() : 0;
        download_bytes_ = b_full_file ? rid_info_.GetFileLength() : 0;
    }

    SubPieceManager::~SubPieceManager()
    {
        for (uint32_t i = 0; i < blocks_.size(); ++i) {
            if (blocks_[i])
                blocks_[i].reset();
        }
    }

    bool SubPieceManager::GenerateRid() {
        if (rid_info_.HasRID()) {
            LOG4CPLUS_DEBUG_LOG(logger_subpiecemanager, "false: !rid_info_.GetRID().IsEmpty()" << rid_info_);
            return false;
        }
        if (!IsFullFile()) {
            return false;
        }
        framework::string::Md5 hash;
        for (uint32_t i = 0; i < rid_info_.GetBlockCount(); i++) {
            if (rid_info_.block_md5_s_[i].is_empty()) {
                LOG4CPLUS_DEBUG_LOG(logger_subpiecemanager, 
                    " false: rid_info_.block_md5_s_[i].IsEmpty() " << rid_info_);
                return false;
            }
            hash.update(rid_info_.block_md5_s_[i].to_little_endian_bytes().data(), sizeof(RID));
        }

        hash.final();
        rid_info_.rid_.from_little_endian_bytes(hash.to_bytes());
        LOG4CPLUS_DEBUG_LOG(logger_subpiecemanager, "Resource download finish! RidInfo:" << rid_info_);
        return true;
    }

    bool SubPieceManager::InitRidInfo(const protocol::RidInfo& rid_info) {
        rid_info_ = rid_info;
        return true;
    }

    bool SubPieceManager::AddSubPiece(const protocol::SubPieceInfo& in, const protocol::SubPieceBuffer& buf) {
        if (block_bit_map_->HasBlock(in.block_index_))
        {
            LOG4CPLUS_DEBUG_LOG(logger_subpiecemanager, "Block[" << in.block_index_ << "] is already full!");
            return false;
        }

        if (!buf.IsValid(SUB_PIECE_SIZE))
        {
            LOG4CPLUS_DEBUG_LOG(logger_subpiecemanager, "add " << in << "failed, buffer invalid");
            return false;
        }

        if (!blocks_[in.block_index_]) {
            uint32_t block_cap = last_block_capacity_;
            if (in.block_index_ != blocks_.size()-1) {
                block_cap = rid_info_.GetBlockSize() / bytes_num_per_subpiece_g_;
            }
            blocks_[in.block_index_] = BlockNode::Create(in.block_index_, block_cap);
            ++blocks_count_;
            LOG4CPLUS_DEBUG_LOG(logger_subpiecemanager, "New BlockNode[" << in.block_index_ << "] : block_cap = " 
                << block_cap << ", blocks_count = " <<  blocks_count_);
        }
        BlockNode::p & node = blocks_[in.block_index_];
        if (false == node->IsFull()) {
            node->AddSubpiece(in.subpiece_index_, buf);
            download_bytes_ += buf.Length();
            if (node->IsFull()) {
                block_bit_map_->Set(in.block_index_);
                LOG4CPLUS_DEBUG_LOG(logger_subpiecemanager, "Bolck[" << in.block_index_ << "] Full");
            }
            return true;
        }
        assert(false);
        return false;
    }

    bool SubPieceManager::LoadSubPiece(const protocol::SubPieceInfo& in, protocol::SubPieceContent::pointer con) {
        if (!blocks_[in.block_index_])
        {
            LOG4CPLUS_DEBUG_LOG(logger_subpiecemanager, "SubPiece " << in << " Block is null.");
            return true;
        }

        boost::uint16_t subpiece_size = (in == last_subpiece_info_) ? last_subpiece_size_ : SUB_PIECE_SIZE;
        return blocks_[in.block_index_]->LoadSubPieceBuffer(in.subpiece_index_, protocol::SubPieceBuffer(con, subpiece_size));
    }

    bool SubPieceManager::HasSubPiece(const protocol::SubPieceInfo &in) const {
        if (block_bit_map_->HasBlock(in.block_index_))
            return true;

        const BlockNode::p & node = blocks_[in.block_index_];
        if (node) {
            return node->HasSubPiece(in.subpiece_index_);
        }
        return false;
    }

    bool SubPieceManager::HasSubPieceInMem(const protocol::SubPieceInfo &in) const {
        const BlockNode::p & node = blocks_[in.block_index_];
        if (node) {
            return node->HasSubPieceInMem(in.subpiece_index_);
        }
        return false;
    }

    bool SubPieceManager::IsBlockDataInMemCache(uint32_t block_index) const
    {
        BlockNode::p node = blocks_[block_index];
        //TODO, ericzheng, BlockNode->IsEmpty在DiskMode下并不确切表示内存是否为空
        //(即使内存中的数据都空了，如果磁盘中其实有数据，它也return false，导致IsBlockDataInMemCache返回true)，这里需要再仔细考量。
        return node && false == node->IsEmpty();
    }

    void SubPieceManager::RemoveBlockDataFromMemCache(Resource::p resource_p, uint32_t block_index)
    {
        BlockNode::p& node = blocks_[block_index];

        if (!node)
        {
            return;
        }

        //Note: 在node不完整时不能reset node，因为我们还需要其内部所维护的subpiece bitmap。

#ifdef DISK_MODE
        if (node->NeedWrite() && node->IsFull())
        {
            LOG4CPLUS_DEBUG_LOG(logger_subpiecemanager, "Block " << block_index << " WriteBlockToResource.");
            WriteBlockToResource(resource_p, block_index);
        }
        
        if (node->IsFull())
        {
            assert(block_bit_map_->HasBlock(block_index));
            node.reset();
        }
#else
        node->ClearBlockMemCache(GetBlockSize() / bytes_num_per_subpiece_g_);

        // 非磁盘模式ClearBlockMemCache会造成数据丢失
        if (block_bit_map_->HasBlock(block_index) && !node->IsFull())
        {
            block_bit_map_->Reset(block_index);
        }

        // 如果Block已经为空，删除Block对象
        if (node->IsEmpty())
        {
            node.reset();
        }
#endif
    }

    bool SubPieceManager::SetBlockReading(const uint32_t block_index)
    {
        BlockNode::p & node = blocks_[block_index];
        if (!node) {
            assert(block_bit_map_->HasBlock(block_index));
            if (!block_bit_map_->HasBlock(block_index))
                return false;

            // block_capacity indicating subpiece count in block @herain
            uint32_t block_capacity = 0;
            if (block_index == last_subpiece_info_.block_index_)
                block_capacity = last_subpiece_info_.subpiece_index_ + 1;
            else
                block_capacity = rid_info_.GetBlockSize() / bytes_num_per_subpiece_g_;
            blocks_[block_index] = BlockNode::Create(block_index, block_capacity, true);
        }

        return node->SetBlockReading();
    }

    protocol::SubPieceBuffer SubPieceManager::GetSubPiece(const protocol::SubPieceInfo& in) const {
        const BlockNode::p & node = blocks_[in.block_index_];
        if (node) {
            return node->GetSubPiece(in.subpiece_index_);
        }
        return protocol::SubPieceBuffer();
    }

    bool SubPieceManager::HasPiece(const protocol::PieceInfo& piece_info) const {
        if (block_bit_map_->HasBlock(piece_info.block_index_))
            return true;

        const BlockNode::p & node = blocks_[piece_info.block_index_];
        if (node) {
            return node->HasPiece(piece_info.piece_index_);
        }
        return false;
    }

    bool SubPieceManager::GetNextNullSubPiece(
        const protocol::SubPieceInfo& start_subpiece_info, protocol::SubPieceInfo& subpiece_for_download) const
    {
        for (uint32_t bidx = start_subpiece_info.block_index_;
            bidx < blocks_.size(); ++bidx)
        {
            if (block_bit_map_->HasBlock(bidx))
                continue;

            uint32_t start_sidx = (bidx == start_subpiece_info.block_index_) ?
                start_subpiece_info.subpiece_index_ : 0;

            const BlockNode::p & node = blocks_[bidx];
            if (!node)
            {
                subpiece_for_download.block_index_ = bidx;
                subpiece_for_download.subpiece_index_ = start_sidx;
                return true;
            }
            else if (false == node->IsFull())
            {
                uint32_t sidx_for_download;

                if (node->GetNextNullSubPiece(start_sidx, sidx_for_download))
                {
                    subpiece_for_download.block_index_ = bidx;
                    subpiece_for_download.subpiece_index_ = sidx_for_download;
                    return true;
                }
            }
            else
            {
                assert(false);
            }
        }
        return false;
    }

    void SubPieceManager::RemoveBlockInfo(uint32_t block_index) {
        if (block_bit_map_->HasBlock(block_index))
            block_bit_map_->Reset(block_index);

        BlockNode::p & node = blocks_[block_index];
        if (node) {
            --blocks_count_;
            node.reset();
            return;
        }
        assert(false);
    }

    void SubPieceManager::OnWriteBlockFinish(uint32_t block_index) {
        BlockNode::p & node = blocks_[block_index];
        if (node) {
            node->OnWriteFinish();
            assert(block_bit_map_->HasBlock(block_index));
            // Block已完整存入磁盘，不再需要管理对象 @herain
            node.reset();
            return;
        }
        // assert(false);
    }

#ifdef DISK_MODE
    void SubPieceManager::WriteBlockToResource(Resource::p resource_p, uint32_t block_index)
    {
        LOG4CPLUS_DEBUG_LOG(logger_subpiecemanager, "block " << block_index);
        BlockNode::p & node = blocks_[block_index];
        if (node)
        {
            assert(node->NeedWrite());
            assert(node->IsFull());
            assert(block_bit_map_->HasBlock(block_index));
            // block is full, include data in memory and disk @herain
            std::map<protocol::SubPieceInfo, protocol::SubPieceBuffer> *buffer_set_p = new std::map<protocol::SubPieceInfo, protocol::SubPieceBuffer>();
            node->GetBufferForSave(*buffer_set_p);
            if (buffer_set_p->empty())
            {
                delete buffer_set_p;
                return;
            }
            StorageThread::Inst().Post(
                boost::bind(&Resource::ThreadPendingHashBlock, resource_p, block_index, buffer_set_p));
            LOG4CPLUS_DEBUG_LOG(logger_subpiecemanager, "will post to ThreadPendingHashBlock:" << block_index << 
                " buffer count:" << buffer_set_p->size());
            StorageThread::Inst().Post(boost::bind(&Resource::SecSaveResourceFileInfo, resource_p));
            LOG4CPLUS_DEBUG_LOG(logger_subpiecemanager, "will post to SecSaveResourceFileInfo");
        }
    }
#endif

    void SubPieceManager::SaveAllBlock(Resource::p resource_p)
    {
        for (uint32_t bidx = 0; bidx < blocks_.size(); ++bidx)
        {
            RemoveBlockDataFromMemCache(resource_p, bidx);

            BlockNode::p& node = blocks_[bidx];

            if (node && !node->IsFull())
            {
                node->ClearBlockMemCache(GetBlockSize() / bytes_num_per_subpiece_g_);
            }
        }
    }

    SubPieceManager::p SubPieceManager::Parse(base::AppBuffer cfgfile_buf, const protocol::RidInfo &rid_info)
    {
        uint32_t curr_null_subpiece_count = 0;
        SubPieceManager::p pointer;

        // 假定没有信息的block都是完整的 @herain
        pointer = SubPieceManager::p(new SubPieceManager(rid_info, true));
        pointer->block_bit_map_->SetAll(true);
        byte * buf = cfgfile_buf.Data();
        uint32_t offset = 0;

        while (offset < cfgfile_buf.Length())
        {
            if (offset + 8 > cfgfile_buf.Length())
            {
                assert(false);
                return SubPieceManager::p();
            }
            uint32_t block_index;
            memcpy2(&block_index, sizeof(block_index), buf + offset, sizeof(uint32_t));
            offset += 4;

            if (block_index >= pointer->blocks_.size())
            {
                assert(false);
                return SubPieceManager::p();
            }

            uint32_t blockbuflen;
            memcpy2(&blockbuflen, sizeof(blockbuflen), buf + offset, sizeof(uint32_t));
            offset += 4;

            if (blockbuflen == 0)
            {
                assert(false);
                return SubPieceManager::p();
            }

            if (offset + blockbuflen > cfgfile_buf.Length())
            {
                assert(false);
                return SubPieceManager::p();
            }

            base::AppBuffer blockbuf(buf + offset, blockbuflen);
            offset += blockbuflen;

            BlockNode::p block_pointer;
            if (!BlockNode::Parse(block_index, blockbuf, block_pointer))
            {
                return SubPieceManager::p();
            }

            pointer->block_bit_map_->Reset(block_index);
            pointer->blocks_[block_index] = block_pointer;
            if (block_pointer)
            {
                curr_null_subpiece_count += block_pointer->GetCurrNullSubPieceCount();
            }
            else
            {
                --pointer->blocks_count_;
                if (block_index == pointer->last_subpiece_info_.block_index_)
                    curr_null_subpiece_count += (pointer->last_subpiece_info_.subpiece_index_ + 1);
                else
                    curr_null_subpiece_count += pointer->rid_info_.GetBlockSize()/bytes_num_per_subpiece_g_;
            }
        }
        assert(offset == cfgfile_buf.Length());
        // 计算下载字节数！
        if (!pointer->HasSubPiece(pointer->last_subpiece_info_))
            pointer->download_bytes_ = pointer->rid_info_.GetFileLength() - ((curr_null_subpiece_count-1)*bytes_num_per_subpiece_g_ + pointer->rid_info_.GetFileLength()%bytes_num_per_subpiece_g_);
        else
            pointer->download_bytes_ = pointer->rid_info_.GetFileLength() - curr_null_subpiece_count*bytes_num_per_subpiece_g_;
        return pointer;
    }

    bool SubPieceManager::ToBuffer(base::AppBuffer & resource_desc_buf) const
    {
        uint32_t tmp_buffer_size = 16 * 1024;
        byte *tmpbuf = new byte[tmp_buffer_size];
        uint32_t buflen = 0;

        // 只记录空block、空或半空的piece
        for (uint32_t bidx = 0; bidx < blocks_.size(); ++bidx)
        {
            if (block_bit_map_->HasBlock(bidx))
                continue;

            // index
            memcpy2(tmpbuf + buflen, tmp_buffer_size - buflen, &bidx, sizeof(uint32_t));
            buflen += 4;
            // blockbuf
            base::AppBuffer block_buf;
            if (!blocks_[bidx])
            {
                block_buf = BlockNode::NullBlockToBuffer();
            }
            else
            {
                block_buf = blocks_[bidx]->ToBuffer();
            }

            uint32_t block_buf_len = block_buf.Length();
            memcpy2(tmpbuf + buflen, tmp_buffer_size - buflen, &block_buf_len, sizeof(uint32_t));
            buflen += 4;

            if (tmp_buffer_size < buflen + block_buf_len + 256)
            {
                tmp_buffer_size = tmp_buffer_size*2;
                byte *tmp2 = tmpbuf;
                tmpbuf = new byte[tmp_buffer_size];
                memcpy2(tmpbuf, tmp_buffer_size, tmp2, buflen);
                delete[] tmp2;
            }
            if (block_buf_len != 0)
            {
                memcpy2((tmpbuf + buflen), tmp_buffer_size - buflen, block_buf.Data(), block_buf_len);
                buflen += block_buf_len;
            }
        }
        resource_desc_buf = base::AppBuffer(tmpbuf, buflen);
        delete[] tmpbuf;
        return true;
    }

    bool SubPieceManager::IsFullFile() const
    {
        if (block_bit_map_->IsFull())
        {
            for (boost::uint32_t bidx = 0; bidx < blocks_.size(); ++bidx)
            {
                if (blocks_[bidx] && (blocks_[bidx]->NeedWrite() || blocks_[bidx]->IsSaving()))
                {
                    LOG4CPLUS_INFO_LOG(logger_subpiecemanager, "blocks_ " << bidx << " NeedWrite or IsSaving!");
                    return false;
                }
            }
            return true;
        }
        return false;
    }
}
