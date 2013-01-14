//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "GzipDecompresser.h"
#include "HttpConnection.h"
#include "zlib/zlib.h"

namespace p2sp
{
    void GzipDecompresser::Start(boost::shared_ptr<IDecompressListener> handler)
    {
        handler_ = handler;
        is_decompress_complete_ = false;
    }

    void GzipDecompresser::Stop()
    {
        sub_piece_buffer_deque_.clear();
        handler_.reset();
    }

    bool GzipDecompresser::OnRecvData(protocol::SubPieceBuffer const & buffer, 
        boost::uint32_t file_offset, boost::uint32_t content_offset)
    {
        assert(!is_decompress_complete_);

        if (file_offset >= 512*1024)
        {
            return true;
        }

        if (!is_decompress_complete_)
        {
            Decompress(buffer.Data(), buffer.Length());
        }
        
        if (is_decompress_complete_)
        {
            if (handler_)
            {
                handler_->OnDecompressComplete(sub_piece_buffer_deque_);

                Stop();
                
                return true;
            }
        }

        return false;
    }

    void GzipDecompresser::Decompress(const boost::uint8_t *src, boost::uint32_t src_len)
    {
        stream_.avail_in = src_len;
        stream_.next_in = (Bytef *)src;

        do
        {
            boost::shared_ptr<protocol::SubPieceBuffer> buffer;
            if (sub_piece_buffer_deque_.empty() || (*sub_piece_buffer_deque_.rbegin())->Length() == SUB_PIECE_SIZE)
            {
                buffer = boost::shared_ptr<protocol::SubPieceBuffer>(new protocol::SubPieceBuffer(new protocol::SubPieceContent, 0));
                sub_piece_buffer_deque_.push_back(buffer);
            }
            else
            {
                buffer = *sub_piece_buffer_deque_.rbegin();
            }
            
            assert(buffer);
            assert(buffer->Length() <= SUB_PIECE_SIZE);

            stream_.avail_out = SUB_PIECE_SIZE - buffer->Length();
            stream_.next_out = buffer->Data() + buffer->Length();

            boost::uint32_t ret = inflate(&stream_, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR);
            switch (ret)
            {
            case Z_OK:
                buffer->Length(SUB_PIECE_SIZE - stream_.avail_out);
                break;
            case Z_STREAM_END:
                buffer->Length(SUB_PIECE_SIZE - stream_.avail_out);
                inflateEnd(&stream_);
                is_decompress_complete_ = true;
                return;
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                assert(false);
                inflateEnd(&stream_);
                return;
            }
        }
        while (stream_.avail_out == 0);
    }
}