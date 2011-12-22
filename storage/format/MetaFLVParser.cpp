//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "MetaFLVParser.h"
#include <base/util.h>

namespace storage
{
    bool MetaFLVParser::IsFLV(const boost::uint8_t* buffer, int length)
    {
        if (buffer == NULL || length < 3)
            return false;
        if (string((const char*)buffer, 3) == "FLV")
            return true;
        return false;
    }

    uint32_t MetaFLVParser::GetUINT16(const boost::uint8_t* buffer)
    {
        uint32_t value = 0;
        value = ((value << 8) | buffer[0]);
        value = ((value << 8) | buffer[1]);
        return value;
    }

    uint32_t MetaFLVParser::GetUINT24(const boost::uint8_t* buffer)
    {
        uint32_t value = 0;
        value = ((value << 8) | buffer[0]);
        value = ((value << 8) | buffer[1]);
        value = ((value << 8) | buffer[2]);
        return value;
    }

    uint32_t MetaFLVParser::GetUINT32(const boost::uint8_t* buffer)
    {
        uint32_t value = 0;
        value = ((value << 8) | buffer[0]);
        value = ((value << 8) | buffer[1]);
        value = ((value << 8) | buffer[2]);
        value = ((value << 8) | buffer[3]);
        return value;
    }

    double MetaFLVParser::GetDouble(const boost::uint8_t* buffer)
    {
        double value = 0;
        boost::uint8_t data[sizeof(double)];
        for (uint32_t i = 0; i < sizeof(double); i++)
            data[i] = buffer[sizeof(double) - i - 1];
        // value = *(double*)data;
        memcpy((void*)(&value), (void*)(data), sizeof(double)); // nightsuns: 这个地址无法检测
        return value;
    }

    boost::any MetaFLVParser::GetProperty(const string& name) const
    {
        PropertyMap::const_iterator iter  = properties_.find(name);
        if (iter != properties_.end())
        {
            return iter->second;
        }
        return boost::any();
    }

    const MetaFLVParser::PropertyMap& MetaFLVParser::Properties() const
    {
        return properties_;
    }

    void MetaFLVParser::Parse(const boost::uint8_t* buffer, boost::uint32_t length)
    {
        if (buffer == NULL) return;
        properties_.clear();

        boost::uint32_t offset = 0;
        // const FLVHeader* header = (const FLVHeader*) (buffer + offset);
        offset += sizeof(FLVHeader); if (offset >= length) return;

        // previous length
        offset += 4; if (offset >= length) return;
        // tag
        const FLVTag* tag = (const FLVTag*) (buffer + offset);
        switch (tag->TagType)
        {
        case 0x08:  // audio
        case 0x09:  // video
            {
                // skip
            }
            break;
        case 0x12:
            {
                offset += sizeof(FLVTag); if (offset >= length) return;
                // int dataLength = GetUINT24((const boost::uint8_t*)tag->DataSzie);
                //                 if (length < offset + dataLength)
                //                 {
                //                     cerr << "Length is not enough" << endl;
                //                     break;
                //                 }
                // script data object
                offset += 1; if (offset >= length) return;
                const ScriptDataString* name = (const ScriptDataString*) (buffer + offset);
                offset += sizeof(name->Length);  if (offset >= length) return;
                if (offset + GetUINT16((const boost::uint8_t*)(&(name->Length))) >= length) return;
                string dataName((const char*)(name->Value), GetUINT16((const boost::uint8_t*)(&(name->Length))));
                offset += GetUINT16((const boost::uint8_t*)(&(name->Length))); if (offset >= length) return;
                if (dataName == "onMetaData")
                {
                    // parse value
                    const ScriptDataValue* meta = (const ScriptDataValue*) (buffer + offset);
                    offset += sizeof(meta->Type); if (offset >= length) return;
                    if (meta->Type == 0x08)  // ECMAArray
                    {
                        uint32_t ECMAArrayLength = GetUINT32(buffer + offset);
                        offset += sizeof(ECMAArrayLength); if (offset >= length) return;
                        // ECMA Array
                        for (uint32_t i = 0; i < ECMAArrayLength; i++)
                        {
                            if (GetUINT24(buffer + offset) == 0x09U)
                                break;
                            // read properties
                            const ScriptDataString* pVariableName = (const ScriptDataString*) (buffer + offset);
                            offset += sizeof(pVariableName->Length); if (offset >= length) return;
                            if (offset + GetUINT16((const boost::uint8_t*)(&(pVariableName->Length))) >= length) return;
                            string variableName((const char*)(pVariableName->Value), GetUINT16((const boost::uint8_t*)(&(pVariableName->Length))));
                            offset += GetUINT16((const boost::uint8_t*)(&(pVariableName->Length))); if (offset >= length) return;
                            // variable data
                            const ScriptDataValue* pVarData = (const ScriptDataValue*)(buffer + offset);
                            offset += sizeof(pVarData->Type); if (offset >= length) return;
                            switch (pVarData->Type)
                            {
                            case 0x00:  // double number
                                {
                                    double d = GetDouble(buffer + offset);
                                    properties_[variableName] = boost::any(d);
                                    offset += sizeof(double); if (offset >= length) return;
                                }
                                break;
                            case 0x01:  // boolean
                                {
                                    boost::uint8_t b = *(const boost::uint8_t*) (buffer + offset);
                                    properties_[variableName] = boost::any(b);
                                    offset += sizeof(boost::uint8_t); if (offset >= length) return;
                                }
                                break;
                            case 0x02:  // string
                                {
                                    const ScriptDataString* pData = (const ScriptDataString*) (buffer + offset);
                                    offset += sizeof (pData->Length); if (offset >= length) return;
                                    if (offset + GetUINT16((const boost::uint8_t*)(&(pData->Length))) >= length) return;
                                    string vData((const char*)(pData->Value), GetUINT16((const boost::uint8_t*)(&(pData->Length))));
                                    offset += GetUINT16((const boost::uint8_t*)(&(pData->Length))); if (offset >= length) return;
                                    properties_[variableName] = boost::any(vData);
                                }
                                break;
                            case 0x05:  // null
                            case 0x06:  // undefined
                                break;
                            case 0x0B:  // Date
                                {
                                    // const ScriptDataDate* date = (const ScriptDataDate*) (buffer + offset);
                                    offset += sizeof(ScriptDataDate); if (offset >= length) return;
                                    // skip
                                }
                                break;
                            case 0x0C:  // Long String
                                {
                                    const ScriptDataLongString* pData = (const ScriptDataLongString*) (buffer + offset);
                                    offset += sizeof (pData->Length); if (offset >= length) return;
                                    if (offset + GetUINT32((const boost::uint8_t*)(&(pData->Length))) >= length) return;
                                    string vData((const char*)(pData->Value), GetUINT32((const boost::uint8_t*)(&(pData->Length))));
                                    offset += GetUINT32((const boost::uint8_t*)(&(pData->Length))); if (offset >= length) return;
                                    properties_[variableName] = boost::any(vData);
                                }
                                break;
                            case 0x0A:
                                {
                                    // uint32_t len = GetUINT32(buffer + offset);
                                    offset += 4;
                                    // TODO
                                }
                                break;
                            case 0x03:
                                {
                                    // read objects
                                    // TODO
                                }
                                break;
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
                            }
                        }
                    }
                    else
                    {
                        // cerr << "onMetaData should be a ecma array" << endl;
                        break;
                    }
                }
                else
                {
                    // cerr << "No onMetaData available" << endl;
                    break;
                }
            }
            break;
        default:
            break;
        }
    }

}
