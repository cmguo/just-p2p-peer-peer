//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _WSCONVERT_H_
#define _WSCONVERT_H_

#include <locale.h>
#include "Common.h"

namespace base
{
#ifdef PEER_PC_CLIENT
    using std::string;
    using std::wstring;
    static string ws2s(const wstring& ws)
    {
        string curLocale = setlocale(LC_ALL, NULL);        // curLocale = "C";
        setlocale(LC_ALL, "chs");
        const wchar_t* _Source = ws.c_str();
        boost::uint32_t _Dsize = 2 * ws.size() + 1;
        char *_Dest = new char[_Dsize];
        memset(_Dest, 0, _Dsize);
        wcstombs(_Dest, _Source, _Dsize);
        string result = _Dest;
        delete []_Dest;
        setlocale(LC_ALL, curLocale.c_str());
        return result;
    }

    static wstring s2ws(const string& s)
    {
        setlocale(LC_ALL, "chs");
        const char* _Source = s.c_str();
        boost::uint32_t _Dsize = s.size() + 1;
        wchar_t *_Dest = new wchar_t[_Dsize];
        wmemset(_Dest, 0, _Dsize);
        mbstowcs(_Dest, _Source, _Dsize);
        wstring result = _Dest;
        delete []_Dest;
        setlocale(LC_ALL, "C");
        return result;
    }
#endif
}

#endif

