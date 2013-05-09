//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "p2sp/download/SwitchController.h"
#include "DownloadDriver.h"
#include "../bootstrap/BootStrapGeneralConfig.h"
#include "p2sp/proxy/ProxyModule.h"


namespace p2sp
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_switch = log4cplus::Logger::getInstance("[switch_download_control]");
#endif

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
        is_tiny_drag_timer_reset_ = false;
        is_p2p_timer_reset_ = false;
        is_http_timer_reset_ = false;
        is_p2p_connected_timer_reset_ = false;

        LOG4CPLUS_DEBUG_LOG(logger_switch, string(20, '-'));

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

    void SwitchController::DownloadControlMode::ChangeTo0200()
    {
        assert(!GetHTTPControlTarget());
        GetP2PControlTarget()->Resume();
        state_.p2p_ = State::P2P_DOWNLOADING;
    }

    void SwitchController::DownloadControlMode::ChangeTo2000()
    {
        assert(!GetP2PControlTarget());
        GetHTTPControlTarget()->Resume();
        state_.http_ = State::HTTP_DOWNLOADING;
    }

    void SwitchController::DownloadControlMode::ChangeTo2200()
    {
        GetHTTPControlTarget()->Resume();
        GetP2PControlTarget()->Resume();
        state_.http_ = State::HTTP_DOWNLOADING;
        state_.p2p_ = State::P2P_DOWNLOADING;
    }

    void SwitchController::DownloadControlMode::ChangeTo2300()
    {
        GetHTTPControlTarget()->Resume();
        GetP2PControlTarget()->Pause();
        state_.http_ = State::HTTP_DOWNLOADING;
        state_.p2p_ = State::P2P_PAUSING;
    }

    void SwitchController::DownloadControlMode::ChangeTo3200()
    {
        GetHTTPControlTarget()->Pause();
        GetP2PControlTarget()->Resume();
        state_.http_ = State::HTTP_PAUSING;
        state_.p2p_ = State::P2P_DOWNLOADING;
    }

    bool SwitchController::DownloadControlMode::IsP2PBad(bool is_current_p2p_bad)
    {
        if (false == IsRunning())
            return false;

        assert(state_.p2p_ == State::P2P_DOWNLOADING);
        assert(GetP2PControlTarget());
        boost::uint32_t now_speed = GetP2PControlTarget()->GetCurrentDownloadSpeed();
        boost::uint32_t threshold_speed = 0;

        //当前状态Good时判断P2P是否BAD的阈值比当前BAD状态下大10KBps

        if (GetGlobalDataProvider()->GetBWType() == JBW_P2P_INCREASE_CONNECT)
        {
            boost::uint32_t MT_min_speed = BootStrapGeneralConfig::Inst()->GetMinDownloadSpeedToBeEnsuredInKBps();
            threshold_speed = is_current_p2p_bad ? (MT_min_speed + 10) * 1024 :MT_min_speed * 1024;
        }
        else
        {
            threshold_speed = is_current_p2p_bad ? 30 * 1024 : 20 * 1024;
        }
        //保持状态机不变的条件
        if ((!is_current_p2p_bad && now_speed > threshold_speed) || (is_current_p2p_bad && now_speed < threshold_speed))
        {
            return is_current_p2p_bad;
        }
        else
        {
            return !is_current_p2p_bad;
        }
    }

    bool SwitchController::DownloadControlMode::IsHTTPNormal()
    {
        boost::uint32_t http_speed = GetHTTPControlTarget()->GetCurrentDownloadSpeed();
        return http_speed > 0;
    }

    void SwitchController::DownloadControlMode::OnControlTimer(boost::uint32_t times)
    {
        if (false == IsRunning())
            return;

        if (is_paused_by_sdk_)
        {
            return;
        }

        while (true)
        {

            LOG4CPLUS_DEBUG_LOG(logger_switch, (string)state_);

            // 设置为快速模式，无冗余
            if (GetP2PControlTarget())
            {
                GetP2PControlTarget()->SetDownloadMode(IP2PControlTarget::FAST_MODE);
            }

            //////////////////////////////////////////////////////////////////////////
            // Initial State
            // <0000*0>
            if (state_.http_ ==State::HTTP_NONE && state_.p2p_ == State::P2P_NONE)
            {
                if (!GetHTTPControlTarget() && !GetP2PControlTarget())
                {
                    break;
                }
                if (GetHTTPControlTarget())
                {
                    GetHTTPControlTarget()->Pause();
                    state_.http_ = State::HTTP_PAUSING;
                }
                if (GetP2PControlTarget())
                {
                    GetP2PControlTarget()->Pause();
                    state_.p2p_ = State::P2P_PAUSING;
                }
            }
            // <0300*1>
            if (state_.http_ == State::HTTP_NONE && state_.p2p_ == State::P2P_PAUSING)
            {
                assert(!GetHTTPControlTarget());
                assert(GetP2PControlTarget());
                assert(GetGlobalDataProvider()->GetBWType() != JBW_HTTP_ONLY);
                ChangeTo0200();
                break;
            }
            // <3300*1>
            else if (state_.http_ == State::HTTP_PAUSING && state_.p2p_ == State::P2P_PAUSING)
            {
                assert(GetGlobalDataProvider()->HasRID());
                assert(GetP2PControlTarget());
                assert(GetGlobalDataProvider()->GetBWType() != JBW_HTTP_ONLY);
                assert(GetGlobalDataProvider()->GetBWType() != JBW_VOD_P2P_ONLY);

                //不限速时，vip 追求最大速度，直接转入2200
                if (!p2sp::ProxyModule::Inst()->IsDownloadSpeedLimited() && 
                    GetGlobalDataProvider()->GetBWType() == JBW_HTTP_PREFERRED)
                {
                    ChangeTo2200();
                    break;
                }

                //对于isp用户，尽量多用HTTP，从2300开始下载
                if (GetGlobalDataProvider()->GetBWType() == JBW_HTTP_MORE)
                {
                    ChangeTo2300();
                    break;
                }
                //限速且bwtype!=JBW_HTTP_MORE,则从3200开始下载
                else if (p2sp::ProxyModule::Inst()->IsDownloadSpeedLimited())
                {
                    ChangeTo3200();
                    continue;
                }
                else
                {
                    if (is_p2p_connected_timer_reset_)
                    {
                        // 不限速，如果有P2P连接，则转入3200开始下载
                        if (GetP2PControlTarget()->GetConnectedPeersCount() >= 1)
                        {
                            is_p2p_connected_timer_reset_ = false;
                            ChangeTo3200();
                            continue;
                        }
                        // 不限速，如果超过2S都没有建立P2P连接，则转入2300开始下载
                        if (waiting_p2p_connected_timer_counter_.elapsed() >= 2 * 1000)
                        {
                            is_p2p_connected_timer_reset_ = false;
                            ChangeTo2300();
                            break;
                        }
                        break;
                    }
                    waiting_p2p_connected_timer_counter_.reset();
                    is_p2p_connected_timer_reset_ = true;
                    break;
                }
            }
            // <3000*0>
            else if (state_.http_ == State::HTTP_PAUSING && state_.p2p_ == State::P2P_NONE)
            {
                assert(GetGlobalDataProvider()->GetBWType() != JBW_VOD_P2P_ONLY);

                if (GetGlobalDataProvider()->GetBWType() == JBW_HTTP_ONLY || 
                    GetGlobalDataProvider()->GetBWType() == JBW_HTTP_MORE)
                {
                    ChangeTo2000();
                }
                else
                {
                    if (GetP2PControlTarget())
                    {
                        //如果存在P2P,则转向3300
                        GetP2PControlTarget()->Pause();
                        state_.p2p_ = State::P2P_PAUSING;
                    }
                    else
                    {
                        //超时未获取到tinydrag，则转向2000，否则若获取到tinydrag,则转向3300，若未超时，则仍3000
                        if (is_tiny_drag_timer_reset_)
                        {
                            if (waiting_tinydrag_timer_counter_.elapsed() >= BootStrapGeneralConfig::Inst()->
                                GetWaitTimeForTinydragInDownloadMode() * 1000)
                            {
                                ChangeTo2000();
                                is_tiny_drag_timer_reset_ = false;
                            }
                            break;
                        }
                        waiting_tinydrag_timer_counter_.reset();
                        is_tiny_drag_timer_reset_ = true;
                    }
                }
                break;
            }
            //////////////////////////////////////////////////////////////////////////
            // Stable State
            // <0200*1>
            else if (state_.http_ == State::HTTP_NONE && state_.p2p_ == State::P2P_DOWNLOADING)
            {
                break;
            }
            // <2000*0>
            else if (state_.http_ == State::HTTP_DOWNLOADING && state_.p2p_ == State::P2P_NONE)
            {
                //若获取到tinydrag，则转向2300
                if (GetP2PControlTarget())
                {
                    ChangeTo2300();
                }
                break;
            }
            // <2300*1>
            else if (state_.http_ == State::HTTP_DOWNLOADING && state_.p2p_ == State::P2P_PAUSING)
            {
                //若HTTP挂掉，若广告素材则转入3200，否则转入2200
                if (!IsHTTPNormal())
                {
                    if (GetGlobalDataProvider()->GetDownloadLevel() == PlayInfo::PASSIVE_DOWNLOAD_LEVEL)
                    {
                        ChangeTo3200();
                        break;
                    }
                    else
                    {
                        ChangeTo2200();
                        //直接在while循环中,进入2200状态
                        continue;
                    }
                }
                // 有P2P节点时，若广告素材则转入3200，否则若bwtype!=JBW_HTTP_MORE，则转入2200
                else if (GetP2PControlTarget()->GetConnectedPeersCount() >= 1)
                {
                    if (GetGlobalDataProvider()->GetDownloadLevel() == PlayInfo::PASSIVE_DOWNLOAD_LEVEL)
                    {
                        ChangeTo3200();
                        break;
                    }
                    else if (GetGlobalDataProvider()->GetBWType() != JBW_HTTP_MORE)
                    {
                        ChangeTo2200();
                        continue;
                    }
                }
                //其他情况，保持2300
                break;
            }
            // <3200*1>
            else if (state_.http_ == State::HTTP_PAUSING && state_.p2p_ == State::P2P_DOWNLOADING)
            {
                // assert
                assert(GetHTTPControlTarget());
                //若限速，则保持3200状态
                if (p2sp::ProxyModule::Inst()->IsDownloadSpeedLimited())
                {
                    break;
                }
                //vip不限速时，直接转入2200
                else if (GetGlobalDataProvider()->GetBWType() == JBW_HTTP_PREFERRED)
                {
                    ChangeTo2200();
                    break;
                }
                else if (GetGlobalDataProvider()->GetDownloadLevel() == PlayInfo::PASSIVE_DOWNLOAD_LEVEL)
                {
                    //若广告素材不限速时，P2P节点数为0，则转入2200
                    if (GetP2PControlTarget()->GetConnectedPeersCount() == 0)
                    {
                        ChangeTo2300();
                    }
                    //广告素材，有P2P节点，则保持3200
                    break;
                }

                //若在连续 10S 的时间内，P2P速度都不理想，则转入2200，用HTTP提升速度
                if (is_p2p_timer_reset_)
                {
                    if (!IsP2PBad(false))
                    {
                        waiting_p2p_stable_timer_counter_.reset();
                    }
                    if (waiting_p2p_stable_timer_counter_.elapsed() >= 10 * 1000)
                    {
                        ChangeTo2200();
                        is_p2p_timer_reset_ = false;
                        //直接在while循环中,进入2200状态
                        continue;
                    }
                    break;
                }
                waiting_p2p_stable_timer_counter_.reset();
                is_p2p_timer_reset_ = true;
                break;
            }
            // <2200*1>
            else if (state_.http_ == State::HTTP_DOWNLOADING && state_.p2p_ == State::P2P_DOWNLOADING)
            {
                // asserts
                assert(GetHTTPControlTarget());
                assert(GetP2PControlTarget());

                //若vip且不限速，则保留在2200状态
                if (GetGlobalDataProvider()->GetBWType() == JBW_HTTP_PREFERRED && 
                    !p2sp::ProxyModule::Inst()->IsDownloadSpeedLimited())
                {
                    break;
                }

                //bwtype为JBW_HTTP_MORE时，若HTTP正常则转入2300，否则继续2200
                if (GetGlobalDataProvider()->GetBWType() == JBW_HTTP_MORE)
                {
                    if (IsHTTPNormal())
                    {
                        ChangeTo2300();
                    }
                    break;
                }
                //bwtype不是JBW_HTTP_MORE，若限速则转入3200
                else if (p2sp::ProxyModule::Inst()->IsDownloadSpeedLimited())
                {
                    ChangeTo3200();
                    break;
                }
                //不限速，bwtype!=JBW_HTTP_MORE，P2P没有节点，则转入2300
                else if (GetP2PControlTarget()->GetConnectedPeersCount() == 0)
                {
                    ChangeTo2300();
                    break;
                }
                else
                {
                    //不限速，bwt!= JBW_HTTP_MORE,HTTP连续2s速度为0，则认为HTTP挂掉，转入3200
                    if (is_http_timer_reset_)
                    {
                        if (IsHTTPNormal())
                        {
                            waiting_http_stable_timer_counter_.reset();
                        }
                        if (waiting_http_stable_timer_counter_.elapsed() >= 2 * 1000)
                        {
                            ChangeTo3200();
                            is_http_timer_reset_ = false;
                            continue;
                        }
                        //HTTP正常、非vip、bwtype!=JBW_HTTP_MORE、不限速时，若连续5秒P2P速度理想，则转入3200
                        else
                        {
                            if (IsP2PBad(true))
                            {
                                waiting_p2p_stable_timer_counter_.reset();
                            }
                            if (waiting_p2p_stable_timer_counter_.elapsed() >= 5 * 1000)
                            {
                                ChangeTo3200();
                                is_p2p_timer_reset_ = false;
                                //直接在while循环中,进入3200状态
                                continue;
                            }
                        }
                        break;
                    }
                    waiting_p2p_stable_timer_counter_.reset();
                    waiting_http_stable_timer_counter_.reset();
                    is_http_timer_reset_ = true;
                    break;
                }
            }
            else
            {
                assert(!"SwitchController::DownloadControlMode::OnControlTimer: No Such State!");
                break;
            }
        }
    }
}