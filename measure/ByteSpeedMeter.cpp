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

        start_time_ = framework::timer::TickCounter::tick_count();
        CheckTickCount(start_time_);
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

    uint32_t ByteSpeedMeter::AverageByteSpeed(boost::uint64_t tick_count) const
    {
        if (is_running_ == false)
            return 0;

        return 1000 * (boost::int64_t)total_bytes_ / GetElapsedTimeInMilliSeconds(tick_count);
    }

    uint32_t ByteSpeedMeter::SecondByteSpeed(boost::uint64_t tick_count)  // 2 second
    {
        return CalcSpeedInDuration(SECONDS_IN_SECOND, tick_count);
    }

    uint32_t ByteSpeedMeter::CurrentByteSpeed(boost::uint64_t tick_count)  // 5 seconds
    {
        return CalcSpeedInDuration(SECONDS_IN_RECENT, tick_count);
    }

    uint32_t ByteSpeedMeter::RecentByteSpeed(boost::uint64_t tick_count)  // 20 seconds
    {
        return CalcSpeedInDuration(SECONDS_IN_RECENT_20SEC, tick_count);
    }

    uint32_t ByteSpeedMeter::RecentMinuteByteSpeed(boost::uint64_t tick_count)  // 1 minute
    {
        return CalcSpeedInDuration(HISTORY_INTERVAL_IN_SEC, tick_count);
    }
}
