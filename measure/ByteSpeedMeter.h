//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef FRAMEWORK_STATISTIC_BYTESPEEDMETER_H
#define FRAMEWORK_STATISTIC_BYTESPEEDMETER_H

#include <boost/noncopyable.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>

namespace measure
{

    class ByteSpeedMeter
#ifdef DUMP_OBJECT
        : public count_object_allocate<ByteSpeedMeter>
#endif
        {
    public:
        ByteSpeedMeter();

        void Start();

        void Stop();

        bool IsRunning() const;

        void Clear();

    public:

        void SubmitBytes(uint32_t bytes);

        uint32_t AverageByteSpeed() const;  // bytes per second

        uint32_t SecondByteSpeed();  // 1 second

        uint32_t CurrentByteSpeed();  // 5 seconds

        uint32_t RecentByteSpeed();  // 20 seconds

        uint32_t RecentMinuteByteSpeed();  // 1 minute

        uint32_t GetElapsedTimeInMilliSeconds() const;

        uint32_t TotalBytes() const;

    private:

        static uint32_t GetPositionFromSeconds(uint32_t seconds);

        void CheckTickCount();

        void UpdateTickCount(uint32_t & curr_sec);

    private:

        static const uint32_t SECONDS_IN_SECOND = 1;

        static const uint32_t SECONDS_IN_RECENT = 5;

        static const uint32_t SECONDS_IN_RECENT_20SEC = 20;

        static const uint32_t SECONDS_IN_MINUTE = 60;

        static const uint32_t HISTORY_INTERVAL_IN_SEC = SECONDS_IN_MINUTE;

    private:

        uint32_t start_time_;

        uint32_t total_bytes_;

        uint32_t history_bytes_[HISTORY_INTERVAL_IN_SEC];

        uint32_t last_sec_;

        bool is_running_;
    };

    inline void ByteSpeedMeter::SubmitBytes(uint32_t bytes)
    {
        CheckTickCount();
        total_bytes_ += bytes;
        history_bytes_[GetPositionFromSeconds(last_sec_)] += bytes;
    }

    inline void ByteSpeedMeter::CheckTickCount()
    {
        uint32_t curr_sec = framework::timer::TickCounter::tick_count() / 1000;
        if (curr_sec == last_sec_)
            return;
        UpdateTickCount(curr_sec);
    }
}

#endif  // FRAMEWORK_STATISTIC_BYTESPEEDMETER_H
