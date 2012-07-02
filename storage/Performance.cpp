//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

/***
*
* Performance.cpp
*
*/

#include "Common.h"

#include "storage/storage_base.h"
#include "storage/Performance.h"

namespace storage
{
    Performance::p Performance::inst_;

    Performance::Performance()
        : is_running(false)
    {
    }

    void Performance::Start()
    {
        if (true == is_running)
        {
            return;
        }
        is_running = true;
    }

    void Performance::Stop()
    {
        if (false == is_running)
        {
            return;
        }

        is_running = false;
        inst_.reset();
    }

    bool Performance::IsIdleInSeconds(uint32_t sec)
    {
        if (false == is_running)
        {
            return false;
        }
#ifdef BOOST_WINDOWS_API
        LASTINPUTINFO li;
        li.cbSize = sizeof(LASTINPUTINFO);
        ::GetLastInputInfo(&li);
        uint32_t idle_time = (::GetTickCount() - li.dwTime) / 1000;
        return idle_time >= sec;
#else
        return false;
#endif
    }

    bool Performance::IsIdle(uint32_t min)
    {
        return IsIdleInSeconds(min * 60);
    }

    bool Performance::IsIdle()
    {
        return IsIdle(USER_IDLE_ELAPSED_TIME);
    }

    uint32_t Performance::GetIdleInSeconds()
    {
        if (false == is_running)
        {
            return false;
        }
#ifdef BOOST_WINDOWS_API
        LASTINPUTINFO li;
        li.cbSize = sizeof(LASTINPUTINFO);
        ::GetLastInputInfo(&li);
        uint32_t idle_time = (::GetTickCount() - li.dwTime) / 1000;
        return idle_time;
#else
        return 0;
#endif
    }

    bool Performance::IsScreenSaverRunning()
    {
#ifdef BOOST_WINDOWS_API
        BOOL is_run = FALSE;
        ::SystemParametersInfo(SPI_GETSCREENSAVERRUNNING, 0, &is_run, 0);
        if (is_run)
        {
            return true;
        }
        return false;
#else
        return false;
#endif
    }

    storage::DTType Performance::GetCurrDesktopType()
    {
#ifdef BOOST_WINDOWS_API
        if (Performance::IsScreenSaverRunning())
        {
            return DT_SCREEN_SAVER;
        }
        HDESK hDesk = ::OpenInputDesktop(0, false, DESKTOP_READOBJECTS);
        if (NULL == hDesk)
        {
            return DT_WINLOGON;
        }
        ::CloseDesktop(hDesk);
        return DT_DEFAULT;
#else
        return DT_DEFAULT;
#endif
    }
}
