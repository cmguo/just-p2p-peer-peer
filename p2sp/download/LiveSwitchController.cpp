#include "Common.h"
#include "LiveSwitchController.h"
#include "p2sp/download/LiveDownloadDriver.h"
#include "p2sp/proxy/PlayInfo.h"

namespace p2sp
{
    LiveSwitchController::LiveSwitchController()
        : control_timer_(global_second_timer(), 1000,  boost::bind(&LiveSwitchController::OnTimerElapsed, this, &control_timer_))
        , is_started_(true)
        , rest_play_time_when_switched_(0)
        , is_http_fast_(true)
        , changed_to_http_because_of_large_upload_(false)
        , blocked_this_time_(false)
    {

    }

    void LiveSwitchController::Start(LiveDownloadDriver__p live_download_driver)
    {
        live_download_driver_ = live_download_driver;

        assert(GetHTTPControlTarget() || GetP2PControlTarget());

        if (GetHTTPControlTarget())
            GetHTTPControlTarget()->Pause();

        if (GetP2PControlTarget())
            GetP2PControlTarget()->Pause();

        state_.http_ = (GetHTTPControlTarget() ? State::HTTP_PAUSING : State::HTTP_NONE);
        state_.p2p_ = (GetP2PControlTarget() ? State::P2P_PAUSING : State::P2P_NONE);

        if (GetHTTPControlTarget() && !GetP2PControlTarget())
        {
            ResumeHttpDownloader();
        }
        else if (!GetHTTPControlTarget() && GetP2PControlTarget())
        {
            ResumeP2PDownloader();
        }

        control_timer_->start();


#ifdef USE_MEMORY_POOL
        is_memory_full = false;
#endif
        OnControlTimer(0);
    }

    void LiveSwitchController::Stop()
    {
        control_timer_->stop();
        live_download_driver_.reset();
    }

    void LiveSwitchController::OnTimerElapsed(framework::timer::PeriodicTimer * timer)
    {
        assert(timer == &control_timer_);

        OnControlTimer(timer->times());
        live_download_driver_->SetSwitchState(state_.http_, state_.p2p_);
    }

    void LiveSwitchController::OnControlTimer(boost::uint32_t times)
    {
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

    void LiveSwitchController::ChangeTo3200()
    {
        PauseHttpDownloader();
        ResumeP2PDownloader();

        time_counter_3200_.reset();
        rest_play_time_when_switched_ = live_download_driver_->GetRestPlayableTime();

        if (changed_to_http_because_of_large_upload_)
        {
            live_download_driver_->SetUseP2P();
        }
    }

    void LiveSwitchController::ChangeTo2300()
    {
        PauseP2PDownloader();
        ResumeHttpDownloader();

        time_counter_2300_.reset();
        rest_play_time_when_switched_ = live_download_driver_->GetRestPlayableTime();
        blocked_this_time_ = false;

        if (changed_to_http_because_of_large_upload_)
        {
            live_download_driver_->SetUseCdnBecauseOfLargeUpload();
        }
    }

    void LiveSwitchController::ChangeTo3300()
    {
        is_started_ = false;
        PauseP2PDownloader();
        PauseHttpDownloader();
    }

    bool LiveSwitchController::NeedChangeTo2300()
    {
        boost::uint32_t rest_play_time_in_second = live_download_driver_->GetRestPlayableTime();

        boost::uint32_t time_of_advancing_switching_to_http = 0;

        if (GetP2PControlTarget()->GetSpeedInfoEx().NowDownloadSpeed < BootStrapGeneralConfig::Inst()->GetP2PSpeedThreshold() * 1024)
        {
            time_of_advancing_switching_to_http = BootStrapGeneralConfig::Inst()->GetTimeOfAdvancingSwitchingHttp();
        }

        if (live_download_driver_->DoesFallBehindTooMuch())
        {
            return true;
        }

        if (is_started_)
        {
            if (rest_play_time_in_second < BootStrapGeneralConfig::Inst()->GetP2PRestPlayableTimeDelim() + time_of_advancing_switching_to_http &&
                time_counter_3200_.elapsed() > BootStrapGeneralConfig::Inst()->GetP2PProtectTimeWhenStart())
            {
                SetChangeToHttpBecauseOfUrgent();
                return true;
            }

            if (GetP2PControlTarget()->GetConnectedPeersCount() == 0 &&
                time_counter_3200_.elapsed() > 5 * 1000)
            {
                SetChangeToHttpBecauseOfUrgent();
                return true;
            }

            if (GetP2PControlTarget()->GetSpeedInfoEx().NowDownloadSpeed == 0 &&
                rest_play_time_in_second < BootStrapGeneralConfig::Inst()->GetUrgentRestPlayableTimeDelim() &&
                time_counter_3200_.elapsed() > BootStrapGeneralConfig::Inst()->GetP2PProtectTimeIfStartAndSpeedIs0())
            {
                SetChangeToHttpBecauseOfUrgent();
                return true;
            }
        }
        else
        {
            if (GetP2PControlTarget()->GetConnectedPeersCount() == 0)
            {
                return true;
            }

            // http速度很好，等到剩余时间比较短时才切过去，提高节约比并且不会卡
            if (rest_play_time_when_switched_ > BootStrapGeneralConfig::Inst()->GetSafeEnoughRestPlayableTime())
            {
                if (rest_play_time_in_second < BootStrapGeneralConfig::Inst()->GetP2PRestPlayableTimeDelimWhenSwitchedWithLargeTime() + time_of_advancing_switching_to_http)
                {
                    SetChangeToHttpBecauseOfUrgent();
                    return true;
                }
            }

            // http跑了1分钟或者3分钟剩余时间还不够多但也没有卡时切过来的
            if (rest_play_time_when_switched_ > 0)
            {
                if (rest_play_time_in_second < BootStrapGeneralConfig::Inst()->GetP2PRestPlayableTimeDelim() + time_of_advancing_switching_to_http
                    && time_counter_3200_.elapsed() > BootStrapGeneralConfig::Inst()->GetP2PProtectTimeWhenSwitchedWithNotEnoughTime())
                {
                    SetChangeToHttpBecauseOfUrgent();
                    return true;
                }
            }

            // 卡了后切过来的
            // P2P同样卡，再切换回去试试
            if (rest_play_time_when_switched_ == 0 &&
                rest_play_time_in_second == 0 &&
                time_counter_3200_.elapsed() > BootStrapGeneralConfig::Inst()->GetP2PProtectTimeWhenSwitchedWithBuffering())
            {
                SetChangeToHttpBecauseOfUrgent();
                return true;
            }

            if (GetP2PControlTarget()->GetSpeedInfoEx().NowDownloadSpeed == 0 &&
                rest_play_time_in_second < BootStrapGeneralConfig::Inst()->GetUrgentRestPlayableTimeDelim() &&
                time_counter_3200_.elapsed() > BootStrapGeneralConfig::Inst()->GetP2PProtectTimeIfSpeedIs0() &&
                rest_play_time_when_switched_ != 0)
            {
                SetChangeToHttpBecauseOfUrgent();
                return true;
            }

        }

        if (BootStrapGeneralConfig::Inst()->ShouldUseCDNWhenLargeUpload()
            && (is_http_fast_ || time_counter_3200_.elapsed() > BootStrapGeneralConfig::Inst()->GetTimeToIgnoreHttpBad())
            && rest_play_time_in_second > BootStrapGeneralConfig::Inst()->GetRestPlayTimeDelim()
            && live_download_driver_->IsUploadSpeedLargeEnough())
        {
            changed_to_http_because_of_large_upload_ = true;
            return true;
        }

        return false;
    }

    bool LiveSwitchController::NeedChangeTo3200()
    {
        boost::uint32_t rest_play_time_in_second = live_download_driver_->GetRestPlayableTime();

        if (is_started_)
        {
            if (BootStrapGeneralConfig::Inst()->ShouldUseCDNWhenLargeUpload()
                && live_download_driver_->IsUploadSpeedLargeEnough()
                && rest_play_time_in_second > 0)
            {
                return false;
            }

            if (rest_play_time_in_second > BootStrapGeneralConfig::Inst()->GetSafeEnoughRestPlayableTime())
            {
                live_download_driver_->SubmitChangedToP2PCondition(REST_PLAYABLE_TIME_ENOUGTH);
                return true;
            }

            if (time_counter_2300_.elapsed() >= BootStrapGeneralConfig::Inst()->GetHttpRunningLongEnoughTimeWhenStart())
            {
                live_download_driver_->SubmitChangedToP2PCondition(LONG_TIME_USING_CDN);
                return true;
            }

            if (rest_play_time_in_second == 0 &&
                time_counter_2300_.elapsed() >= BootStrapGeneralConfig::Inst()->GetHttpProtectTimeWhenStart())
            {
                live_download_driver_->SubmitChangedToP2PCondition(BLOCK);
                return true;
            }

            return false;
        }

        if (!BootStrapGeneralConfig::Inst()->ShouldUseCDNWhenLargeUpload() || changed_to_http_because_of_large_upload_ == false)
        {
            if (rest_play_time_in_second == 0 && time_counter_2300_.elapsed() <= 10000)
            {
                if (blocked_this_time_ == false)
                {
                    live_download_driver_->SubmitBlockTimesWhenUseHttpUnderUrgentCondition();
                    blocked_this_time_ = true;
                }
            }

            // 跑了超过20秒之后跟P2P效果一样或者是还不如P2P
            if (rest_play_time_in_second <= rest_play_time_when_switched_ &&
                time_counter_2300_.elapsed() > BootStrapGeneralConfig::Inst()->GetHttpProtectTimeWhenUrgentSwitched())
            {
                return true;
            }

            // 剩余时间足够了或者是跑了很长时间并且剩余时间还可以，再去试试P2P
            if ((rest_play_time_in_second > BootStrapGeneralConfig::Inst()->GetSafeEnoughRestPlayableTime())
                || (time_counter_2300_.elapsed() > BootStrapGeneralConfig::Inst()->GetHttpRunningLongEnoughTimeWhenUrgentSwitched()
                && rest_play_time_in_second > BootStrapGeneralConfig::Inst()->GetSafeRestPlayableTimeDelim()))
            {
                return true;
            }

            return false;
        }

        if (rest_play_time_in_second < BootStrapGeneralConfig::Inst()->GetSafeRestPlayableTimeDelim()
            && time_counter_2300_.elapsed() > BootStrapGeneralConfig::Inst()->GetHttpProtectTimeWhenLargeUpload())
        {
            return true;
        }

        if (live_download_driver_->IsUploadSpeedSmallEnough()
            && time_counter_2300_.elapsed() > BootStrapGeneralConfig::Inst()->GetUsingCDNOrUdpServerTimeDelim() * 1000)
        {
            return true;
        }

        return false;
    }

    bool LiveSwitchController::NeedChangeTo3300()
    {
        return live_download_driver_->GetRestPlayableTime() > BootStrapGeneralConfig::Inst()->GetMaxRestPlayableTime();
    }

    void LiveSwitchController::CheckState2300()
    {
        assert(GetHTTPControlTarget());
        assert(GetP2PControlTarget());

        if (NeedChangeTo3300())
        {
            ChangeTo3300();
            return;
        }

        // 如果P2P节点数为0，不再进行判断
        if (GetP2PControlTarget()->GetConnectedPeersCount() == 0)
            return;

        // 如果落后太多，一直用Http，不再切换P2P
        if (live_download_driver_->DoesFallBehindTooMuch())
            return;

        boost::uint32_t rest_play_time_in_second = live_download_driver_->GetRestPlayableTime();

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

    void LiveSwitchController::CheckState3200()
    {
        assert(GetHTTPControlTarget());
        assert(GetP2PControlTarget());

        if (NeedChangeTo3300())
        {
            ChangeTo3300();
            return;
        }

        if (NeedChangeTo2300())
        {
            is_started_ = false;

            ChangeTo2300();
        }
    }

    void LiveSwitchController::CheckState3300()
    {
        if (is_started_)
        {
            if (BootStrapGeneralConfig::Inst()->GetShouldUseBWType() && GetHTTPControlTarget() && GetP2PControlTarget())
            {
                // bwtype = JBW_NORMAL p2p启动
                if (live_download_driver_->GetBWType() == JBW_NORMAL && live_download_driver_->GetReplay() == false
                    && live_download_driver_->GetSourceType() == PlayInfo::SOURCE_PPLIVE_LIVE2)
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
        else if(live_download_driver_->GetRestPlayableTime() < BootStrapGeneralConfig::Inst()->GetMinRestPlayableTime())
        {
            if (GetP2PControlTarget() && GetHTTPControlTarget())
            {
                if (GetP2PControlTarget()->GetConnectedPeersCount() == 0 || live_download_driver_->DoesFallBehindTooMuch())
                {
                    ResumeHttpDownloader();
                }
                else
                {
                    ResumeP2PDownloader();
                }
            }
            else if (GetP2PControlTarget())
            {
                ResumeP2PDownloader();
            }
            else if (GetHTTPControlTarget())
            {
                ResumeHttpDownloader();
            }
        }
    }

    void LiveSwitchController::ResumeHttpDownloader()
    {
        state_.http_ = State::HTTP_DOWNLOADING;
        GetHTTPControlTarget()->Resume();
    }

    void LiveSwitchController::PauseHttpDownloader()
    {
        state_.http_ = State::HTTP_PAUSING;
        GetHTTPControlTarget()->Pause();
    }

    void LiveSwitchController::ResumeP2PDownloader()
    {
        state_.p2p_ = State::P2P_DOWNLOADING;
        GetP2PControlTarget()->Resume();
    }

    void LiveSwitchController::PauseP2PDownloader()
    {
        state_.p2p_ = State::P2P_PAUSING;
        GetP2PControlTarget()->Pause();
    }

    LiveHttpDownloader__p LiveSwitchController::GetHTTPControlTarget()
    {
        return live_download_driver_->GetHTTPControlTarget();
    }

    LiveP2PDownloader__p LiveSwitchController::GetP2PControlTarget()
    {
        return live_download_driver_->GetP2PControlTarget();
    }

    void LiveSwitchController::SetChangeToHttpBecauseOfUrgent()
    {
        live_download_driver_->SubmitChangedToHttpTimesWhenUrgent();
        changed_to_http_because_of_large_upload_ = false;
    }

#ifdef USE_MEMORY_POOL
    bool LiveSwitchController::CheckMemory()
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
}