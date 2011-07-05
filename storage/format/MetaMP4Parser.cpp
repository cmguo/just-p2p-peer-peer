//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------


#include "Common.h"
#include "MetaMP4Parser.h"
#include <base/util.h>

namespace storage
{

    MetaMP4Parser::MetaMP4Parser()
    {
        Clear();
    }

    void MetaMP4Parser::Clear()
    {
        memset(this, 0, sizeof(MetaMP4Parser));
    }

    bool MetaMP4Parser::IsMP4(const boost::uint8_t* buffer, int length)
    {
        // find 'moov'
        int offset = 0;
        string moov("moov");
        for (offset = 0; offset < length - 4; ++offset)
        {
            if (string((const char*)(buffer + offset), 4) == moov)
                return true;
        }
        return false;
    }

    void MetaMP4Parser::Parse(const boost::uint8_t* buffer, int length)
    {
        Clear();

        if (length < 1024 || buffer == NULL)
            return;

        // find 'moov'
        int offset = 0;
        string moov("moov");
        for (offset = 0; offset < length - 4; ++offset)
        {
            if (string((const char*)(buffer + offset), 4) == moov)
                break;
        }

        // moov
        offset += 4; if (offset >= length) return;

        // atom size
        AtomSize = GetUINT32(buffer + offset);
        offset += 4; if (offset >= length) return;

        // type
        strncpy(Type, (const char*)(buffer + offset), sizeof(Type)-1);

        offset += 4; if (offset >= length) return;

        // version
        Version = *(boost::uint8_t*)(buffer + offset);
        offset += 1; if (offset >= length) return;

        // Flags
        base::util::memcpy2(Flags, length - offset , buffer + offset, 3);
        offset += 3; if (offset >= length) return;

        // Creation Time
        CreationTime = GetUINT32(buffer + offset);
        offset += 4; if (offset >= length) return;

        // Modification Time
        ModificationTime = GetUINT32(buffer + offset);
        offset += 4; if (offset >= length) return;

        // TimeScale
        TimeScale = GetUINT32(buffer + offset);
        offset += 4; if (offset >= length) return;

        // Duration
        Duration = GetUINT32(buffer + offset);
        offset += 4; if (offset >= length) return;
    }

    uint32_t MetaMP4Parser::GetDurationInSeconds() const
    {
        if (TimeScale == 0) return 0;
        return (uint32_t)((0.0 + Duration) / TimeScale + 0.5);
    }

    uint32_t MetaMP4Parser::GetUINT32(const boost::uint8_t* buffer)
    {
        uint32_t value = 0;
        value = ((value << 8) | buffer[0]);
        value = ((value << 8) | buffer[1]);
        value = ((value << 8) | buffer[2]);
        value = ((value << 8) | buffer[3]);
        return value;
    }

}
