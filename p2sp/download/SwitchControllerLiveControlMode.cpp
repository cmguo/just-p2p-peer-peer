//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "p2sp/download/SwitchController.h"
#include "statistic/StatisticModule.h"
#include "p2sp/bootstrap/BootStrapGeneralConfig.h"
#include "p2sp/proxy/PlayInfo.h"

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


#ifdef USE_MEMORY_POOL
        is_memory_full = false;
#endif
        time_counter_live_control_mode_.reset();

        settings_.LoadSettings();

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
        // 3300
        else if (state_.http_ == State::HTTP_PAUSING && state_.p2p_ == State::P2P_PAUSING)
        {
            CheckState3300();
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
        PauseP2PDownloader();
        ResumeHttpDownloader();

        time_counter_2300_.reset();
        rest_play_time_when_switched_ = GetGlobalDataProvider()->GetRestPlayableTime();
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
            is_started_ = false;

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
            is_started_ = false;

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

        if (is_started_)
        {
            if (rest_play_time_in_second < settings_.GetP2PRestPlayableTimeDelim() &&
                time_counter_3200_.elapsed() > settings_.GetP2PProtectTimeWhenStart())
            {
                return true;
            }
        }
        else
        {
            // http速度很好，等到剩余时间比较短时才切过去，提高节约比并且不会卡
            if (rest_play_time_when_switched_ > settings_.GetSafeEnoughRestPlayableTime())
            {
                if (rest_play_time_in_second < settings_.GetP2PRestPlayableTimeDelimWhenSwitchedWithLargeTime())
                {
                    GetGlobalDataProvider()->SubmitChangedToHttpTimesWhenUrgent();
                    changed_to_http_because_of_large_upload_ = false;
                    return true;
                }
            }

            // http跑了1分钟或者3分钟剩余时间还不够多但也没有卡时切过来的
            if (rest_play_time_when_switched_ > 0)
            {
                if (rest_play_time_in_second < settings_.GetP2PRestPlayableTimeDelim()
                    && time_counter_3200_.elapsed() > settings_.GetP2PProtectTimeWhenSwitchedWithNotEnoughTime())
                {
                    GetGlobalDataProvider()->SubmitChangedToHttpTimesWhenUrgent();
                    changed_to_http_because_of_large_upload_ = false;
                    return true;
                }
            }

            // 卡了后切过来的
            // P2P同样卡，再切换回去试试
            if (rest_play_time_in_second == 0 && time_counter_3200_.elapsed() > settings_.GetP2PProtectTimeWhenSwitchedWithBuffering())
            {
                GetGlobalDataProvider()->SubmitChangedToHttpTimesWhenUrgent();
                changed_to_http_because_of_large_upload_ = false;
                return true;
            }
        }

        if (GetGlobalDataProvider()->ShouldUseCDNWhenLargeUpload()
            && (is_http_fast_ || time_counter_3200_.elapsed() > settings_.GetTimeToIgnoreHttpBad())
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

            if (rest_play_time_in_second > settings_.GetSafeEnoughRestPlayableTime())
            {
                GetGlobalDataProvider()->SubmitChangedToP2PCondition(REST_PLAYABLE_TIME_ENOUGTH);
                return true;
            }

            if (time_counter_2300_.elapsed() >= settings_.GetHttpRunningLongEnoughTimeWhenStart())
            {
                GetGlobalDataProvider()->SubmitChangedToP2PCondition(LONG_TIME_USING_CDN);
                return true;
            }

            if (rest_play_time_in_second == 0 &&
                time_counter_2300_.elapsed() >= settings_.GetHttpProtectTimeWhenStart())
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
            if (rest_play_time_in_second <= rest_play_time_when_switched_ &&
                time_counter_2300_.elapsed() > settings_.GetHttpProtectTimeWhenUrgentSwitched())
            {
                return true;
            }

            // 剩余时间足够了或者是跑了很长时间并且剩余时间还可以，再去试试P2P
            if ((rest_play_time_in_second > settings_.GetSafeEnoughRestPlayableTime())
                || (time_counter_2300_.elapsed() > settings_.GetHttpRunningLongEnoughTimeWhenUrgentSwitched()
                && rest_play_time_in_second > settings_.GetSafeRestPlayableTimeDelim()))
            {
                return true;
            }

            return false;
        }

        if (rest_play_time_in_second < settings_.GetSafeRestPlayableTimeDelim()
            && time_counter_2300_.elapsed() > settings_.GetHttpProtectTimeWhenLargeUpload())
        {
            return true;
        }

        if (GetGlobalDataProvider()->IsUploadSpeedSmallEnough()
            && time_counter_2300_.elapsed() > GetGlobalDataProvider()->GetUsingCdnTimeAtLeastWhenLargeUpload() * 1000)
        {
            return true;
        }

        return false;
    }

    void SwitchController::LiveControlMode::LiveControlModeSettings::LoadSettings()
    {
        safe_enough_rest_playable_time_delim_under_http_ = BootStrapGeneralConfig::Inst()->GetSafeEnoughRestPlayableTime();
        http_running_long_enough_time_when_start_ = BootStrapGeneralConfig::Inst()->GetHttpRunningLongEnoughTimeWhenStart();
        http_protect_time_when_start_ = BootStrapGeneralConfig::Inst()->GetHttpProtectTimeWhenStart();
        http_protect_time_when_urgent_switched_ = BootStrapGeneralConfig::Inst()->GetHttpProtectTimeWhenUrgentSwitched();
        http_running_long_enough_time_when_urgent_switched_ = BootStrapGeneralConfig::Inst()->GetHttpRunningLongEnoughTimeWhenUrgentSwitched();
        safe_rest_playable_time_delim_ = BootStrapGeneralConfig::Inst()->GetSafeRestPlayableTimeDelimWhenUseHttp();
        http_protect_time_when_large_upload_ = BootStrapGeneralConfig::Inst()->GetHttpProtectTimeWhenLargeUpload();
        p2p_rest_playable_time_delim_when_switched_with_large_time_ = BootStrapGeneralConfig::Inst()->GetP2PRestPlayableTimeDelimWhenSwitchedWithLargeTime();
        p2p_rest_playable_time_delim_ = BootStrapGeneralConfig::Inst()->GetP2PRestPlayableTimeDelim();
        p2p_protect_time_when_switched_with_not_enough_time_ = BootStrapGeneralConfig::Inst()->GetP2PProtectTimeWhenSwitchedWithNotEnoughTime();
        p2p_protect_time_when_switched_with_buffering_ = BootStrapGeneralConfig::Inst()->GetP2PProtectTimeWhenSwitchedWithBuffering();
        time_to_ignore_http_bad_ = BootStrapGeneralConfig::Inst()->GetTimeToIgnoreHttpBad();
        p2p_protect_time_when_start_ = BootStrapGeneralConfig::Inst()->GetP2PProtectTimeWhenStart();
        should_use_bw_type_ = BootStrapGeneralConfig::Inst()->GetShouldUseBWType();
    }

    void SwitchController::LiveControlMode::CheckState3300()
    {
        if (settings_.GetShouldUseBWType() && GetHTTPControlTarget() && GetP2PControlTarget())
        {
            // bwtype = JBW_NORMAL p2p启动
            if (GetGlobalDataProvider()->GetBWType() == JBW_NORMAL && GetGlobalDataProvider()->GetReplay() == false
                && GetGlobalDataProvider()->GetSourceType() == PlayInfo::SOURCE_PPLIVE_LIVE2)
            {
                ChangeTo3200();
            }
            else
            {
                ChangeTo2300();
            }

            return;
        }

        // bs开关为关，只要有Http，则Http启动
        if (GetHTTPControlTarget())
        {
            ResumeHttpDownloader();
        }
        else if (GetP2PControlTarget())
        {
            ResumeP2PDownloader();
        }
    }
}
