#ifndef _LIVE_REST_TIME_TRACKER_H_
#define _LIVE_REST_TIME_TRACKER_H_

#include "framework/timer/Timer.h"

namespace p2sp
{
    class RestTimeTracker
    {
    public:
        class Progress
        {
        public:
            Progress()
            {

            }

            Progress(boost::uint32_t block_id, boost::uint32_t percentage, boost::uint32_t live_interval)
                : block_id_(block_id)
                , percentage_(percentage)
                , live_interval_(live_interval)
            {

            }

            boost::uint32_t DistanceInSeconds(Progress & last_reset_progress)
            {
                assert(last_reset_progress <= *this);

                if (percentage_ > last_reset_progress.percentage_)
                {
                    return block_id_ - last_reset_progress.block_id_ + 
                        (percentage_ - last_reset_progress.percentage_) * live_interval_ / 100;
                }
                else
                {
                    return block_id_ - last_reset_progress.block_id_ - 
                        (last_reset_progress.percentage_ - percentage_) * live_interval_ / 100;
                }
            }

            bool operator <= (const Progress & p)
            {
                if (block_id_ == p.block_id_)
                {
                    return percentage_ <= p.percentage_;
                }
                return block_id_ <= p.block_id_;
            }

        private:
            boost::uint32_t block_id_;
            boost::uint32_t percentage_;
            boost::uint32_t live_interval_;
        };


        RestTimeTracker();
        void Start(boost::uint32_t block_id, boost::uint32_t live_interval);
        void UpdateCurrentProgress(boost::uint32_t current_block_id, boost::uint32_t current_progress_percentage);
        boost::uint32_t GetRestTimeInSeconds();
        void OnPause(bool pause);
        bool IsPaused();

        void SetRestTimeInSecond(boost::uint32_t rest_time_in_second);

    private:
        void Reset();
        void UpdatePausingTime(framework::timer::Timer * pointer);
        void CalculateRestTime();

    private:
        framework::timer::TickCounter ticks_since_last_progress_update_;
        Progress current_progress_;
        Progress last_reset_progress_;
        boost::uint32_t rest_time_in_seconds_;
        boost::uint32_t live_interval_;
        bool paused_;

        bool need_calculate_rest_time_;

        boost::uint32_t accumulate_pausing_time_in_seconds_;

        framework::timer::PeriodicTimer timer_;
    };
}

#endif