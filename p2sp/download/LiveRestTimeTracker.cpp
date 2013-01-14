#include "Common.h"
#include "LiveRestTimeTracker.h"

namespace p2sp
{
    RestTimeTracker::RestTimeTracker()
        : paused_(false)
        , accumulate_pausing_time_in_seconds_(0)
        , need_calculate_rest_time_(true)
        , timer_(global_second_timer(), 1000, boost::bind(&RestTimeTracker::UpdatePausingTime, this, &timer_))
    {

    }

    void RestTimeTracker::Start(boost::uint32_t block_id, boost::uint32_t live_interval)
    {
        live_interval_ = live_interval;
        current_progress_ = Progress(block_id, 0, live_interval_);
        timer_->start();
        Reset();
    }

    void RestTimeTracker::UpdateCurrentProgress(boost::uint32_t current_block_id, boost::uint32_t current_progress_percentage)
    {
        assert(current_progress_percentage <= 100);
        current_progress_ = Progress(current_block_id, current_progress_percentage, live_interval_);
    }

    boost::uint32_t RestTimeTracker::GetRestTimeInSeconds()
    {
        if (need_calculate_rest_time_)
        {
            CalculateRestTime();
        }

        return rest_time_in_seconds_;
    }

    void RestTimeTracker::UpdatePausingTime(framework::timer::Timer * pointer)
    {
        if (pointer == &timer_ && paused_)
        {
            ++accumulate_pausing_time_in_seconds_;
        }
    }

    void RestTimeTracker::CalculateRestTime()
    {
        boost::uint32_t downloaded_data_in_seconds_since_reset = current_progress_.DistanceInSeconds(last_reset_progress_);

        downloaded_data_in_seconds_since_reset += accumulate_pausing_time_in_seconds_;

        boost::uint32_t elapsed_seconds_since_reset = ticks_since_last_progress_update_.elapsed() / 1000;
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
        accumulate_pausing_time_in_seconds_ = 0;
    }

    void RestTimeTracker::OnPause(bool pause)
    {
        paused_ = pause;
    }

    // 这个函数的返回值可能会不准
    // 因为客户端在长时间暂停后会重新发连接过来，但是没有紧跟着发一个暂停的连接
    // 内核会认为当前不在暂停的状态，但是客户端界面上显示的是暂停的状态
    bool RestTimeTracker::IsPaused()
    {
        return paused_;
    }

    void RestTimeTracker::SetRestTimeInSecond(boost::uint32_t rest_time_in_second)
    {
        need_calculate_rest_time_ = false;
        rest_time_in_seconds_ = rest_time_in_second;
    }
}