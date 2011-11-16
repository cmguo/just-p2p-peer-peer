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

            Progress(uint32_t block_id, uint32_t percentage, uint32_t live_interval)
                : block_id_(block_id)
                , percentage_(percentage)
                , live_interval_(live_interval)
            {

            }

            uint32_t DistanceInSeconds(Progress & last_reset_progress)
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
            uint32_t block_id_;
            uint32_t percentage_;
            uint32_t live_interval_;
        };


        RestTimeTracker();
        void Start(uint32_t block_id, uint32_t live_interval);
        void UpdateCurrentProgress(uint32_t current_block_id, uint32_t current_progress_percentage);
        uint32_t GetRestTimeInSeconds();
        void OnPause(bool pause);
        bool IsPaused();

    private:
        void Reset();
        void UpdatePausingTime(framework::timer::Timer * pointer);
        void CalculateRestTime();

    private:
        framework::timer::TickCounter ticks_since_last_progress_update_;
        Progress current_progress_;
        Progress last_reset_progress_;
        uint32_t rest_time_in_seconds_;
        uint32_t live_interval_;
        bool paused_;

        boost::uint32_t accumulate_pausing_time_in_seconds_;

        framework::timer::PeriodicTimer timer_;
    };
}

#endif