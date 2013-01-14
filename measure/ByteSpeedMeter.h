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

        void SubmitBytes(boost::uint32_t bytes);

        boost::uint32_t AverageByteSpeed(boost::uint64_t tick_count) const;  // bytes per second

        boost::uint32_t SecondByteSpeed(boost::uint64_t tick_count);  // 1 second

        boost::uint32_t CurrentByteSpeed(boost::uint64_t tick_count);  // 5 seconds

        boost::uint32_t RecentByteSpeed(boost::uint64_t tick_count);  // 20 seconds

        boost::uint32_t RecentMinuteByteSpeed(boost::uint64_t tick_count);  // 1 minute

        inline boost::uint32_t GetElapsedTimeInMilliSeconds(boost::uint64_t tick_count) const;

        boost::uint32_t TotalBytes() const;

    private:

        static boost::uint32_t GetPositionFromSeconds(boost::uint32_t seconds);

        inline void CheckTickCount(boost::uint64_t tick_count);

        inline void UpdateTickCount(boost::uint32_t curr_sec);

        inline boost::uint32_t CalcSpeedInDuration(boost::uint32_t duration, boost::uint64_t tick_count);

    private:

        static const boost::uint32_t SECONDS_IN_SECOND = 1;

        static const boost::uint32_t SECONDS_IN_RECENT = 5;

        static const boost::uint32_t SECONDS_IN_RECENT_20SEC = 20;

        static const boost::uint32_t SECONDS_IN_MINUTE = 60;

        static const boost::uint32_t HISTORY_INTERVAL_IN_SEC = SECONDS_IN_MINUTE;

    private:

        boost::uint64_t start_time_;

        boost::uint32_t total_bytes_;

        boost::uint32_t history_bytes_[HISTORY_INTERVAL_IN_SEC];

        boost::uint32_t current_sec_;

        bool is_running_;
    };

    inline void ByteSpeedMeter::SubmitBytes(boost::uint32_t bytes)
    {
        CheckTickCount(framework::timer::TickCounter::tick_count());
        total_bytes_ += bytes;
        history_bytes_[GetPositionFromSeconds(current_sec_)] += bytes;
    }

    inline void ByteSpeedMeter::CheckTickCount(boost::uint64_t tick_count)
    {
        boost::uint32_t curr_sec = tick_count / 1000;
        if (curr_sec == current_sec_)
            return;
        UpdateTickCount(curr_sec);
    }

    inline void ByteSpeedMeter::UpdateTickCount(boost::uint32_t curr_sec)
    {
        if (curr_sec - current_sec_ >= HISTORY_INTERVAL_IN_SEC)
        {
            memset(history_bytes_, 0, sizeof(history_bytes_));
        }
        else
        {
            for (boost::uint32_t i = curr_sec; i > current_sec_; i--)
                history_bytes_[GetPositionFromSeconds(i)] = 0;
        }

        current_sec_ = curr_sec;
    }

    inline boost::uint32_t ByteSpeedMeter::CalcSpeedInDuration(boost::uint32_t duration, boost::uint64_t tick_count)
    {
        CheckTickCount(tick_count);

        boost::int64_t bytes_in_recent = 0;
        boost::uint32_t last_sec = current_sec_ - 1;
        for (boost::uint32_t i = last_sec; i > last_sec - duration; i--)
            bytes_in_recent += history_bytes_[GetPositionFromSeconds(i)];

        boost::uint32_t elapsed_time = GetElapsedTimeInMilliSeconds(tick_count);
        if (elapsed_time > duration * 1000)
            return static_cast<boost::uint32_t>(bytes_in_recent / duration);
        else
            return static_cast<boost::uint32_t>(1000 * bytes_in_recent / elapsed_time);
    }

    inline boost::uint32_t ByteSpeedMeter::GetElapsedTimeInMilliSeconds(boost::uint64_t tick_count) const
    {
        boost::uint32_t ms = tick_count - start_time_;
        return ms <= 0 ? 1 : ms;
    }
}

#endif  // FRAMEWORK_STATISTIC_BYTESPEEDMETER_H
