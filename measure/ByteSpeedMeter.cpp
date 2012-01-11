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
        current_sec_ = 0;
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
        return CalcSpeedInDuration(SECONDS_IN_SECOND);
    }

    uint32_t ByteSpeedMeter::CurrentByteSpeed()  // 5 seconds
    {
        return CalcSpeedInDuration(SECONDS_IN_RECENT);
    }

    uint32_t ByteSpeedMeter::RecentByteSpeed()  // 20 seconds
    {
        return CalcSpeedInDuration(SECONDS_IN_RECENT_20SEC);
    }

    uint32_t ByteSpeedMeter::RecentMinuteByteSpeed()  // 1 minute
    {
        return CalcSpeedInDuration(HISTORY_INTERVAL_IN_SEC);
    }

    void ByteSpeedMeter::UpdateTickCount(uint32_t & curr_sec)
    {
        if (curr_sec - current_sec_ >= HISTORY_INTERVAL_IN_SEC)
        {
            memset(history_bytes_, 0, sizeof(history_bytes_));
        }
        else
        {
            for (uint32_t i = curr_sec; i > current_sec_; i--)
                history_bytes_[GetPositionFromSeconds(i)] = 0;
        }

        current_sec_ = curr_sec;
    }

    uint32_t ByteSpeedMeter::CalcSpeedInDuration(uint32_t duration)
    {
        if (is_running_ == false)
        {
            return 0;
        }

        CheckTickCount();

        boost::int64_t bytes_in_recent = 0;
        uint32_t last_sec = current_sec_ - 1;
        for (uint32_t i = last_sec; i > last_sec - duration; i--)
            bytes_in_recent += history_bytes_[GetPositionFromSeconds(i)];

        uint32_t elapsed_time = GetElapsedTimeInMilliSeconds();
        if (elapsed_time > duration * 1000)
            return static_cast<uint32_t>(bytes_in_recent / duration);
        else
            return static_cast<uint32_t>(1000 * bytes_in_recent / elapsed_time);
    }
}
