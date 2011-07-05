//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------
#ifdef BOOST_WINDOWS_API

#pragma once
#include <boost/utility.hpp>
#include <boost/shared_ptr.hpp>
#include <Windows.h>
#include <string>
using namespace std;

class WindowsMessage
    : public boost::noncopyable
{
    public:
    typedef boost::shared_ptr<WindowsMessage> p;
    HWND hwnd_;
    public:
    void PostWindowsMessage(UINT msg, WPARAM wParam = (WPARAM)0, LPARAM lParam = (WPARAM)0);
    void SendWindowsMessage(UINT msg, WPARAM wParam = (WPARAM)0, LPARAM lParam = (WPARAM)0);
    void SetHWND(HWND hwnd) { hwnd_ = hwnd; }
    HWND GetHWND() const { return hwnd_; }

    private:
    WindowsMessage();
    static WindowsMessage::p windows_message_;
    public:
    static WindowsMessage& Inst() { return *windows_message_; };
};

string w2b(const wstring& _src);
wstring b2w(const string& _src);

#endif