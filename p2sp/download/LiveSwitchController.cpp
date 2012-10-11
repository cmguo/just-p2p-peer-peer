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

    void LiveSwitchController::Start(LiveDownloadDriver__p live_download_driver, bool is_too_near_from_last_vv_of_same_channel, bool is_saving_mode)
    {
        live_download_driver_ = live_download_driver;
        is_too_near_from_last_vv_of_same_channel_ = is_too_near_from_last_vv_of_same_channel;
        is_saving_mode_ = is_saving_mode;

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

            if (state_.http_ != State::HTTP_DOWNLOADING)
            {
                live_download_driver_->StopHttp(reason_of_stoping_http_);
            }
        }
        // 3200
        else if (state_.http_ == State::HTTP_PAUSING && state_.p2p_ == State::P2P_DOWNLOADING)
        {
            CheckState3200();

            if (state_.http_ == State::HTTP_DOWNLOADING)
            {
                live_download_driver_->StartHttp(reason_of_using_http_);
            }
        }
        // 3300
        else if (state_.http_ == State::HTTP_PAUSING && state_.p2p_ == State::P2P_PAUSING)
        {
            CheckState3300();

            if (state_.http_ == State::HTTP_DOWNLOADING)
            {
                live_download_driver_->StartHttp(reason_of_using_http_);
            }
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

        if (GetP2PControlTarget()->GetSpeedInfoEx().NowDownloadSpeed < BootStrapGeneralConfig::Inst()->GetP2PSpeedThreshold(is_saving_mode_) * 1024)
        {
            time_of_advancing_switching_to_http = BootStrapGeneralConfig::Inst()->GetTimeOfAdvancingSwitchingHttp(is_saving_mode_);
        }

        if (live_download_driver_->DoesFallBehindTooMuch())
        {
            reason_of_using_http_ = FALL_BEHIND;
            return true;
        }

        if (BootStrapGeneralConfig::Inst()->GetUseCdnToAccelerateBasedOnHistory(is_saving_mode_) &&
            BootStrapGeneralConfig::Inst()->ShouldUseCDNWhenLargeUpload(is_saving_mode_) &&
            live_download_driver_->ShouldUseCdnToAccelerate())
        {
            changed_to_http_because_of_large_upload_ = true;
            reason_of_using_http_ = LARGE_UPLOAD;
            return true;
        }

        if (BootStrapGeneralConfig::Inst()->ShouldUseCDNWhenLargeUpload(is_saving_mode_)
            && (is_http_fast_ || time_counter_3200_.elapsed() > BootStrapGeneralConfig::Inst()->GetTimeToIgnoreHttpBad(is_saving_mode_))
            && rest_play_time_in_second > BootStrapGeneralConfig::Inst()->GetRestPlayTimeDelim(is_saving_mode_)
            && live_download_driver_->IsUploadSpeedLargeEnough())
        {
            changed_to_http_because_of_large_upload_ = true;
            reason_of_using_http_ = LARGE_UPLOAD;
            return true;
        }

        if (is_started_)
        {
            if (GetP2PControlTarget()->GetConnectedPeersCount() == 0 &&
                time_counter_3200_.elapsed() > 5 * 1000 &&
                BootStrapGeneralConfig::Inst()->GetUseHttpWhenConnectedNonePeers(is_saving_mode_) &&
                rest_play_time_in_second < BootStrapGeneralConfig::Inst()->GetRestPlayTimeDelimWhenNonePeers(is_saving_mode_))
            {
                SetChangeToHttpBecauseOfUrgent();
                reason_of_using_http_ = NONE_PEERS;
                return true;
            }
        }
        else
        {
            if (GetP2PControlTarget()->GetConnectedPeersCount() == 0 &&
                BootStrapGeneralConfig::Inst()->GetUseHttpWhenConnectedNonePeers(is_saving_mode_) &&
                rest_play_time_in_second < BootStrapGeneralConfig::Inst()->GetRestPlayTimeDelimWhenNonePeers(is_saving_mode_))
            {
                SetChangeToHttpBecauseOfUrgent();
                reason_of_using_http_ = NONE_PEERS;
                return true;
            }
        }

        // 如果之前没有因为落后太多、节点数为0、大上传切换到Http，并且紧急情况下不使用Http的话，则停留在P2P状态下
        if (!BootStrapGeneralConfig::Inst()->GetUseHttpWhenUrgent(is_saving_mode_))
        {
            return false;
        }

        if (is_started_)
        {
            if (rest_play_time_in_second < BootStrapGeneralConfig::Inst()->GetP2PRestPlayableTimeDelim(is_saving_mode_) + time_of_advancing_switching_to_http &&
                time_counter_3200_.elapsed() > BootStrapGeneralConfig::Inst()->GetP2PProtectTimeWhenStart(is_saving_mode_))
            {
                SetChangeToHttpBecauseOfUrgent();
                reason_of_using_http_ = URGENT;
                return true;
            }

            if (GetP2PControlTarget()->GetSpeedInfoEx().NowDownloadSpeed == 0 &&
                rest_play_time_in_second < BootStrapGeneralConfig::Inst()->GetUrgentRestPlayableTimeDelim(is_saving_mode_) &&
                time_counter_3200_.elapsed() > BootStrapGeneralConfig::Inst()->GetP2PProtectTimeIfStartAndSpeedIs0(is_saving_mode_))
            {
                SetChangeToHttpBecauseOfUrgent();
                reason_of_using_http_ = URGENT;
                return true;
            }
        }
        else
        {
            // http速度很好，等到剩余时间比较短时才切过去，提高节约比并且不会卡
            if (rest_play_time_when_switched_ > BootStrapGeneralConfig::Inst()->GetSafeEnoughRestPlayableTime(is_saving_mode_))
            {
                if (rest_play_time_in_second < BootStrapGeneralConfig::Inst()->GetP2PRestPlayableTimeDelimWhenSwitchedWithLargeTime(is_saving_mode_)
                    + time_of_advancing_switching_to_http)
                {
                    SetChangeToHttpBecauseOfUrgent();
                    reason_of_using_http_ = URGENT;
                    return true;
                }
            }

            // http跑了1分钟或者3分钟剩余时间还不够多但也没有卡时切过来的
            if (rest_play_time_when_switched_ > 0)
            {
                if (rest_play_time_in_second < BootStrapGeneralConfig::Inst()->GetP2PRestPlayableTimeDelim(is_saving_mode_) + time_of_advancing_switching_to_http
                    && time_counter_3200_.elapsed() > BootStrapGeneralConfig::Inst()->GetP2PProtectTimeWhenSwitchedWithNotEnoughTime(is_saving_mode_))
                {
                    SetChangeToHttpBecauseOfUrgent();
                    reason_of_using_http_ = URGENT;
                    return true;
                }
            }

            // 卡了后切过来的
            // P2P同样卡，再切换回去试试
            if (rest_play_time_when_switched_ == 0 &&
                rest_play_time_in_second == 0 &&
                time_counter_3200_.elapsed() > BootStrapGeneralConfig::Inst()->GetP2PProtectTimeWhenSwitchedWithBuffering(is_saving_mode_))
            {
                SetChangeToHttpBecauseOfUrgent();
                reason_of_using_http_ = URGENT;
                return true;
            }

            if (GetP2PControlTarget()->GetSpeedInfoEx().NowDownloadSpeed == 0 &&
                rest_play_time_in_second < BootStrapGeneralConfig::Inst()->GetUrgentRestPlayableTimeDelim(is_saving_mode_) &&
                time_counter_3200_.elapsed() > BootStrapGeneralConfig::Inst()->GetP2PProtectTimeIfSpeedIs0(is_saving_mode_) &&
                rest_play_time_when_switched_ != 0)
            {
                SetChangeToHttpBecauseOfUrgent();
                reason_of_using_http_ = URGENT;
                return true;
            }
        }

        return false;
    }

    bool LiveSwitchController::NeedChangeTo3200()
    {
        boost::uint32_t rest_play_time_in_second = live_download_driver_->GetRestPlayableTime();

        // 紧急情况不使用http
        if (!BootStrapGeneralConfig::Inst()->GetUseHttpWhenUrgent(is_saving_mode_))
        {
            // http启动，检查当前上传是否足够大，足够大就停留在2300，否则切走
            if (is_started_)
            {
                if (BootStrapGeneralConfig::Inst()->ShouldUseCDNWhenLargeUpload(is_saving_mode_)
                    && live_download_driver_->IsUploadSpeedLargeEnough()
                    && rest_play_time_in_second > 0)
                {
                    return false;
                }

                reason_of_stoping_http_ = NO_USE_HTTP_WHEN_URGENT;
                return true;
            }
            else
            {
                // 因为大上传使用http的，上传不够大就切走
                if (BootStrapGeneralConfig::Inst()->ShouldUseCDNWhenLargeUpload(is_saving_mode_) && changed_to_http_because_of_large_upload_)
                {
                    if (rest_play_time_in_second < BootStrapGeneralConfig::Inst()->GetSafeRestPlayableTimeDelim(is_saving_mode_)
                        && time_counter_2300_.elapsed() > BootStrapGeneralConfig::Inst()->GetHttpProtectTimeWhenLargeUpload(is_saving_mode_))
                    {
                        reason_of_stoping_http_ = UPLOAD_NOT_LARGE_ENOUGH;
                        return true;
                    }

                    if (live_download_driver_->IsUploadSpeedSmallEnough()
                        && time_counter_2300_.elapsed() > BootStrapGeneralConfig::Inst()->GetUsingCDNOrUdpServerTimeDelim(is_saving_mode_) * 1000)
                    {
                        reason_of_stoping_http_ = UPLOAD_NOT_LARGE_ENOUGH;
                        return true;
                    }

                    return false;
                }
                // 不是因为大上传使用http的，切走
                else
                {
                    reason_of_stoping_http_ = NO_USE_HTTP_WHEN_URGENT;
                    return true;
                }
            }
        }

        if (is_started_)
        {
            if (BootStrapGeneralConfig::Inst()->ShouldUseCDNWhenLargeUpload(is_saving_mode_)
                && live_download_driver_->IsUploadSpeedLargeEnough()
                && rest_play_time_in_second > 0)
            {
                return false;
            }

            if (rest_play_time_in_second > BootStrapGeneralConfig::Inst()->GetSafeEnoughRestPlayableTime(is_saving_mode_))
            {
                live_download_driver_->SubmitChangedToP2PCondition(REST_PLAYABLE_TIME_ENOUGTH);
                reason_of_stoping_http_ = REST_PLAYABLE_TIME_ENOUGTH;
                return true;
            }

            if (time_counter_2300_.elapsed() >= BootStrapGeneralConfig::Inst()->GetHttpRunningLongEnoughTimeWhenStart(is_saving_mode_))
            {
                live_download_driver_->SubmitChangedToP2PCondition(LONG_TIME_USING_CDN);
                reason_of_stoping_http_ = LONG_TIME_USING_CDN;
                return true;
            }

            if (time_counter_2300_.elapsed() >= BootStrapGeneralConfig::Inst()->GetHttpProtectTime2WhenStart(is_saving_mode_) &&
                GetHTTPControlTarget()->GetSpeedInfo().TotalDownloadBytes <= BootStrapGeneralConfig::Inst()->GetHttpDownloadBytesDelimWhenStart(is_saving_mode_))
            {
                live_download_driver_->SubmitChangedToP2PCondition(BLOCK);
                reason_of_stoping_http_ = BLOCK;
                return true;
            }

            if (rest_play_time_in_second == 0 &&
                time_counter_2300_.elapsed() >= BootStrapGeneralConfig::Inst()->GetHttpProtectTimeWhenStart(is_saving_mode_))
            {
                live_download_driver_->SubmitChangedToP2PCondition(BLOCK);
                reason_of_stoping_http_ = BLOCK;
                return true;
            }

            return false;
        }

        if (!BootStrapGeneralConfig::Inst()->ShouldUseCDNWhenLargeUpload(is_saving_mode_) || changed_to_http_because_of_large_upload_ == false)
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
                time_counter_2300_.elapsed() > BootStrapGeneralConfig::Inst()->GetHttpProtectTimeWhenUrgentSwitched(is_saving_mode_))
            {
                reason_of_stoping_http_ = WORSE_THAN_P2P;
                return true;
            }

            // 剩余时间足够了或者是跑了很长时间并且剩余时间还可以，再去试试P2P
            if (rest_play_time_in_second > BootStrapGeneralConfig::Inst()->GetSafeEnoughRestPlayableTime(is_saving_mode_))
            {
                reason_of_stoping_http_ = REST_PLAYABLE_TIME_ENOUGTH;
                return true;
            }

            // 跑了很长时间并且剩余时间还可以，再去试试P2P
            if ((time_counter_2300_.elapsed() > BootStrapGeneralConfig::Inst()->GetHttpRunningLongEnoughTimeWhenUrgentSwitched(is_saving_mode_)
                && rest_play_time_in_second > BootStrapGeneralConfig::Inst()->GetSafeRestPlayableTimeDelim(is_saving_mode_)))
            {
                reason_of_stoping_http_ = REST_PLAYABLE_TIME_NOT_SHORT;
                return true;
            }

            if (time_counter_2300_.elapsed() >= BootStrapGeneralConfig::Inst()->GetHttpRunningLongestTimeWhenUrgentSwitched(is_saving_mode_))
            {
                reason_of_stoping_http_ = LONG_TIME_USING_CDN;
                return true;
            }

            return false;
        }

        if (rest_play_time_in_second < BootStrapGeneralConfig::Inst()->GetSafeRestPlayableTimeDelim(is_saving_mode_)
            && time_counter_2300_.elapsed() > BootStrapGeneralConfig::Inst()->GetHttpProtectTimeWhenLargeUpload(is_saving_mode_))
        {
            reason_of_stoping_http_ = UPLOAD_NOT_LARGE_ENOUGH;
            return true;
        }

        if (live_download_driver_->IsUploadSpeedSmallEnough()
            && time_counter_2300_.elapsed() > BootStrapGeneralConfig::Inst()->GetUsingCDNOrUdpServerTimeDelim(is_saving_mode_) * 1000)
        {
            reason_of_stoping_http_ = UPLOAD_NOT_LARGE_ENOUGH;
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

        boost::uint32_t rest_play_time_in_second = live_download_driver_->GetRestPlayableTime();

        // 如果P2P节点数为0，不再进行判断
        if (GetP2PControlTarget()->GetConnectedPeersCount() == 0 &&
            BootStrapGeneralConfig::Inst()->GetUseHttpWhenConnectedNonePeers(is_saving_mode_) &&
            rest_play_time_in_second < BootStrapGeneralConfig::Inst()->GetEnoughRestPlayTimeDelimWhenNonePeers(is_saving_mode_))
            return;

        // 如果落后太多，一直用Http，不再切换P2P
        if (live_download_driver_->DoesFallBehindTooMuch())
            return;

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
        if (!BootStrapGeneralConfig::Inst()->GetUseHttpWhenUrgent(is_saving_mode_))
        {
            if (GetP2PControlTarget())
            {
                ResumeP2PDownloader();
            }
            else if (GetHTTPControlTarget())
            {
                reason_of_using_http_ = NO_P2P;
                ResumeHttpDownloader();
            }

            return;
        }

        if (is_started_)
        {
            if (BootStrapGeneralConfig::Inst()->GetShouldJudgeSwitchingDatarateManually() && is_too_near_from_last_vv_of_same_channel_
                && live_download_driver_->GetRestPlayableTime() < BootStrapGeneralConfig::Inst()->GetRestPlayableTimeDelimWhenSwitching())
            {
                if (GetHTTPControlTarget())
                {
                    reason_of_using_http_ = DRAG;
                    ChangeTo2300();
                    return;
                }
            }

            if (BootStrapGeneralConfig::Inst()->GetShouldUseBWType() && GetHTTPControlTarget() && GetP2PControlTarget())
            {
                // bwtype = JBW_NORMAL p2p启动
                if ((live_download_driver_->GetBWType() == JBW_NORMAL || is_saving_mode_)
                    && live_download_driver_->GetReplay() == false
                    && live_download_driver_->GetSourceType() == PlayInfo::SOURCE_PPLIVE_LIVE2)
                {
                    ChangeTo3200();
                }
                else
                {
                    reason_of_using_http_ = START;
                    ChangeTo2300();
                }

                return;
            }

            // bs开关为关，只要有Http，则Http启动
            if (GetHTTPControlTarget())
            {
                reason_of_using_http_ = START;
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
                if (GetP2PControlTarget()->GetConnectedPeersCount() == 0)
                {
                    reason_of_using_http_ = NONE_PEERS;
                    ResumeHttpDownloader();
                }
                else if (live_download_driver_->DoesFallBehindTooMuch())
                {
                    reason_of_using_http_ = FALL_BEHIND;
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
                reason_of_using_http_ = NO_P2P;
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