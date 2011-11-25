#include "Common.h"
#include "UploadModule.h"
#include "p2sp/stun/StunClient.h"
#include "p2sp/p2p/P2SPConfigs.h"
#include "p2sp/proxy/ProxyModule.h"
#include "statistic/DACStatisticModule.h"
#include "UploadCacheModule.h"

namespace p2sp
{
    static const int32_t MinUploadSpeedLimitInKbsResidentStatus = 10;
    static const int32_t MinUploadSpeedLimitInKbs = 20;

    boost::shared_ptr<UploadModule> UploadModule::inst_;

    UploadModule::UploadModule()
        : recent_play_series_(0)
        , is_disable_upload_(false)
        , upload_policy_(BootStrapGeneralConfig::policy_default)
        , max_upload_speed_include_same_subnet_(0)
        , live_max_upload_speed_exclude_same_subnet_(0)
    {
    }

    void UploadModule::Start(const string& config_path)
    {
        upload_limiter_.reset(new UploadSpeedLimiter());

        upload_manager_list_.push_back(VodUploadManager::create(upload_limiter_));

        live_upload_manager_ = LiveUploadManager::create(upload_limiter_);
        upload_manager_list_.push_back(live_upload_manager_);
        live_upload_manager_->Start();

        upload_manager_list_.push_back(TcpUploadManager::create(upload_limiter_));

        if (config_path.length() == 0)
        {
#ifdef DISK_MODE
            base::util::GetAppDataPath(config_path_);
#endif
        }
        else
        {
            config_path_ = config_path;
        }

        LoadHistoricalMaxUploadSpeed();

        network_quality_monitor_.reset(new NetworkQualityMonitor(global_io_svc()));
        BootStrapGeneralConfig::Inst()->AddUpdateListener(shared_from_this());
    }

    void UploadModule::Stop()
    {
        SaveHistoricalMaxUploadSpeed();
        live_upload_manager_->Stop();
    }

    bool UploadModule::TryHandlePacket(const protocol::Packet & packet)
    {
        if (is_disable_upload_)
        {
            return false;
        }

        for (std::list<boost::shared_ptr<IUploadManager> >::const_iterator
            iter = upload_manager_list_.begin(); iter != upload_manager_list_.end(); ++iter)
        {
            if ((*iter)->TryHandlePacket(packet))
            {
                return true;
            }
        }

        return false;
    }

    void UploadModule::OnP2PTimer(boost::uint32_t times)
    {
        if (times % 4 == 0)
        {
            boost::uint32_t current_upload_speed = MeasureCurrentUploadSpeed();
            upload_speed_limit_tracker_.ReportUploadSpeed(current_upload_speed);

            boost::uint32_t current_upload_speed_include_same_subnet = statistic::UploadStatisticModule::Inst()->GetUploadSpeed();
            if (max_upload_speed_include_same_subnet_ < current_upload_speed_include_same_subnet)
            {
                max_upload_speed_include_same_subnet_ = current_upload_speed_include_same_subnet;
            }

            MeasureLiveMaxUploadSpeedExcludeSameSubnet();

            for (std::list<boost::shared_ptr<IUploadManager> >::const_iterator
                iter = upload_manager_list_.begin(); iter != upload_manager_list_.end(); ++iter)
            {
                (*iter)->AdjustConnections();
            }
        }

        // speed managerment
        OnUploadSpeedControl(times);

        // Statistic
        SubmitUploadInfoStatistic();

        if (times % 4 == 0)
        {
            if (times % 40 == 0)
            {
                // 缓存淘汰
                UploadCacheModule::Inst()->OnP2PTimer(times);

                if (CStunClient::GetLocalFirstIP() != local_ip_from_ini_)
                {
                    upload_speed_limit_tracker_.Reset(P2SPConfigs::UPLOAD_MIN_UPLOAD_BANDWIDTH);

                    SaveHistoricalMaxUploadSpeed();
                    LoadHistoricalMaxUploadSpeed();
                }
            }

            if (times % (10 * 60 * 4) == 0)
            {
                SaveHistoricalMaxUploadSpeed();
            }
        }
    }

    void UploadModule::SetUploadSwitch(bool is_disable_upload)
    {
        is_disable_upload_ = is_disable_upload;
    }

    boost::uint32_t UploadModule::GetUploadBandWidthInBytes()
    {
        return GetMaxUploadSpeedForControl();
    }

    boost::int32_t UploadModule::GetUploadSpeedLimitInKBps() const
    {
        return upload_speed_param_.GetMaxSpeedInKBps();
    }

    boost::int32_t UploadModule::GetMaxConnectLimitSize() const
    {
        return max_connect_peers_;
    }

    boost::int32_t UploadModule::GetMaxUploadLimitSize() const
    {
        return max_upload_peers_;
    }

    bool UploadModule::NeedUseUploadPingPolicy()
    {
        bool is_watching_live = ((AppModule::Inst()->GetPeerState() & 0x0000ffff) == PEERSTATE_LIVE_WORKING);
        if (upload_policy_ == BootStrapGeneralConfig::policy_ping
            && !is_watching_live
            && network_quality_monitor_->IsRunning() && network_quality_monitor_->HasGateWay())
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    void UploadModule::UploadControlOnPingPolicy()
    {
        uint32_t upload_speed_kbs = statistic::StatisticModule::Inst()->GetUploadDataSpeedInKBps();
        DebugLog("upload lost_rate:%d%%, avg_delay:%d ms, upload_speed:%d, upload_bd:%d\n", network_quality_monitor_->GetPingLostRate(),
            network_quality_monitor_->GetAveragePingDelay(), upload_speed_kbs, GetMaxUploadSpeedForControl() / 1024);

        bool is_network_good;
        if (GetMaxUploadSpeedForControl() > 100 * 1024)
        {
            is_network_good = network_quality_monitor_->GetAveragePingDelay() < 20 && 
                network_quality_monitor_->GetPingLostRate() == 0;
        }
        else
        {
            is_network_good = network_quality_monitor_->GetAveragePingDelay() < 80 &&
                network_quality_monitor_->GetPingLostRate() == 0;
        }

        if (network_quality_monitor_->GetPingLostRate() != 0)
        {
            network_quality_monitor_->ClearPingLostRate();
        }

        UpdateSpeedLimit(CalcUploadSpeedLimitOnPingPolicy(is_network_good));
    }

    void UploadModule::LoadHistoricalMaxUploadSpeed()
    {
#ifdef BOOST_WINDOWS_API
        if (config_path_.length() > 0)
        {
            boost::filesystem::path configpath(config_path_);
            configpath /= TEXT("ppvaconfig.ini");
            string filename = configpath.file_string();

            uint32_t upload_velocity = 64*1024;

            try
            {
                framework::configure::Config conf(filename);
                framework::configure::ConfigModule & ppva_um_conf =
                    conf.register_module("PPVA_UM_NEW");

                // IP check
                ppva_um_conf(CONFIG_PARAM_NAME_RDONLY("I", local_ip_from_ini_));
                uint32_t ip_local = CStunClient::GetLocalFirstIP();

                if (local_ip_from_ini_ == ip_local)
                {
                    ppva_um_conf(CONFIG_PARAM_NAME_RDONLY("V", upload_velocity));
                }
            }
            catch(...)
            {
                base::filesystem::remove_nothrow(filename);
            }

            upload_speed_limit_tracker_.Reset(upload_velocity);
        }
#endif
    }

    void UploadModule::SaveHistoricalMaxUploadSpeed()
    {
#ifdef BOOST_WINDOWS_API
        if (config_path_.length() > 0)
        {
            boost::filesystem::path configpath(config_path_);
            configpath /= TEXT("ppvaconfig.ini");
            string filename = configpath.file_string();

            try
            {
                framework::configure::Config conf(filename);
                framework::configure::ConfigModule & ppva_um_conf =
                    conf.register_module("PPVA_UM_NEW");

                uint32_t ip_local;
                uint32_t v;
                ppva_um_conf(CONFIG_PARAM_NAME_RDONLY("I", ip_local));
                ppva_um_conf(CONFIG_PARAM_NAME_RDONLY("V", v));

                ip_local = CStunClient::GetLocalFirstIP();
                v = upload_speed_limit_tracker_.GetMaxUnlimitedUploadSpeedInRecord();

                conf.sync();
            }
            catch(...)
            {
                base::filesystem::remove_nothrow(filename);
            }
        }
#endif
    }

    void UploadModule::OnConfigUpdated()
    {
        BootStrapGeneralConfig::UploadPolicy new_policy = BootStrapGeneralConfig::Inst()->GetUploadPolicy();
        if (new_policy != upload_policy_)
        {
            if (new_policy == BootStrapGeneralConfig::policy_default)
            {
                if (upload_policy_ == BootStrapGeneralConfig::policy_ping)
                {
                    upload_policy_ = BootStrapGeneralConfig::policy_default;
                    network_quality_monitor_->Stop();
                }
            }
            else if (new_policy == BootStrapGeneralConfig::policy_ping)
            {
                if (upload_policy_ == BootStrapGeneralConfig::policy_default)
                {
                    upload_policy_ = BootStrapGeneralConfig::policy_ping;
                    network_quality_monitor_->Start();
                }
            }
        }
    }

    boost::uint32_t UploadModule::MeasureCurrentUploadSpeed() const
    {
        boost::uint32_t current_upload_speed = 0;
        for (std::list<boost::shared_ptr<IUploadManager> >::const_iterator 
            iter = upload_manager_list_.begin(); iter != upload_manager_list_.end(); ++iter)
        {
            current_upload_speed += (*iter)->MeasureCurrentUploadSpeed();
        }

        return current_upload_speed;
    }

    void UploadModule::OnUploadSpeedControl(uint32_t times)
    {
        if (times % 4 == 0)
        {
            if (P2SPConfigs::UPLOAD_BOOL_CONTROL_MODE)
            {
                UpdateSpeedLimit(P2SPConfigs::UPLOAD_SPEED_LIMIT);
                return;
            }

            if (NeedUseUploadPingPolicy())
            {
                UploadControlOnPingPolicy();
            }
            else
            {
                if (times % (4*60) == 0)
                {
                    recent_play_series_ <<= 1;
                }

                if (times % (4*5) == 0)
                {
                    if (p2sp::ProxyModule::Inst()->IsWatchingMovie())
                    {
                        recent_play_series_ |= 1;
                    }
                }

                UploadControlOnIdleTimePolicy();
            }
        }
    }

    void UploadModule::UploadControlOnIdleTimePolicy()
    {
        bool is_main_state = ((AppModule::Inst()->GetPeerState() & 0xFFFF0000) == PEERSTATE_MAIN_STATE);
        bool is_watching_live = ((AppModule::Inst()->GetPeerState() & 0x0000ffff) == PEERSTATE_LIVE_WORKING);
        bool is_download_with_slowmode = p2sp::ProxyModule::Inst()->IsDownloadWithSlowMode();
        bool is_downloading_movie = p2sp::ProxyModule::Inst()->IsDownloadingMovie();
        bool is_http_downloading = p2sp::ProxyModule::Inst()->IsHttpDownloading();
        bool is_p2p_downloading = p2sp::ProxyModule::Inst()->IsP2PDownloading();
        bool is_watching_movie = p2sp::ProxyModule::Inst()->IsWatchingMovie();            
        uint32_t idle_time_in_seconds = storage::Performance::Inst()->GetIdleInSeconds();
        uint32_t upload_bandwidth = GetMaxUploadSpeedForControl();
        boost::uint32_t revised_up_speedlimit = (std::max)((boost::int32_t)upload_bandwidth - 262144, boost::int32_t(0));     // 超过256KBps的上传带宽
        bool is_watching_live_by_peer = p2sp::ProxyModule::Inst()->IsWatchingLive();

        desktop_type_ = storage::Performance::Inst()->GetCurrDesktopType();
        if (false == is_locking_)
        {
            if (storage::DT_WINLOGON == desktop_type_ || storage::DT_SCREEN_SAVER == desktop_type_)
            {
                if (idle_time_in_seconds >= 5)
                {
                    is_locking_ = true;
                }
            }
        }
        else
        {
            if (storage::DT_DEFAULT == desktop_type_)
            {
                is_locking_ = false;
            }
        }

        // locking
        if (true == is_locking_)
        {
            UpdateSpeedLimit(-1);
        }
        // 1G-live
        else if (true == is_watching_live)
        {
            UpdateSpeedLimit(0);
        }
        else if (true == is_watching_live_by_peer && 
                 false == BootStrapGeneralConfig::Inst()->LimitLive2UploadSpeed())
        {
            UpdateSpeedLimit(-1);
        }
        // slow down mode
        else if (is_download_with_slowmode)
        {
            boost::int32_t speed_limit = revised_up_speedlimit > 0 ? revised_up_speedlimit : std::min(static_cast<uint32_t>(upload_bandwidth * 0.3 + 0.5), (uint32_t)15 * 1024);
            UpdateSpeedLimit(speed_limit);
        }
        // downloading
        else if (true == is_downloading_movie && is_http_downloading)
        {
            if (is_p2p_downloading)
            {
                boost::int32_t speed_limit = revised_up_speedlimit > 0 ? revised_up_speedlimit : 20 * 1024;
                UpdateSpeedLimit(speed_limit);
            }
            else
            {
                boost::int32_t speed_limit = revised_up_speedlimit > 0 ? revised_up_speedlimit : 32 * 1024;
                UpdateSpeedLimit(speed_limit);
            }
        }
        else if (true == is_downloading_movie && !is_http_downloading)
        {
            boost::int32_t speed_limit = revised_up_speedlimit > 0 ? revised_up_speedlimit : static_cast<boost::int32_t>(upload_bandwidth * 0.5 + 0.5);
            UpdateSpeedLimit(speed_limit);
        }
        // watching or 5min after watching
        else if (true == is_watching_movie)
        {
            boost::int32_t speed_limit = -1;
            UpdateSpeedLimit(speed_limit);
        }
        else if (is_main_state && (recent_play_series_ & 0x1Fu) > 0)
        {
            boost::int32_t speed_limit = revised_up_speedlimit > 0 ? (revised_up_speedlimit + 235930) : static_cast<boost::int32_t>(upload_bandwidth * 0.9 + 0.5);  // 235930 = 256*1024*0.9
            UpdateSpeedLimit(speed_limit);
        }
        // [0, 1)
        else if (idle_time_in_seconds >= 0 && idle_time_in_seconds < 1 * 60)
        {
            boost::int32_t speed_limit = revised_up_speedlimit > 0 ? revised_up_speedlimit : std::min(static_cast<uint32_t>(upload_bandwidth * 0.3 + 0.5), (uint32_t)32 * 1024);
            UpdateSpeedLimit(speed_limit);
        }
        // [1, 5)
        else if (idle_time_in_seconds >= 1 && idle_time_in_seconds < 5 * 60)
        {
            // 78643 = 256*1024*0.3
            boost::int32_t speed_limit = revised_up_speedlimit > 0 ? (revised_up_speedlimit  + 78643) : std::min(static_cast<uint32_t>(upload_bandwidth * 0.5 + 0.5), (uint32_t)32 * 1024);
            UpdateSpeedLimit(speed_limit);
        }
        // [5, 20)
        else if (idle_time_in_seconds >= 5 * 60 && idle_time_in_seconds < 20 * 60)
        {
            // 183501 = 256*1024*0.7
            boost::int32_t speed_limit = revised_up_speedlimit > 0 ? (revised_up_speedlimit  + 183501) : std::min(static_cast<uint32_t>(upload_bandwidth * 0.8 + 0.5), (uint32_t)128 * 1024);
            UpdateSpeedLimit(speed_limit);
        }
        // [20, + Inf)
        else if (idle_time_in_seconds >= 20 * 60)
        {
            boost::int32_t speed_limit = -1;
            UpdateSpeedLimit(speed_limit);
        }
        else
        {
            assert(false);
        }
    }

    boost::uint32_t UploadModule::GetMaxUploadSpeedForControl() const
    {
        return upload_speed_limit_tracker_.GetMaxUploadSpeedForControl();
    }

    void UploadModule::UpdateSpeedLimit(boost::int32_t speed_limit)
    {
        boost::int32_t speed_limit_in_KBps = 0;

        if (speed_limit < 0)
        {
            upload_speed_limit_tracker_.SetUploadWithoutLimit(true);

            speed_limit_in_KBps = speed_limit;
            max_upload_peers_ = P2SPConfigs::UPLOAD_MAX_UPLOAD_PEER_COUNT;
        }
        else
        {
            upload_speed_limit_tracker_.SetUploadWithoutLimit(false);

            speed_limit_in_KBps = speed_limit / 1024;
            const size_t expected_upload_speed_per_peer = p2sp::ProxyModule::Inst()->IsWatchingLive() ? 
                LiveUploadManager::DesirableUploadSpeedPerPeerInKBps : VodUploadManager::DesirableUploadSpeedPerPeerInKBps;
            max_upload_peers_ = speed_limit_in_KBps / expected_upload_speed_per_peer;

            LIMIT_MIN_MAX(max_upload_peers_, 1, P2SPConfigs::UPLOAD_MAX_UPLOAD_PEER_COUNT);
        }

        if (upload_speed_param_.GetMaxSpeedInKBps() != speed_limit_in_KBps)
        {
            // 提交上传限速的变化，不限速时提交最大历史上传速度
            if (speed_limit_in_KBps != -1)
            {
                statistic::DACStatisticModule::Inst()->SubmitP2PUploadSpeedLimitInKBps(speed_limit_in_KBps);
            }
            else
            {
                statistic::DACStatisticModule::Inst()->SubmitP2PUploadSpeedLimitInKBps(GetMaxUploadSpeedForControl()/1024);
            }
        }

        upload_speed_param_.SetMaxSpeedInKBps(speed_limit_in_KBps);
        max_connect_peers_ = max_upload_peers_ + 5;

        upload_limiter_->SetSpeedLimitInKBps(upload_speed_param_.GetMaxSpeedInKBps());
    }

    int32_t UploadModule::CalcUploadSpeedLimitOnPingPolicy(bool is_network_good)
    {
        int32_t upload_speed_limit_kbs = GetUploadSpeedLimitInKBps();
        bool is_main_state = ((AppModule::Inst()->GetPeerState() & 0xFFFF0000) == PEERSTATE_MAIN_STATE);

        if (upload_speed_limit_kbs < 0)
        {
            if (is_network_good)
            {
                return upload_speed_limit_kbs;
            }
            else
            {
                if (is_main_state)
                {
                    return MinUploadSpeedLimitInKbs * 1024;
                }
                else
                {
                    return MinUploadSpeedLimitInKbsResidentStatus * 1024;
                }
            }
        }
        else
        {
            if (!is_network_good)
            {
                upload_speed_limit_kbs /= 2;

                if (is_main_state)
                {
                    LIMIT_MIN(upload_speed_limit_kbs, MinUploadSpeedLimitInKbs);
                }
                else
                {
                    LIMIT_MIN(upload_speed_limit_kbs, MinUploadSpeedLimitInKbsResidentStatus);
                }
            }
            else
            {
                uint32_t upload_speed_kbs = statistic::StatisticModule::Inst()->GetUploadDataSpeedInKBps();
                if (upload_speed_limit_kbs < upload_speed_kbs + 120 ||
                    upload_speed_limit_kbs < upload_speed_kbs * 12 / 10)
                {
                    upload_speed_limit_kbs *= 1.2;
                }
            }

            return upload_speed_limit_kbs * 1024;
        }
    }

    size_t UploadModule::GetCurrentCacheSize() const
    {
        return UploadCacheModule::Inst()->GetCurrentCacheSize();
    }

    void UploadModule::SetCurrentCacheSize(size_t cache_size)
    {
        UploadCacheModule::Inst()->SetCurrentCacheSize(cache_size);
    }

    void UploadModule::SubmitUploadInfoStatistic()
    {
        std::set<boost::asio::ip::address> accept_uploading_peers;

        for (std::list<boost::shared_ptr<IUploadManager> >::const_iterator
            iter = upload_manager_list_.begin(); iter != upload_manager_list_.end(); ++iter)
        {
            ((*iter)->GetUploadingPeers(accept_uploading_peers));
        }
        DebugLog("accept_uploading_peers size = %d\n", accept_uploading_peers.size());
        statistic::UploadStatisticModule::Inst()->SubmitUploadInfo(upload_speed_param_.GetMaxSpeedInKBps(), accept_uploading_peers);
    }

    void UploadModule::SetUploadUserSpeedLimitInKBps(boost::int32_t user_speed_in_KBps)
    {
        assert(user_speed_in_KBps >= -1);
        upload_speed_param_.SetUserSpeedInKBps(user_speed_in_KBps);
        upload_limiter_->SetSpeedLimitInKBps(upload_speed_param_.GetMaxSpeedInKBps());
    }

    boost::uint32_t UploadModule::GetMaxUnlimitedUploadSpeedInRecord() const
    {
        return upload_speed_limit_tracker_.GetMaxUnlimitedUploadSpeedInRecord();
    }

    boost::uint32_t UploadModule::GetMaxUploadSpeedIncludeSameSubnet() const
    {
        return max_upload_speed_include_same_subnet_;
    }

    void UploadModule::MeasureLiveMaxUploadSpeedExcludeSameSubnet()
    {
        boost::uint32_t current_upload_speed = 0;

        std::set<boost::asio::ip::address> accept_uploading_peers;
        live_upload_manager_->GetUploadingPeersExcludeSameSubnet(accept_uploading_peers);

        for (std::set<boost::asio::ip::address>::const_iterator iter = accept_uploading_peers.begin();
            iter != accept_uploading_peers.end(); ++iter)
        {
            current_upload_speed += statistic::UploadStatisticModule::Inst()->GetUploadSpeed(*iter);
        }

        if (live_max_upload_speed_exclude_same_subnet_ < current_upload_speed)
        {
            live_max_upload_speed_exclude_same_subnet_ = current_upload_speed;
        }
    }

    boost::uint32_t UploadModule::GetMaxUploadSpeedExcludeSameSubnet() const
    {
        return live_max_upload_speed_exclude_same_subnet_;
    }
}