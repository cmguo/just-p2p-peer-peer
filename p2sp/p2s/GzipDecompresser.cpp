//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "GzipDecompresser.h"
#include "HttpConnection.h"
#include "zlib/zlib.h"

namespace p2sp
{
#ifdef BOOST_WINDOWS_API
#pragma optimize("", off)
#endif

    void GzipDecompresser::Start(boost::shared_ptr<IDecompressListener> handler)
    {
        handler_ = handler;
        is_header_decompress_ = false;
    }

    bool GzipDecompresser::OnRecvData(protocol::SubPieceBuffer const & buffer, 
        uint32_t file_offset, uint32_t content_offset)
    {
        assert(!is_header_decompress_);

        if (file_offset >= 256*1024)
        {
            return true;
        }

        if (!is_header_decompress_)
        {
            Decompress(buffer.Data(), buffer.Length());
        }
        
        if (is_header_decompress_)
        {
            handler_->OnDecompressComplete(sub_piece_buffer_deque_);
            return true;
        }

        return false;
    }

    bool GzipDecompresser::Decompress(const boost::uint8_t *src, boost::uint32_t src_len)
    {
        stream_.avail_in = src_len;
        stream_.next_in = (Bytef *)src;

        do
        {
            boost::shared_ptr<protocol::SubPieceBuffer> buffer;
            if (sub_piece_buffer_deque_.empty() || (*sub_piece_buffer_deque_.rbegin())->Length() == SUB_PIECE_SIZE)
            {
                buffer = boost::shared_ptr<protocol::SubPieceBuffer>(new protocol::SubPieceBuffer(new protocol::SubPieceContent, SUB_PIECE_SIZE));
                sub_piece_buffer_deque_.push_back(buffer);
            }
            else
            {
                buffer = *sub_piece_buffer_deque_.rbegin();
            }
            
            assert(buffer);
            assert(buffer->Length() <= SUB_PIECE_SIZE);

            boost::uint32_t buffer_size = 0;
            if (buffer->Length() != SUB_PIECE_SIZE)
            {
                buffer_size = buffer->Length();
                buffer->Length(SUB_PIECE_SIZE);
            }

            stream_.avail_out = buffer->Length() - buffer_size;
            stream_.next_out = buffer->Data() + buffer_size;

            boost::uint32_t ret = inflate(&stream_, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR);
            switch (ret)
            {
            case Z_OK:
                buffer->Length(buffer->Length() - stream_.avail_out);
                break;
            case Z_STREAM_END:
                inflateEnd(&stream_);
                is_header_decompress_ = true;
                return true;
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                assert(false);
                inflateEnd(&stream_);
                return false;
            }
        }
        while (stream_.avail_out == 0);

        return false;
    }
#ifdef BOOST_WINDOWS_API
#pragma optimize("", on)
#endif
}