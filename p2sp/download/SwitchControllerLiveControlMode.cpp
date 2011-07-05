//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "p2sp/download/SwitchController.h"

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

        // 如果只有一个downloader，立即开始下载
        if (!GetHTTPControlTarget())
        {
            GetP2PControlTarget()->Resume();
            state_.p2p_ = State::P2P_DOWNLOADING;
        }

        if (!GetP2PControlTarget())
        {
            GetHTTPControlTarget()->Resume();
            state_.http_ = State::HTTP_DOWNLOADING;
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

        // 3000
        if (state_.http_ == State::HTTP_PAUSING && state_.p2p_ == State::P2P_NONE)
        {
            CheckState3000();
        }
        // 0300
        else if (state_.http_ == State::HTTP_NONE && state_.p2p_ == State::P2P_PAUSING)
        {
            CheckState0300();
        }
        // 3300
        else if (state_.http_ == State::HTTP_PAUSING && state_.p2p_ == State::P2P_PAUSING)
        {
            CheckState3300();
        }
        // 2300
        else if (state_.http_ == State::HTTP_DOWNLOADING && state_.p2p_ == State::P2P_PAUSING)
        {
            CheckState2300();
        }
        // 2200
        else if (state_.http_ == State::HTTP_DOWNLOADING && state_.p2p_ == State::P2P_DOWNLOADING)
        {
            CheckState2200();
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
    }

    void SwitchController::LiveControlMode::ChangeTo2300()
    {
        ResumeHttpDownloader();
        PauseP2PDownloader();

        time_counter_2300_.reset();
    }

    void SwitchController::LiveControlMode::ChangeTo2200()
    {
        ResumeHttpDownloader();
        ResumeP2PDownloader();
    }

    SwitchController::LiveControlMode::HTTPSPEED SwitchController::LiveControlMode::Check2300HttpSpeed()
    {
        IHTTPControlTarget::p http = GetHTTPControlTarget();
        IP2PControlTarget::p p2p = GetP2PControlTarget();
        boost::uint32_t data_rate = GetGlobalDataProvider()->GetDataRate();

        // Http速度接近带宽，认为Http速度比较好
        if (http->GetCurrentDownloadSpeed() > GetGlobalDataProvider()->GetBandWidth() * 8 / 10)
        {
            http_status_ = http_good;
            return http_fast;
        }
        else
        {
            // P2P的速度比较好或者是不知道，则对Http的验证比较严格
            // 以此来避免Http速度不好但还是跑很久的2300而不去尝试速度可能好的P2P
            if (p2p_status_ == p2p_unknown || p2p_status_ == p2p_good)
            {
                if (time_counter_2300_.elapsed() >= 4000)
                {
                    if (http->GetCurrentDownloadSpeed() < data_rate && http->GetSecondDownloadSpeed() < data_rate)
                    {
                        http_status_ = http_bad;
                        return http_slow;
                    }
                    else
                    {
                        http_status_ = http_good;
                        return http_fast;
                    }
                }
                else if ((time_counter_2300_.elapsed() >= 3000 && http->GetCurrentDownloadSpeed() < data_rate * 8 / 10)
                    ||
                    (time_counter_2300_.elapsed() >= 2000 && http->GetCurrentDownloadSpeed() < data_rate / 2))
                {
                    http_status_ = http_bad;
                    return http_slow;
                }
                else
                {
                    http_status_ = http_unknown;
                    return http_checking;
                }
            }
            // P2P的速度不好，放宽对Http的验证，避免状态机不停切换
            else
            {
                if (time_counter_2300_.elapsed() >= 10000)
                {
                    if (http->GetCurrentDownloadSpeed() < data_rate)
                    {
                        http_status_ = http_bad;
                        return http_slow;
                    }
                    else
                    {
                        http_status_ = http_good;
                        return http_fast;
                    }
                }
                else if (time_counter_2300_.elapsed() >= 5000 && http->GetCurrentDownloadSpeed() < data_rate / 2)
                {
                    http_status_ = http_bad;
                    return http_slow;
                }
                else
                {
                    http_status_ = http_unknown;
                    return http_checking;
                }
            }
        }
    }

    bool SwitchController::LiveControlMode::Is2300RestTimeEnough()
    {
        double data_rate_v;
        boost::uint32_t data_rate_in_kbps = GetGlobalDataProvider()->GetDataRate() / 1024 * 8;
        boost::uint32_t rest_play_time_in_second = GetGlobalDataProvider()->GetRestPlayableTime();
        boost::uint32_t peer_count = GetP2PControlTarget()->GetConnectedPeersCount();

        if (data_rate_in_kbps < 400)
        {
            data_rate_v = 1;
        }
        else if (data_rate_in_kbps < 600)
        {
            data_rate_v = 1.2;
        }
        else if (data_rate_in_kbps < 800)
        {
            data_rate_v = 1.5;
        }
        else
        {
            data_rate_v = 1.8;
        }

        return rest_play_time_in_second >= 15 * data_rate_v && peer_count > 10
            ||
            rest_play_time_in_second >= 30 && peer_count > 0;
    }

    bool SwitchController::LiveControlMode::Is3200P2PSlow()
    {
        boost::uint32_t data_rate = GetGlobalDataProvider()->GetDataRate();
        boost::uint32_t band_width = GetGlobalDataProvider()->GetBandWidth();

        IP2PControlTarget::p p2p = GetP2PControlTarget();

        if (time_counter_3200_.elapsed() >= 10000)
        {
            if (p2p->GetCurrentDownloadSpeed() > data_rate * 11 / 10
                || p2p->GetCurrentDownloadSpeed() > band_width * 8 / 10)
            {
                p2p_status_ = p2p_good;
                p2p_failed_times_ = 0;
                return false;
            }
            else
            {
                return true;
            }
        }
        else if (time_counter_3200_.elapsed() >= 5000)
        {
            return p2p->GetCurrentDownloadSpeed() < data_rate / 2
                && p2p->GetCurrentDownloadSpeed() < band_width * 8 / 10;
        }

        return false;
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

    void SwitchController::LiveControlMode::CheckState3000()
    {
        // 只有Http，并且还处于暂停状态，马上开始

        assert(GetHTTPControlTarget());
        ResumeHttpDownloader();
    }

    void SwitchController::LiveControlMode::CheckState0300()
    {
        // 只有P2P，并且还处于暂停状态，马上开始

        assert(GetP2PControlTarget());
        ResumeP2PDownloader();
    }

    void SwitchController::LiveControlMode::CheckState3300()
    {
        assert(GetHTTPControlTarget());
        assert(GetP2PControlTarget());

        boost::uint32_t rest_play_time_in_second = GetGlobalDataProvider()->GetRestPlayableTime();
        if (rest_play_time_in_second <= 10)
        {
            ChangeTo2300();
        }
        else
        {
            if (GetP2PControlTarget()->GetConnectedPeersCount() > 0)
            {
                ChangeTo3200();
            }
            else
            {
                ChangeTo2300();
            }
        }
    }

    void SwitchController::LiveControlMode::CheckState2300()
    {
        assert(GetHTTPControlTarget());
        assert(GetP2PControlTarget());

        // 已经很久没有试过P2P了，把P2P的失败次数清零
        if (time_counter_2300_.elapsed() > 120 * 1000)
        {
            p2p_failed_times_ = 0;
        }

        // 如果P2P节点数为0，不再进行判断
        if (GetP2PControlTarget()->GetConnectedPeersCount() == 0)
            return;

        http_speed_ = Check2300HttpSpeed();
        switch(http_speed_)
        {
        case http_fast:
            if (Is2300RestTimeEnough())
            {
                // 如果失败太多次，即便剩余时间够，也不要再切换P2P了
                if (p2p_failed_times_ < 3)
                {
                    ChangeTo3200();
                }
            }
            break;
        case http_slow:
            if (p2p_status_ == p2p_bad)
            {
                ChangeTo2200();
            }
            else
            {
                ChangeTo3200();
            }
            break;
        case http_checking:
            // 刚启动时多给Http一些机会
            if (p2p_status_ != p2p_bad && Is2300RestTimeEnough()
                && time_counter_live_control_mode_.elapsed() > 8 * 1000)
            {
                ChangeTo3200();
            }
        }
    }

    void SwitchController::LiveControlMode::CheckState3200()
    {
        assert(GetHTTPControlTarget());
        assert(GetP2PControlTarget());

        boost::uint32_t rest_play_time_in_second = GetGlobalDataProvider()->GetRestPlayableTime();

        // Http是好的，可以等到剩余时间不足15秒的时候再切到Http
        if (http_status_ == http_good)
        {
            if (rest_play_time_in_second < 15)
            {
                // TODO(emma): 判断是否很接近直播点了，
                // 或者当前下载点是否已超过全部（或绝大多数）peer所返回的announce map的覆盖范围
                // 如果是的话，这里把p2p_status_设置为bad并不合理
                ++p2p_failed_times_;
                p2p_status_ = p2p_bad;
                ChangeTo2300();
            }
        }
        else
        {
            // 只有当剩余时间小于30秒时才切换
            is_3200_p2p_slow_ = Is3200P2PSlow();
            if (is_3200_p2p_slow_ && rest_play_time_in_second < 30)
            {
                ++p2p_failed_times_;
                p2p_status_ = p2p_bad;

                // 如果http的速度未知，则再试一次http
                if (http_status_ == http_unknown)
                {
                    ChangeTo2300();
                }
                else
                {
                    ChangeTo2200();
                }
            }
        }
    }

    void SwitchController::LiveControlMode::CheckState2200()
    {
        assert(GetHTTPControlTarget());
        assert(GetP2PControlTarget());

        IHTTPControlTarget::p http = GetHTTPControlTarget();
        IP2PControlTarget::p p2p = GetP2PControlTarget();
        boost::uint32_t data_rate = GetGlobalDataProvider()->GetDataRate();
        boost::uint32_t band_width = GetGlobalDataProvider()->GetBandWidth();

        framework::timer::TickCounter time_counter_x_;
        framework::timer::TickCounter time_counter_y_;

        if (http->GetCurrentDownloadSpeed() < data_rate * 11 / 10
            && http->GetCurrentDownloadSpeed() < band_width * 8 / 10)
        {
            time_counter_x_.reset();
        }

        if (p2p->GetCurrentDownloadSpeed() < data_rate * 12 / 10
            && p2p->GetCurrentDownloadSpeed() < band_width * 8 / 10)
        {
            time_counter_y_.reset();
        }

        // 如果剩余时间比较多了，则根据P2P是否连接上节点来决定切到P2P还是Http
        if (GetGlobalDataProvider()->GetRestPlayableTime() > 30)
        {
            if (GetP2PControlTarget()->GetConnectedPeersCount() > 0)
            {
                ChangeTo3200();
                return;
            }

            ChangeTo2300();
            return;
        }

        // 如果P2P连续5秒速度都比较好，则切换成3200
        if (time_counter_y_.elapsed() >= 5000)
        {
            ChangeTo3200();
            return;
        }

        // 如果Http连续5秒速度都比较好，并且P2P的速度不好，则切换成2300
        if (time_counter_x_.elapsed() >= 5000)
        {
            ChangeTo2300();
            return;
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

    boost::int32_t SwitchController::LiveControlMode::GetP2PFailedTimes() const
    {
        return p2p_failed_times_;
    }

    boost::uint8_t SwitchController::LiveControlMode::Get2300HttpSpeedStatus() const
    {
        if (state_.http_ == State::HTTP_DOWNLOADING && state_.p2p_ == State::P2P_PAUSING)
        {
            return http_speed_;
        }
        return http_none;
    }

    boost::uint8_t SwitchController::LiveControlMode::GetHttpStatus() const
    {
        return http_status_;
    }

    boost::uint8_t SwitchController::LiveControlMode::GetP2PStatus() const
    {
        return p2p_status_;
    }

    bool SwitchController::LiveControlMode::GetWhether3200P2PSlow() const
    {
        if (state_.http_ == State::HTTP_PAUSING && state_.p2p_ == State::P2P_DOWNLOADING)
        {
            return is_3200_p2p_slow_;
        }
        return false;
    }
}
