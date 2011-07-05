//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "p2sp/download/SwitchController.h"


namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("download");

    //////////////////////////////////////////////////////////////////////////
    // VideoControlMode

    SwitchController::VideoControlMode::p SwitchController::VideoControlMode::Create(SwitchController::p controller)
    {
        return VideoControlMode::p(new VideoControlMode(controller));
    }

    void SwitchController::VideoControlMode::Start()
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
        state_.rid_ = (GetP2PControlTarget() ? State::RID_GOT : State::RID_NONE);
        state_.range_ = (GetHTTPControlTarget() ? (GetHTTPControlTarget()->IsDetecting() ? State::RANGE_DETECTING : State::RANGE_DETECTED) : State::RANGE_NONE);
        state_.timer_ = State::TIMER_NONE;
        state_.timer_using_ = State::TIMER_USING_NONE;
        // parameters
        t_ = 4 * 1000;  // http一开始一定要跑满4秒
        time_counter_elapsed_.reset();

        SWITCH_DEBUG(string(20, '-'));

        // next
        OnControlTimer(0);
    }

    void SwitchController::VideoControlMode::Stop()
    {
        if (false == IsRunning())
            return;
        // release
        // stop
        ControlMode::Stop();
    }

    bool SwitchController::VideoControlMode::P2PCanDropHttp()
    {
        if (false == is_running_)
            return false;
        // asserts
        assert(GetP2PControlTarget());
        assert(!GetP2PControlTarget()->IsPausing());
        // condition
        uint32_t speed_5s = GetP2PControlTarget()->GetCurrentDownloadSpeed();
        uint32_t data_rate = GetGlobalDataProvider()->GetDataRate();
        uint32_t conn_peers_count = GetP2PControlTarget()->GetConnectedPeersCount();
        uint32_t active_peers_count =  GetP2PControlTarget()->GetActivePeersCount();
        uint32_t full_peers_count = GetP2PControlTarget()->GetFullBlockPeersCount();
        uint32_t pooled_peers_count = GetP2PControlTarget()->GetPooledPeersCount();
        uint32_t bandwidth = GetGlobalDataProvider()->GetBandWidth();
        bool range = (state_.range_ == State::RANGE_SUPPORT ? true : false);

        (void)conn_peers_count;
        SWITCH_DEBUG("speed_5s=" << speed_5s << " speed_h=" << speed_h_ << " data_rate=" << data_rate << " conn_peers=" << conn_peers_count << " active_peers=" << active_peers_count << " full_peers=" << full_peers_count << " bandwidth=" << bandwidth << " range=" << range);

        //////////////////////////////////////////////////////////////////////////
        // Range Support
        if (range)
        {
            if (bandwidth >= 2*data_rate)
            {
                bool b1 = (speed_5s > data_rate * 3 / 2);
                bool b2 = (full_peers_count >= 2);
                bool b3 = (active_peers_count >= 3);
                bool b4 = (speed_5s > speed_h_);

                bool b5 = (full_peers_count >= 5);
                bool b6 = (active_peers_count >= 6);
                bool b7 = (speed_5s > speed_h_ * 2 / 3);
                bool b8 = (pooled_peers_count >= 70);

                SWITCH_DEBUG("... range=1 (bandwidth >= 2*data_rate)=1 " << b1 << b2 << b3 << b4 << " " << b1 << b5 << b6 << b7 << b8);

                if (b1 && b2 && b3 && b4)
                {
                    return true;
                }
                else if (b1 && b5 && b6 && b7 && b8)
                {
                    return true;
                }
            }
            else if (bandwidth >= data_rate)
            {
                bool b1 = (speed_5s > data_rate);
                bool b2 = (full_peers_count >= 2);
                bool b3 = (active_peers_count >= 3);
                bool b4 = (speed_5s > speed_h_);

                bool b5 = (full_peers_count >= 5);
                bool b6 = (active_peers_count >= 6);
                bool b7 = (speed_5s > speed_h_ * 2 / 3);
                bool b8 = (pooled_peers_count >= 70);

                SWITCH_DEBUG("... range=1 (bandwidth >= data_rate)=1 " << b1 << b2 << b3 << b4 << " " << b1 << b5 << b6 << b7 << b8);

                if (b1 && b2 && b3 && b4)
                {
                    return true;
                }
                else if (b1 && b5 && b6 && b7 && b8)
                {
                    return true;
                }
            }
            else
            {
                // bool b1 = (speed_5s > data_rate);
                bool b2 = (full_peers_count >= 2);
                bool b3 = (active_peers_count >= 3);
                bool b4 = (speed_5s > speed_h_);

                bool b5 = (full_peers_count >= 5);
                bool b6 = (active_peers_count >= 6);
                bool b7 = (speed_5s > speed_h_ * 2 / 3);
                bool b8 = (pooled_peers_count >= 70);

                SWITCH_DEBUG("... range=1 (bandwitdh < data_rate)=1 " << b2 << b3 << b4 << " " << b5 << b6 << b7 << b8);

                if (b2 && b3 && b4)
                {
                    return true;
                }
                else if (b5 && b6 && b7 && b8)
                {
                    return true;
                }
            }
        }
        //////////////////////////////////////////////////////////////////////////
        // Range Unsupport
        else
        {
            if (bandwidth >= 2*data_rate)
            {
                bool b1 = (speed_5s > data_rate * 2);
                bool b2 = (full_peers_count >= 3);
                bool b3 = (active_peers_count >= 4);
                bool b4 = (speed_5s > speed_h_);

                bool b5 = (full_peers_count >= 5);
                bool b6 = (active_peers_count >= 6);
                bool b7 = (speed_5s > speed_h_ * 2 / 3);
                bool b8 = (pooled_peers_count >= 70);

                (void)b5;
                (void)b6;
                (void)b7;
                (void)b8;
                SWITCH_DEBUG("... range=0 (bandwidth >= 2*data_rate)=1 " << b1 << b2 << b3 << b4 << " " << b1 << b5 << b6 << b7 << b8);

                if (b1 && b2 && b3 && b4)
                {
                    return true;
                }
            }
            else if (bandwidth >= data_rate)
            {
                bool b1 = (speed_5s > data_rate);
                bool b2 = (full_peers_count >= 3);
                bool b3 = (active_peers_count >= 4);
                bool b4 = (speed_5s > speed_h_);

                bool b5 = (full_peers_count >= 5);
                bool b6 = (active_peers_count >= 6);
                bool b7 = (speed_5s > speed_h_ * 2 / 3);
                bool b8 = (pooled_peers_count >= 70);

                SWITCH_DEBUG("... range=0 (bandwidth >= data_rate)=1 " << b1 << b2 << b3 << b4 << " " << b1 << b5 << b6 << b7 << b8);

                if (b1 && b2 && b3 && b4)
                {
                    return true;
                }
                else if (b1 && b5 && b6 && b7 && b8)
                {
                    return true;
                }
            }
            else
            {
                bool b2 = (full_peers_count >= 3);
                bool b3 = (active_peers_count >= 4);
                bool b4 = (speed_5s > speed_h_);

                bool b5 = (full_peers_count >= 5);
                bool b6 = (active_peers_count >= 6);
                bool b7 = (speed_5s > speed_h_ * 2 / 3);
                bool b8 = (pooled_peers_count >= 70);

                SWITCH_DEBUG("... range=0 (bandwidth < data_rate)=1 " << b2 << b3 << b4 << " " << b5 << b6 << b7 << b8);

                if (b2 && b3 && b4)
                {
                    return true;
                }
                else if (b5 && b6 && b7 && b8)
                {
                    return true;
                }
            }
        }

        return false;
    }

    bool SwitchController::VideoControlMode::P2PCanPlayStably()
    {
        if (false == is_running_)
            return false;
        // asserts
        assert(GetP2PControlTarget());
        assert(!GetP2PControlTarget()->IsPausing());
        // condition
        uint32_t speed_20s = GetP2PControlTarget()->GetRecentDownloadSpeed();
        uint32_t data_rate = GetGlobalDataProvider()->GetDataRate();
        // uint32_t conn_peers_count = GetP2PControlTarget()->GetConnectedPeersCount();
        uint32_t active_peers_count =  GetP2PControlTarget()->GetActivePeersCount();
        uint32_t full_peers_count = GetP2PControlTarget()->GetFullBlockPeersCount();
        uint32_t bandwidth = GetGlobalDataProvider()->GetBandWidth();
        bool range = (state_.range_ == State::RANGE_SUPPORT ? true : false);

        //////////////////////////////////////////////////////////////////////////
        // Range Support
        if (range)
        {
            if (bandwidth > data_rate)
            {
                bool b1 = (speed_20s > data_rate);
                bool b2 = (full_peers_count >= 2);
                bool b3 = (active_peers_count >= 3);

                if (b1 && b2 && b3)
                {
                    return true;
                }
            }
        }
        //////////////////////////////////////////////////////////////////////////
        // Range UnSupport
        else
        {
            if (bandwidth > data_rate)
            {
                bool b1 = (speed_20s > data_rate);
                bool b2 = (full_peers_count >= 2);
                bool b3 = (active_peers_count >= 3);

                if (b1 && b2 && b3)
                {
                    return true;
                }
            }
        }

        return false;
    }

    bool SwitchController::VideoControlMode::P2PCanDownloadStably()
    {
        if (false == is_running_)
            return false;
        // asserts
        assert(GetP2PControlTarget());
        assert(!GetP2PControlTarget()->IsPausing());
        // condition
        uint32_t speed_20s = GetP2PControlTarget()->GetRecentDownloadSpeed();
        uint32_t data_rate = GetGlobalDataProvider()->GetDataRate();
        // uint32_t conn_peers_count = GetP2PControlTarget()->GetConnectedPeersCount();
        uint32_t active_peers_count =  GetP2PControlTarget()->GetActivePeersCount();
        uint32_t full_peers_count = GetP2PControlTarget()->GetFullBlockPeersCount();
        // uint32_t bandwidth = GetGlobalDataProvider()->GetBandWidth();
        // bool range = (state_.range_ == State::RANGE_SUPPORT ? true : false);

        bool b1 = (speed_20s > data_rate / 2);
        bool b2 = (full_peers_count >= 3);
        bool b3 = (active_peers_count >= 4);

        if (b1 && b2 && b3)
        {
            return true;
        }

        return false;
    }

    bool SwitchController::VideoControlMode::P2PMayPlayStably()
    {
        if (false == is_running_)
            return false;
        assert(GetP2PControlTarget());
        // condition
        uint32_t conn_peers_count = GetP2PControlTarget()->GetConnectedPeersCount();
        uint32_t full_peers_count = GetP2PControlTarget()->GetFullBlockPeersCount();
        // uint32_t bandwidth = GetGlobalDataProvider()->GetBandWidth();

        bool b1 = (conn_peers_count >= 7);
        bool b2 = (full_peers_count >= 4);
        if (b1 && b2)
        {
            return true;
        }

        return false;
    }

    void SwitchController::VideoControlMode::OnControlTimer(uint32_t times)
    {
        if (false == IsRunning())
            return;

        // asserts
        assert(GetHTTPControlTarget());

        //////////////////////////////////////////////////////////////////////////
        // Range
        // CheckRange();

        //////////////////////////////////////////////////////////////////////////
        // RID
        // if (state_.GetRID() == State::RID_NONE)
        // {
        //    // state_.GetRID() = (GetGlobalDataProvider()->HasRID() ? State::RID_GOT : State::RID_NONE);
        //    state_.GetRID() = (GetGlobalDataProvider()->GetP2PControlTarget() ? State::RID_GOT : State::RID_NONE);
        // }

        while (true)
        {
            //////////////////////////////////////////////////////////////////////////
            // SpeedH
            if (state_.http_ == State::HTTP_DOWNLOADING)
            {
                assert(GetHTTPControlTarget());
                speed_h_ = GetHTTPControlTarget()->GetCurrentDownloadSpeed();
            }

            SWITCH_DEBUG((string)state_ << " " << times);

            //////////////////////////////////////////////////////////////////////////
            // Initial State
            // <3300*1>
            if (
                state_.http_ == State::HTTP_PAUSING &&
                state_.p2p_ == State::P2P_PAUSING   &&
                state_.timer_ == State::TIMER_NONE &&
                state_.timer_using_ == State::TIMER_USING_NONE)
            {
                // asserts
                assert(GetP2PControlTarget()->IsPausing());
                assert(GetHTTPControlTarget()->IsPausing());
                // action
                GetHTTPControlTarget()->Resume();
                time_counter_t_.reset();

                // state
                state_.http_ = State::HTTP_DOWNLOADING;
                state_.timer_ = State::TIMER_STARTED;
                state_.timer_using_ = State::TIMER_USING_T;
                // next
                continue;
                break;
            }
            // <3000*0>
            else if (
                state_.http_ == State::HTTP_PAUSING &&
                state_.p2p_ == State::P2P_NONE &&
                state_.timer_ == State::TIMER_NONE &&
                state_.timer_using_ == State::TIMER_USING_NONE)
            {
                // asserts
                // assert(state_.GetRID() == State::RID_NONE);
                assert(GetHTTPControlTarget()->IsPausing());
                assert(!GetP2PControlTarget());
                // action
                GetHTTPControlTarget()->Resume();
                // t_ = 2 * 1000;
                // state
                state_.http_ = State::HTTP_DOWNLOADING;
                // next
                continue;
                break;
            }
            //////////////////////////////////////////////////////////////////////////
            // Common State
            // <2351*1>
            else if (
                state_.http_ == State::HTTP_DOWNLOADING &&
                state_.p2p_ == State::P2P_PAUSING &&
                state_.timer_using_ == State::TIMER_USING_T &&
                state_.timer_ == State::TIMER_STARTED
               )
            {
                // asserts
                assert(GetHTTPControlTarget());
                assert(!GetHTTPControlTarget()->IsPausing());
                assert(GetP2PControlTarget());
                assert(GetP2PControlTarget()->IsPausing());

                if (time_counter_t_.elapsed() >= t_)
                {
                    // action

                    // state
                    state_.timer_ = State::TIMER_STOPPED;
                    // next
                    continue;
                }

                break;
            }
            // <2000*0>
            else if (
                state_.http_ == State::HTTP_DOWNLOADING &&
                state_.p2p_ == State::P2P_NONE &&
                state_.timer_ == State::TIMER_NONE &&
                state_.timer_using_ == State::TIMER_USING_NONE
               )
            {
                // asserts
                assert(state_.timer_ == State::TIMER_NONE);
                assert(state_.timer_using_ == State::TIMER_USING_NONE);
                // actions
                if (state_.rid_ == State::RID_NONE && GetP2PControlTarget())
                {
                    // asserts
                    assert(GetP2PControlTarget());
                    // action
                    GetP2PControlTarget()->Pause();
                    uint32_t elapsed = time_counter_elapsed_.elapsed();
                    t_ = (t_ > elapsed ? t_ - elapsed : 0);
                    // state
                    state_.rid_ = State::RID_GOT;
                    state_.p2p_ = State::P2P_PAUSING;
                    state_.timer_using_ = State::TIMER_USING_T;
                    state_.timer_ = State::TIMER_STARTED;
                    // next
                    continue;
                }
                break;
            }
            // <2352*1>
            else if (
                state_.http_ == State::HTTP_DOWNLOADING &&
                state_.p2p_ == State::P2P_PAUSING &&
                state_.timer_using_ == State::TIMER_USING_T&&
                state_.timer_ == State::TIMER_STOPPED)
            {
                // asserts
                assert(GetHTTPControlTarget());
                assert(GetP2PControlTarget());
                assert(!GetHTTPControlTarget()->IsPausing());
                assert(GetP2PControlTarget()->IsPausing());

                // check
                if (GetGlobalDataProvider()->GetDataRate() > GetHTTPControlTarget()->GetMinuteDownloadSpeed())
                {
                    // action
                    GetHTTPControlTarget()->Pause();
                    GetP2PControlTarget()->Resume();
                    h_ = 10 * 1000;
                    time_counter_h_.reset();
                    // state
                    state_.http_ = State::HTTP_PAUSING;
                    state_.p2p_ = State::P2P_DOWNLOADING;
                    state_.timer_ = State::TIMER_STARTED;
                    state_.timer_using_ = State::TIMER_USING_H;
                    // next
                }
                else
                {
                    // action
                    x_ = 5 * 1000;  // 根据码流率计算
                    time_counter_x_.reset();
                    // state
                    state_.timer_ = State::TIMER_STARTED;
                    state_.timer_using_ = State::TIMER_USING_X;
                    // next
                }
                // next
                continue;
                break;
            }
            // <2311*1>
            else if (
                state_.http_ == State::HTTP_DOWNLOADING &&
                state_.p2p_ == State::P2P_PAUSING &&
                state_.timer_using_ == State::TIMER_USING_X &&
                state_.timer_ == State::TIMER_STARTED)
            {
                // asserts

                // condition
                if (GetGlobalDataProvider()->GetDataRate() > GetHTTPControlTarget()->GetMinuteDownloadSpeed())
                {
                    // action
                    speed_h_ = GetHTTPControlTarget()->GetMinuteDownloadSpeed();
                    GetHTTPControlTarget()->Pause();
                    GetP2PControlTarget()->Resume();
                    h_ = 5 * 1000;  // 根据p2p健康程度来计算
                    time_counter_h_.reset();
                    // state
                    state_.http_ = State::HTTP_PAUSING;
                    state_.p2p_ = State::P2P_DOWNLOADING;
                    state_.timer_ = State::TIMER_STARTED;
                    state_.timer_using_ = State::TIMER_USING_H;
                    // next
                    continue;
                }
                // condition
                else if (time_counter_x_.elapsed() > x_)
                {
                    state_.timer_ = State::TIMER_STOPPED;
                    // next
                    continue;
                }

                break;
            }
            // <2312*1>
            else if (
                state_.http_ == State::HTTP_DOWNLOADING &&
                state_.p2p_ == State::P2P_PAUSING &&
                state_.timer_using_ == State::TIMER_USING_X &&
                state_.timer_ == State::TIMER_STOPPED)
            {
                // asserts

                // values
                uint32_t conn_peers = GetP2PControlTarget()->GetConnectedPeersCount();

                // action
                GetHTTPControlTarget()->Pause();
                GetP2PControlTarget()->Resume();
                if (conn_peers == 0)
                {
                    y_ = 0 * 1000;
                }
                else
                {
                    // ! 富余数据足够，可以多尝试
                    y_ = 8 * 1000;
                }

                time_counter_y_.reset();

                // state
                state_.http_ = State::HTTP_PAUSING;
                state_.p2p_ = State::P2P_DOWNLOADING;
                state_.timer_ = State::TIMER_STARTED;
                state_.timer_using_ = State::TIMER_USING_Y;

                // next
                continue;

                break;
            }
            // <3241*1>
            else if (
                state_.http_ == State::HTTP_PAUSING &&
                state_.p2p_ == State::P2P_DOWNLOADING &&
                state_.timer_using_ == State::TIMER_USING_H &&
                state_.timer_ == State::TIMER_STARTED)
            {
                // asserts
                // condition
                if (time_counter_h_.elapsed() > h_)
                {
                    // action
                    // state
                    state_.timer_ = State::TIMER_STOPPED;
                    CheckRange();
                    // next
                    continue;
                }
                break;
            }
            // <3221*1>
            else if (
                state_.http_ == State::HTTP_PAUSING &&
                state_.p2p_ == State::P2P_DOWNLOADING  &&
                state_.timer_using_ == State::TIMER_USING_Y &&
                state_.timer_ == State::TIMER_STARTED)
            {
                // asserts
                // condition
                if (time_counter_y_.elapsed() > y_)
                {
                    // action
                    // state
                    state_.timer_ = State::TIMER_STOPPED;
                    CheckRange();
                    // next
                    continue;
                }
                break;
            }
            //////////////////////////////////////////////////////////////////////////
            // UnRange

            //////////////////////////////////////////////////////////////////////////
            // Unstable States
            // <324241>
            else if (
                state_.http_ == State::HTTP_PAUSING &&
                state_.p2p_ == State::P2P_DOWNLOADING &&
                state_.timer_using_ == State::TIMER_USING_H &&
                state_.timer_ == State::TIMER_STOPPED &&
                state_.range_ == State::RANGE_UNSUPPORT)
            {
                // asserts
                assert(GetHTTPControlTarget()->IsPausing());
                assert(!GetP2PControlTarget()->IsPausing());

                // condition
                if (P2PCanDropHttp()
                    || P2PCanPlayStably()
                    || (P2PCanDownloadStably() && GetP2PControlTarget()->GetCurrentDownloadSpeed() > speed_h_))
                {
                    // action
                    // state
                    state_.timer_ = State::TIMER_NONE;
                    state_.timer_using_ = State::TIMER_USING_NONE;
                }
                else
                {
                    // asserts
                    // action
                    GetHTTPControlTarget()->Resume();
                    GetP2PControlTarget()->Pause();
                    // state
                    state_.http_ = State::HTTP_DOWNLOADING;
                    state_.p2p_ = State::P2P_PAUSING;
                    state_.timer_ = State::TIMER_NONE;
                    state_.timer_using_ = State::TIMER_USING_NONE;
                }
                // next
                continue;
                break;
            }
            // <322241>
            else if (
                state_.http_ == State::HTTP_PAUSING &&
                state_.p2p_ == State::P2P_DOWNLOADING &&
                state_.timer_using_ == State::TIMER_USING_Y &&
                state_.timer_ == State::TIMER_STOPPED &&
                state_.range_ == State::RANGE_UNSUPPORT)
            {
                uint32_t data_rate = GetGlobalDataProvider()->GetDataRate();
                uint32_t bandwidth = GetGlobalDataProvider()->GetBandWidth();
                bool can_drop = P2PCanDropHttp();
                if (can_drop)
                {
                    // action
                    // state
                    state_.timer_ = State::TIMER_NONE;
                    state_.timer_using_ = State::TIMER_USING_NONE;
                }
                else
                {
                    if (speed_h_ >= 2 * data_rate || bandwidth < 2 * data_rate)
                    {
                        // action
                        GetHTTPControlTarget()->Resume();
                        GetP2PControlTarget()->Pause();
                        // state
                        state_.http_ = State::HTTP_DOWNLOADING;
                        state_.p2p_ = State::P2P_PAUSING;
                        state_.timer_ = State::TIMER_NONE;
                        state_.timer_using_ = State::TIMER_USING_NONE;
                    }
                    else if (speed_h_ < 2 * data_rate && bandwidth >= 2 * data_rate)
                    {
                        // action
                        GetHTTPControlTarget()->Resume();
                        time_counter_z_.reset();
                        z_ = 3 * 1000;
                        // state
                        state_.http_ = State::HTTP_DOWNLOADING;
                        state_.timer_ = State::TIMER_STARTED;
                        state_.timer_using_ = State::TIMER_USING_Z;
                    }
                }
                continue;
                break;
            }
            // <223141>
            else if (
                state_.http_ == State::HTTP_DOWNLOADING &&
                state_.p2p_ == State::P2P_DOWNLOADING &&
                state_.timer_using_ == State::TIMER_USING_Z &&
                state_.timer_ == State::TIMER_STARTED &&
                state_.range_ == State::RANGE_UNSUPPORT)
            {
                // ! 1. CheckRange  2. 条件
                // asserts
                assert(GetP2PControlTarget());
                assert(!GetP2PControlTarget()->IsPausing());
                assert(GetHTTPControlTarget());
                assert(!GetHTTPControlTarget()->IsPausing());
                // check
                uint32_t pure_speed = GetGlobalDataProvider()->GetDataDownloadSpeed();
                uint32_t data_rate = GetGlobalDataProvider()->GetDataRate();
                uint32_t downloading_position = GetGlobalDataProvider()->GetDownloadingPosition();
                uint32_t playing_time = GetGlobalDataProvider()->GetPlayElapsedTimeInMilliSec();
                uint32_t can_drop_http = P2PCanDropHttp();

                bool b1 = (pure_speed < data_rate);      // 有效速度
                bool b2 = (downloading_position < data_rate * playing_time);    // 卡
                bool b3 = (downloading_position < data_rate * (playing_time + 5));   // 快卡了

                SWITCH_DEBUG(" pure_speed=" << pure_speed << " speed_h=" << speed_h_ << " data_rate=" << data_rate << " download_position=" << downloading_position << " playing_time=" << playing_time);
                SWITCH_DEBUG("... " << b1 << b2 << b3);

                if (can_drop_http)
                {
                    // action
                    GetHTTPControlTarget()->Pause();
                    // state
                    state_.http_ = State::HTTP_PAUSING;
                    state_.timer_ = State::TIMER_NONE;
                    state_.timer_using_ = State::TIMER_USING_NONE;
                    // next
                    continue;
                }
                else if (b1 || b2 || b3)
                {
                    // action
                    GetP2PControlTarget()->Pause();
                    // state
                    state_.p2p_ = State::P2P_PAUSING;
                    state_.timer_ = State::TIMER_NONE;
                    state_.timer_using_ = State::TIMER_USING_NONE;
                    // next
                    continue;
                }
                else if (time_counter_z_.elapsed() > z_)
                {
                    // state
                    state_.timer_ = State::TIMER_STOPPED;
                    CheckRange();
                    // next
                    continue;
                }
                break;
            }
            // <223241>
            else if (
                state_.http_ == State::HTTP_DOWNLOADING &&
                state_.p2p_ == State::P2P_DOWNLOADING &&
                state_.timer_using_ == State::TIMER_USING_Z &&
                state_.timer_ == State::TIMER_STOPPED &&
                state_.range_ == State::RANGE_UNSUPPORT)
            {
                bool can_drop = P2PCanDropHttp();
                if (can_drop)
                {
                    // action
                    GetHTTPControlTarget()->Pause();
                    // state
                    state_.http_ = State::HTTP_PAUSING;
                    state_.timer_ = State::TIMER_NONE;
                    state_.timer_using_ = State::TIMER_USING_NONE;
                }
                else
                {
                    // action
                    GetP2PControlTarget()->Pause();
                    // state
                    state_.p2p_ = State::P2P_PAUSING;
                    state_.timer_ = State::TIMER_NONE;
                    state_.timer_using_ = State::TIMER_USING_NONE;
                }
                continue;
                break;
            }

            //////////////////////////////////////////////////////////////////////////
            // Stable States
            // <320041>
            else if (
                state_.http_ == State::HTTP_PAUSING &&
                state_.p2p_ == State::P2P_DOWNLOADING &&
                state_.timer_ == State::TIMER_NONE &&
                state_.timer_using_ == State::TIMER_USING_NONE &&
                state_.range_ == State::RANGE_UNSUPPORT)
            {
                // nothing
                CheckRange();
                break;
            }
            // <230041>
            else if (
                state_.http_ == State::HTTP_DOWNLOADING &&
                state_.p2p_ == State::P2P_PAUSING &&
                state_.timer_ == State::TIMER_NONE &&
                state_.timer_using_ == State::TIMER_USING_NONE &&
                state_.range_ == State::RANGE_UNSUPPORT)
            {
                // nothing
                CheckRange();
                break;
            }

            //////////////////////////////////////////////////////////////////////////
            // Range

            //////////////////////////////////////////////////////////////////////////
            // Unstable States
            // <324231>
            else if (
                state_.http_ == State::HTTP_PAUSING &&
                state_.p2p_ == State::P2P_DOWNLOADING &&
                state_.timer_using_ == State::TIMER_USING_H &&
                state_.timer_ == State::TIMER_STOPPED  &&
                state_.range_ == State::RANGE_SUPPORT)
            {
                uint32_t bandwidth = GetGlobalDataProvider()->GetBandWidth();
                uint32_t data_rate = GetGlobalDataProvider()->GetDataRate();
                bool can_download_stably = P2PCanDownloadStably();
                bool can_drop_http = P2PCanDropHttp();
                bool can_play = P2PCanPlayStably();

                if (can_drop_http
                    || (bandwidth < 2*data_rate && can_play))
                {
                    // action
                    // state
                    state_.timer_ = State::TIMER_NONE;
                    state_.timer_using_ = State::TIMER_USING_NONE;
                }
                else if ((bandwidth >= 2 * data_rate && can_download_stably)
                    || (bandwidth >= 2*data_rate && !can_download_stably))
                {
                    // action
                    GetHTTPControlTarget()->Resume();
                    // state
                    state_.http_ = State::HTTP_DOWNLOADING;
                    state_.timer_ = State::TIMER_NONE;
                    state_.timer_using_ = State::TIMER_USING_NONE;
                }
                else if (bandwidth < 2*data_rate && !can_play)
                {
                    // action
                    GetHTTPControlTarget()->Resume();
                    GetP2PControlTarget()->Pause();
                    // state
                    state_.http_ = State::HTTP_DOWNLOADING;
                    state_.p2p_ = State::P2P_PAUSING;
                    state_.timer_ = State::TIMER_NONE;
                    state_.timer_using_ = State::TIMER_USING_NONE;
                }
                else
                {
                    SWITCH_DEBUG("Oh, NO!!!");
                    assert(!"Oh, NO!!");
                }
                // next
                continue;
                break;
            }
            // <322231>
            else if (
                state_.http_ == State::HTTP_PAUSING &&
                state_.p2p_ == State::P2P_DOWNLOADING &&
                state_.timer_using_ == State::TIMER_USING_Y &&
                state_.timer_ == State::TIMER_STOPPED  &&
                state_.range_ == State::RANGE_SUPPORT)
            {
                uint32_t bandwidth = GetGlobalDataProvider()->GetBandWidth();
                uint32_t data_rate = GetGlobalDataProvider()->GetDataRate();
                bool can_download_stably = P2PCanDownloadStably();
                bool can_drop_http = P2PCanDropHttp();
                bool can_play = P2PCanPlayStably();

                if (can_drop_http || (bandwidth < 2*data_rate && can_play))
                {
                    // action
                    // state
                    state_.timer_ = State::TIMER_NONE;
                    state_.timer_using_ = State::TIMER_USING_NONE;
                }
                else if (bandwidth >= 2*data_rate && can_download_stably)
                {
                    // action
                    GetHTTPControlTarget()->Resume();
                    // state
                    state_.http_ = State::HTTP_DOWNLOADING;
                    state_.timer_ = State::TIMER_NONE;
                    state_.timer_using_ = State::TIMER_USING_NONE;
                }
                else if ((bandwidth < 2*data_rate && !can_play)
                    || (bandwidth >= 2*data_rate && !can_download_stably)
                   )
                {
                    // action
                    GetHTTPControlTarget()->Resume();
                    GetP2PControlTarget()->Pause();
                    // state
                    state_.http_ = State::HTTP_DOWNLOADING;
                    state_.p2p_ = State::P2P_PAUSING;
                    state_.timer_ = State::TIMER_NONE;
                    state_.timer_using_ = State::TIMER_USING_NONE;
                }
                // next
                continue;
                break;
            }
            // <223131>
            else if (state_.http_ == State::HTTP_DOWNLOADING && state_.p2p_ == State::P2P_DOWNLOADING
                && state_.timer_ == State::TIMER_STARTED && state_.timer_using_ == State::TIMER_USING_Z && state_.range_ == State::RANGE_SUPPORT)
            {
                // asserts
                assert(GetHTTPControlTarget());
                assert(GetP2PControlTarget());
                assert(!GetHTTPControlTarget()->IsPausing());
                assert(!GetP2PControlTarget()->IsPausing());

                // condition
                if (time_counter_z_.elapsed() > z_)
                {
                    // action
                    // state
                    state_.timer_ = State::TIMER_STOPPED;
                    // next
                    continue;
                }
                break;
            }
            // <223231>
            else if (
                state_.http_ == State::HTTP_DOWNLOADING &&
                state_.p2p_ == State::P2P_DOWNLOADING &&
                state_.timer_using_ == State::TIMER_USING_Z &&
                state_.timer_ == State::TIMER_STOPPED &&
                state_.range_ == State::RANGE_SUPPORT)
            {
                // asserts
                assert(GetP2PControlTarget());
                assert(GetHTTPControlTarget());
                assert(!GetHTTPControlTarget()->IsPausing());
                assert(!GetP2PControlTarget()->IsPausing());

                // values
                uint32_t bandwidth = GetGlobalDataProvider()->GetBandWidth();
                uint32_t data_rate = GetGlobalDataProvider()->GetDataRate();
                bool can_download_stably = P2PCanDownloadStably();
                // bool can_drop_http = P2PCanDropHttp();
                // bool can_play = P2PCanPlayStably();

                if (bandwidth >= 2*data_rate && can_download_stably)
                {
                    // action

                    // state
                    state_.timer_ = State::TIMER_NONE;
                    state_.timer_using_ = State::TIMER_USING_NONE;
                }
                else if (bandwidth < 2*data_rate && can_download_stably)
                {
                    // action
                    GetHTTPControlTarget()->Pause();
                    // state
                    state_.http_ = State::HTTP_PAUSING;
                    state_.timer_ = State::TIMER_NONE;
                    state_.timer_using_ = State::TIMER_USING_NONE;
                }
                else if (!can_download_stably)
                {
                    // action
                    GetP2PControlTarget()->Pause();
                    // state
                    state_.p2p_ = State::P2P_PAUSING;
                    state_.timer_ = State::TIMER_NONE;
                    state_.timer_using_ = State::TIMER_USING_NONE;
                }
                else
                {
                    SWITCH_DEBUG("guiyiaguiyi!!!");
                    assert(!"guiyiaguiyi!!");
                }
                // next
                continue;
                break;
            }
            //////////////////////////////////////////////////////////////////////////
            // Stable States
            // <320031>
            else if (
                state_.http_ == State::HTTP_PAUSING &&
                state_.p2p_ == State::P2P_DOWNLOADING &&
                state_.timer_ == State::TIMER_NONE &&
                state_.timer_using_ == State::TIMER_USING_NONE &&
                state_.range_ == State::RANGE_SUPPORT)
            {
                uint32_t bandwidth = GetGlobalDataProvider()->GetBandWidth();
                uint32_t data_rate = GetGlobalDataProvider()->GetDataRate();
                // bool can_download_stably = P2PCanDownloadStably();
                // bool can_drop_http = P2PCanDropHttp();
                // uint32_t active_peers_count = GetP2PControlTarget()->GetActivePeersCount();
                bool can_play = P2PCanPlayStably();

                if (bandwidth < 2*data_rate && !can_play)
                {
                    // action
                    GetHTTPControlTarget()->Resume();
                    GetP2PControlTarget()->Pause();
                    // state
                    state_.http_ = State::HTTP_DOWNLOADING;
                    state_.p2p_ = State::P2P_PAUSING;
                }
                else if (bandwidth >= 2*data_rate)
                {
                    // action
                    GetHTTPControlTarget()->Resume();
                    // state
                    state_.http_ = State::HTTP_DOWNLOADING;
                }

                break;
            }
            // <230031>
            else if (
                state_.http_ == State::HTTP_DOWNLOADING &&
                state_.p2p_ == State::P2P_PAUSING &&
                state_.timer_ == State::TIMER_NONE &&
                state_.timer_using_ == State::TIMER_USING_NONE &&
                state_.range_ == State::RANGE_SUPPORT)
            {
                // asserts
                assert(GetHTTPControlTarget());
                assert(GetP2PControlTarget());
                assert(!GetHTTPControlTarget()->IsPausing());
                assert(GetP2PControlTarget()->IsPausing());

                // values
                uint32_t bandwidth = GetGlobalDataProvider()->GetBandWidth();
                uint32_t pure_speed = GetGlobalDataProvider()->GetDataDownloadSpeed();
                uint32_t data_rate = GetGlobalDataProvider()->GetDataRate();
                uint32_t http_speed = GetHTTPControlTarget()->GetCurrentDownloadSpeed();
                uint32_t downloading_position = GetGlobalDataProvider()->GetDownloadingPosition();
                uint32_t playing_time = GetGlobalDataProvider()->GetPlayElapsedTimeInMilliSec();
                bool p2p_may_play = P2PMayPlayStably();

                (void)pure_speed;
                SWITCH_DEBUG("bandwidth=" << bandwidth << " pure_speed=" << pure_speed << " data_rate=" << data_rate << " http_speed=" << http_speed << " downloading_position=" << downloading_position << " playing_time=" << playing_time << " p2p_may_play=" << p2p_may_play);

                bool b1 = downloading_position < data_rate * (playing_time + 5);  // 快卡
                bool b2 = p2p_may_play;

                bool b3 = bandwidth > 2*data_rate;
                bool b4 = http_speed < data_rate;

                SWITCH_DEBUG("... " << b1 << b2 << " " << b3 << b4);

                if (b1 && b2)
                {
                    GetP2PControlTarget()->Resume();
                    time_counter_z_.reset();
                    z_ = 3 * 1000;
                    // state
                    state_.p2p_ = State::P2P_DOWNLOADING;
                    state_.timer_ = State::TIMER_STARTED;
                    state_.timer_using_ = State::TIMER_USING_Z;
                }
                else if ((b3 && b2) || (b4 && b1))
                {
                    GetP2PControlTarget()->Resume();
                    // state
                    state_.p2p_ = State::P2P_DOWNLOADING;
                }
                break;
            }
            // <220031>
            else if (
                state_.http_ == State::HTTP_DOWNLOADING &&
                state_.p2p_ == State::P2P_DOWNLOADING &&
                state_.timer_ == State::TIMER_NONE &&
                state_.timer_using_ == State::TIMER_USING_NONE &&
                state_.range_ == State::RANGE_SUPPORT)
            {
                uint32_t bandwidth = GetGlobalDataProvider()->GetBandWidth();
                uint32_t data_rate = GetGlobalDataProvider()->GetDataRate();
                bool can_play = P2PCanPlayStably();

                if (bandwidth < 2 * data_rate && can_play)
                {
                    GetHTTPControlTarget()->Pause();
                    // state
                    state_.http_ = State::HTTP_PAUSING;
                }
                else if (bandwidth < 2*data_rate && !can_play)
                {
                    GetP2PControlTarget()->Pause();
                    // state
                    state_.p2p_ = State::P2P_PAUSING;
                }
                break;
            }
            else
            {
                SWITCH_DEBUG("Unreachable State!!! State: " << (string)state_);
                assert(!"Unreachable State!!!");
                break;
            }
        }

    }

}
