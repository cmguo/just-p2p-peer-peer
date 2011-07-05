#include "Common.h"
#include "LiveRestTimeTracker.h"

namespace p2sp
{
    void RestTimeTracker::UpdateCurrentProgress(uint32_t current_block_id, uint32_t current_progress_percentage)
    {
        assert(current_progress_percentage <= 100);
        current_progress_ = Progress(current_block_id, current_progress_percentage, live_interval_);
    }

    uint32_t RestTimeTracker::GetRestTimeInSeconds()
    {
        return rest_time_in_seconds_;
    }

    void RestTimeTracker::UpdateRestTime()
    {
        uint32_t downloaded_data_in_seconds_since_reset = current_progress_.DistanceInSeconds(last_reset_progress_);
        uint32_t elapsed_seconds_since_reset = ticks_since_last_progress_update_.elapsed() / 1000;
        if (elapsed_seconds_since_reset >= downloaded_data_in_seconds_since_reset)
        {
            Reset();
        }
        else
        {
            rest_time_in_seconds_ = downloaded_data_in_seconds_since_reset - elapsed_seconds_since_reset;
        }
    }

    void RestTimeTracker::Reset()
    {
        last_reset_progress_ = current_progress_;
        ticks_since_last_progress_update_.reset();
        rest_time_in_seconds_ = 0;
    }
}