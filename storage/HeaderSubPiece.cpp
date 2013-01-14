//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "HeaderSubPiece.h"
#include "framework/string/Md5.h"

namespace storage
{
    HeaderSubPiece::HeaderSubPiece()
    {
        block_header_length_ = 0;
        data_length_ = 0;
        block_id_ = 0;
        version_ = 0;
        memset(block_hash_, 0, sizeof(block_hash_));
        memset(pieces_checksum_, 0, sizeof(pieces_checksum_));
    }

    bool HeaderSubPiece::IsValid() const
    {
        if (block_header_length_ < Constants::PiecesCheckSumOffset)
        {
            return false;
        }

        if (block_header_length_ > Constants::SubPieceSizeInBytes)
        {
            return false;
        }

        if (data_length_ < block_header_length_)
        {
            return false;
        }

        if (data_length_ > Constants::MaximumPiecesPerBlock*Constants::SubPiecesPerPiece*Constants::SubPieceSizeInBytes)
        {
            return false;
        }

        return VerifyBlockHash();
    }

    bool HeaderSubPiece::VerifyBlockHash() const
    {
        assert(block_header_length_ > Constants::BlockHashSizeInBytes);

        framework::string::Md5 message_digest;
        const boost::uint8_t* buffer_header = reinterpret_cast<const boost::uint8_t *>(this) + Constants::BlockHashSizeInBytes;
        message_digest.update(buffer_header, block_header_length_ - Constants::BlockHashSizeInBytes);
        message_digest.final();

        framework::string::Md5::bytes_type actual_message_digest_bytes = message_digest.to_bytes();
        assert(actual_message_digest_bytes.size() == Constants::BlockHashSizeInBytes);
        return 0 == memcmp(actual_message_digest_bytes.data(), this->block_hash_, Constants::BlockHashSizeInBytes);
    }

    boost::shared_ptr<HeaderSubPiece> HeaderSubPiece::Load(char* buffer, size_t buffer_size)
    {
        assert(buffer);
        assert(buffer_size == HeaderSubPiece::Constants::SubPieceSizeInBytes);

        boost::shared_ptr<HeaderSubPiece> header_subpiece(new HeaderSubPiece());

        util::archive::ArchiveBuffer<> archive_buffer(buffer, buffer_size, buffer_size);
        util::archive::LittleEndianBinaryIArchive<> little_endian_archive(archive_buffer);
        
        for(size_t i= 0; i < sizeof(header_subpiece->block_hash_); ++i)
        {
            little_endian_archive >> header_subpiece->block_hash_[i]; 
        }

        little_endian_archive & header_subpiece->rid_;

        little_endian_archive >> header_subpiece->block_header_length_;
        little_endian_archive >> header_subpiece->data_length_;
        little_endian_archive >> header_subpiece->block_id_;
        little_endian_archive >> header_subpiece->version_;

        for(size_t i = 0; i < Constants::MaximumPiecesPerBlock; ++i)
        {
            little_endian_archive >> header_subpiece->pieces_checksum_[i];
        }

        return header_subpiece;
    }
}
