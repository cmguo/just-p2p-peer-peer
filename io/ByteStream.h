//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

/*
 *  bytestrea.h
 *  convert between byte stream and integer
 */

#ifndef _BYTESTREAM_H_
#define _BYTESTREAM_H_

#include <boost/cstdint.hpp>

namespace framework {

namespace io {

boost::uint8_t BytesToUI08(const char* buf);

boost::uint16_t BytesToUI16(const char* buf);

uint32_t BytesToUI24(const char* buf);

uint32_t BytesToUI32(const char* buf);

boost::uint64_t BytesToUI64(const char* buf);

void UI08ToBytes(char* buf, boost::uint8_t val);

void UI16ToBytes(char* buf, boost::uint16_t val);

void UI24ToBytes(char* buf, uint32_t val);

void UI32ToBytes(char* buf, uint32_t val);

void UI64ToBytes(char* buf, boost::uint64_t val);

}

}

#endif  // _BYTESTREAM_H_

