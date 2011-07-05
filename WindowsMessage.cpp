//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#ifdef BOOST_WINDOWS_API
#include "WindowsMessage.h"

WindowsMessage::p WindowsMessage::windows_message_ (new WindowsMessage());

WindowsMessage::WindowsMessage()
: hwnd_(NULL)
{
}

void WindowsMessage::SendWindowsMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (hwnd_)
    {
        ::SendMessage(hwnd_, msg, wParam, lParam);
    }
}

void WindowsMessage::PostWindowsMessage(UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (hwnd_)
    {
        ::PostMessage(hwnd_, msg, wParam, lParam);
    }
}


string w2b(const wstring& _src)
{
    int nBufSize = ::WideCharToMultiByte(GetACP(), 0, _src.c_str(), -1, NULL, 0, 0, FALSE);

    char *szBuf = new char[nBufSize + 1];

    ::WideCharToMultiByte(GetACP(), 0, _src.c_str(), -1, szBuf, nBufSize, 0, FALSE);

    string strRet(szBuf);

    delete []szBuf;
    szBuf = NULL;

    return strRet;
}

wstring b2w(const string& _src)
{
    setlocale(LC_ALL, "chs");

    // 计算字符串 string 转成 wchar_t 之后占用的内存字节数
    int nBufSize = ::MultiByteToWideChar(GetACP(), 0, _src.c_str(), -1, NULL, 0);

    // 为 wsbuf 分配内存 BufSize 个字节
    wchar_t *wsBuf = new wchar_t[nBufSize + 1];

    // 转化为 unicode 的 WideString
    ::MultiByteToWideChar(GetACP(), 0, _src.c_str(), -1, wsBuf, nBufSize);

    // 恢复系统默认的设置
    setlocale(LC_ALL, "");

    wstring wstrRet(wsBuf);

    delete []wsBuf;
    wsBuf = NULL;

    return wstrRet;
}
#endif
