//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "p2sp/download/SwitchController.h"
#include "DownloadDriver.h"
#include "../bootstrap/BootStrapGeneralConfig.h"


namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("download");

    //////////////////////////////////////////////////////////////////////////
    // Download

    SwitchController::DownloadControlMode::p SwitchController::DownloadControlMode::Create(SwitchController::p controller)
    {
        return DownloadControlMode::p(new DownloadControlMode(controller));
    }
    void SwitchController::DownloadControlMode::Start()
    {
        if (true == IsRunning())
            return;

        // start
        ControlMode::Start();

        if (GetHTTPControlTarget()) GetHTTPControlTarget()->Pause();
        if (GetP2PControlTarget()) GetP2PControlTarget()->Pause();

        // initial status
        state_.http_ = (GetHTTPControlTarget() ? State::HTTP_PAUSING : State::HTTP_NONE);
        state_.p2p_ = (GetP2PControlTarget() ? State::P2P_PAUSING : State::P2P_NONE);
        state_.rid_ = (GetGlobalDataProvider()->GetP2PControlTarget() ? State::RID_GOT : State::RID_NONE);
        state_.timer_ = State::TIMER_NONE;
        state_.timer_using_ = State::TIMER_USING_NONE;

        SWITCH_DEBUG(string(20, '-'));

        // Next(0);
        OnControlTimer(0);
    }
    void SwitchController::DownloadControlMode::Stop()
    {
        if (false == IsRunning())
            return;
        // release
        // stop
        ControlMode::Stop();
    }

    bool SwitchController::DownloadControlMode::CanP2PDownloadStably()
    {
        if (false == IsRunning())
            return false;

        // download stably
        assert(state_.p2p_ == State::P2P_DOWNLOADING);
        assert(GetP2PControlTarget());

        // uint32_t minute_speed = GetP2PControlTarget()->GetMinuteDownloadSpeed();
        // uint32_t active_peers_count = GetP2PControlTarget()->GetActivePeersCount();
        uint32_t full_block_peers_count = GetP2PControlTarget()->GetFullBlockPeersCount();
        uint32_t now_speed = GetP2PControlTarget()->GetCurrentDownloadSpeed();

        if (now_speed >= 8 * 1024 && full_block_peers_count >= 1)
        {
            return true;
        }
        return false;
    }

    bool SwitchController::DownloadControlMode::IsP2PBad()
    {
        if (false == IsRunning())
            return false;

        assert(state_.p2p_ == State::P2P_DOWNLOADING);
        assert(state_.timer_ == State::TIMER_NONE);
        assert(GetP2PControlTarget());

        uint32_t minute_speed = GetP2PControlTarget()->GetMinuteDownloadSpeed();
         uint32_t now_speed = GetP2PControlTarget()->GetCurrentDownloadSpeed();

        if (GetGlobalDataProvider()->GetVipLevel() == VIP_LEVEL::VIP)
        {
            boost::uint32_t vip_download_min_p2p_speed = BootStrapGeneralConfig::Inst()->GetVipDownloadMinP2PSpeed();
            return now_speed < vip_download_min_p2p_speed * 1024;
        }

        if (minute_speed < 5 * 1024)
        {
            return true;
        }
        return false;
    }

    void SwitchController::DownloadControlMode::OnControlTimer(uint32_t times)
    {
        if (false == IsRunning())
            return;

        while (true)
        {

            SWITCH_DEBUG((string)state_);

            // 设置为快速模式，无冗余
            if (GetP2PControlTarget())
            {
                GetP2PControlTarget()->SetDownloadMode(IP2PControlTarget::FAST_MODE);
            }

            //////////////////////////////////////////////////////////////////////////
            // Initial State
            // <0300*1>
            if (state_.http_ == State::HTTP_NONE && state_.p2p_ == State::P2P_PAUSING
                && state_.timer_ == State::TIMER_NONE && state_.timer_using_ == State::TIMER_USING_NONE)
            {
                // asserts
                assert(state_.timer_ == State::TIMER_NONE);
                assert(state_.timer_using_ == State::TIMER_USING_NONE);
                assert(!GetHTTPControlTarget());
                assert(GetP2PControlTarget());
                // action
                GetP2PControlTarget()->Resume();
                // state
                state_.p2p_ = State::P2P_DOWNLOADING;
                // next
                // Next(times);
                continue;
            }
            // <3300*1>
            else if (state_.http_ == State::HTTP_PAUSING && state_.p2p_ == State::P2P_PAUSING
                && state_.timer_ == State::TIMER_NONE && state_.timer_using_ == State::TIMER_USING_NONE)
            {
                // asserts
                assert(GetGlobalDataProvider()->HasRID());
                assert(GetP2PControlTarget());
                assert(GetP2PControlTarget()->IsPausing());
                // action
                GetP2PControlTarget()->Resume();
                time_counter_x_.reset();
                // state
                state_.p2p_ = State::P2P_DOWNLOADING;
                state_.timer_ = State::TIMER_STARTED;
                state_.timer_using_ = State::TIMER_USING_X;
                // next
                // Next(times);
                continue;
            }
            // <3000*0>
            else if (state_.http_ == State::HTTP_PAUSING && state_.p2p_ == State::P2P_NONE
                && state_.timer_ == State::TIMER_NONE && state_.timer_using_ == State::TIMER_USING_NONE)
            {
                // asserts
                assert(state_.timer_using_ == State::TIMER_USING_NONE);

                if (GetGlobalDataProvider()->GetBWType() != JBW_HTTP_ONLY && 
                    GetGlobalDataProvider()->GetBWType() != JBW_HTTP_PREFERRED)
                {
                    // action
                    time_counter_h_.reset();
                    // state
                    state_.timer_ = State::TIMER_STARTED;
                    state_.timer_using_ = State::TIMER_USING_H;
                }
                else
                {
                    // action
                    GetHTTPControlTarget()->Resume();
                    // state
                    state_.http_ = State::HTTP_DOWNLOADING;
                }

                // next
                continue;
            }
            //////////////////////////////////////////////////////////////////////////
            // Unstable State
            // <3041*0>
            else if (state_.http_ == State::HTTP_PAUSING && state_.p2p_ == State::P2P_NONE
                && state_.timer_ == State::TIMER_STARTED && state_.timer_using_ == State::TIMER_USING_H)
            {
                if (GetP2PControlTarget())
                {
                    assert(GetP2PControlTarget());
                    GetP2PControlTarget()->Resume();
                    state_.rid_ = State::RID_GOT;
                    state_.p2p_ = State::P2P_DOWNLOADING;
                    state_.timer_ = State::TIMER_STARTED;
                    state_.timer_using_ = State::TIMER_USING_X;
                    time_counter_x_.reset();
                    // next
                    // Next(times);
                    continue;
                }
                else if (time_counter_h_.elapsed() > 10 * 1000)
                {
                    // 超时
                    GetHTTPControlTarget()->Resume();
                    state_.http_ = State::HTTP_DOWNLOADING;
                    state_.timer_ = State::TIMER_NONE;
                    state_.timer_using_ = State::TIMER_USING_NONE;
                    // next
                    // Next(times);
                    continue;
                }
                break;
            }
            // <3211*1>
            else if (state_.http_ == State::HTTP_PAUSING && state_.p2p_ == State::P2P_DOWNLOADING
                && state_.timer_ == State::TIMER_STARTED && state_.timer_using_ == State::TIMER_USING_X)
            {
                // check
                if (time_counter_x_.elapsed() > 10 * 1000)
                {
                    // state
                    state_.timer_ = State::TIMER_STOPPED;
                    // next
                    // Next(times);
                    continue;
                }
                break;
            }
            // <3212*1>
            else if (state_.http_ == State::HTTP_PAUSING && state_.p2p_ == State::P2P_DOWNLOADING
                && state_.timer_ == State::TIMER_STOPPED && state_.timer_using_ == State::TIMER_USING_X)
            {
                if (CanP2PDownloadStably())
                {
                    // state
                    state_.timer_ = State::TIMER_NONE;
                    state_.timer_using_ = State::TIMER_USING_NONE;
                    state_.rid_ = State::RID_GOT;
                    // stable
                }
                else
                {
                    // action
                    GetP2PControlTarget()->Pause();
                    GetHTTPControlTarget()->Resume();
                    // state
                    state_.http_ = State::HTTP_DOWNLOADING;
                    state_.p2p_ = State::P2P_PAUSING;
                    state_.timer_ = State::TIMER_NONE;
                    state_.timer_using_ = State::TIMER_USING_NONE;
                    // stable
                }
                // next
                // Next(times);
                continue;
            }
            //////////////////////////////////////////////////////////////////////////
            // Stable State
            // <0200*1>
            else if (state_.http_ == State::HTTP_NONE && state_.p2p_ == State::P2P_DOWNLOADING
                && state_.timer_ == State::TIMER_NONE && state_.timer_using_ == State::TIMER_USING_NONE)
            {
                assert(state_.timer_ == State::TIMER_NONE);
                break;
            }
            // <2000*0>
            else if (state_.http_ == State::HTTP_DOWNLOADING && state_.p2p_ == State::P2P_NONE
                && state_.timer_ == State::TIMER_NONE && state_.timer_using_ == State::TIMER_USING_NONE)
            {
                assert(state_.timer_ == State::TIMER_NONE);
                break;
            }
            // <2300*1>
            else if (state_.http_ == State::HTTP_DOWNLOADING && state_.p2p_ == State::P2P_PAUSING
                && state_.timer_ == State::TIMER_NONE && state_.timer_using_ == State::TIMER_USING_NONE)
            {
                assert(state_.timer_ == State::TIMER_NONE);
                // check Peer Number
                if (GetP2PControlTarget()->GetConnectedPeersCount() >= 5) {
                    // resume
                    GetP2PControlTarget()->Resume();
                    // state
                    state_.p2p_ = State::P2P_DOWNLOADING;
                    // next
                    // Next(times);
                    continue;
                }
                break;
            }
            // <3200*1>
            else if (state_.http_ == State::HTTP_PAUSING && state_.p2p_ == State::P2P_DOWNLOADING
                && state_.timer_ == State::TIMER_NONE && state_.timer_using_ == State::TIMER_USING_NONE)
            {
                // assert
                assert(GetHTTPControlTarget());

                // 最近一分钟的p2p速度小于 5KBps
                if (IsP2PBad())
                {
                    // action
                    GetHTTPControlTarget()->Resume();
                    // state
                    state_.http_ = State::HTTP_DOWNLOADING;
                }
                else
                {
                    // nothing
                }
                break;
            }
            // <2200*1>
            else if (state_.http_ == State::HTTP_DOWNLOADING && state_.p2p_ == State::P2P_DOWNLOADING
                && state_.timer_ == State::TIMER_NONE && state_.timer_using_ == State::TIMER_USING_NONE)
            {
                // asserts
                assert(state_.timer_ == State::TIMER_NONE);
                assert(GetHTTPControlTarget());
                assert(GetP2PControlTarget());

                // p2p 能稳定下载
                if (!IsP2PBad())
                {
                    // action
                    GetHTTPControlTarget()->Pause();
                    // state
                    state_.http_ = State::HTTP_PAUSING;
                }
                else
                {
                    // nothing
                }
                break;
            }
            else
            {
                assert(!"SwitchController::DownloadControlMode::OnControlTimer: No Such State!");
                break;
            }
        }
    }
}