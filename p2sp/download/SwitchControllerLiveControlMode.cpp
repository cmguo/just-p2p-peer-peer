//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "p2sp/download/SwitchController.h"
#include "statistic/StatisticModule.h"

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("live_download");

    //////////////////////////////////////////////////////////////////////////
    // LiveControlMode

    SwitchController::LiveControlMode::p SwitchController::LiveControlMode::Create(SwitchController::p controller)
    {
        return LiveControlMode::p(new LiveControlMode(controller));
    }

    void SwitchController::LiveControlMode::Start()
    {
        if (true == IsRunning())
            return;

        assert(GetHTTPControlTarget() || GetP2PControlTarget());

        ControlMode::Start();

        if (GetHTTPControlTarget())
            GetHTTPControlTarget()->Pause();

        if (GetP2PControlTarget())
            GetP2PControlTarget()->Pause();

        state_.http_ = (GetHTTPControlTarget() ? State::HTTP_PAUSING : State::HTTP_NONE);
        state_.p2p_ = (GetP2PControlTarget() ? State::P2P_PAUSING : State::P2P_NONE);

        // 如果有Http，则用http下载(2300)
        if (GetHTTPControlTarget())
        {
            GetHTTPControlTarget()->Resume();
            state_.http_ = State::HTTP_DOWNLOADING;
        }
        else if (GetP2PControlTarget())
        {
            GetP2PControlTarget()->Resume();
            state_.p2p_ = State::P2P_DOWNLOADING;
        }

#ifdef USE_MEMORY_POOL
        is_memory_full = false;
#endif
        time_counter_live_control_mode_.reset();

        OnControlTimer(0);
    }

    void SwitchController::LiveControlMode::OnControlTimer(uint32_t times)
    {
        if (false == IsRunning())
            return;

#ifdef USE_MEMORY_POOL
        if (CheckMemory())
        {
            return;
        }
#endif

        // 2300
        if (state_.http_ == State::HTTP_DOWNLOADING && state_.p2p_ == State::P2P_PAUSING)
        {
            CheckState2300();
        }
        // 3200
        else if (state_.http_ == State::HTTP_PAUSING && state_.p2p_ == State::P2P_DOWNLOADING)
        {
            CheckState3200();
        }
    }

    void SwitchController::LiveControlMode::ChangeTo3200()
    {
        PauseHttpDownloader();
        ResumeP2PDownloader();

        time_counter_3200_.reset();
        rest_play_time_when_switched_ = GetGlobalDataProvider()->GetRestPlayableTime();

        if (changed_to_http_because_of_large_upload_)
        {
            GetGlobalDataProvider()->SetUseP2P();
        }
    }

    void SwitchController::LiveControlMode::ChangeTo2300()
    {
        ResumeHttpDownloader();
        PauseP2PDownloader();

        time_counter_2300_.reset();
        rest_play_time_when_switched_ = GetGlobalDataProvider()->GetRestPlayableTime();
        is_started_ = false;
        blocked_this_time_ = false;

        if (changed_to_http_because_of_large_upload_)
        {
            GetGlobalDataProvider()->SetUseCdnBecauseOfLargeUpload();
        }
    }

#ifdef USE_MEMORY_POOL
    bool SwitchController::LiveControlMode::CheckMemory()
    {
        // 一直下载，直到剩余内存小于1024时停止，保持停止状态直到剩余内存大于2048时开始下载
        if (!is_memory_full)
        {
            if (protocol::LiveSubPieceContent::get_left_capacity() < 1024)
            {
                if (GetHTTPControlTarget())
                {
                    PauseHttpDownloader();
                }

                if (GetP2PControlTarget())
                {
                    PauseP2PDownloader();
                }

                return true;
            }
            else
            {
                return false;
            }
        }
        else
        {
            if (protocol::LiveSubPieceContent::get_left_capacity() > 2048)
            {
                is_memory_full = false;
                return false;
            }
            else
            {
                return true;
            }
        }
    }
#endif

    void SwitchController::LiveControlMode::CheckState2300()
    {
        assert(GetHTTPControlTarget());
        assert(GetP2PControlTarget());

        // 如果P2P节点数为0，不再进行判断
        if (GetP2PControlTarget()->GetConnectedPeersCount() == 0)
            return;

        boost::uint32_t rest_play_time_in_second = GetGlobalDataProvider()->GetRestPlayableTime();

        if (NeedChangeTo3200())
        {
            if (rest_play_time_in_second > 20)
            {
                is_http_fast_ = true;
            }
            else if (rest_play_time_in_second < 5)
            {
                is_http_fast_ = false;
            }
            ChangeTo3200();
        }
    }

    void SwitchController::LiveControlMode::CheckState3200()
    {
        assert(GetHTTPControlTarget());
        assert(GetP2PControlTarget());

        if (NeedChangeTo2300())
        {
            ChangeTo2300();
        }
    }

    void SwitchController::LiveControlMode::PauseHttpDownloader()
    {
        state_.http_ = State::HTTP_PAUSING;
        GetHTTPControlTarget()->Pause();
    }

    void SwitchController::LiveControlMode::ResumeHttpDownloader()
    {
        state_.http_ = State::HTTP_DOWNLOADING;
        GetHTTPControlTarget()->Resume();
    }

    void SwitchController::LiveControlMode::PauseP2PDownloader()
    {
        state_.p2p_ = State::P2P_PAUSING;
        GetP2PControlTarget()->Pause();
    }

    void SwitchController::LiveControlMode::ResumeP2PDownloader()
    {
        state_.p2p_ = State::P2P_DOWNLOADING;
        GetP2PControlTarget()->Resume();
    }

    bool SwitchController::LiveControlMode::NeedChangeTo2300()
    {
        boost::uint32_t rest_play_time_in_second = GetGlobalDataProvider()->GetRestPlayableTime();

        // http速度很好，等到剩余时间比较短时才切过去，提高节约比并且不会卡
        if (rest_play_time_when_switched_ > 20)
        {
            if (rest_play_time_in_second < 6)
            {
                GetGlobalDataProvider()->SubmitChangedToHttpTimesWhenUrgent();
                changed_to_http_because_of_large_upload_ = false;
                return true;
            }
        }

        // http跑了1分钟或者3分钟剩余时间还不够多但也没有卡时切过来的
        if (rest_play_time_when_switched_ > 0)
        {
            if (rest_play_time_in_second < 5 && time_counter_3200_.elapsed() > 15000)
            {
                GetGlobalDataProvider()->SubmitChangedToHttpTimesWhenUrgent();
                changed_to_http_because_of_large_upload_ = false;
                return true;
            }
        }

        // 卡了后切过来的
        // P2P同样卡，再切换回去试试
        if (rest_play_time_in_second == 0 && time_counter_3200_.elapsed() > 30000)
        {
            GetGlobalDataProvider()->SubmitChangedToHttpTimesWhenUrgent();
            changed_to_http_because_of_large_upload_ = false;
            return true;
        }

        if (GetGlobalDataProvider()->ShouldUseCDNWhenLargeUpload()
            && (is_http_fast_ || time_counter_3200_.elapsed() > 180000)
            && rest_play_time_in_second > GetGlobalDataProvider()->GetRestPlayTimeDelim()
            && GetGlobalDataProvider()->IsUploadSpeedLargeEnough())
        {
            changed_to_http_because_of_large_upload_ = true;
            return true;
        }

        return false;
    }

    bool SwitchController::LiveControlMode::NeedChangeTo3200()
    {
        boost::uint32_t rest_play_time_in_second = GetGlobalDataProvider()->GetRestPlayableTime();

        if (is_started_)
        {
            if (GetGlobalDataProvider()->ShouldUseCDNWhenLargeUpload()
                && GetGlobalDataProvider()->IsUploadSpeedLargeEnough()
                && rest_play_time_in_second > 0)
            {
                return false;
            }

            if (rest_play_time_in_second > 20)
            {
                GetGlobalDataProvider()->SubmitChangedToP2PCondition(REST_PLAYABLE_TIME_ENOUGTH);
                return true;
            }

            if (time_counter_2300_.elapsed() >= 3 * 60 * 1000)
            {
                GetGlobalDataProvider()->SubmitChangedToP2PCondition(LONG_TIME_USING_CDN);
                return true;
            }

            if (rest_play_time_in_second == 0 && time_counter_2300_.elapsed() >= 10000)
            {
                GetGlobalDataProvider()->SubmitChangedToP2PCondition(BLOCK);
                return true;
            }

            return false;
        }

        if (!GetGlobalDataProvider()->ShouldUseCDNWhenLargeUpload() || changed_to_http_because_of_large_upload_ == false)
        {
            if (rest_play_time_in_second == 0 && time_counter_2300_.elapsed() <= 10000)
            {
                if (blocked_this_time_ == false)
                {
                    GetGlobalDataProvider()->SubmitBlockTimesWhenUseHttpUnderUrgentCondition();
                    blocked_this_time_ = true;
                }
            }

            // 跑了超过20秒之后跟P2P效果一样或者是还不如P2P
            if (rest_play_time_in_second <= rest_play_time_when_switched_ && time_counter_2300_.elapsed() > 20000)
            {
                return true;
            }

            // 剩余时间足够了或者是跑了很长时间并且剩余时间还可以，再去试试P2P
            if ((rest_play_time_in_second > 20)
                || (time_counter_2300_.elapsed() > 60000 && rest_play_time_in_second > 5))
            {
                return true;
            }

            return false;
        }

        if (rest_play_time_in_second < 5 && time_counter_2300_.elapsed() > 10000)
        {
            return true;
        }

        if (!GetGlobalDataProvider()->IsUploadSpeedLargeEnough()
            && time_counter_2300_.elapsed() > 30000)
        {
            return true;
        }

        return false;
    }
}
