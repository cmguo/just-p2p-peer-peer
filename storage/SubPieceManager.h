//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef SUBPEICE_MANAGER_H
#define SUBPEICE_MANAGER_H

#include "storage/Resource.h"
#include "storage/PieceNode.h"
#include "storage/BlockNode.h"


namespace storage
{
    class SubPieceManager
#ifdef DUMP_OBJECT
        : public count_object_allocate<SubPieceManager>
#endif
    {
    public:
        typedef boost::shared_ptr<SubPieceManager> p;
        static const uint32_t SUBPIECE_SIZE = 1024;
        static p Create(uint32_t file_length, bool b_full_file);
        static p Create(const protocol::RidInfo& rid_info, bool b_full_file);
        static p Parse(base::AppBuffer cfgfile_buf, const protocol::RidInfo& ridinfo);
        bool ToBuffer(base::AppBuffer & resource_desc_buf) const;

        SubPieceManager(const protocol::RidInfo& rid_info, bool b_full_file);
        ~SubPieceManager();

        // RIDInfo related function @herain
        bool GenerateRid();
        bool InitRidInfo(const protocol::RidInfo& rid_info);
        protocol::RidInfo& GetRidInfo() {return rid_info_;}
        bool HasRID() const {return rid_info_.HasRID();}
        RID GetRID() const {return rid_info_.GetRID();}
        uint32_t GetBlockCount() const {return rid_info_.GetBlockCount();}
        uint32_t GetBlockSize() const {return rid_info_.GetBlockSize();}
        uint32_t GetFileLength() const {return rid_info_.GetFileLength();}
        bool SetBlockHash(uint32_t block_index, MD5 hash_md5);

        bool IsBlockDataInMemCache(uint32_t block_index) const;
        void RemoveBlockDataFromMemCache(Resource::p resource_p, uint32_t block_index);

        bool AddSubPiece(const protocol::SubPieceInfo &in, const protocol::SubPieceBuffer& buffer);
        bool LoadSubPiece(const protocol::SubPieceInfo &in, protocol::SubPieceContent::pointer con);
        bool HasSubPiece(const protocol::SubPieceInfo &in) const;
        bool HasSubPieceInMem(const protocol::SubPieceInfo &in) const;
        bool HasPiece(const protocol::PieceInfo& piece_info) const;
        protocol::SubPieceBuffer GetSubPiece(const protocol::SubPieceInfo& in) const;
        bool SetSubPieceReading(const protocol::SubPieceInfo &in);
        bool IsSubPieceValid(const protocol::SubPieceInfo &in) const { return in <= last_subpiece_info_;}
        protocol::SubPieceInfo GetMaxSubPieceInfo() {return last_subpiece_info_;}

        protocol::BlockMap::p GetBlockMap() const {return block_bit_map_;}
        uint32_t GetDownloadedBlockCount() const {return block_bit_map_->GetCount();}
        bool HasFullBlock(uint32_t block_index) const {
            return block_bit_map_->HasBlock(block_index);
        }
        void RemoveBlockInfo(uint32_t block_index);
        void OnWriteBlockFinish(uint32_t block_index);
        void OnWriteSubPieceFinish(protocol::SubPieceInfo &subpiece_info);

        bool GetNextNullSubPiece(const protocol::SubPieceInfo& subpiece_info, protocol::SubPieceInfo& subpiece_for_download) const;
#ifdef DISK_MODE
        void WriteBlockToResource(Resource::p resource_p, uint32_t block_index);
#endif
        void SaveAllBlock(Resource::p resource_p);

        uint32_t GetDownloadBytes() {
            return download_bytes_;
        }

        bool IsFullFile() const;
        bool IsFull() const {return block_bit_map_->IsFull();}

        bool IsEmpty() const {
            return blocks_count_ == 0;
        }

        bool DownloadingBlock(uint32_t block_id) const 
        {
            if(block_id < blocks_.size() && blocks_[block_id])
            {
                return blocks_[block_id]->NeedWrite() && 
                    false == blocks_[block_id]->IsFull() &&
                    false == blocks_[block_id]->IsAccessTimeout();
            }

            return false;
        }

    public:

        bool PosToSubPieceInfo(uint32_t position, protocol::SubPieceInfo&subpiec_info)
        {
            if (position >= rid_info_.GetFileLength())
            {
                return false;
            }
            subpiec_info.block_index_ = position / rid_info_.GetBlockSize();
            subpiec_info.subpiece_index_ = position % rid_info_.GetBlockSize() / bytes_num_per_subpiece_g_;
            return true;
        }

        bool PosToPieceInfo(uint32_t position, protocol::PieceInfo &piece_info)
        {
            if (position >= rid_info_.GetFileLength())
            {
                return false;
            }
            piece_info.block_index_ = position / rid_info_.GetBlockSize();
            piece_info.piece_index_ = position % rid_info_.GetBlockSize() / bytes_num_per_piece_g_;
            return true;
        }

        bool PosToPieceInfoEx(uint32_t position, protocol::PieceInfoEx &piece_info)
        {
            if (position >= rid_info_.GetFileLength())
            {
                return false;
            }
            piece_info.block_index_ = position / rid_info_.GetBlockSize();
            piece_info.piece_index_ = position % rid_info_.GetBlockSize() / bytes_num_per_piece_g_;
            piece_info.subpiece_index_ = position % rid_info_.GetBlockSize() % bytes_num_per_piece_g_ / bytes_num_per_subpiece_g_;
            return true;
        }

        void SubPieceInfoToPieceInfoEx(const protocol::SubPieceInfo &subpiece_info, protocol::PieceInfoEx &piece_info_ex)
        {
            piece_info_ex.block_index_ = subpiece_info.block_index_;
            piece_info_ex.piece_index_ = subpiece_info.subpiece_index_ / subpiece_num_per_piece_g_;
            piece_info_ex.subpiece_index_ = subpiece_info.subpiece_index_ % subpiece_num_per_piece_g_;
        }

        void PieceInfoToSubPieceInfo(const protocol::PieceInfo &piece_info, protocol::SubPieceInfo &subpiece_info)
        {
            subpiece_info.block_index_ = piece_info.block_index_;
            subpiece_info.subpiece_index_ = piece_info.piece_index_ * subpiece_num_per_piece_g_;
        }

        void PieceInfoExToSubPieceInfo(const protocol::PieceInfoEx &piece_info, protocol::SubPieceInfo &subpiece_info)
        {
            subpiece_info.block_index_ = piece_info.block_index_;
            subpiece_info.subpiece_index_ = piece_info.piece_index_ * subpiece_num_per_piece_g_ + piece_info.subpiece_index_;
        }

        uint32_t SubPieceInfoToPosition(const protocol::SubPieceInfo &subpiec_info)
        {
            return subpiec_info.block_index_ * rid_info_.GetBlockSize() + subpiec_info.subpiece_index_
                * bytes_num_per_subpiece_g_;
        }

        int GetBlockSubPieceCount(uint32_t block_index)
        {
            if (block_index == last_subpiece_info_.block_index_)
            {
                return last_subpiece_info_.subpiece_index_ + 1;
            }
            return rid_info_.GetBlockSize() / bytes_num_per_subpiece_g_;
        }

        void GetBlockPosition(uint32_t block_index, uint32_t &offset, uint32_t &length)
        {
            assert(block_index <= last_subpiece_info_.block_index_);
            offset = block_index * rid_info_.GetBlockSize();
            if (block_index == rid_info_.GetFileLength() / rid_info_.GetBlockSize())
            {
                length = rid_info_.GetFileLength() % rid_info_.GetBlockSize();
            }
            else
            {
                length = rid_info_.GetBlockSize();
            }
            return;
        }

        void GetSubPiecePosition(const protocol::SubPieceInfo &subpiec_info, uint32_t &offset, uint32_t &length)
        {
            assert(subpiec_info <= last_subpiece_info_);
            if (subpiec_info == last_subpiece_info_)
            {
                length = (rid_info_.GetFileLength() - 1) % bytes_num_per_subpiece_g_ + 1;
            }
            else
            {
                length = bytes_num_per_subpiece_g_;
            }
            offset = subpiec_info.block_index_ * rid_info_.GetBlockSize() + subpiec_info.subpiece_index_
                * bytes_num_per_subpiece_g_;
            assert(offset + length <= rid_info_.GetFileLength());
        }

        bool IncSubPieceInfo(protocol::SubPieceInfo &subpiec_info)
        {
            if (subpiec_info.subpiece_index_ == rid_info_.GetBlockSize() / bytes_num_per_subpiece_g_ - 1)
            {
                subpiec_info.subpiece_index_ = 0;
                subpiec_info.block_index_++;
            }
            else
            {
                subpiec_info.subpiece_index_++;
            }
            if (SubPieceInfoToPosition(subpiec_info) >= rid_info_.GetFileLength())
            {
                return false;
            }
             return true;
        }


    private:
        protocol::RidInfo rid_info_;
        uint32_t total_subpiece_count_;      // 总的subpiece
        uint32_t last_block_capacity_;       // �最后一个block的容
        protocol::SubPieceInfo last_subpiece_info_;    // �最后一片subpiece的位
        uint32_t last_subpiece_size_;        // �最后一片subpiece的容

        std::vector<BlockNode::p> blocks_;
        protocol::BlockMap::p block_bit_map_;
        uint32_t blocks_count_;              // �非空的block
        uint32_t download_bytes_;            // �已下载字节数
    };
}

#endif