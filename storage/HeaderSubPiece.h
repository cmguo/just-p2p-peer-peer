//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _HEADER_SUBPIECE_H
#define _HEADER_SUBPIECE_H

namespace storage
{
    //HeaderSubPiece表示每个block的第一个subpiece, 那里记录着block的ID/size以及checksum等。
    struct HeaderSubPiece
    {
        //注意:每次往HeaderSubPiece添加/删除成员时要记得更新Constants::PiecesCheckSumOffset
        class Constants
        {
        public:
            //TODO, ericzheng, consolidate these constants with existing ones
            static const int BlockHashSizeInBytes = 16;
            static const int SubPiecesPerPiece = 16;
            static const int SubPieceSizeInBytes = 1400;
            static const int PiecesCheckSumOffset = 4*sizeof(boost::uint32_t) + sizeof(RID) + SubPiecesPerPiece;
            static const int MaximumPiecesPerBlock = (Constants::SubPieceSizeInBytes - PiecesCheckSumOffset)/sizeof(boost::uint32_t);
        };

        bool IsValid() const;

        boost::uint32_t GetBlockId() const { return block_id_; }

        const RID& GetRID() const { return rid_; }

        boost::uint32_t GetDataLength() const { return data_length_; }

        boost::uint32_t GetPieceChecksum(boost::uint16_t piece_index) const 
        { 
            assert(piece_index < Constants::MaximumPiecesPerBlock);
            return pieces_checksum_[piece_index]; 
        }

        static boost::shared_ptr<HeaderSubPiece> Load(char* buffer, size_t buffer_size);

    private:
        HeaderSubPiece();
        bool VerifyBlockHash() const;

    private:
        unsigned char block_hash_[Constants::BlockHashSizeInBytes];
        RID rid_;
        boost::uint32_t  block_header_length_;
        boost::uint32_t  data_length_;
        boost::uint32_t  block_id_;
        boost::uint32_t  version_;
        boost::uint32_t  pieces_checksum_[Constants::MaximumPiecesPerBlock];
    };
}

#endif  //_HEADER_SUBPIECE_H
