//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef HTTP_HEADER_PARSER_H
#define HTTP_HEADER_PARSER_H

#include "zlib/zlib.h"

namespace p2sp
{
    struct IDecompressListener
    {
        virtual void OnDecompressComplete(std::deque<boost::shared_ptr<protocol::SubPieceBuffer> > & sub_piece_buffer_deque) = 0;
        virtual ~IDecompressListener()
        {

        }
    };

    class GzipDecompresser
    {
    public:
        void Start(boost::shared_ptr<IDecompressListener> handler);

        GzipDecompresser()
        {
            /* allocate inflate state */
            stream_.zalloc = Z_NULL;
            stream_.zfree = Z_NULL;
            stream_.opaque = Z_NULL;
            stream_.avail_in = 0;
            stream_.next_in = Z_NULL;

            /* 
            windowBits can also be greater than 15 for optional gzip decoding.
            Add 32 to windowBits to enable zlib and gzip decoding with automatic header detection,
            or add 16 to decode only the gzip format (the zlib format will return a Z_DATA_ERROR).
            If a gzip stream is being decoded, strm->adler is a crc32 instead of an adler32.
            http://stackoverflow.com/questions/1838699/how-can-i-decompress-a-gzip-stream-with-zlib
            */

            int ret = inflateInit2(&stream_, 32 + MAX_WBITS);

            assert(ret == Z_OK);
        }

        bool OnRecvData(protocol::SubPieceBuffer const & buffer, uint32_t file_offset, uint32_t content_offset);

    private:
        void Decompress(const boost::uint8_t *src, boost::uint32_t src_len);

    private:
        boost::shared_ptr<IDecompressListener> handler_;
        
        bool is_header_decompress_;

        z_stream stream_;

        std::deque<boost::shared_ptr<protocol::SubPieceBuffer> > sub_piece_buffer_deque_;
    };
}

#endif