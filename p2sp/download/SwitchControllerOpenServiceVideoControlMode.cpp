//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// ! 应该把智能限速考虑到在带宽的判断上
// ! 可以考虑将一个文件末尾的下载考虑进来，如果在P2P下载，末尾了，P2P速度必然低，不应该切换到HTTP；
#include "Common.h"
#include "p2sp/download/SwitchController.h"
#include "p2sp/p2p/P2SPConfigs.h"
#include "struct/SubPieceContent.h"
#include "statistic/StatisticModule.h"
#include "p2sp/bootstrap/BootStrapGeneralConfig.h"

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("download");

    //////////////////////////////////////////////////////////////////////////
    // OpenServiceControlMode

    SwitchController::OpenServiceVideoControlMode::p SwitchController::OpenServiceVideoControlMode::Create(SwitchController::p controller)
    {
        return OpenServiceVideoControlMode::p(new OpenServiceVideoControlMode(controller));
    }

    void SwitchController::OpenServiceVideoControlMode::Start()
    {
        if (true == IsRunning())
            return;
        // start
        ControlMode::Start();

        http_target_manager_.SetHttpTargets(GetGlobalDataProvider()->GetAllHttpControlTargets());

        if (GetHTTPControlTarget()) GetHTTPControlTarget()->Pause();
        if (GetP2PControlTarget()) GetP2PControlTarget()->Pause();

        // initial status
        state_.http_ = (GetHTTPControlTarget() ? State::HTTP_PAUSING : State::HTTP_NONE);
        state_.p2p_ = (GetP2PControlTarget() ? State::P2P_PAUSING : State::P2P_NONE);
        state_.rid_ = (GetP2PControlTarget() ? State::RID_GOT : State::RID_NONE);
        state_.timer_ = State::TIMER_NONE;
        state_.timer_using_ = State::TIMER_USING_NONE;

        is_memory_full = false;
        http_status_ = http_unknown;
        p2p_status_ = p2p_unknown;
        p2p_failed_times_ = 0;
        is_timer_h_reset = false;

        // parameters
        SWITCH_DEBUG(string(20, '-'));

        // 只有一个downloader，则立即开始下载
        if (!GetHTTPControlTarget())
        {
            GetP2PControlTarget()->Resume();
        }

        // HTTP限速
        if (GetHTTPControlTarget())
        {
            GetHTTPControlTarget()->SetSpeedLimitInKBps(P2SPConfigs::HTTP_DOWNLOAD_SPEED_LIMIT);
        }

        // GetP2PControlTarget()->Resume();
        // GetHTTPControlTarget()->Resume();

        // next
        // Next(0);
        OnControlTimer(0);
    }

    void SwitchController::OpenServiceVideoControlMode::Stop()
    {
        if (false == IsRunning())
            return;
        // release
        // stop
        ControlMode::Stop();
    }

    // 是否可以切换到纯P2P
    SwitchController::OpenServiceVideoControlMode::HTTPSPEED 
        SwitchController::OpenServiceVideoControlMode::Check2300HttpSpeed()
    {
        uint32_t rest_play_time_inms = GetGlobalDataProvider()->GetRestPlayableTime();
        uint32_t peer_count = GetP2PControlTarget()->GetConnectedPeersCount();
        IHTTPControlTarget::p http = GetHTTPControlTarget();
        uint32_t data_rate = GetGlobalDataProvider()->GetDataRate();
        bool is_http_speed_slow = false;
        bool is_drag = GetGlobalDataProvider()->IsDrag() || GetGlobalDataProvider()->IsStartFromNonZero();

        if (http->GetCurrentDownloadSpeed() > 0.8 * GetGlobalDataProvider()->GetBandWidth())
        {
            // http速度很接近带宽，认为速度没问题
            http_status_ = http_good;
            return http_fast;
        }
        else if (false == is_drag && p2p_status_ == p2p_unknown)
        {
            // 非拖动请求p2p没有失败过，对于http的判断很严格，4s内速度没有达到码流率，直接转p2p
            if (time_counter_2300.elapsed() >= 4000)
            {
                if (http->GetCurrentDownloadSpeed() < data_rate)
                {
                    is_http_speed_slow = true;
                }
                else
                {
                    http_status_ = http_good;
                    return http_fast;
                }
            }
            else if (time_counter_2300.elapsed() >= 3000)
            {
                if (http->GetCurrentDownloadSpeed() < data_rate * 8 / 10)
                {
                    is_http_speed_slow = true;
                }
            }
            else if (time_counter_2300.elapsed() >= 2000)
            {
                if (http->GetCurrentDownloadSpeed() < data_rate * 5 / 10)
                {
                    is_http_speed_slow = true;
                }
            }
        }
        else
        {
            // 拖动请求或者p2p已经试过不行，http判断的条件放宽，至少运行5s再行判断
            if (time_counter_2300.elapsed() >= 10000)
            {
                if (http->GetCurrentDownloadSpeed() < data_rate)
                {
                    is_http_speed_slow = true;
                }
                else
                {
                    http_status_ = http_good;
                    return http_fast;
                }
            }
            else if (time_counter_2300.elapsed() >= 5000)
            {
                if (http->GetCurrentDownloadSpeed() < data_rate / 2)
                {
                    is_http_speed_slow = true;
                }
            }
        }

        if (is_http_speed_slow)
        {
            switch (http_status_)
            {
            case http_unknown:
                if (is_drag)
                {
                    http_status_ = http_bad;
                }
                else
                {
                    http_status_ = http_bad_once;
                }
                
                break;
            case http_bad_once:
                http_status_ = http_bad;
                break;
            case http_bad:
            case http_good:
                http_status_ = http_bad;
                break;
            default:
                break;
            }
            return http_slow;
        }
        else
        {
            return http_checking;
        }
    }

    bool SwitchController::OpenServiceVideoControlMode::Is2300RestTimeEnough()
    {
        double data_rate_v;
        uint32_t data_rate_inkbps = GetGlobalDataProvider()->GetDataRate() / 1024 * 8;        
        uint32_t rest_play_time_inms = GetGlobalDataProvider()->GetRestPlayableTime();
        uint32_t peer_count = GetP2PControlTarget()->GetConnectedPeersCount();

        if (data_rate_inkbps < 700)
        {
            data_rate_v = 1;
        }
        else if (data_rate_inkbps < 1200)
        {
            data_rate_v = 1.2;
        }
        else if (data_rate_inkbps < 1500)
        {
            data_rate_v = 1.5;
        }
        else
        {
            data_rate_v = 1.8;
        }

        SWITCH_DEBUG(" time_counter_2300.GetElapsed() = " << time_counter_2300.elapsed()
            << " peer_count = " << peer_count
            << " data_rate_inkbps = " << data_rate_inkbps
            << " data_rate_v = " << data_rate_v
            << " rest_play_time_inms = " << rest_play_time_inms);

        // http速度可以满足要求，如果剩余缓冲足够大了，也需要转到p2p
        // 这个地方将对带宽节约比有很大的影响！zw
        return rest_play_time_inms >= BootStrapGeneralConfig::Inst()->GetRestTimeEnoughLaunchP2P10() * 1000 * data_rate_v &&
            peer_count > 10
            ||
            rest_play_time_inms >= BootStrapGeneralConfig::Inst()->GetRestTimeEnoughLaunchP2P0() * 1000 &&
            peer_count > 0;
    }

    bool SwitchController::OpenServiceVideoControlMode::Is2200RestTimeEnough()
    {
        return GetGlobalDataProvider()->GetRestPlayableTime() >= 50000;
    }

    bool SwitchController::OpenServiceVideoControlMode::Is3200P2pSlow()
    {
        uint32_t data_rate = GetGlobalDataProvider()->GetDataRate();
        uint32_t band_width = GetGlobalDataProvider()->GetBandWidth();
        IP2PControlTarget::p p2p = GetP2PControlTarget();

        if (time_counter_3200.elapsed() >= 10000)
        {
            if (p2p->GetCurrentDownloadSpeed() > data_rate * 11 / 10 ||
                p2p->GetCurrentDownloadSpeed() > band_width * 8 / 10)
            {
                time_counter_x_.reset();
            }

            return time_counter_x_.elapsed() > 5000;
        }
        else if (time_counter_3200.elapsed() >= 5000)
        {
            return p2p->GetCurrentDownloadSpeed() < data_rate / 2 &&
                p2p->GetCurrentDownloadSpeed() < band_width * 8 / 10;
        } 
        return false;
    }

    void SwitchController::OpenServiceVideoControlMode::ChangeTo2200()
    {
        GetHTTPControlTarget()->Resume();
        GetP2PControlTarget()->Resume();

        state_.http_ = State::HTTP_DOWNLOADING;
        state_.p2p_ = State::P2P_DOWNLOADING;
        state_.timer_using_ = State::TIMER_USING_NONE;
        state_.timer_ = State::TIMER_NONE;

        time_counter_2200.reset();
        time_counter_x_.reset();
        time_counter_y_.reset();
    }

    void SwitchController::OpenServiceVideoControlMode::ChangeTo3200(bool is_p2p_start)
    {
        is_p2p_start_ = is_p2p_start;

        GetP2PControlTarget()->Resume();
        GetHTTPControlTarget()->Pause();

        state_.http_ = State::HTTP_PAUSING;
        state_.p2p_ = State::P2P_DOWNLOADING;
        state_.timer_using_ = State::TIMER_USING_NONE;
        state_.timer_ = State::TIMER_NONE;

        time_counter_3200.reset();
        time_counter_x_.reset();
        time_counter_y_.reset();
    }

    void SwitchController::OpenServiceVideoControlMode::ChangeTo2300()
    { 
        GetP2PControlTarget()->Pause();
        GetHTTPControlTarget()->Resume();

        state_.http_ = State::HTTP_DOWNLOADING;
        state_.p2p_ = State::P2P_PAUSING;
        state_.timer_using_ = State::TIMER_USING_NONE;
        state_.timer_ = State::TIMER_NONE;

        time_counter_2300.reset();
        time_counter_x_.reset();
        time_counter_y_.reset();
    }

    void SwitchController::OpenServiceVideoControlMode::ChangeTo2000()
    {
        assert(!GetP2PControlTarget());
        GetHTTPControlTarget()->Resume();
        state_.http_ = State::HTTP_DOWNLOADING;
        state_.p2p_ = State::P2P_NONE;
        state_.timer_ = State::TIMER_NONE;
        state_.timer_using_ = State::TIMER_USING_NONE;
    }

#ifdef USE_MEMORY_POOL
    bool SwitchController::OpenServiceVideoControlMode::CheckMemory()
    {
        statistic::StatisticModule::Inst()->SetMemoryPoolLeftSize(protocol::SubPieceContent::get_left_capacity());
        if (!is_memory_full)
        {
            if (protocol::SubPieceContent::get_left_capacity() < 1024)
            {
                // 内存池可用内存低于800 KB停止下载
                is_memory_full = true;

                // goto 3300
                if (GetHTTPControlTarget())
                {
                    GetHTTPControlTarget()->Pause();
                    state_.http_ = State::HTTP_PAUSING;
                }
                else
                {
                    state_.http_ = State::HTTP_NONE;
                }

                if (GetP2PControlTarget())
                {
                    GetP2PControlTarget()->Pause();
                    state_.p2p_ = State::P2P_PAUSING;
                }
                else
                {
                    state_.p2p_ = State::P2P_NONE;
                }

                state_.timer_using_ = State::TIMER_USING_NONE;
                state_.timer_ = State::TIMER_NONE;

                return true;
            }
            else
            {
                return false;
            }
        }
        else
        {
            if (protocol::SubPieceContent::get_left_capacity() >= 1280)
            {
                // 内存池可用内存多于1000 KB恢复下载
                is_memory_full = false;

                if (!GetHTTPControlTarget())
                {
                    GetP2PControlTarget()->Resume();
                    state_.http_ = State::HTTP_NONE;
                    state_.p2p_ = State::P2P_DOWNLOADING;
                    state_.timer_using_ = State::TIMER_USING_NONE;
                    state_.timer_ = State::TIMER_NONE;
                    return true;
                }

                if (!GetP2PControlTarget())
                {
                    GetHTTPControlTarget()->Resume();
                    state_.http_ = State::HTTP_DOWNLOADING;
                    state_.p2p_ = State::P2P_NONE;
                    state_.timer_using_ = State::TIMER_USING_NONE;
                    state_.timer_ = State::TIMER_NONE;
                    return true;
                }

                // HTTPDownload & P2PDownload都有
                GetP2PControlTarget()->Resume();

                state_.http_ = State::HTTP_PAUSING;
                state_.p2p_ = State::P2P_DOWNLOADING;
                state_.timer_using_ = State::TIMER_USING_NONE;
                state_.timer_ = State::TIMER_NONE;

                time_counter_3200.reset();
                return true;
            }
            else
            {
                return true;
            }
        }
    }
#endif


    void SwitchController::OpenServiceVideoControlMode::OnControlTimer(uint32_t times)
    {
        if (false == IsRunning())
            return;

// Debug模式下用MONITOR状态机的外部控制
// #if true
#ifndef NDEBUG
        FILE *fp = fopen("switch.ini", "r");
        if (fp != NULL)
        {
            LOG(__DEBUG, "switch", "BINGO 1");
            int switch_type = 0;
            fscanf(fp, "%d", &switch_type);
            LOG(__DEBUG, "switch", "BINGO type = " << switch_type);
            fclose(fp);

            switch (switch_type)
            {
            case 2200:
                ChangeTo2200();
                break;
            case 2300:
                ChangeTo2300();
                break;
            case 3200:
                ChangeTo3200(false);
                break;
            default:
                break;
            }

            GetGlobalDataProvider()->SetSwitchState(state_.http_, state_.p2p_, state_.timer_using_, state_.timer_);
            return;
        }
#endif

#ifdef USE_MEMORY_POOL
        if (CheckMemory())
        {
            return;
        }
#endif

        while (true)
        {
            SWITCH_DEBUG((string)state_ << " " << times);

            // <3000>
            if (
                state_.http_ == State::HTTP_PAUSING &&
                state_.p2p_ == State::P2P_NONE &&
                state_.timer_using_ == State::TIMER_USING_NONE &&
                state_.timer_ == State::TIMER_NONE)
            {
                if (NeedHttpStart())
                {
                    ChangeTo2000();
                    break;
                }

                if (GetP2PControlTarget())
                {
                    state_.p2p_ = State::P2P_PAUSING;
                    state_.timer_using_ = State::TIMER_USING_NONE;
                    state_.timer_ = State::TIMER_NONE;
                    continue;
                }
                else
                {
                    bool should_prevent_http_predownload = BootStrapGeneralConfig::Inst()->ShouldPreventHttpPredownload();
                    if(!should_prevent_http_predownload ||
                        is_timer_h_reset)
                    {                        
                        if(!should_prevent_http_predownload ||
                            time_counter_h_.elapsed() > BootStrapGeneralConfig::Inst()->GetWaitTimeForTinydrag() * 1000)
                        {
                            //action
                            ChangeTo2000();
                            is_timer_h_reset = false;
                            continue;
                        }

                        break;
                    }
                    //aciton
                    time_counter_h_.reset();
                    is_timer_h_reset = true;
                }   

                break;
            }
            // <3300>
            if ( 
                state_.http_ == State::HTTP_PAUSING &&
                state_.p2p_ == State::P2P_PAUSING &&
                state_.timer_using_ == State::TIMER_USING_NONE &&
                state_.timer_ == State::TIMER_NONE)
            {
                //
                //   3300 - 播放开始时的初始状态，决定播放是HTTP开始下载(跳转至2311或2300)还是P2P开始下载(跳转至3200)
                //   有几个参数会影响跳转决定：
                //
                //      drag - 参数由客户端请求带入。当drag参数存在且drag = 1时，表明为用户的拖动请求，或vip用户开始
                //             播放节目请求，此时应由http开始下载
                //
                //      start - 参数由ikan播放器请求带入，当start参数存在且start > 0时，表明为用户的拖动请求，此时应由
                //              http开始下载
                //
                //        headonly - 参数由客户端请求带入。当headonly参数存在且headonly = 1时，表明此请求只下载mp4
                //                文件的头部，为保证速度应由http直接下载
                //
                //      BWType - 跳转中心返回的服务器可用带宽值，由客户端传入。
                //               BWType = 0，表明服务器带宽正常
                //               BWType = 1, 表明服务器带宽充裕
                //               BWType = 2, 表明跳转中心要求该节目为纯HTTP播放
                //               BWType = 3, 表明跳转中心要求尽可能（只要HTTP速度足够）用纯HTTP播放
                //      TODO:yujinwu:2010/12/11:BUG: 3300没有利用BWType!
                //
                //  具体有以下几种情况：
                //
                //   1. 普通客户端开始播放: 无任何特殊参数
                //
                //   2. VIP客户端开始播放: drag = 1
                //
                //   3. 客户端预下载下一段数据：drag = 1或0
                //      预下载正在播放段的下一段时, drag = 1
                //      预下载正在播放段的下一段后面的段，drag = 0
                //
                //   4. 客户端拖动，下载头部数据： drag = 1, headonly = 1
                //      注:客户端拖动时需发起两次请求，第一次请求下载该段mp4的头部，第二次请求下载拖动点开始的数据
                //
                //   5. 客户端拖动，下载拖动点开始的数据： drag = 1
                //
                //   6. ikan开始播放:  无任何特殊参数 (注: ikan不区分vip用户)
                //
                //   7. ikan预下载下一段数据:  无任何特殊参数
                //
                //   8. ikan拖动: start > 0 (注: ikan拖动不需要头部请求)
                //
                //
                //  2010/12/11:yujinwu:判断逻辑简化为：
                //
                //   0. 本地拖动，转3200
                //   1. 头部请求，headonly = 1, 转2300
                //   2. 客户端拖动，drag = 1, 转2311
                //   3. ikan拖动，start > 0, 转2311
                //   4. 其余情况转3200

                SWITCH_DEBUG((string)state_ << "3300" << times);

                if (NeedHttpStart())
                {
                    ChangeTo2300();
                    break;
                }

                // 其他所有情况，均由p2p启动
                ChangeTo3200(true);                
                continue;
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
                uint32_t data_rate = GetGlobalDataProvider()->GetDataRate();
                uint32_t band_width = GetGlobalDataProvider()->GetBandWidth();
                boost::uint32_t rest_playable_time_in_ms = GetGlobalDataProvider()->GetRestPlayableTime();

                // 进入2200状态表明p2p和http速度不能满足播放，两个一起跑，尽量跑快

                p2p->SetDownloadMode(IP2PControlTarget::NORMAL_MODE);
                p2p->SetDownloadPriority(CalcDownloadPriority());

                if (p2p->GetCurrentDownloadSpeed() < data_rate * 12 / 10 && 
                    p2p->GetCurrentDownloadSpeed() < band_width * 7 / 10)
                {
                    time_counter_x_.reset();
                }

                if (http->GetCurrentDownloadSpeed() < data_rate * 11 / 10 && 
                    http->GetCurrentDownloadSpeed() < band_width * 7 / 10)
                {
                    time_counter_y_.reset();
                }

                //判断顺序会影响到在HTTP和P2P都好的情况下，优先切换到哪种纯模式
                bool fast_http_download = time_counter_y_.elapsed() >= 5000;
                bool fast_p2p_download = time_counter_x_.elapsed() >= 5000;

                if (fast_http_download)
                {
                    http_status_ = http_good;
                }

                if (PrefersSavingServerBandwidth())
                {
                    if (fast_p2p_download || Is2200RestTimeEnough())
                    {
                        ChangeTo3200(false);
                    }
                    else if (fast_http_download)
                    {
                        ChangeTo2300();
                    }
                }
                else
                {
                    if (fast_http_download || Is2200RestTimeEnough())
                    {
                        ChangeTo2300();
                    }
                    else if (fast_p2p_download)
                    {
                        ChangeTo3200(false);
                    }
                }

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
                uint32_t data_rate = GetGlobalDataProvider()->GetDataRate();
                uint32_t band_width = GetGlobalDataProvider()->GetBandWidth();
                boost::uint32_t rest_playable_time_in_ms = GetGlobalDataProvider()->GetRestPlayableTime();

                if (is_p2p_start_ && time_counter_3200.elapsed() <= 8000 && rest_playable_time_in_ms <= 30000)
                {
                    if (time_counter_3200.elapsed() <= 4000)
                        p2p->SetDownloadMode(IP2PControlTarget::FAST_MODE);
                    else
                        p2p->SetDownloadMode(IP2PControlTarget::CONTINUOUS_MODE);
                }
                else
                {
                    if (rest_playable_time_in_ms <= 30000)
                        p2p->SetDownloadMode(IP2PControlTarget::NORMAL_MODE);
                    else if (rest_playable_time_in_ms <= 70000 && p2p->GetCurrentDownloadSpeed() < data_rate)
                        p2p->SetDownloadMode(IP2PControlTarget::NORMAL_MODE);
                    else
                        p2p->SetDownloadMode(IP2PControlTarget::FAST_MODE);
                }

                p2p->SetDownloadPriority(CalcDownloadPriority());

                if (http_status_ == http_good)
                {
                    // http是好的，可以等到剩余缓冲下降到15秒以内才切换到http
                    if ((rest_playable_time_in_ms < 15000 && Is3200P2pSlow())
                        || (rest_playable_time_in_ms < 10000))
                    {
                        p2p_failed_times_++;
                        p2p_status_ = p2p_bad;
                        ChangeTo2300();
                    }
                }
                else if (rest_playable_time_in_ms < BootStrapGeneralConfig::Inst()->GetRestTimeNeedCheckP2P() * 1000)
                {
                    // http不好或者未知
                    if (Is3200P2pSlow())
                    {
                        p2p_failed_times_++;
                        p2p_status_ = p2p_bad;
                        if (http_status_ == http_bad_once || http_status_ == http_unknown)
                        {
                            // http试过一次不好或者未知，p2p也不好，那么让http再试一次，并且多试一段时间
                            ChangeTo2300();
                        }
                        else
                        {
                            // http试过2次以上都不好，p2p也不好，转到2个一起下
                            ChangeTo2200();
                        }
                    }
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
                boost::uint32_t rest_playable_time = GetGlobalDataProvider()->GetRestPlayableTime();
                uint32_t data_rate = GetGlobalDataProvider()->GetDataRate();
                uint32_t band_width = GetGlobalDataProvider()->GetBandWidth();

                SWITCH_DEBUG((string)state_ << " 2300 " << times << " time_counter_2300.GetElapsed() = " << time_counter_2300.elapsed()
                    << " p2p->GetConnectedPeersCount() = " << p2p->GetConnectedPeersCount());

                if (p2p->GetConnectedPeersCount() == 0)
                {
                    return;
                }

                if (GetGlobalDataProvider()->IsHeadOnly())
                {
                    if (
                        (time_counter_2300.elapsed() >= 5000 &&
                        (http->GetCurrentDownloadSpeed() < 0.8 * band_width ||
                        http->GetCurrentDownloadSpeed() < 0.8 * data_rate
                        )
                        ) ||
                        (time_counter_2300.elapsed() >= 3000 &&
                        (http->GetCurrentDownloadSpeed() < 0.5 * band_width ||
                        http->GetCurrentDownloadSpeed() < 0.5 * data_rate
                        )
                        ) ||
                        (time_counter_2300.elapsed() >= 2000 &&
                        (http->GetCurrentDownloadSpeed() < 0.2 * band_width ||
                        http->GetCurrentDownloadSpeed() < 0.2 * data_rate
                        )
                        )
                        )
                    {
                        ChangeTo2200();
                        return;
                    }
                }
                else
                {
                    HTTPSPEED speed = Check2300HttpSpeed();
                    switch(speed)
                    {
                    case http_slow:
                        {
                            //在当前download driver下载后面一集的情况下，GetRestPlayableTime返回0xffffff。
                            //在这种情况下，我们不能确定正在播放的那一集的剩余缓冲时间。
                            //为避免出现HTTP不够好时，即使上一集即将播完我们也没有尝试速度可能较好的P2P，
                            //我们保守地假设rest_time==-1意味着数据是紧急的
                            uint32_t rest_time = GetGlobalDataProvider()->GetRestPlayableTime();
                            bool urgent_to_download = rest_time < 30*1000 || rest_time == -1;

                            if (PrefersSavingServerBandwidth() || urgent_to_download)
                            {
                                if (p2p_status_ == p2p_unknown)
                                {
                                    ChangeTo3200(true);
                                    if (GetGlobalDataProvider()->IsDrag() || GetGlobalDataProvider()->IsStartFromNonZero())
                                    {
                                        // 拖动后http速度差
                                        GetGlobalDataProvider()->SetDragHttpStatus(2);
                                    }
                                }
                                else
                                {
                                    assert(p2p_status_ == p2p_bad);

                                    if (http_target_manager_.HaveNextHttpTarget())
                                    {
                                        DebugLog("Change Http downloader");
                                        GetHTTPControlTarget()->Pause();
                                        http_target_manager_.MoveNextHttpTarget();
                                        GetHTTPControlTarget()->Resume();
                                        time_counter_2300.reset();

                                        GetGlobalDataProvider()->ReportUseBakHost();
                                    }
                                    else
                                    {
                                        if (http_target_manager_.size() > 1)
                                        {
                                            GetGlobalDataProvider()->ReportBakHostFail();
                                        }

                                        http_target_manager_.MoveBestHttpTarget();
                                        ChangeTo2200();
                                    }
                                }
                            }
                        }
                        break;

                    case http_fast:
                        if (PrefersSavingServerBandwidth() && Is2300RestTimeEnough())
                        {
                            if (p2p_failed_times_ < 3)
                            {
                                ChangeTo3200(true);
                            }
                        }

                        if (GetGlobalDataProvider()->IsDrag() || GetGlobalDataProvider()->IsStartFromNonZero())
                        {
                            // 拖动后http速度很好
                            GetGlobalDataProvider()->SetDragHttpStatus(1);
                        }
                        break;

                    case http_checking:
                        if (p2p_status_ == p2p_unknown)
                        {
                            if (PrefersSavingServerBandwidth() && Is2300RestTimeEnough())
                            {
                                ChangeTo3200(true);
                            }
                        }
                        break;
                    }

                    if (state_.http_ != State::HTTP_DOWNLOADING || state_.p2p_ != State::P2P_PAUSING)
                    {
                        // http被关闭或者p2p被打开，表明已经离开2300状态
                        GetGlobalDataProvider()->NoticeLeave2300();
                    }
                }
                
                break;
            }

			// <2000>
            else if (
                state_.http_ == State::HTTP_DOWNLOADING &&
                state_.p2p_ == State::P2P_NONE &&
                state_.timer_using_ == State::TIMER_USING_NONE &&
                state_.timer_ == State::TIMER_NONE)
                {
                    if (GetP2PControlTarget())
                    {
                        assert(GetGlobalDataProvider()->GetBWType() != JBW_HTTP_ONLY);
                        if (NeedHttpStart())
                        {
                            ChangeTo2300();
                        }
                        else
                            ChangeTo3200(true);

                        GetGlobalDataProvider()->NoticeLeave2000();
                        continue;
                    }

                    break;
                }
            else
            {
                SWITCH_DEBUG("No Such State: " << (string)state_);
                // assert(!"SwitchController::OpenServiceVideoControlMode::OnControlTimer: No Such State");
                // assert(0);
                break;
            }
        }
    }

    bool SwitchController::OpenServiceVideoControlMode::PrefersSavingServerBandwidth()
    {
        assert(GetGlobalDataProvider());

        switch (GetGlobalDataProvider()->GetBWType())
        {
        case JBW_NORMAL:
        case JBW_HTTP_MORE:
        case JBW_P2P_MORE:
            return true;
        case JBW_HTTP_ONLY:
        case JBW_HTTP_PREFERRED:
            break;
        default:
            //unexpected BWType value
            assert(false);
            break;
        }

        return false;
    }

    boost::int32_t SwitchController::OpenServiceVideoControlMode::CalcDownloadPriority()
    {
        boost::uint32_t rest_playable_time_in_ms = GetGlobalDataProvider()->GetRestPlayableTime();

        if (rest_playable_time_in_ms < 15000)
        {
            return protocol::RequestSubPiecePacket::PRIORITY_10;
        }
        else
        {
            return protocol::RequestSubPiecePacket::DEFAULT_PRIORITY;
        }
    }

    bool SwitchController::OpenServiceVideoControlMode::NeedHttpStart()
    {
        if (GetGlobalDataProvider()->IsHeadOnly() ||                       // 头部请求，直接HTTP启动
            GetGlobalDataProvider()->IsDrag() ||                           // 客户端拖动，或VIP开始播放，或次段预下载,
            GetGlobalDataProvider()->IsStartFromNonZero() ||               // ikan拖动，HTTP启动
            GetGlobalDataProvider()->GetBWType() == JBW_HTTP_MORE ||       // BWType = 1, HTTP启动
            !PrefersSavingServerBandwidth())                                // BWType = 2,3 HTTP启动
        {
            SWITCH_DEBUG("HTTP FIRST!");
            return true;
        }

        return false;
    }
}
