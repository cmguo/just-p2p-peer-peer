//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "p2sp/download/SwitchController.h"


namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("download");

    //////////////////////////////////////////////////////////////////////////
    // OpenServiceControlMode

    SwitchController::OpenServicePushControlMode::p SwitchController::OpenServicePushControlMode::Create(SwitchController::p controller)
    {
        return OpenServicePushControlMode::p(new OpenServicePushControlMode(controller));
    }

    void SwitchController::OpenServicePushControlMode::Start()
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

        SWITCH_DEBUG(string(20, '-'));

        assert(GetHTTPControlTarget());
        assert(GetP2PControlTarget());

        GetP2PControlTarget()->SetDownloadPriority(protocol::RequestSubPiecePacket::PUSH_PRIORITY);
        GetP2PControlTarget()->SetDownloadMode(IP2PControlTarget::FAST_MODE);

        // next
        // Next(0);
        OnControlTimer(0);
    }

    void SwitchController::OpenServicePushControlMode::Stop()
    {
        if (false == IsRunning())
            return;
        // release
        // stop
        ControlMode::Stop();
    }

    void SwitchController::OpenServicePushControlMode::OnControlTimer(uint32_t times)
    {
        if (false == IsRunning())
            return;

        while (true)
        {
            // asserts
            assert(GetHTTPControlTarget());

            // SpeedH
            if (state_.http_ == State::HTTP_DOWNLOADING)
            {
                assert(GetHTTPControlTarget());
            }

            SWITCH_DEBUG((string)state_ << " " << times);

            // <3300>
            if (
                state_.http_ == State::HTTP_PAUSING &&
                state_.p2p_ == State::P2P_PAUSING &&
                state_.timer_using_ == State::TIMER_USING_NONE &&
                state_.timer_ == State::TIMER_NONE)
            {
                // assert
                // action
                GetP2PControlTarget()->Resume();
                time_counter_x_.reset();
                x_ = 10000;
                // state
                state_.p2p_ = State::P2P_DOWNLOADING;
                state_.timer_using_ = State::TIMER_USING_X;
                state_.timer_ = State::TIMER_STARTED;
                // next
                continue;
            }
            // <3211>
            else if (
                state_.http_ == State::HTTP_PAUSING &&
                state_.p2p_ == State::P2P_DOWNLOADING &&
                state_.timer_using_ == State::TIMER_USING_X &&
                state_.timer_ == State::TIMER_STARTED)
            {
                // assert
                if (time_counter_x_.elapsed() >= x_)
                {
                    // action
                    // state
                    state_.timer_ = State::TIMER_STOPPED;
                    // next
                    continue;
                }
                break;
            }
            // <3212>
            else if (
                state_.http_ == State::HTTP_PAUSING &&
                state_.p2p_ == State::P2P_DOWNLOADING &&
                state_.timer_using_ == State::TIMER_USING_X &&
                state_.timer_ == State::TIMER_STOPPED)
            {
                // assert
                IP2PControlTarget::p p2p = GetP2PControlTarget();
                if (p2p->GetConnectedPeersCount() == 0 || p2p->GetPooledPeersCount() == 0)
                {
                    IHTTPControlTarget::p http = GetHTTPControlTarget();
                    http->Resume();
                    p2p->Pause();
                    // state
                    state_.http_ = State::HTTP_DOWNLOADING;
                    state_.p2p_ = State::P2P_PAUSING;
                    state_.timer_using_ = State::TIMER_USING_NONE;
                    state_.timer_ = State::TIMER_NONE;
                    // next
                    continue;
                }
                else
                {
                    // action
                    y_ = 20*1000;
                    time_counter_y_.reset();
                    // state
                    state_.timer_using_ = State::TIMER_USING_Y;
                    state_.timer_ = State::TIMER_STARTED;
                    // next
                    continue;
                }
            }
            // <3221>
            else if (
                state_.http_ == State::HTTP_PAUSING &&
                state_.p2p_ == State::P2P_DOWNLOADING &&
                state_.timer_using_ == State::TIMER_USING_Y &&
                state_.timer_ == State::TIMER_STARTED)
            {
                // assert
                if (time_counter_y_.elapsed() >= y_)
                {
                    state_.timer_ = State::TIMER_STOPPED;
                    // next
                    continue;
                }
                else
                {
                    // ! 判断P2P连接数
                }
                break;
            }
            // <3222>
            else if (
                state_.http_ == State::HTTP_PAUSING &&
                state_.p2p_ == State::P2P_DOWNLOADING &&
                state_.timer_using_ == State::TIMER_USING_Y &&
                state_.timer_ == State::TIMER_STOPPED)
            {
                // assert
                IP2PControlTarget::p p2p = GetP2PControlTarget();
                IHTTPControlTarget::p http = GetHTTPControlTarget();
                // action
                uint32_t active_peer_count = p2p->GetActivePeersCount();
                uint32_t speed5 = p2p->GetCurrentDownloadSpeed();
                uint32_t data_rate = 20 * 1024;  // GetGlobalDataProvider()->GetDataRate();
                // uint32_t bandwidth = GetGlobalDataProvider()->GetBandWidth();

                bool b1 = (active_peer_count >= 1);
                bool b2 = (speed5 >= data_rate);

                if (b1 && b2)
                {
                    state_.timer_using_ = State::TIMER_USING_NONE;
                    state_.timer_ = State::TIMER_NONE;
                    // next
                    continue;
                }
                else if (active_peer_count == 0)
                {
                    http->Resume();
                    p2p->Pause();
                    // state
                    state_.http_ = State::HTTP_DOWNLOADING;
                    state_.p2p_ = State::P2P_PAUSING;
                    state_.timer_using_ = State::TIMER_USING_NONE;
                    state_.timer_ = State::TIMER_NONE;
                    // next
                    continue;
                }
                else
                {
                    // ! PeerCount Limit
                    http->Resume();
                    //                 if (bandwidth < 2 * data_rate)
                    //                 {
                    //                     p2p->SetAssignPeerCountLimit(8);
                    //                 }
                    // state
                    state_.http_ = State::HTTP_DOWNLOADING;
                    state_.timer_using_ = State::TIMER_USING_NONE;
                    state_.timer_ = State::TIMER_NONE;
                    // next
                    continue;
                }
                break;
            }
            // <2200>
            else if (
                state_.http_ == State::HTTP_DOWNLOADING &&
                state_.p2p_ == State::P2P_DOWNLOADING &&
                state_.timer_using_ == State::TIMER_USING_NONE &&
                state_.timer_ == State::TIMER_NONE)
            {
                // assert
                IP2PControlTarget::p p2p = GetP2PControlTarget();
                IHTTPControlTarget::p http = GetHTTPControlTarget();
                // action
                // uint32_t active_peer_count = p2p->GetActivePeersCount();
                uint32_t available_peer_count = p2p->GetAvailableBlockPeerCount();
                uint32_t speed_p = p2p->GetCurrentDownloadSpeed();
                uint32_t speed_h = http->GetCurrentDownloadSpeed();
                // uint32_t data_rate = GetGlobalDataProvider()->GetDataRate();
                // uint32_t bandwidth = GetGlobalDataProvider()->GetBandWidth();
                // action
                bool b1 = (speed_h < speed_p);
                bool b2 = (speed_p >= 20 * 1024);
                bool b3 = (speed_p >= 40 * 1024);

                if ((b1&&b2) || b3)
                {
                    // action
                    p2p->SetAssignPeerCountLimit(0);
                    http->Pause();
                    // state
                    state_.http_ = State::HTTP_PAUSING;
                    // next
                    continue;
                }
                else if (available_peer_count == 0)
                {
                    // action
                    p2p->SetAssignPeerCountLimit(0);
                    p2p->Pause();
                    state_.p2p_ = State::P2P_PAUSING;
                    // next
                    continue;
                }
                // state
                // next
                break;
            }
            // <3200>
            else if (
                state_.http_ == State::HTTP_PAUSING &&
                state_.p2p_ == State::P2P_DOWNLOADING &&
                state_.timer_using_ == State::TIMER_USING_NONE &&
                state_.timer_ == State::TIMER_NONE)
            {
                // assert
                IP2PControlTarget::p p2p = GetP2PControlTarget();
                IHTTPControlTarget::p http = GetHTTPControlTarget();
                // action
                // uint32_t active_peer_count = p2p->GetActivePeersCount();
                uint32_t available_peer_count = p2p->GetAvailableBlockPeerCount();
                uint32_t speed_p = p2p->GetCurrentDownloadSpeed();
                // uint32_t speed_h = http->GetCurrentDownloadSpeed();
                // uint32_t data_rate = GetGlobalDataProvider()->GetDataRate();
                // uint32_t bandwidth = GetGlobalDataProvider()->GetBandWidth();

                bool b1 = (speed_p < 10 * 1024);

                if (b1)
                {
                    http->Resume();
                    // state
                    state_.http_ = State::HTTP_DOWNLOADING;
                    // next
                    continue;
                }
                else if (available_peer_count == 0)
                {
                    http->Resume();
                    p2p->Pause();
                    // state
                    state_.http_ = State::HTTP_DOWNLOADING;
                    state_.p2p_ = State::P2P_PAUSING;
                    // next
                    continue;
                }
                else
                {
                    // ! 要卡了
                }
                break;
            }
            // <2300>
            else if (
                state_.http_ == State::HTTP_DOWNLOADING &&
                state_.p2p_ == State::P2P_PAUSING &&
                state_.timer_using_ == State::TIMER_USING_NONE &&
                state_.timer_ == State::TIMER_NONE)
            {
                // assert
                IP2PControlTarget::p p2p = GetP2PControlTarget();
                IHTTPControlTarget::p http = GetHTTPControlTarget();
                // action
                // uint32_t connected_peer_count = p2p->GetConnectedPeersCount();
                // uint32_t active_peer_count = p2p->GetActivePeersCount();
                uint32_t available_peer_count = p2p->GetAvailableBlockPeerCount();
                // uint32_t speed5 = p2p->GetCurrentDownloadSpeed();
                // uint32_t speed_h = http->GetCurrentDownloadSpeed();
                // uint32_t data_rate = GetGlobalDataProvider()->GetDataRate();
                // uint32_t bandwidth = GetGlobalDataProvider()->GetBandWidth();

                bool b1 = (available_peer_count >= 1);

                if (b1)
                {
                    p2p->Resume();
                    // state
                    state_.p2p_ = State::P2P_DOWNLOADING;
                    // next
                    continue;
                }
                break;
            }
            else
            {
                SWITCH_DEBUG("No Such State: " << (string)state_);
                assert(!"SwitchController::OpenServicePushControlMode::OnControlTimer: No Such State");
                break;
            }
        }

    }

}
