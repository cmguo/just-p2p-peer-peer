//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "p2sp/download/SwitchController.h"


namespace p2sp
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_switch = log4cplus::Logger::getInstance("[switch_push_control]");
#endif

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
        state_.timer_ = State::TIMER_NONE;
        state_.timer_using_ = State::TIMER_USING_NONE;
        // parameters

        LOG4CPLUS_DEBUG_LOG(logger_switch, string(20, '-'));

        assert(GetHTTPControlTarget());
        assert(GetP2PControlTarget());

        if (GetP2PControlTarget())
        {
            GetP2PControlTarget()->SetDownloadPriority(protocol::RequestSubPiecePacket::PUSH_PRIORITY);
            GetP2PControlTarget()->SetDownloadMode(IP2PControlTarget::FAST_MODE);
        }

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

    void SwitchController::OpenServicePushControlMode::OnControlTimer(boost::uint32_t times)
    {
        if (false == IsRunning())
            return;

        if (is_paused_by_sdk_)
        {
            return;
        }

        while (true)
        {
            // asserts
            assert(GetHTTPControlTarget());

            // SpeedH
            if (state_.http_ == State::HTTP_DOWNLOADING)
            {
                assert(GetHTTPControlTarget());
            }
            LOG4CPLUS_DEBUG_LOG(logger_switch, (string)state_ << " " << times);

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
                boost::uint32_t active_peer_count = p2p->GetActivePeersCount();
                boost::uint32_t speed5 = p2p->GetCurrentDownloadSpeed();

                bool b1 = (active_peer_count >= 1);
                bool b2 = (speed5 >= 10 * 1024);

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
                boost::uint32_t available_peer_count = p2p->GetAvailableBlockPeerCount();
                boost::uint32_t speed_p = p2p->GetCurrentDownloadSpeed();
                boost::uint32_t speed_h = http->GetCurrentDownloadSpeed();
                // action
                bool b2 = (speed_p >= 20 * 1024);

                if (available_peer_count == 0)
                {
                    // action
                    p2p->Pause();
                    state_.p2p_ = State::P2P_PAUSING;
                    // next
                    continue;
                }
                else if (b2)
                {
                    // action
                    http->Pause();
                    // state
                    state_.http_ = State::HTTP_PAUSING;
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
                boost::uint32_t available_peer_count = p2p->GetAvailableBlockPeerCount();
                boost::uint32_t speed_p = p2p->GetCurrentDownloadSpeed();

                bool b1 = (speed_p < 10 * 1024);

                if (available_peer_count == 0)
                {
                    http->Resume();
                    p2p->Pause();
                    // state
                    state_.http_ = State::HTTP_DOWNLOADING;
                    state_.p2p_ = State::P2P_PAUSING;
                    // next
                    continue;
                }
                else if (b1)
                {
                    http->Resume();
                    // state
                    state_.http_ = State::HTTP_DOWNLOADING;
                    // next
                    continue;
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
                boost::uint32_t available_peer_count = p2p->GetAvailableBlockPeerCount();

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
                LOG4CPLUS_DEBUG_LOG(logger_switch, "No Such State: " << (string)state_);
                assert(!"SwitchController::OpenServicePushControlMode::OnControlTimer: No Such State");
                break;
            }
        }

    }

}
