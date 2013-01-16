#ifndef _P2SP_BOOTSTRAP_GENERAL_CONFIG_H_
#define _P2SP_BOOTSTRAP_GENERAL_CONFIG_H_

namespace p2sp
{
    class ConfigUpdateListener
    {
    public:
        virtual void OnConfigUpdated() = 0;
        virtual ~ConfigUpdateListener(){}
    };

    class BootStrapGeneralConfig
    {
    public:
        static boost::shared_ptr<BootStrapGeneralConfig> Inst() { return inst_; }

        void Start(string const & config_path);
        void Stop();

        void SetConfigString(string const & config_string, bool save_to_disk);

        string GetHouServerList() const {return hou_server_list_;}
        bool UsePush() const {return use_push_;}

        bool IsDataCollectionOn() const { return GetDataCollectionServers().size() > 0; }

        std::vector<string> GetDataCollectionServers() const;

        void AddUpdateListener(boost::shared_ptr<ConfigUpdateListener> listener);
        bool RemoveUpdateListener(boost::shared_ptr<ConfigUpdateListener> listener);

        enum UploadPolicy
        {
            policy_default = 0,
            policy_ping
        };
        UploadPolicy GetUploadPolicy() {return upload_policy_;}
        bool IsConnectionPolicyEnable() {return connection_policy_enable_;}
        bool ShouldUseCDNWhenLargeUpload(bool is_saving_mode) const
        {
            return is_saving_mode ? use_cdn_when_large_upload_in_saving_mode_ : use_cdn_when_large_upload_;
        }

        boost::uint32_t GetDesirableVodIpPoolSize() const 
        { 
            return desirable_vod_ippool_size_; 
        }
        
        boost::uint32_t GetDesirableLiveIpPoolSize() const 
        { 
            return desirable_live_ippool_size_; 
        }

        boost::uint32_t GetRestPlayTimeDelim(bool is_saving_mode) const
        {
            return is_saving_mode ? rest_play_time_delim_in_saving_mode_: rest_play_time_delim_;
        }

        boost::uint32_t GetRatioDelimOfUploadSpeedToDatarate(bool is_saving_mode) const
        {
            return is_saving_mode ? ratio_delim_of_upload_speed_to_datarate_in_saving_mode_
                : ratio_delim_of_upload_speed_to_datarate_;
        }

        bool LimitLive2UploadSpeed() const 
        { 
            return limit_upload_speed_for_live2_; 
        }

        boost::uint32_t GetSendPeerInfoPacketIntervalInSecond() const
        {
            return send_peer_info_packet_interval_in_second_;
        }

        boost::uint32_t GetSafeEnoughRestPlayableTime(bool is_saving_mode) const
        {
            return is_saving_mode ? safe_enough_rest_playable_time_delim_under_http_in_saving_mode_
                : safe_enough_rest_playable_time_delim_under_http_;
        }

        boost::uint32_t GetHttpRunningLongEnoughTimeWhenStart(bool is_saving_mode) const
        {
            return is_saving_mode ? http_running_long_enough_time_when_start_in_saving_mode_
                : http_running_long_enough_time_when_start_;
        }

        boost::uint32_t GetHttpProtectTimeWhenStart(bool is_saving_mode) const
        {
            return is_saving_mode ? http_protect_time_when_start_in_saving_mode_ : http_protect_time_when_start_;
        }

        boost::uint32_t GetHttpProtectTimeWhenUrgentSwitched(bool is_saving_mode) const
        {
            return is_saving_mode ? http_protect_time_when_urgent_switched_in_saving_mode_
                : http_protect_time_when_urgent_switched_;
        }

        boost::uint32_t GetHttpRunningLongEnoughTimeWhenUrgentSwitched(bool is_saving_mode) const
        {
            return is_saving_mode ? http_running_long_enough_time_when_urgent_switched_in_saving_mode_
                : http_running_long_enough_time_when_urgent_switched_;
        }

        boost::uint32_t GetSafeRestPlayableTimeDelimWhenUseHttp(bool is_saving_mode) const
        {
            return is_saving_mode ? safe_rest_playable_time_delim_when_use_http_in_saving_mode_
                : safe_rest_playable_time_delim_when_use_http_;
        }

        boost::uint32_t GetHttpProtectTimeWhenLargeUpload(bool is_saving_mode) const
        {
            return is_saving_mode ? http_protect_time_when_large_upload_in_saving_mode_
                : http_protect_time_when_large_upload_;
        }

        boost::uint32_t GetP2PRestPlayableTimeDelimWhenSwitchedWithLargeTime(bool is_saving_mode) const
        {
            return is_saving_mode ? p2p_rest_playable_time_delim_when_switched_with_large_time_in_saving_mode_
                : p2p_rest_playable_time_delim_when_switched_with_large_time_;
        }

        boost::uint32_t GetP2PRestPlayableTimeDelim(bool is_saving_mode) const
        {
            return is_saving_mode ? p2p_rest_playable_time_delim_in_saving_mode_
                : p2p_rest_playable_time_delim_;
        }

        boost::uint32_t GetP2PProtectTimeWhenSwitchedWithNotEnoughTime(bool is_saving_mode) const
        {
            return is_saving_mode ? p2p_protect_time_when_switched_with_not_enough_time_in_saving_mode_
                : p2p_protect_time_when_switched_with_not_enough_time_;
        }

        boost::uint32_t GetP2PProtectTimeWhenSwitchedWithBuffering(bool is_saving_mode) const
        {
            return is_saving_mode ? p2p_protect_time_when_switched_with_buffering_in_saving_mode_
                : p2p_protect_time_when_switched_with_buffering_;
        }

        boost::uint32_t GetTimeToIgnoreHttpBad(bool is_saving_mode) const
        {
            return is_saving_mode ? time_to_ignore_http_bad_in_saving_mode_ : time_to_ignore_http_bad_;
        }

        boost::uint32_t GetUrgentRestPlayableTimeDelim(bool is_saving_mode) const
        {
            return is_saving_mode ? urgent_rest_playable_time_delim_in_saving_mode_
                : urgent_rest_playable_time_delim_;
        }

        boost::uint32_t GetSafeRestPlayableTimeDelim(bool is_saving_mode) const
        {
            return is_saving_mode ? safe_rest_playable_time_delim_in_saving_mode_ : safe_rest_playable_time_delim_;
        }

        boost::uint32_t GetSafeEnoughRestPlayabelTimeDelim(bool is_saving_mode) const
        {
            return is_saving_mode ? safe_enough_rest_playable_time_delim_in_saving_mode_
                : safe_enough_rest_playable_time_delim_;
        }

        boost::uint32_t GetUsingUdpServerTimeDelim(bool is_saving_mode) const
        {
            return is_saving_mode ? using_udpserver_time_in_second_delim_in_saving_mode_
                : using_udpserver_time_in_second_delim_;
        }

        boost::uint32_t GetUsingCDNOrUdpServerTimeDelim(bool is_saving_mode) const
        {
            return is_saving_mode ? using_cdn_or_udpserver_time_at_least_when_large_upload_in_saving_mode_
                : using_cdn_or_udpserver_time_at_least_when_large_upload_;
        }

        boost::uint32_t GetSmallRatioDelimOfUploadSpeedToDatarate(bool is_saving_mode) const
        {
            return is_saving_mode ? small_ratio_delim_of_upload_speed_to_datarate_in_saving_mode_
                : small_ratio_delim_of_upload_speed_to_datarate_;
        }

        boost::uint32_t GetUseUdpserverCount() const
        {
            return use_udpserver_count_;
        }

        boost::uint32_t GetP2PProtectTimeWhenStart(bool is_saving_mode) const
        {
            return is_saving_mode ? p2p_protect_time_when_start_in_saving_mode_
                : p2p_protect_time_when_start_;
        }

        bool GetShouldUseBWType() const
        {
            return should_use_bw_type_;
        }

        boost::uint32_t GetUdpServerProtectTimeWhenStart(bool is_saving_mode) const
        {
            return is_saving_mode ? udpserver_protect_time_when_start_in_saving_mode_
                : udpserver_protect_time_when_start_;
        }

        boost::uint32_t GetEnhancedAnnounceThresholdInMillseconds() const
        {
            return enhanced_announce_threshold_in_millseconds_;
        }
        
        boost::uint32_t GetEnhancedAnnounceCopies() const
        {
            return enhanced_announce_copies_;
        }

        boost::uint32_t GetPeerCountWhenUseSn() const
        {
            return peer_count_when_use_sn_;
        }

        boost::uint32_t GetLivePeerMaxConnections() const
        {
            return live_peer_max_connections_;
        }

        boost::uint32_t GetLiveConnectLowNormalThresHold() const
        {
            return live_connect_low_normal_threshold_;
        }

        boost::uint32_t GetLiveConnectNormalHighThresHold() const
        {
            return live_connect_normal_high_threshold_;
        }

        boost::uint32_t GetLiveMinimumWindowSize() const
        {
            return live_minimum_window_size_;
        }

        boost::uint32_t GetLiveMaximumWindowSize() const
        {
            return live_maximum_window_size_;
        }

        boost::uint32_t GetLiveExchangeLargeUploadAbilityDelim() const
        {
            return live_exchange_large_upload_ability_delim_;
        }

        boost::uint32_t GetLiveExchangeLargeUploadAbilityMaxCount() const
        {
            return live_exchange_large_upload_ability_max_count_;
        }

        boost::uint32_t GetLiveExchangeLargeUploadToMeDelim() const
        {
            return live_exchange_large_upload_to_me_delim_;
        }

        boost::uint32_t GetLiveExchangeLargeUploadToMeMaxCount() const
        {
            return live_exchange_large_upload_to_me_max_count_;
        }

        bool ShouldUseExchangePeersFirstly() const
        {
            return should_use_exchange_peers_firstly_;
        }

        boost::uint32_t GetLiveExchangeIntervalInSecond() const
        {
            return live_exchange_interval_in_second_;
        }

        boost::uint32_t GetLiveExtendedConnections() const
        {
            return live_extended_connections_;
        }

        bool IsLiveLostPrejudgeEnable() const
        {
            return live_lost_prejudge_;
        }

        boost::uint32_t GetUdpServerMaximumRequests() const
        {
            return udpserver_maximum_requests_;
        }

        boost::uint32_t GetUdpServerMaximumWindow() const
        {
            return udpserver_maximum_window_size_;
        }

        boost::uint32_t GetLiveMinimumUploadSpeedInKiloBytes() const
        {
            return live_minimum_upload_speed_in_kilobytes_;
        }

        boost::uint32_t GetP2PSpeedThreshold(bool is_saving_mode) const
        {
            return is_saving_mode ? p2p_speed_threshold_in_saving_mode_ : p2p_speed_threshold_;
        }

        boost::uint32_t GetTimeOfAdvancingSwitchingHttp(bool is_saving_mode) const
        {
            return is_saving_mode ? time_of_advancing_switching_to_http_when_p2p_slow_in_saving_mode_
                : time_of_advancing_switching_to_http_when_p2p_slow_;
        }

        boost::uint32_t GetP2PProtectTimeIfStartAndSpeedIs0(bool is_saving_mode) const
        {
            return is_saving_mode ? p2p_protect_time_if_start_and_speed_is_0_in_saving_mode_
                : p2p_protect_time_if_start_and_speed_is_0_;
        }

        boost::uint32_t GetP2PProtectTimeIfSpeedIs0(bool is_saving_mode) const
        {
            return is_saving_mode ? p2p_protect_time_if_speed_is_0_in_saving_mode_
                : p2p_protect_time_if_speed_is_0_;
        }

        boost::uint32_t GetFallBehindSecondsThreshold() const
        {
            return fall_behind_seconds_threshold_;
        }

        boost::uint32_t GetMaxRestPlayableTime() const
        {
            return max_rest_playable_time_;
        }

        boost::uint32_t GetMinRestPlayableTime() const
        {
            return min_rest_playable_time_;
        }

        bool ShouldPreventHttpPredownload() const
        {
            return prevent_http_predownload;
        }

        boost::uint32_t GetMaxRatioOfUploadToDownloadDelim(bool is_saving_mode) const
        {
            return is_saving_mode ? max_ratio_of_upload_to_download_delim_in_saving_mode_
                : max_ratio_of_upload_to_download_delim_;
        }

        boost::uint32_t GetLargeRatioOfUploadToDownloadDelim(bool is_saving_mode) const
        {
            return is_saving_mode ? large_ratio_of_upload_to_download_delim_in_saving_mode_
                : large_ratio_of_upload_to_download_delim_;
        }

        boost::uint32_t GetRatioOfLargeUploadTimesToTotalTimesDelim(bool is_saving_mode) const
        {
            return is_saving_mode ? ratio_of_large_upload_times_to_total_times_delim_in_saving_mode_
                : ratio_of_large_upload_times_to_total_times_delim_;
        }

        boost::uint32_t GetUploadConnectionCountDelim(bool is_saving_mode) const
        {
            return is_saving_mode ? upload_connection_count_delim_in_saving_mode_
                : upload_connection_count_delim_;
        }

        boost::uint32_t GetNotStrictRatioDelimOfUploadToDatarate(bool is_saving_mode) const
        {
            return is_saving_mode ? not_strict_ratio_delim_of_upload_speed_to_datarate_in_saving_mode_
                : not_strict_ratio_delim_of_upload_speed_to_datarate_;
        }

        boost::uint32_t GetMinIntervalOfCdnAccelerationDelim(bool is_saving_mode) const
        {
            return is_saving_mode ? min_interval_of_cdn_acceleration_delim_in_saving_mode_
                : min_interval_of_cdn_acceleration_delim_;
        }

        bool GetUseCdnToAccelerateBasedOnHistory(bool is_saving_mode) const
        {
            return is_saving_mode ? use_cdn_to_accelerate_based_on_history_in_saving_mode_
                : use_cdn_to_accelerate_based_on_history_;
        }

        boost::uint32_t GetMinTimesOfRecord() const
        {
            return min_times_of_record_;
        }

        boost::uint32_t GetMaxTimesOfRecord() const
        {
            return max_times_of_record_;
        }

        boost::uint32_t GetIntervalOfRequestingAnnounceFromUdpserver() const
        {
            return interval_of_requesting_announce_from_udpserver_;
        }

        boost::uint32_t GetMaxPeerConnectionCount() const
        {
            return p2p_download_max_connect_count_bound;
        }

        boost::uint32_t GetMinPeerConnectionCount() const
        {
            return p2p_download_min_connect_count_bound;
        }

        bool UdpServerUsageHistoryEnabled() const
        {
            return udp_server_usage_history_enabled_;
        }

        bool AutoSwitchStream() const
        {
            return auto_switch_stream_;
        }

        boost::uint32_t MaxSNListSize() const
        {
            return max_sn_list_size_;
        }

        boost::uint32_t MaxSNListSizeForVIP() const
        {
            return max_sn_list_size_for_vip_;
        }

        boost::uint32_t GetMaxMaterialDolistCount() const
        {
            return max_material_do_list_count_;
        }
        boost::uint32_t GetSNRequestNumber() const
        {
            return sn_request_number_;
        }

        bool GetShouldJudgeSwitchingDatarateManually() const
        {
            return should_judge_switching_datarate_manually_;
        }

        boost::uint32_t GetIntervalOfTwoVVDelim() const
        {
            return interval_of_two_vv_delim_;
        }

        boost::uint32_t GetRestPlayableTimeDelimWhenSwitching() const
        {
            return rest_playable_time_delim_when_switching_;
        }

        boost::uint32_t GetRestTimeEnoughLaunchP2P10() const
        {
            return rest_time_enough_launch_P2P_2300_10;
        }

        boost::uint32_t GetRestTimeEnoughLaunchP2P0() const
        {
            return rest_time_enough_lauch_P2P_2300_0;
        }

        boost::uint32_t GetRestTimeNeedCheckP2P() const
        {
            return rest_time_need_check_P2P;
        }

        bool WriteBlockWhenFull() const
        {
            return write_block_when_full_;
        }

        bool WriteBlockWhenVerified() const
        {
            return write_block_when_verified_;
        }

        boost::uint32_t GetMaxUploadSpeedUsedInNetCheck() const
        {
            return max_upload_speed_used_in_network_check_;
        }

        boost::uint32_t PingUsedInNetCheckWhenBiggerUploadSpeed() const
        {
            return ping_time_net_check_big_upload_speed_;
        }

        boost::uint32_t pingUsedInNetCheckWhenSmallerUploadSpeed() const
        {
            return ping_time_net_check_small_upload_speed_;
        }

        boost::uint32_t MinusValueWhenUploadSpeedOverlarge() const
        {
            return minus_value_when_upload_speed_overlarge_;
        }

        bool NeedLoadSnOnCDN() const
        {
            return load_sn_on_cdn_;
        }

        boost::uint32_t GetWaitTimeForTinydrag() const
        {
            return time_wait_for_tinydrag_;
        }

        boost::uint32_t GetSnPortOnCDN() const
        {
            return sn_port_on_cdn_;
        }

        boost::uint32_t GetWaitTimeForTinydragInDownloadMode() const
        {
            return seconds_wait_for_tinydrag_in_download_mode_;
        }

        bool GetUseHttpWhenConnectedNonePeers(bool is_saving_mode) const
        {
            return is_saving_mode ? use_http_when_connected_none_peers_in_saving_mode_ : use_http_when_connected_none_peers_;
        }

        boost::uint32_t GetRestPlayTimeDelimWhenNonePeers(bool is_saving_mode) const
        {
            return is_saving_mode ? rest_play_time_delim_when_none_peers_in_saving_mode_ :
                rest_play_time_delim_when_none_peers_;
        }

        boost::uint32_t GetEnoughRestPlayTimeDelimWhenNonePeers(bool is_saving_mode) const
        {
            return is_saving_mode ? enough_rest_play_time_delim_when_none_peers_in_saving_mode_ :
                enough_rest_play_time_delim_when_none_peers_;
        }

        bool GetUseHttpWhenUrgent(bool is_saving_mode) const
        {
            return is_saving_mode ? use_http_when_urgent_in_saving_mode_ : use_http_when_urgent_;
        }

        boost::uint32_t GetHttpRunningLongestTimeWhenUrgentSwitched(bool is_saving_mode) const
        {
            return is_saving_mode ? http_running_longest_time_when_urgent_switched_in_saving_mode_ :
                http_running_longest_time_when_urgent_switched_;
        }

        boost::uint32_t GetConfigStringLength() const
        {
            return config_string_length_;
        }

        bool OpenRequestSubpiecePacketOld() const
        {
            return open_request_subpiece_packet_old_;
        }

        bool OpenRIDInfoRequestResponse() const
        {
            return open_rid_info_request_response_;
        }

        boost::uint16_t GetUdpserverUdpPort() const
        {
            return udpserver_udp_port_;
        }

        bool UseUdpServerFromCdn() const
        {
            return use_udpserver_from_cdn_;
        }

        bool IsVipUseSnAllTime() const
        {
            return is_vip_use_sn_all_time_;
        }

        string GetNatCheckServerIP() const
        {
            return nat_check_server_ip_;
        }

        boost::uint32_t GetNatNeedCheckMinDays() const
        {
            return nat_need_check_min_days_;
        }

        bool NeedUseStunClientNatCheck() const
        {
            return need_use_stunclient_nat_check_;
        }

        boost::uint32_t GetCdnSnSpeedFactor() const
        {
            return factor_used_on_cdn_sn_speed_;
        }

        bool NeedProtectCDNSn() const
        {
            return need_protect_cdn_sn_;
        }

        boost::uint32_t SecondDelayForCheckSnSpeed() const
        {
            return second_delay_for_check_sn_speed_;
        }

        boost::uint32_t GetHttpProtectTime2WhenStart(bool is_saving_mode) const
        {
            return is_saving_mode ? http_protect_time2_when_start_in_saving_mode_ :
                http_protect_time2_when_start_;
        }

        boost::uint32_t GetHttpDownloadBytesDelimWhenStart(bool is_saving_mode) const
        {
            return is_saving_mode ? http_download_bytes_delim_when_start_in_saving_mode_ :
                http_download_bytes_delim_when_start_;
        }

        boost::uint32_t GetHttpDownloadSpeedLimitInKBps() const
        {
            return http_download_speed_limit_;
        }

        boost::uint32_t GetP2PDownloadSpeedLimitInKBps() const
        {
            return p2p_download_speed_limit_;
        }

        boost::uint32_t GetMaxUdpDoListTimes() const
        {
            return max_udp_dolist_times_;
        }

        boost::uint32_t GetMinDownloadSpeedToBeEnsuredInKBps() const
        {
            return min_download_speed_to_be_ensured_in_KBps_;
        }

    private:
        BootStrapGeneralConfig();
        void LoadLocalConfig();
        void SaveLocalConfig(string const & config_string);
        void NotifyConfigUpdateEvent();

    private:
        static boost::shared_ptr<BootStrapGeneralConfig> inst_;

        string local_config_file_path_;

        string hou_server_list_;   

        string data_collection_server_list_;

        std::set<boost::shared_ptr<ConfigUpdateListener> > update_listeners_;

        bool use_push_;
    
        UploadPolicy upload_policy_;

        bool connection_policy_enable_;

        boost::uint32_t desirable_vod_ippool_size_;
        boost::uint32_t desirable_live_ippool_size_;

        // 当上传速度很大，并且正在下比较靠前的块时，切换到cdn下载，让peer充当udpserver来快速分发
        bool use_cdn_when_large_upload_;
        bool use_cdn_when_large_upload_in_saving_mode_;

        // 当剩余时间大于这个值时，认为下载的足够靠前，可以用cdn来下载以达到快速分发的目的
        boost::uint32_t rest_play_time_delim_;
        boost::uint32_t rest_play_time_delim_in_saving_mode_;

        // 1分钟内的平均上传速度大于码流率的ratio_delim_of_upload_speed_to_datarate_ / 100倍时，认为上传速度足够大
        boost::uint32_t ratio_delim_of_upload_speed_to_datarate_;
        boost::uint32_t ratio_delim_of_upload_speed_to_datarate_in_saving_mode_;

        // 是否对二代直播上传进行限速
        bool limit_upload_speed_for_live2_;

        // 每隔多久发送一次PeerInfoPacket，用于直播时Peer间信息交换
        boost::uint32_t send_peer_info_packet_interval_in_second_;

        // Http状态下剩余时间足够，在bs配置文件中用a表示
        boost::uint32_t safe_enough_rest_playable_time_delim_under_http_;
        boost::uint32_t safe_enough_rest_playable_time_delim_under_http_in_saving_mode_;

        // 在启动时http最多跑多长时间，在bs配置文件中用b表示
        boost::uint32_t http_running_long_enough_time_when_start_;
        boost::uint32_t http_running_long_enough_time_when_start_in_saving_mode_;

        // 启动时如果剩余时间不够多，最少跑多长时间然后才去检测有没有卡，如果卡了则切换，在bs配置文件中用c表示
        boost::uint32_t http_protect_time_when_start_;
        boost::uint32_t http_protect_time_when_start_in_saving_mode_;

        // 在P2P剩余时间不够的情况下切换到Http后，如果没有因为剩余时间足够多切换到P2P的话，Http最少跑多长时间
        // 在bs配置文件中用d表示
        boost::uint32_t http_protect_time_when_urgent_switched_;
        boost::uint32_t http_protect_time_when_urgent_switched_in_saving_mode_;

        // 在P2P剩余时间不够的情况下切换到Http后，如果剩余时间还可以，Http最多跑多长时间，在bs配置文件中用e表示
        boost::uint32_t http_running_long_enough_time_when_urgent_switched_;
        boost::uint32_t http_running_long_enough_time_when_urgent_switched_in_saving_mode_;

        // 与http_running_long_enough_time_when_urgent_switched_结合起来，在P2P剩余时间不够的情况下切换到Http后，
        // 如果剩余时间大于safe_rest_playable_time_delim_when_use_http_并且在这个状态持续的时间大于http_running_long_enough_time_when_urgent_switched_
        // 则切换到P2P，在bs配置文件中用f表示
        boost::uint32_t safe_rest_playable_time_delim_when_use_http_;
        boost::uint32_t safe_rest_playable_time_delim_when_use_http_in_saving_mode_;

        // 如果是因为上传比较大切换到Http来的，Http最少跑多长时间，在bs配置文件中用g表示
        boost::uint32_t http_protect_time_when_large_upload_;
        boost::uint32_t http_protect_time_when_large_upload_in_saving_mode_;

        // 如果Http切换到P2P时剩余时间足够多，则P2P切换回Http时剩余时间的判断条件，在bs配置文件中用h表示
        boost::uint32_t p2p_rest_playable_time_delim_when_switched_with_large_time_;
        boost::uint32_t p2p_rest_playable_time_delim_when_switched_with_large_time_in_saving_mode_;

        // 如果Http切换到P2P时剩余时间不够多，则P2P切换回Http时剩余时间的判断条件，在bs配置文件中用i表示
        boost::uint32_t p2p_rest_playable_time_delim_;
        boost::uint32_t p2p_rest_playable_time_delim_in_saving_mode_;

        // 如果Http切换到P2P时剩余时间不够多，则在P2P状态下最少持续多长时间，在bs配置文件中用j表示
        boost::uint32_t p2p_protect_time_when_switched_with_not_enough_time_;
        boost::uint32_t p2p_protect_time_when_switched_with_not_enough_time_in_saving_mode_;

        // 如果Http因为卡了才切到P2P的，则在P2P状态下最少持续多长时间，在bs配置文件中用k表示
        boost::uint32_t p2p_protect_time_when_switched_with_buffering_;
        boost::uint32_t p2p_protect_time_when_switched_with_buffering_in_saving_mode_;

        // 在P2P状态下持续多长时间后可忽略以前Http状态不好的情况，可以再去尝试Http，在bs配置文件中用l表示
        boost::uint32_t time_to_ignore_http_bad_;
        boost::uint32_t time_to_ignore_http_bad_in_saving_mode_;

        // P2P启动时的保护时间，在bs配置中用m表示
        boost::uint32_t p2p_protect_time_when_start_;
        boost::uint32_t p2p_protect_time_when_start_in_saving_mode_;

        // 当剩余时间小于这个值时，会认为紧急，需要使用UdpServer，在BS配置文件中用rpt1表示(rest playable time)
        boost::uint32_t urgent_rest_playable_time_delim_;
        boost::uint32_t urgent_rest_playable_time_delim_in_saving_mode_;

        // 当剩余时间大于这个值时，会认为安全了，如果已经用了足够长时间的UdpServer，则暂停使用UdpServer
        // 在BS配置文件中用rpt2表示
        boost::uint32_t safe_rest_playable_time_delim_;
        boost::uint32_t safe_rest_playable_time_delim_in_saving_mode_;

        // 当剩余时间大于这个值时，会认为足够安全了，停止使用UdpServer并且踢掉UdpServer的连接
        // 在BS配置文件中用rpt3表示
        boost::uint32_t safe_enough_rest_playable_time_delim_;
        boost::uint32_t safe_enough_rest_playable_time_delim_in_saving_mode_;

        // 当已经使用UdpServer的时间超过这个值时，会认为用的足够长了
        // 在BS配置文件中用ut1表示(using time)
        boost::uint32_t using_udpserver_time_in_second_delim_;
        boost::uint32_t using_udpserver_time_in_second_delim_in_saving_mode_;

        // 如果因为上传足够大使用CDN或者是UdpServer，最少使用长时间
        // 在BS配置文件中用ut2表示
        boost::uint32_t using_cdn_or_udpserver_time_at_least_when_large_upload_;
        boost::uint32_t using_cdn_or_udpserver_time_at_least_when_large_upload_in_saving_mode_;

        // 当上传小于码流率的small_ratio_delim_of_upload_speed_to_datarate_ / 100倍时，认为上传速度不够大，停止使用UdpServer或者CDN
        // 在BS配置文件中用sr表示(small ratio)
        boost::uint32_t small_ratio_delim_of_upload_speed_to_datarate_;
        boost::uint32_t small_ratio_delim_of_upload_speed_to_datarate_in_saving_mode_;

        // 同时连接几个UdpServer，在BS配置文件中用uuc来表示(use udpserver count)
        boost::uint32_t use_udpserver_count_;

        // 是不是应该利用BWTypt，在bs中用n表示
        bool should_use_bw_type_;

        // 超过这个值peer不回announce包，加速announce
        boost::uint32_t enhanced_announce_threshold_in_millseconds_;

        // 加速announce模式下，每秒冗余发几个announce包
        boost::uint32_t enhanced_announce_copies_;

        // P2P启动时UdpServer的保护时间，在bs中用o表示
        boost::uint32_t udpserver_protect_time_when_start_;
        boost::uint32_t udpserver_protect_time_when_start_in_saving_mode_;

        // 当P2P连接的peer数小于这个值，启用SN
        boost::uint32_t peer_count_when_use_sn_;

        // 点播状态机在peer_count>10时判断2300状态下剩余时间是否足够启动P2P,单位s
        boost::uint32_t rest_time_enough_launch_P2P_2300_10;

        // 点播状态机在peer_count<10时判断2300状态下剩余时间是否能够稳定启动P2P
        boost::uint32_t rest_time_enough_lauch_P2P_2300_0;

        // 点播状态机判断剩余时间是否需要检查P2P下载速度
        boost::uint32_t rest_time_need_check_P2P;

        // 二代直播p2p最大连接数
        boost::uint32_t live_peer_max_connections_;

        // 二代直播连接策略：多少秒进入不紧急状态
        boost::uint32_t live_connect_low_normal_threshold_;

        // 二代直播连接策略：多少秒进入紧急状态
        boost::uint32_t live_connect_normal_high_threshold_;

        // 二代直播最小window size
        boost::uint32_t live_minimum_window_size_;

        //二代直播最大Window size
        boost::uint32_t live_maximum_window_size_;

        // 二代直播中用于判断剩余上传能力大小的分界值，在配置文件中用leuad(live exchange upload ability delim)表示
        boost::uint32_t live_exchange_large_upload_ability_delim_;

        // 二代直播中PeerExchange中包含的剩余上传大的节点数目，在配置文件中用leuac(live exchange upload ability count)表示
        boost::uint32_t live_exchange_large_upload_ability_max_count_;

        // 二代直播中用于判断给我上传速度大小的分界值，在配置文件中用leumd(live exchange upload to me delim)表示
        boost::uint32_t live_exchange_large_upload_to_me_delim_;

        // 二代直播中PeerExchange中包含的给我上传速度大的节点数目，在配置文件中用leumc(live exchange upload to me count)表示
        boost::uint32_t live_exchange_large_upload_to_me_max_count_;

        // 是否应该优先连接exchange到的节点
        bool should_use_exchange_peers_firstly_;

        // 二代直播中由于速度不够好而PeerExchange的最小间隔
        boost::uint32_t live_exchange_interval_in_second_;

        //二代直播紧急时增加peer连接
        boost::uint32_t live_extended_connections_;

        // 二代直播，丢包预判策略
        bool live_lost_prejudge_;

        // 二代直播udp server请求包包含的最大请求subpieces数
        boost::uint32_t udpserver_maximum_requests_;

        // 二代直播udp server最大window size
        boost::uint32_t udpserver_maximum_window_size_;

        // 二代直播最小上传速度
        boost::uint32_t live_minimum_upload_speed_in_kilobytes_;

        // P2P速度的阈值
        boost::uint32_t p2p_speed_threshold_;
        boost::uint32_t p2p_speed_threshold_in_saving_mode_;

        // 在P2P速度不好的情况下，提前多少秒切Http
        boost::uint32_t time_of_advancing_switching_to_http_when_p2p_slow_;
        boost::uint32_t time_of_advancing_switching_to_http_when_p2p_slow_in_saving_mode_;

        // 刚启动时，在P2P下至少停留多长时间，然后检测P2P速度是不是为0
        boost::uint32_t p2p_protect_time_if_start_and_speed_is_0_;
        boost::uint32_t p2p_protect_time_if_start_and_speed_is_0_in_saving_mode_;

        // 非刚启动时，在P2P下至少停留多长时间，然后才检测P2P速度是不是为0
        boost::uint32_t p2p_protect_time_if_speed_is_0_;
        boost::uint32_t p2p_protect_time_if_speed_is_0_in_saving_mode_;

        // 落后的阈值
        boost::uint32_t fall_behind_seconds_threshold_;

        // 当剩余时间超过该值时，暂停下载
        boost::uint32_t max_rest_playable_time_;

        // 当剩余时间小于该值时，开始下载
        boost::uint32_t min_rest_playable_time_;

        //是否在获取tindy-drag的过程中禁止http下载以节省带宽
        bool prevent_http_predownload;

        // 如果有一次历史上传下载比大于该值，则认为上传足够多
        boost::uint32_t max_ratio_of_upload_to_download_delim_;
        boost::uint32_t max_ratio_of_upload_to_download_delim_in_saving_mode_;

        // 如果历史上传下载比大于该值，则认为上传还可以
        boost::uint32_t large_ratio_of_upload_to_download_delim_;
        boost::uint32_t large_ratio_of_upload_to_download_delim_in_saving_mode_;

        // 上传下载比还可以的次数占总次数比例的阈值
        boost::uint32_t ratio_of_large_upload_times_to_total_times_delim_;
        boost::uint32_t ratio_of_large_upload_times_to_total_times_delim_in_saving_mode_;

        // 当满足以下三个条件时，才认为当前需要切换到CDN加速模式
        // IPPool 不小于desirable_live_ippool_size_
        // 当前上传连接大于该值
        boost::uint32_t upload_connection_count_delim_;
        boost::uint32_t upload_connection_count_delim_in_saving_mode_;
        // 当前上传速度大于码流率的该倍
        boost::uint32_t not_strict_ratio_delim_of_upload_speed_to_datarate_;
        boost::uint32_t not_strict_ratio_delim_of_upload_speed_to_datarate_in_saving_mode_;

        // 两次提前切换到CDN加速模式之间相隔的最小时间
        boost::uint32_t min_interval_of_cdn_acceleration_delim_;
        boost::uint32_t min_interval_of_cdn_acceleration_delim_in_saving_mode_;

        // 是否利用历史值来决定提前使用CDN加速
        bool use_cdn_to_accelerate_based_on_history_;
        bool use_cdn_to_accelerate_based_on_history_in_saving_mode_;

        // 历史记录的最大次数
        boost::uint32_t max_times_of_record_;

        // 历史记录有效的最小次数
        boost::uint32_t min_times_of_record_;

        // 在收到Announce回包后，对UdpServer进行Announce的时间间隔
        boost::uint32_t interval_of_requesting_announce_from_udpserver_;

        //最大peer连接数
        boost::uint32_t p2p_download_max_connect_count_bound;

        //最小peer连接数
        boost::uint32_t p2p_download_min_connect_count_bound;

        //普通用户最大SN list size
        boost::uint32_t max_sn_list_size_;

        //VIP用户使用SN数
        boost::uint32_t max_sn_list_size_for_vip_;

        //最大广告素材DoList次数
        boost::uint32_t max_material_do_list_count_;

        //是否根据udpserver的历史使用情况来优选使用过去服务得较好的udpserver
        bool udp_server_usage_history_enabled_;

        // 是否启用二代直播的多码率自动切换
        bool auto_switch_stream_;

        //单次向SN请求包个数
        boost::uint32_t sn_request_number_;

        // 根据channel id上一次结束距下一次开始的时间间隔来判断是不是手动切换码流
        bool should_judge_switching_datarate_manually_;

        // 相同channel id的两次播放间隔小于这个值则认为是手动切换码流，单位毫秒
        boost::uint32_t interval_of_two_vv_delim_;

        // 手动切换码流时，如果剩余时间小于这个值，则http启动
        boost::uint32_t rest_playable_time_delim_when_switching_;

        // 是否加载CDN节点上的SN
        bool load_sn_on_cdn_;

        bool write_block_when_full_;

        bool write_block_when_verified_;

        // 历史最大上传速度，用户选取不同的判断网络好坏的策略
        boost::uint32_t max_upload_speed_used_in_network_check_;

        // 最大上传速度大于某一BS可配值时，用于判断网络好坏的ping值
        boost::uint32_t ping_time_net_check_big_upload_speed_;

        // 最大上传速度小于某一BS可配值时，用于判断网络好坏的ping值
        boost::uint32_t ping_time_net_check_small_upload_speed_;

        // 上传限速值过大时，每次削减的值
        boost::uint32_t minus_value_when_upload_speed_overlarge_;

        // 点播状态机中用于等待获取tinydrag的时间
        boost::uint32_t time_wait_for_tinydrag_;

        // CDN上配置的SN端口
        boost::uint32_t sn_port_on_cdn_;

        // 下载状态机中用于等待获取tinydrag的时间
        boost::uint32_t seconds_wait_for_tinydrag_in_download_mode_;

        // 在节点数为0时要不要使用Http
        bool use_http_when_connected_none_peers_;
        bool use_http_when_connected_none_peers_in_saving_mode_;

        // 在节点数为0时，剩余时间小于多少开始使用Http
        boost::uint32_t rest_play_time_delim_when_none_peers_;
        boost::uint32_t rest_play_time_delim_when_none_peers_in_saving_mode_;

        // 在节点数为0时，剩余时间大于多少开始使用P2P
        boost::uint32_t enough_rest_play_time_delim_when_none_peers_;
        boost::uint32_t enough_rest_play_time_delim_when_none_peers_in_saving_mode_;

        // 紧急情况下是否使用Http
        bool use_http_when_urgent_;
        bool use_http_when_urgent_in_saving_mode_;

        // 非启动时因为紧急在http下最长停留多长时间
        boost::uint32_t http_running_longest_time_when_urgent_switched_;
        boost::uint32_t http_running_longest_time_when_urgent_switched_in_saving_mode_;

        // 配置字符串长度
        boost::uint32_t config_string_length_;

        bool open_request_subpiece_packet_old_;
        bool open_rid_info_request_response_;

        // 与CDN同机部署的udpserver的默认端口
        boost::uint16_t udpserver_udp_port_;

        // 是不是要用跟CDN同机部署的UdpServer
        bool use_udpserver_from_cdn_;

        //VIP用户是否全程使用SN
        bool is_vip_use_sn_all_time_;

        //nat check服务器ip
        string nat_check_server_ip_;

        //nat check复查时间间隔
        boost::uint32_t nat_need_check_min_days_;

        //是否启用原有stunclient nat检测
        bool need_use_stunclient_nat_check_;

        //用于KickSnConnection中，同机部署SN即时速度相乘系数
        boost::uint32_t factor_used_on_cdn_sn_speed_;

        //是否永久保留同机部署SN
        bool need_protect_cdn_sn_;

        //SN延迟检测时间，单位：s
        boost::uint32_t second_delay_for_check_sn_speed_;

        // Http启动保护时间2
        boost::uint32_t http_protect_time2_when_start_;
        boost::uint32_t http_protect_time2_when_start_in_saving_mode_;

        // Http启动时的最小下载量（超过保护时间2，下载量小于这个值就切换到P2P）
        boost::uint32_t http_download_bytes_delim_when_start_;
        boost::uint32_t http_download_bytes_delim_when_start_in_saving_mode_;

        boost::uint32_t http_download_speed_limit_;
        boost::uint32_t p2p_download_speed_limit_;

        //需要启动HTTP做DoList的条件：UDP进行DoList的最大次数
        boost::uint32_t max_udp_dolist_times_;

        //需保证的多终端最小下载速度
        boost::uint32_t min_download_speed_to_be_ensured_in_KBps_;
    };
}
#endif