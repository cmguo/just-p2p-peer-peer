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

        uint32_t AverageByteSpeed(boost::uint64_t tick_count) const;  // bytes per second

        uint32_t SecondByteSpeed(boost::uint64_t tick_count);  // 1 second

        uint32_t CurrentByteSpeed(boost::uint64_t tick_count);  // 5 seconds

        uint32_t RecentByteSpeed(boost::uint64_t tick_count);  // 20 seconds

        uint32_t RecentMinuteByteSpeed(boost::uint64_t tick_count);  // 1 minute

        inline uint32_t GetElapsedTimeInMilliSeconds(boost::uint64_t tick_count) const;

        uint32_t TotalBytes() const;

    private:

        static uint32_t GetPositionFromSeconds(uint32_t seconds);

        inline void CheckTickCount(boost::uint64_t tick_count);

        inline void UpdateTickCount(uint32_t curr_sec);

        inline uint32_t CalcSpeedInDuration(uint32_t duration, boost::uint64_t tick_count);

    private:

        static const uint32_t SECONDS_IN_SECOND = 1;

        static const uint32_t SECONDS_IN_RECENT = 5;

        static const uint32_t SECONDS_IN_RECENT_20SEC = 20;

        static const uint32_t SECONDS_IN_MINUTE = 60;

        static const uint32_t HISTORY_INTERVAL_IN_SEC = SECONDS_IN_MINUTE;

    private:

        boost::uint64_t start_time_;

        uint32_t total_bytes_;

        uint32_t history_bytes_[HISTORY_INTERVAL_IN_SEC];

        uint32_t current_sec_;

        bool is_running_;
    };

    inline void ByteSpeedMeter::SubmitBytes(uint32_t bytes)
    {
        CheckTickCount(framework::timer::TickCounter::tick_count());
        total_bytes_ += bytes;
        history_bytes_[GetPositionFromSeconds(current_sec_)] += bytes;
    }

    inline void ByteSpeedMeter::CheckTickCount(boost::uint64_t tick_count)
    {
        uint32_t curr_sec = tick_count / 1000;
        if (curr_sec == current_sec_)
            return;
        UpdateTickCount(curr_sec);
    }
}

#endif  // FRAMEWORK_STATISTIC_BYTESPEEDMETER_H
