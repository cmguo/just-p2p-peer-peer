//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "statistic/GZipCompressor.h"
#include "zlib/zutil.h"

namespace statistic
{
    const int GZipCompressor::OutputBufferSize = 4096;

    GZipCompressor::GZipCompressor()
        : output_buffer_(0)
    {
    }

    GZipCompressor::~GZipCompressor()
    {
        delete[] output_buffer_;
    }

    bool GZipCompressor::Compress(uint8_t* raw_data, int raw_data_size, std::ostream& result)
    {
        std::vector<RawData> buffers;
        buffers.push_back(RawData(raw_data, raw_data_size));

        return Compress(buffers, result);
    }

    bool GZipCompressor::InitializeForCompression()
    {
        if (!output_buffer_)
        {
            output_buffer_ = new uint8_t[GZipCompressor::OutputBufferSize];
        }

        compression_stream_.zalloc = (alloc_func)0;
        compression_stream_.zfree = (free_func)0;
        compression_stream_.opaque = (voidpf)0;

        raw_data_crc_ = crc32(0L, Z_NULL, 0);

        int err = deflateInit2(&compression_stream_, Z_DEFAULT_COMPRESSION,Z_DEFLATED, -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY);
        return (err == Z_OK);
    }

    void GZipCompressor::AppendValue(unsigned long value, std::ostream& result)
    {
        for(int n = 0; n < sizeof(value); n++)
        {
            uint8_t byte_value = static_cast<uint8_t>(value & 0xFF);
            result.write(reinterpret_cast<char*>(&byte_value),sizeof(byte_value));
            value >>= 8;
        }
    }

    bool GZipCompressor::Compress(const std::vector<RawData>& buffers, std::ostream& result)
    {
        if (!InitializeForCompression())
        {
            return false;
        }

        //头部
        char header[10]={0x1f,0x8b,Z_DEFLATED, 0 /*flags*/, 0,0,0,0 /*time*/, 0 /*xflags*/, OS_CODE};

        result.write(header,10);
        result.flush();

        //数据
        compression_stream_.avail_in = 0;
        for(size_t buffer_index = 0; buffer_index < buffers.size();)
        {
            //内部待压缩数据缓冲已空?
            if (compression_stream_.avail_in==0) 
            {
                compression_stream_.next_in = buffers[buffer_index].buffer;
                compression_stream_.avail_in = buffers[buffer_index].buffer_size;
                raw_data_crc_ = crc32(raw_data_crc_, buffers[buffer_index].buffer, buffers[buffer_index].buffer_size);

                ++buffer_index;
            }

            compression_stream_.next_out = output_buffer_;
            compression_stream_.avail_out = GZipCompressor::OutputBufferSize;
            int err = deflate(&compression_stream_, Z_NO_FLUSH);
            if (err != Z_OK && err != Z_STREAM_END)
            {
                return false;
            }

            assert(compression_stream_.avail_out <= GZipCompressor::OutputBufferSize);

            result.write(reinterpret_cast<char*>(output_buffer_), GZipCompressor::OutputBufferSize - compression_stream_.avail_out);
        }

        while(true)
        {
            compression_stream_.next_out = output_buffer_;
            compression_stream_.avail_out = GZipCompressor::OutputBufferSize;
            int err = deflate(&compression_stream_, Z_FINISH);
            if (err != Z_OK && err != Z_STREAM_END)
            {
                return false;
            }

            result.write(reinterpret_cast<char*>(output_buffer_),GZipCompressor::OutputBufferSize - compression_stream_.avail_out);

            if (err == Z_STREAM_END)
            {
                break;
            }
        } 

        int err = deflateEnd(&compression_stream_);
        if (err != Z_OK)
        {
            return false;
        }

        //尾部
        AppendValue(raw_data_crc_, result);
        AppendValue(compression_stream_.total_in, result);

        result.flush();

        return true;
    }
}
