//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "measure/ByteSpeedMeter.h"

namespace measure
{

    ByteSpeedMeter::ByteSpeedMeter()
        : is_running_(false)
    {
        Clear();
    }

    void ByteSpeedMeter::Clear()
    {
        start_time_ = 0;
        total_bytes_ = 0;
        last_sec_ = 0;
        memset(history_bytes_, 0, sizeof(history_bytes_));
    }

    void ByteSpeedMeter::Start()
    {
        if (is_running_ == true)
            return;

        Clear();

        CheckTickCount();
        start_time_ = framework::timer::TickCounter::tick_count();

        is_running_ = true;
    }

    void ByteSpeedMeter::Stop()
    {
        if (is_running_ == false)
            return;

        Clear();

        is_running_ = false;
    }

    bool ByteSpeedMeter::IsRunning() const
    {
        return is_running_;
    }

    uint32_t ByteSpeedMeter::TotalBytes() const
    {
        return total_bytes_;
    }

    uint32_t ByteSpeedMeter::GetPositionFromSeconds(uint32_t seconds)
    {
        return seconds % HISTORY_INTERVAL_IN_SEC;
    }

    uint32_t ByteSpeedMeter::GetElapsedTimeInMilliSeconds() const
    {
        if (is_running_ == false)
            return 0;

        uint32_t ms = framework::timer::TickCounter::tick_count() - start_time_;
        return ms <= 0 ? 1 : ms;
    }

    uint32_t ByteSpeedMeter::AverageByteSpeed() const
    {
        if (is_running_ == false)
            return 0;

        return 1000 * (boost::int64_t)total_bytes_ / GetElapsedTimeInMilliSeconds();
    }

    uint32_t ByteSpeedMeter::SecondByteSpeed()  // 2 second
    {
        if (is_running_ == false)
            return 0;

        CheckTickCount();

        uint32_t bytes_in_recent = 0;
        for (uint32_t i = last_sec_; i > last_sec_ - SECONDS_IN_SECOND; i--)
            bytes_in_recent += history_bytes_[GetPositionFromSeconds(i)];

        uint32_t elapsed_time = GetElapsedTimeInMilliSeconds();
        if (elapsed_time > SECONDS_IN_SECOND * 1000)
            return bytes_in_recent / SECONDS_IN_SECOND;
        else
            return 1000 * bytes_in_recent / elapsed_time;
    }

    uint32_t ByteSpeedMeter::CurrentByteSpeed()  // 5 seconds
    {
        if (is_running_ == false)
            return 0;

        CheckTickCount();

        uint32_t bytes_in_recent = 0;
        for (uint32_t i = last_sec_; i > last_sec_ - SECONDS_IN_RECENT; i--)
            bytes_in_recent += history_bytes_[GetPositionFromSeconds(i)];

        uint32_t elapsed_time = GetElapsedTimeInMilliSeconds();
        if (elapsed_time > SECONDS_IN_RECENT * 1000)
            return bytes_in_recent / SECONDS_IN_RECENT;
        else
            return 1000 * bytes_in_recent / elapsed_time;
    }

    uint32_t ByteSpeedMeter::RecentByteSpeed()  // 20 seconds
    {
        if (is_running_ == false)
            return 0;

        CheckTickCount();

        boost::int64_t bytes_in_recent_20sec = 0;
        for (uint32_t i = last_sec_; i > last_sec_ - SECONDS_IN_RECENT_20SEC; i--)
            bytes_in_recent_20sec += history_bytes_[GetPositionFromSeconds(i)];

        uint32_t elapsed_time = GetElapsedTimeInMilliSeconds();
        if (elapsed_time > SECONDS_IN_RECENT_20SEC * 1000)
            return (uint32_t)(bytes_in_recent_20sec / elapsed_time);
        else
            return (uint32_t)(1000 * bytes_in_recent_20sec / elapsed_time);
    }

    uint32_t ByteSpeedMeter::RecentMinuteByteSpeed()  // 1 minute
    {
        if (is_running_ == false)
            return 0;

        CheckTickCount();

        boost::int64_t bytes_in_minute = 0;
        for (uint32_t i = 0; i < HISTORY_INTERVAL_IN_SEC; i++)
            bytes_in_minute += history_bytes_[i];

        uint32_t elapsed_time = GetElapsedTimeInMilliSeconds();
        if (elapsed_time > HISTORY_INTERVAL_IN_SEC * 1000)
            return (uint32_t)(bytes_in_minute / HISTORY_INTERVAL_IN_SEC);
        else
            return (uint32_t)(1000 * bytes_in_minute / elapsed_time);
    }

    void ByteSpeedMeter::UpdateTickCount(uint32_t & curr_sec)
    {
        if (curr_sec - last_sec_ >= HISTORY_INTERVAL_IN_SEC)
        {
            memset(history_bytes_, 0, sizeof(history_bytes_));
        }
        else
        {
            for (uint32_t i = curr_sec; i > last_sec_; i--)
                history_bytes_[GetPositionFromSeconds(i)] = 0;
        }

        last_sec_ = curr_sec;
    }
}
