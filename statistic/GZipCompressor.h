//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _STATISTIC_GZIPCOMPRESSOR_H_
#define _STATISTIC_GZIPCOMPRESSOR_H_

#include "Compressor.h"
#include "zlib/zlib.h"
#include <iosfwd>

namespace statistic
{
    class GZipCompressor
        :public ICompressor
    {
        struct RawData
        {
            RawData(uint8_t* raw_data_buffer, int raw_data_size)
                :buffer(raw_data_buffer), buffer_size(raw_data_size)
            {
            }

            uint8_t* buffer;
            int buffer_size;
        };

    public:
        GZipCompressor();

        ~GZipCompressor();

        bool Compress(uint8_t* raw_data, int raw_data_size, std::ostream& result);
    private:
        bool Compress(const std::vector<RawData>& buffers, std::ostream& result);

        bool InitializeForCompression();

        static void AppendValue(unsigned long value, std::ostream& result);

    private:
        static const int OutputBufferSize;
        uint8_t *output_buffer_;
        z_stream compression_stream_;
        unsigned long raw_data_crc_;
    };
}

#endif  // _STATISTIC_GZIPCOMPRESSOR_H_
