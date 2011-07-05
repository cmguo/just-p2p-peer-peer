//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

void DebugLog(const char* format, ...)
{
#if ((defined _DEBUG || defined DEBUG) && (defined BOOST_WINDOWS_API))
    char buffer[2048] = "PEER: ";    
    char* str = buffer + 6;

    va_list vl;
    va_start(vl, format);
    vsnprintf(str, 2042, format, vl);
    OutputDebugString(buffer);
    va_end(vl);
#endif
}
