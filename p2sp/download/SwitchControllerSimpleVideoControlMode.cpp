//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "p2sp/download/SwitchController.h"
#include "p2sp/p2p/P2SPConfigs.h"

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("download");

    //////////////////////////////////////////////////////////////////////////
    // SimpleVideoControlMode

    SwitchController::SimpleVideoControlMode::p SwitchController::SimpleVideoControlMode::Create(SwitchController::p controller)
    {
        return SimpleVideoControlMode::p(new SimpleVideoControlMode(controller));
    }

    void SwitchController::SimpleVideoControlMode::Start()
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
        t_ = 5 * 1000;
        time_counter_elapsed_.reset();

        SWITCH_DEBUG(string(20, '-'));

        // next
        Next(0);
        p2p_tried = false;
        is_2200_from_2300 = false;
    }

    void SwitchController::SimpleVideoControlMode::Stop()
    {
        if (false == IsRunning())
            return;
        // release
        // stop
        ControlMode::Stop();
    }

    uint32_t SwitchController::SimpleVideoControlMode::P2PEstimatedDownloadTimeInSec()
    {
        if (false == IsRunning())
            return 0;

        assert(GetP2PControlTarget());
        assert(!GetP2PControlTarget()->IsPausing());

        // value
        IP2PControlTarget::p p2p = GetP2PControlTarget();
        IGlobalControlTarget::p global = GetGlobalDataProvider();

        // condition
        uint32_t speed_5s = p2p->GetCurrentDownloadSpeed();

        if (speed_5s == 0)
            return 0;

        uint32_t file_length = global->GetFileLength();
        uint32_t downloaded_bytes = global->GetDownloadedBytes();

        uint32_t data_need_to_download = file_length - downloaded_bytes;

        uint32_t estimated_download_time = static_cast<uint32_t>((data_need_to_download + 0.0) / speed_5s + 0.5);

        SWITCH_DEBUG("data_need_to_download=" << data_need_to_download << " estimated_download_time=" << estimated_download_time);

        return estimated_download_time;
    }

    bool SwitchController::SimpleVideoControlMode::P2PCanDropHttp()
    {
        if (false == IsRunning())
            return false;
        // asserts
        assert(GetP2PControlTarget());
        assert(!GetP2PControlTarget()->IsPausing());
        // value
        IP2PControlTarget::p p2p = GetP2PControlTarget();
        IGlobalControlTarget::p global = GetGlobalDataProvider();
        // condition
        boost::uint32_t speed_5s = p2p->GetCurrentDownloadSpeed();
        boost::uint32_t conn_peers_count = p2p->GetConnectedPeersCount();
        boost::uint32_t active_peers_count =  p2p->GetActivePeersCount();
        boost::uint32_t full_peers_count = p2p->GetFullBlockPeersCount();
        boost::uint32_t pooled_peers_count = p2p->GetPooledPeersCount();
        boost::uint32_t data_rate = global->GetDataRate();
        boost::uint32_t bandwidth = global->GetBandWidth();
        boost::uint32_t file_length = global->GetFileLength();
        boost::uint32_t downloaded_bytes = global->GetDownloadedBytes();
        bool range = (state_.range_ == State::RANGE_SUPPORT ? true : false);

        boost::uint32_t estimated_download_time_in_sec = P2PEstimatedDownloadTimeInSec();

        bool can_download = (estimated_download_time_in_sec <= P2SPConfigs::SWITCH_CONTROLLER_SIMPLE_MAX_P2P_DOWNLOAD_TIME_IN_SEC);

        SWITCH_DEBUG("speed_5s=" << speed_5s << " speed_h=" << speed_h_ << " data_rate=" << data_rate << " conn_peers=" << conn_peers_count << " active_peers=" << active_peers_count << " full_peers=" << full_peers_count << " pooled_peers_count=" << pooled_peers_count << " bandwidth=" << bandwidth << " range=" << range << " can_download=" << can_download);

        //////////////////////////////////////////////////////////////////////////
        // Range Support
        if (range)
        {
            bool b6 = (active_peers_count >= 3);
            bool b7 = (speed_5s > speed_h_ * 6 / 5);
            bool b8 = true;  // (pooled_peers_count >= 35);

            SWITCH_DEBUG("... range=1 " << b6 << b7 << b8 << " " << can_download);

            if (b6 && b7 && b8)
            {
                return true;
            }
        }
        //////////////////////////////////////////////////////////////////////////
        // Range Unsupport
        else
        {
            bool b1 = (full_peers_count >= 3);
            bool b2 = (active_peers_count >= 3);
            bool b3 = (speed_5s > speed_h_ * 6 / 5);

            SWITCH_DEBUG("... range=0 " << b1 << b2 << b3);

            if (b1 && b2 && b3)
            {
                return true;
            }
        }

        return false;
    }

    bool SwitchController::SimpleVideoControlMode::P2PCanPlayStably()
    {
        if (false == IsRunning())
            return false;
        // asserts
        assert(GetP2PControlTarget());
        assert(!GetP2PControlTarget()->IsPausing());
        // condition
        boost::uint32_t speed_5s = GetP2PControlTarget()->GetCurrentDownloadSpeed();
        boost::uint32_t data_rate = GetGlobalDataProvider()->GetDataRate();
        boost::uint32_t conn_peers_count = GetP2PControlTarget()->GetConnectedPeersCount();
        boost::uint32_t active_peers_count =  GetP2PControlTarget()->GetActivePeersCount();
        boost::uint32_t full_peers_count = GetP2PControlTarget()->GetFullBlockPeersCount();
        boost::uint32_t bandwidth = GetGlobalDataProvider()->GetBandWidth();
        bool range = (state_.range_ == State::RANGE_SUPPORT ? true : false);

        boost::uint32_t estimated_download_time_in_sec = P2PEstimatedDownloadTimeInSec();
        bool can_download = (estimated_download_time_in_sec <= P2SPConfigs::SWITCH_CONTROLLER_SIMPLE_MAX_P2P_DOWNLOAD_TIME_IN_SEC);

        SWITCH_DEBUG("speed_5s:" << speed_5s << ", data_rate:" << data_rate << ", conn_peers_count:" << conn_peers_count
            << ", active_peers_count:" << active_peers_count << ", full_peers_count:" << full_peers_count);

        //////////////////////////////////////////////////////////////////////////
        // Range Support
        if (range)
        {
            if (bandwidth > data_rate)
            {
                bool b1 = (speed_5s > data_rate * 120 / 100);
                bool b2 = (active_peers_count >= 2);

                SWITCH_DEBUG("... range=1 " << b1 << b2 << " can_download=" << can_download << " estimated_download_time_in_sec=" << estimated_download_time_in_sec << " active_peers_count=" << active_peers_count);

                if (b1 && b2)
                {
                    return true;
                }
                else if (b1 && can_download
                    && active_peers_count >= 1)
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
                bool b1 = (speed_5s > data_rate * 120 / 100);
                bool b2 = (full_peers_count >= 2);
                bool b3 = (active_peers_count >= 3);

                SWITCH_DEBUG("... range=0 " << b1 << b2 << b3 << " can_download=" << can_download << " estimated_download_time_in_sec=" << estimated_download_time_in_sec);
                if (b1 && b2 && b3)
                {
                    return true;
                }
                else if (b1 && can_download && active_peers_count >= 1 && full_peers_count >= 1)
                {
                    SWITCH_DEBUG("true: (can_download && active_peers_count >= 1 && full_peers_count >= 1 && speed_5s > data_rate * 4 / 3)");
                    return true;
                }
            }
        }

        return false;
    }

    bool SwitchController::SimpleVideoControlMode::P2PCanDownloadStably()
    {
        if (false == IsRunning())
            return false;
        // asserts
        assert(GetP2PControlTarget());
        assert(!GetP2PControlTarget()->IsPausing());
        // condition
        boost::uint32_t speed_5s = GetP2PControlTarget()->GetCurrentDownloadSpeed();
        boost::uint32_t data_rate = GetGlobalDataProvider()->GetDataRate();
        boost::uint32_t conn_peers_count = GetP2PControlTarget()->GetConnectedPeersCount();
        boost::uint32_t active_peers_count =  GetP2PControlTarget()->GetActivePeersCount();
        boost::uint32_t full_peers_count = GetP2PControlTarget()->GetFullBlockPeersCount();
        boost::uint32_t bandwidth = GetGlobalDataProvider()->GetBandWidth();
        bool range = (state_.range_ == State::RANGE_SUPPORT ? true : false);

        SWITCH_DEBUG("speed_5s:" << speed_5s << ", data_rate:" << data_rate << ", conn_peers_count:" << conn_peers_count
            << ", active_peers_count:" << active_peers_count << ", full_peers_count:" << full_peers_count);

        // Range Support
        if (range)
        {
            bool b1 = (speed_5s > 20000);
            bool b2 = (full_peers_count >= 0);
            bool b3 = (active_peers_count >= 2);
            bool b4 = (speed_5s > data_rate * 4 / 3);

            SWITCH_DEBUG("... range=1 " << b1 << b2 << b3 << " " << b4);
            if ((b1 && b2 && b3) || b4)
            {
                return true;
            }
        }
        // Range Unsupport
        else
        {
            bool b1 = (speed_5s > 20000);
            bool b2 = (full_peers_count >= 2);
            bool b3 = (active_peers_count >= 3);
            bool b4 = (speed_5s > data_rate);

            SWITCH_DEBUG("... range=0 " << b1 << b2 << b3 << " " << b4);
            if ((b1&&b2&&b3) || b4)
            {
                return true;
            }
        }

        return false;
    }

    boost::uint32_t SwitchController::SimpleVideoControlMode::CalcY()
    {
        if (false == IsRunning())
            return 0;
        // asserts
        assert(GetP2PControlTarget());
        //
        boost::uint32_t conn_peers = GetP2PControlTarget()->GetConnectedPeersCount();
        boost::uint32_t pooled_peers = GetP2PControlTarget()->GetPooledPeersCount();
        bool is_http_bad = IsHttpBad();

        if (conn_peers >= 8 || pooled_peers >= 50 || true == is_http_bad)
        {
            y_ = 8 * 1000;
        }
        else if (conn_peers >= 1 && pooled_peers >= 5)
        {
            y_ = 6 * 1000;
        }
        else if (pooled_peers >= 3)
        {
            y_ = 3 * 1000;
        }
        else
        {
            y_ = 0 * 1000;
        }
        SWITCH_DEBUG("y_=" << y_ << " conn_peers=" << conn_peers);
        return y_;
    }

    bool SwitchController::SimpleVideoControlMode::IsHttpBad()
    {
        if (false == IsRunning())
            return false;
        boost::uint32_t data_rate = GetGlobalDataProvider()->GetDataRate();
        bool b1 = (speed_h_ < data_rate);
        SWITCH_DEBUG("speed_h_=" << speed_h_ << " data_rate=" << data_rate << " is_http_bad=" << b1);
        LOG(__EVENT, "leak", __FUNCTION__ << " speed_h_=" << speed_h_ << " data_rate=" << data_rate << " is_http_bad=" << b1);
        return b1;
    }

    void SwitchController::SimpleVideoControlMode::CheckRID()
    {
        if (state_.rid_ == State::RID_NONE && GetP2PControlTarget())
        {
            state_.rid_ = State::RID_GOT;
        }
    }
    void SwitchController::SimpleVideoControlMode::OnControlTimer(boost::uint32_t times)
    {
        if (false == IsRunning())
            return;

        // asserts
        assert(GetHTTPControlTarget());

        // SpeedH
        if (state_.http_ == State::HTTP_DOWNLOADING)
        {
            assert(GetHTTPControlTarget());
            speed_h_ = GetHTTPControlTarget()->GetCurrentDownloadSpeed();
        }

        SWITCH_DEBUG((string)state_ << " " << times);
        GetGlobalDataProvider()->SetSwitchState(state_.http_, state_.p2p_, state_.timer_using_, state_.timer_);

        //////////////////////////////////////////////////////////////////////////
        // Initial State
        // <3300*1>
        if (
            state_.http_ == State::HTTP_PAUSING &&
            state_.p2p_ == State::P2P_PAUSING   &&
            state_.timer_ == State::TIMER_NONE &&
            state_.timer_using_ == State::TIMER_USING_NONE)
        {
            // 初始状态，HTTP和P2P都暂停，表示P2P有RID
            GetGlobalDataProvider()->SetAcclerateStatus(10);
            // asserts
            assert(GetP2PControlTarget()->IsPausing());
            assert(GetHTTPControlTarget()->IsPausing());

            // action, 首先测试http状况，进入2351
            GetHTTPControlTarget()->Resume();
            time_counter_t_.reset();
            t_ = 5000;

            // state
            state_.http_ = State::HTTP_DOWNLOADING;
            state_.timer_using_ = State::TIMER_USING_T;
            state_.timer_ = State::TIMER_STARTED;

            // next
            Next(times);
        }
        // <3000*0>
        else if (
            state_.http_ == State::HTTP_PAUSING &&
            state_.p2p_ == State::P2P_NONE &&
            state_.timer_ == State::TIMER_NONE &&
            state_.timer_using_ == State::TIMER_USING_NONE)
        {
            // 初始状态，HTTP暂停，P2P为NONE，表示P2P没有RID
            GetGlobalDataProvider()->SetAcclerateStatus(11);
            // asserts
            // assert(state_.rid_ == State::RID_NONE);
            assert(GetHTTPControlTarget()->IsPausing());
            assert(!GetP2PControlTarget());

            // action, 开始测试http状况，进入2000
            GetHTTPControlTarget()->Resume();
            // state
            state_.http_ = State::HTTP_DOWNLOADING;
            state_.timer_using_ = State::TIMER_USING_NONE;
            state_.timer_ = State::TIMER_NONE;
            // next
            Next(times);
        }
        //////////////////////////////////////////////////////////////////////////
        // Common State
        // <2000*0>
        else if (
            state_.http_ == State::HTTP_DOWNLOADING &&
            state_.p2p_ == State::P2P_NONE &&
            state_.timer_using_ == State::TIMER_USING_NONE &&
            state_.timer_ == State::TIMER_NONE
           )
        {
            // asserts
            // HTTP下载，P2P无
            GetGlobalDataProvider()->SetAcclerateStatus(13);
            assert(state_.timer_ == State::TIMER_NONE);
            assert(state_.timer_using_ == State::TIMER_USING_NONE);
            assert(state_.rid_ == State::RID_NONE);
            // actions
            if (state_.rid_ == State::RID_NONE && GetP2PControlTarget())
            {
                // asserts
                assert(GetP2PControlTarget());
                // action
                GetP2PControlTarget()->Pause();
                boost::uint32_t elapsed = time_counter_elapsed_.elapsed();
                t_ = 5 * 1000;
                time_counter_t_.reset();
                // state
                state_.rid_ = State::RID_GOT;
                state_.p2p_ = State::P2P_PAUSING;
                state_.timer_using_ = State::TIMER_USING_T;
                state_.timer_ = State::TIMER_STARTED;
                // next
                Next(times);
            }
        }
        // <2351*1> 测试http状况的状态
        else if (
            state_.http_ == State::HTTP_DOWNLOADING &&
            state_.p2p_ == State::P2P_PAUSING &&
            state_.timer_using_ == State::TIMER_USING_T &&
            state_.timer_ == State::TIMER_STARTED
           )
        {
            // HTTP下载，P2P暂停，定时器T启动
            GetGlobalDataProvider()->SetAcclerateStatus(12);
            // asserts
            assert(GetHTTPControlTarget());
            assert(!GetHTTPControlTarget()->IsPausing());
            assert(GetP2PControlTarget());
            assert(GetP2PControlTarget()->IsPausing());

            IHTTPControlTarget::p http = GetHTTPControlTarget();
            IP2PControlTarget::p p2p = GetP2PControlTarget();

            if (time_counter_t_.elapsed() >= t_)
            {
                // action

                // state
                state_.timer_ = State::TIMER_STOPPED;
                // next
                Next(times);
            }
        }
        // <2352*1> 根据测试http的结果分类处理
        else if (
            state_.http_ == State::HTTP_DOWNLOADING &&
            state_.p2p_ == State::P2P_PAUSING &&
            state_.timer_using_ == State::TIMER_USING_T&&
            state_.timer_ == State::TIMER_STOPPED)
        {
            // asserts
            // HTTP下载，P2P暂停，定时器T停止
            GetGlobalDataProvider()->SetAcclerateStatus(14);
            assert(GetHTTPControlTarget());
            assert(GetP2PControlTarget());
            assert(!GetHTTPControlTarget()->IsPausing());
            assert(GetP2PControlTarget()->IsPausing());

            boost::uint32_t connected_peers = GetP2PControlTarget()->GetConnectedPeersCount();
            boost::uint32_t pooled_peers = GetP2PControlTarget()->GetPooledPeersCount();
            boost::uint32_t data_rate = GetGlobalDataProvider()->GetDataRate();
            boost::uint32_t http_curr_speed = GetHTTPControlTarget()->GetCurrentDownloadSpeed();
            boost::uint32_t downloading_position = GetGlobalDataProvider()->GetDownloadingPosition();

            SWITCH_DEBUG(" http_curr_speed=" << http_curr_speed << " connected_peers=" << connected_peers);

            // check
            if (http_curr_speed >= data_rate * 120 / 100)
            {
                // action; http好，进入2300*1状态
                if (state_.range_ == State::RANGE_SUPPORT)
                    timer_counter_230031.reset();
                else
                    timer_counter_2300x1.reset();

                // state
                state_.timer_using_ = State::TIMER_USING_NONE;
                state_.timer_ = State::TIMER_NONE;

                CheckRange();
                // next
            }
            else
            {
                // action; http速度一般，进入3221
                CalcY();
                time_counter_y_.reset();
                GetHTTPControlTarget()->Pause();
                GetP2PControlTarget()->Resume();
                // state
                state_.http_ = State::HTTP_PAUSING;
                state_.p2p_ = State::P2P_DOWNLOADING;
                state_.timer_using_ = State::TIMER_USING_Y;
                state_.timer_ = State::TIMER_STARTED;
                // next
            }
            // next
            Next(times);
        }
        // <3221*1>
        else if (
            state_.http_ == State::HTTP_PAUSING &&
            state_.p2p_ == State::P2P_DOWNLOADING  &&
            state_.timer_using_ == State::TIMER_USING_Y &&
            state_.timer_ == State::TIMER_STARTED)
        {
            // HTTP暂停, P2P正在下载, Y定时器启动
            GetGlobalDataProvider()->SetAcclerateStatus(17);
            // asserts
            assert(GetHTTPControlTarget());
            assert(GetP2PControlTarget());
            assert(GetHTTPControlTarget()->IsPausing());
            assert(!GetP2PControlTarget()->IsPausing());

            // condition
            if (time_counter_y_.elapsed() > y_)
            {
                // action
                // state；下载y秒之后转入3222
                state_.timer_ = State::TIMER_STOPPED;
                CheckRange();
                // next
                Next(times);
                p2p_tried = true;
            }
        }
        //////////////////////////////////////////////////////////////////////////
        // UnRange

        //////////////////////////////////////////////////////////////////////////
        // Unstable States

        // <322241> | <322211>
        else if (
            state_.http_ == State::HTTP_PAUSING &&
            state_.p2p_ == State::P2P_DOWNLOADING &&
            state_.timer_using_ == State::TIMER_USING_Y &&
            state_.timer_ == State::TIMER_STOPPED &&
            state_.range_ != State::RANGE_SUPPORT)
        {
            // HTTP暂停，P2P下载, 定时器Y停止，不支持RANGE
            GetGlobalDataProvider()->SetAcclerateStatus(18);
            boost::uint32_t data_rate = GetGlobalDataProvider()->GetDataRate();
            boost::uint32_t bandwidth = GetGlobalDataProvider()->GetBandWidth();
            boost::uint32_t speed_5s = GetP2PControlTarget()->GetCurrentDownloadSpeed();

            if (P2PCanDropHttp() || P2PCanPlayStably())
            {
                // action
                timer_counter_3200x1.reset();
                // state；p2p很好，转入3200
                state_.timer_ = State::TIMER_NONE;
                state_.timer_using_ = State::TIMER_USING_NONE;
                // next
            }
            else
            {
                // action；p2p不够好，放弃p2p，转入2300
                GetHTTPControlTarget()->Resume();
                GetP2PControlTarget()->Pause();
                timer_counter_2300x1.reset();
                // state
                state_.http_ = State::HTTP_DOWNLOADING;
                state_.p2p_ = State::P2P_PAUSING;
                state_.timer_ = State::TIMER_NONE;
                state_.timer_using_ = State::TIMER_USING_NONE;
                // next
            }
            Next(times);
        }

        //////////////////////////////////////////////////////////////////////////
        // Stable States

        // <320041> | <320011>
        else if (
            state_.http_ == State::HTTP_PAUSING &&
            state_.p2p_ == State::P2P_DOWNLOADING &&
            state_.timer_ == State::TIMER_NONE &&
            state_.timer_using_ == State::TIMER_USING_NONE &&
            state_.range_ != State::RANGE_SUPPORT)
        {
            // HTTP暂停，P2P正在下载, 无定时器，不支持RANGE
            GetGlobalDataProvider()->SetAcclerateStatus(19);

            SWITCH_DEBUG(", timer_counter_3200x1.elapsed()=" << timer_counter_3200x1.elapsed());

            CheckRange();
            if (state_.range_ == State::RANGE_SUPPORT)
            {
                timer_counter_230031.reset();
                Next(times);
            }
            else if (P2PCanDropHttp() || P2PCanPlayStably())
            {
                timer_counter_3200x1.reset();
            }
            else if (timer_counter_3200x1.elapsed() > 10000)
            {
                // action, goto <320041> | <320011>
                GetHTTPControlTarget()->Resume();
                GetP2PControlTarget()->Pause();
                timer_counter_2300x1.reset();

                // state
                state_.http_ = State::HTTP_DOWNLOADING;
                state_.p2p_ = State::P2P_PAUSING;
                state_.timer_using_ = State::TIMER_USING_NONE;
                state_.timer_ = State::TIMER_NONE;

                // next
                Next(times);
            }
        }
        // <230041> | <230011>
        else if (
            state_.http_ == State::HTTP_DOWNLOADING &&
            state_.p2p_ == State::P2P_PAUSING &&
            state_.timer_ == State::TIMER_NONE &&
            state_.timer_using_ == State::TIMER_USING_NONE &&
            state_.range_ != State::RANGE_SUPPORT)
        {
            // nothing
            // HTTP正在下载, P2P暂停, 无定时器，不支持RANGE
            GetGlobalDataProvider()->SetAcclerateStatus(20);

            CheckRange();
            if (state_.range_ == State::RANGE_SUPPORT)
            {
                timer_counter_230031.reset();
                Next(times);
            }
            else if (!p2p_tried && timer_counter_2300x1.elapsed() >= 5000)
            {
                boost::uint32_t speed_5s = GetHTTPControlTarget()->GetCurrentDownloadSpeed();
                boost::uint32_t data_rate = GetGlobalDataProvider()->GetDataRate();

                if (speed_5s < data_rate * 110 / 100 &&
                    GetP2PControlTarget()->GetConnectedPeersCount() > 15)
                {
                    // action, goto 3221, test p2p
                    GetHTTPControlTarget()->Pause();
                    GetP2PControlTarget()->Resume();
                    CalcY();
                    time_counter_y_.reset();

                    // state
                    state_.http_ = State::HTTP_PAUSING;
                    state_.p2p_ = State::P2P_DOWNLOADING;
                    state_.timer_using_ = State::TIMER_USING_Y;
                    state_.timer_ = State::TIMER_STARTED;

                    // next
                    Next(times);
                }
            }
        }

        //////////////////////////////////////////////////////////////////////////
        // Range

        //////////////////////////////////////////////////////////////////////////
        // Unstable States

        // <322231>
        else if (
            state_.http_ == State::HTTP_PAUSING &&
            state_.p2p_ == State::P2P_DOWNLOADING &&
            state_.timer_using_ == State::TIMER_USING_Y &&
            state_.timer_ == State::TIMER_STOPPED  &&
            state_.range_ == State::RANGE_SUPPORT)
        {
            // HTTP暂停，P2P下载，定时器Y停止，支持RANGE
            GetGlobalDataProvider()->SetAcclerateStatus(21);
            boost::uint32_t bandwidth = GetGlobalDataProvider()->GetBandWidth();
            boost::uint32_t data_rate = GetGlobalDataProvider()->GetDataRate();
            bool can_download_stably = P2PCanDownloadStably();
            bool can_drop_http = P2PCanDropHttp();
            bool can_play = P2PCanPlayStably();

            SWITCH_DEBUG("can_drop_http = " << can_drop_http << " can_play = " << can_play << " can_download_stably = " << can_download_stably);

            if (can_play)
            {
                // action
                // state
                state_.timer_ = State::TIMER_NONE;
                state_.timer_using_ = State::TIMER_USING_NONE;
                SWITCH_DEBUG("P2P");
            }
            else if (can_download_stably)
            {
                // action
                GetHTTPControlTarget()->Resume();
                is_2200_from_2300 = false;
                timer_counter_220031.reset();
                // state
                state_.http_ = State::HTTP_DOWNLOADING;
                state_.timer_ = State::TIMER_NONE;
                state_.timer_using_ = State::TIMER_USING_NONE;
                SWITCH_DEBUG("HTTP & P2P");
            }
            else
            {
                // action
                GetHTTPControlTarget()->Resume();
                GetP2PControlTarget()->Pause();
                timer_counter_230031.reset();
                // state
                state_.http_ = State::HTTP_DOWNLOADING;
                state_.p2p_ = State::P2P_PAUSING;
                state_.timer_ = State::TIMER_NONE;
                state_.timer_using_ = State::TIMER_USING_NONE;
                SWITCH_DEBUG("HTTP");
            }
            //             else
            //             {
            //                 SWITCH_DEBUG("Should never reach here! <322231>");
            //                 assert(!"Should never reach here! <322231>");
            //             }
            // next
            Next(times);
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
            // HTTP暂停，P2P正在下载，无定时器，支持RANGE
            GetGlobalDataProvider()->SetAcclerateStatus(22);

            if (!P2PCanPlayStably())
            {
                // action
                GetHTTPControlTarget()->Resume();
                is_2200_from_2300 = false;
                timer_counter_220031.reset();
                // state
                state_.http_ = State::HTTP_DOWNLOADING;
                // next
                Next(times);
            }
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

            // HTTP正在下载，P2P暂停，无定时器，支持RANGE
            GetGlobalDataProvider()->SetAcclerateStatus(23);
            // stable state

            if (timer_counter_230031.elapsed() >= 5000)
            {
                boost::uint32_t speed_5s = GetHTTPControlTarget()->GetCurrentDownloadSpeed();
                boost::uint32_t data_rate = GetGlobalDataProvider()->GetDataRate();

                if (speed_5s < data_rate * 110 / 100 &&
                    GetP2PControlTarget()->GetConnectedPeersCount() > 5)
                {
                    // action, goto 2200, download with http and p2p
                    GetP2PControlTarget()->Resume();
                    is_2200_from_2300 = true;
                    timer_counter_220031.reset();

                    // state
                    state_.http_ = State::HTTP_DOWNLOADING;
                    state_.p2p_ = State::P2P_DOWNLOADING;
                    state_.timer_using_ = State::TIMER_USING_NONE;
                    state_.timer_ = State::TIMER_NONE;
                }
            }
        }
        // <220031>
        else if (
            state_.http_ == State::HTTP_DOWNLOADING &&
            state_.p2p_ == State::P2P_DOWNLOADING &&
            state_.timer_ == State::TIMER_NONE &&
            state_.timer_using_ == State::TIMER_USING_NONE &&
            state_.range_ == State::RANGE_SUPPORT)
        {
            // HTTP和P2P正在下载，无定时器，支持RANGE
            GetGlobalDataProvider()->SetAcclerateStatus(24);

            p2p_tried = true;

            if (!P2PCanDownloadStably())
            {
                if (!is_2200_from_2300 || timer_counter_220031.elapsed() >= 5000)
                {
                    // action, 回2300
                    GetP2PControlTarget()->Pause();
                    timer_counter_230031.reset();

                    // state
                    state_.p2p_ = State::P2P_PAUSING;
                }
            }
            else if (P2PCanPlayStably())
            {
                // action, goto 3200
                GetHTTPControlTarget()->Pause();

                // state
                state_.http_ = State::HTTP_PAUSING;
            }
        }
        else
        {
            SWITCH_DEBUG("Unreachable State!!! State: " << (string)state_);
            assert(!"SwitchController::SimpleVideoControlMode::OnControlTimer: No Such State!");
        }
    }
}
