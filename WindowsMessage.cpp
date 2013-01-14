//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#ifdef PEER_PC_CLIENT
#include "WindowsMessage.h"

WindowsMessage::p WindowsMessage::windows_message_ ;

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

#endif
