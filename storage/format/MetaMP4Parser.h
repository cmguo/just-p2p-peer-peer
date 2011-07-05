//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// MetaMp4Parser.h

#ifndef _P2SP_FORMAT_META_MP4_PARSER_H_
#define _P2SP_FORMAT_META_MP4_PARSER_H_

namespace storage
{
    class MetaMP4Parser
    {
    public:

        MetaMP4Parser();

        void Clear();

        static bool IsMP4(const boost::uint8_t* buffer, int length);

        void Parse(const boost::uint8_t* buffer, int length);

        uint32_t GetDurationInSeconds() const;

    public:
        uint32_t AtomSize;
        char   Type[5];
        boost::uint8_t  Version;
        char   Flags[3];
        uint32_t CreationTime;
        uint32_t ModificationTime;
        uint32_t TimeScale;
        uint32_t Duration;

    private:

        static uint32_t GetUINT32(const boost::uint8_t* buffer);

    };

}

#endif  // _P2SP_FORMAT_META_MP4_PARSER_H_
