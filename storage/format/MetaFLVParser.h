//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// MetaFLVParser.h

#ifndef _P2SP_FORMAT_META_FLV_PARSER_H_
#define _P2SP_FORMAT_META_FLV_PARSER_H_

#include <boost/any.hpp>

namespace boost
{
    typedef unsigned char uint24_t[3];
}

namespace storage
{
#ifdef BOOST_WINDOWS_API
    #pragma pack(push, 1)
#endif

    struct FLVHeader
    {
        char FileFormat[3];         // FLV
        boost::uint8_t Version;              // version

        struct {
            boost::uint8_t ReservedA : 5;
            boost::uint8_t TypeFlagsAudio : 1;
            boost::uint8_t ReservedB : 1;
            boost::uint8_t TypeFlagsVideo : 1;
        };

        uint32_t DataOffset;           // header length, for version 1, the value is 0x00000009
    };

    struct ScriptDataString
    {
        boost::uint16_t Length;
        boost::uint8_t  Value[];
    };

    struct ScriptDataLongString
    {
        uint32_t Length;
        boost::uint8_t  Value[];
    };

    struct ScriptDataObjectEnd
    {
        boost::uint8_t Value[3];  // UI24, always 9
    };

    struct ScriptDataVariable
    {
        // ScriptDataString     VariableName
        // ScriptDataValue      VariableData
    };

    struct ScriptDataVariableEnd
    {
        boost::uint8_t Value[3];  // UI24, always 9
    };

    struct ScriptDataDate
    {
        double  DateTime;               // milliseconds, timestamp
        short   LocalDateTimeOffset;    // minutes
    };

    struct ScriptDataValue
    {
        // 0    Number
        // 1    Boolean
        // 2    String
        // 3    Object
        // 4    MovieClip
        // 5    Null
        // 6    Undefined
        // 7    Reference
        // 8    ECMA Array
        // 10   Strict Array
        // 11   Date
        // 12   Long String
        boost::uint8_t Type;

        // ScriptDataValue
        // Type == 8:   uint32_t  ECMAArrayLength

        // Value
        // Type == 0:   DOUBLE
        // Type == 1:   boost::uint8_t
        // Type == 2:   ScriptDataString
        // Type == 3:   ScriptDataObject[n]
        // Type == 4:   ScriptDataString (The movie clip path)
        // Type == 7:   boost::uint16_t
        // Type == 8:   ScriptDataVariable[ ECMAArrayLength ]
        // Type == 10:  ScriptDataVariable[n]
        // Type == 11:  ScriptDataDate
        // Type == 12:  ScriptDataLongString

        // ScriptDataTerminator
        // Type == 3:   ScriptDataObjectEnd
        // Type == 8:   ScriptDataVariableEnd
    };

    struct OnMetaData
    {
        double Duration;        // total duration of file in seconds
        double Width;           // width
        double Height;          // height
        double VideoDataRate;   // video bit rate in kilobits / second
        double FrameRate;       // number of frames / second
        double VideoCodecID;    // video codec id
        double AudioSampleRate;  // frequency at which the audio stream is replayed
        double AudioSampleSize;  // resolution of a single audio sample
        bool   Stereo;          // whether the data is stereo
        double AudioCodecID;    // audio codec id
        double FileSize;        // total file size in bytes
    };

    struct ScriptDataObject
    {
        // ScriptDataString ObjectName;
        // ScriptDataValue  ObjectData;
    };

    struct ScriptData
    {
        // ScriptDataObject Objects[];
        // boost::uint24_t           End;          // always 9
    };

    struct FLVTag
    {
        // 0x08:    audio
        // 0x09:    video
        // 0x12:    script data
        // others:  reserved
        boost::uint8_t   TagType;

        boost::uint24_t  DataSzie;       // length of the data in the Data Field
        boost::uint24_t  Timestamp;      // time in milliseconds at which the data in this tag applies
        boost::uint8_t   TimestampExt;   // the upper 8 bits of the timestamp
        boost::uint24_t  StreamID;

        // Type == 8:   AudioData
        // Type == 9:   VideoData
        // Type == 18:  ScriptDataObject
    };

    struct FLVFileBody
    {
        uint32_t PreviousTagSize;
        FLVTag Tag;
    };

#ifdef BOOST_WINDOWS_API
    #pragma pack(pop)
#endif

    class MetaFLVParser
    {
    public:
        typedef std::map<string, boost::any> PropertyMap;

    public:

        void Parse(const boost::uint8_t* buffer, int length);

        boost::any GetProperty(const string& name) const;

        const PropertyMap& Properties() const;

        static bool IsFLV(const boost::uint8_t* buffer, int length);

    private:

        static uint32_t GetUINT16(const boost::uint8_t* buffer);

        static uint32_t GetUINT24(const boost::uint8_t* buffer);

        static uint32_t GetUINT32(const boost::uint8_t* buffer);

        static double GetDouble(const boost::uint8_t* buffer);

    private:
        PropertyMap properties_;
    };

}

#endif  // _P2SP_FORMAT_META_FLV_PARSER_H_
