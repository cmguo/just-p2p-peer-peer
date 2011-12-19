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
        bool ShouldUseCDNWhenLargeUpload() const
        {
            return use_cdn_when_large_upload_;
        }

        boost::uint32_t GetDesirableVodIpPoolSize() const 
        { 
            return desirable_vod_ippool_size_; 
        }
        
        boost::uint32_t GetDesirableLiveIpPoolSize() const 
        { 
            return desirable_live_ippool_size_; 
        }

        boost::uint32_t GetRestPlayTimeDelim() const
        {
            return rest_play_time_delim_;
        }

        boost::uint32_t GetRatioDelimOfUploadSpeedToDatarate() const
        {
            return ratio_delim_of_upload_speed_to_datarate_;
        }

        bool LimitLive2UploadSpeed() const 
        { 
            return limit_upload_speed_for_live2_; 
        }

        boost::uint32_t GetSendPeerInfoPacketIntervalInSecond() const
        {
            return send_peer_info_packet_interval_in_second_;
        }

        boost::uint32_t GetSafeEnoughRestPlayableTime() const
        {
            return safe_enough_rest_playable_time_delim_under_http_;
        }

        boost::uint32_t GetHttpRunningLongEnoughTimeWhenStart() const
        {
            return http_running_long_enough_time_when_start_;
        }

        boost::uint32_t GetHttpProtectTimeWhenStart() const
        {
            return http_protect_time_when_start_;
        }

        boost::uint32_t GetHttpProtectTimeWhenUrgentSwitched() const
        {
            return http_protect_time_when_urgent_switched_;
        }

        boost::uint32_t GetHttpRunningLongEnoughTimeWhenUrgentSwitched() const
        {
            return http_running_long_enough_time_when_urgent_switched_;
        }

        boost::uint32_t GetSafeRestPlayableTimeDelimWhenUseHttp() const
        {
            return safe_rest_playable_time_delim_when_use_http_;
        }

        boost::uint32_t GetHttpProtectTimeWhenLargeUpload() const
        {
            return http_protect_time_when_large_upload_;
        }

        boost::uint32_t GetP2PRestPlayableTimeDelimWhenSwitchedWithLargeTime() const
        {
            return p2p_rest_playable_time_delim_when_switched_with_large_time_;
        }

        boost::uint32_t GetP2PRestPlayableTimeDelim() const
        {
            return p2p_rest_playable_time_delim_;
        }

        boost::uint32_t GetP2PProtectTimeWhenSwitchedWithNotEnoughTime() const
        {
            return p2p_protect_time_when_switched_with_not_enough_time_;
        }

        boost::uint32_t GetP2PProtectTimeWhenSwitchedWithBuffering() const
        {
            return p2p_protect_time_when_switched_with_buffering_;
        }

        boost::uint32_t GetTimeToIgnoreHttpBad() const
        {
            return time_to_ignore_http_bad_;
        }

        boost::uint32_t GetUrgentRestPlayableTimeDelim() const
        {
            return urgent_rest_playable_time_delim_;
        }

        boost::uint32_t GetSafeRestPlayableTimeDelim() const
        {
            return safe_rest_playable_time_delim_;
        }

        boost::uint32_t GetSafeEnoughRestPlayabelTimeDelim() const
        {
            return safe_enough_rest_playable_time_delim_;
        }

        boost::uint32_t GetUsingUdpServerTimeDelim() const
        {
            return using_udpserver_time_in_second_delim_;
        }

        boost::uint32_t GetUsingCDNOrUdpServerTimeDelim() const
        {
            return using_cdn_or_udpserver_time_at_least_when_large_upload_;
        }

        boost::uint32_t GetSmallRatioDelimOfUploadSpeedToDatarate() const
        {
            return small_ratio_delim_of_upload_speed_to_datarate_;
        }

        boost::uint32_t GetUseUdpserverCount() const
        {
            return use_udpserver_count_;
        }

        boost::uint32_t GetP2PProtectTimeWhenStart() const
        {
            return p2p_protect_time_when_start_;
        }

        bool GetShouldUseBWType() const
        {
            return should_use_bw_type_;
        }

        boost::uint32_t GetUdpServerProtectTimeWhenStart() const
        {
            return udpserver_protect_time_when_start_;
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

        // 当剩余时间大于这个值时，认为下载的足够靠前，可以用cdn来下载以达到快速分发的目的
        boost::uint32_t rest_play_time_delim_;

        // 1分钟内的平均上传速度大于码流率的ratio_delim_of_upload_speed_to_datarate_ / 100倍时，认为上传速度足够大
        boost::uint32_t ratio_delim_of_upload_speed_to_datarate_;

        // 是否对二代直播上传进行限速
        bool limit_upload_speed_for_live2_;

        // 每隔多久发送一次PeerInfoPacket，用于直播时Peer间信息交换
        boost::uint32_t send_peer_info_packet_interval_in_second_;

        // Http状态下剩余时间足够，在bs配置文件中用a表示
        boost::uint32_t safe_enough_rest_playable_time_delim_under_http_;

        // 在启动时http最多跑多长时间，在bs配置文件中用b表示
        boost::uint32_t http_running_long_enough_time_when_start_;

        // 启动时如果剩余时间不够多，最少跑多长时间然后才去检测有没有卡，如果卡了则切换，在bs配置文件中用c表示
        boost::uint32_t http_protect_time_when_start_;

        // 在P2P剩余时间不够的情况下切换到Http后，如果没有因为剩余时间足够多切换到P2P的话，Http最少跑多长时间
        // 在bs配置文件中用d表示
        boost::uint32_t http_protect_time_when_urgent_switched_;

        // 在P2P剩余时间不够的情况下切换到Http后，如果剩余时间还可以，Http最多跑多长时间，在bs配置文件中用e表示
        boost::uint32_t http_running_long_enough_time_when_urgent_switched_;

        // 与http_running_long_enough_time_when_urgent_switched_结合起来，在P2P剩余时间不够的情况下切换到Http后，
        // 如果剩余时间大于safe_rest_playable_time_delim_when_use_http_并且在这个状态持续的时间大于http_running_long_enough_time_when_urgent_switched_
        // 则切换到P2P，在bs配置文件中用f表示
        boost::uint32_t safe_rest_playable_time_delim_when_use_http_;

        // 如果是因为上传比较大切换到Http来的，Http最少跑多长时间，在bs配置文件中用g表示
        boost::uint32_t http_protect_time_when_large_upload_;

        // 如果Http切换到P2P时剩余时间足够多，则P2P切换回Http时剩余时间的判断条件，在bs配置文件中用h表示
        boost::uint32_t p2p_rest_playable_time_delim_when_switched_with_large_time_;

        // 如果Http切换到P2P时剩余时间不够多，则P2P切换回Http时剩余时间的判断条件，在bs配置文件中用i表示
        boost::uint32_t p2p_rest_playable_time_delim_;

        // 如果Http切换到P2P时剩余时间不够多，则在P2P状态下最少持续多长时间，在bs配置文件中用j表示
        boost::uint32_t p2p_protect_time_when_switched_with_not_enough_time_;

        // 如果Http因为卡了才切到P2P的，则在P2P状态下最少持续多长时间，在bs配置文件中用k表示
        boost::uint32_t p2p_protect_time_when_switched_with_buffering_;

        // 在P2P状态下持续多长时间后可忽略以前Http状态不好的情况，可以再去尝试Http，在bs配置文件中用l表示
        boost::uint32_t time_to_ignore_http_bad_;

        // P2P启动时的保护时间，在bs配置中用m表示
        boost::uint32_t p2p_protect_time_when_start_;

        // 当剩余时间小于这个值时，会认为紧急，需要使用UdpServer，在BS配置文件中用rpt1表示(rest playable time)
        boost::uint32_t urgent_rest_playable_time_delim_;

        // 当剩余时间大于这个值时，会认为安全了，如果已经用了足够长时间的UdpServer，则暂停使用UdpServer
        // 在BS配置文件中用rpt2表示
        boost::uint32_t safe_rest_playable_time_delim_;

        // 当剩余时间大于这个值时，会认为足够安全了，停止使用UdpServer并且踢掉UdpServer的连接
        // 在BS配置文件中用rpt3表示
        boost::uint32_t safe_enough_rest_playable_time_delim_;

        // 当已经使用UdpServer的时间超过这个值时，会认为用的足够长了
        // 在BS配置文件中用ut1表示(using time)
        boost::uint32_t using_udpserver_time_in_second_delim_;

        // 如果因为上传足够大使用CDN或者是UdpServer，最少使用长时间
        // 在BS配置文件中用ut2表示
        boost::uint32_t using_cdn_or_udpserver_time_at_least_when_large_upload_;

        // 当上传小于码流率的small_ratio_delim_of_upload_speed_to_datarate_ / 100倍时，认为上传速度不够大，停止使用UdpServer或者CDN
        // 在BS配置文件中用sr表示(small ratio)
        boost::uint32_t small_ratio_delim_of_upload_speed_to_datarate_;

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

        // 当P2P连接的peer数小于这个值，启用SN
        boost::uint32_t peer_count_when_use_sn_;

        // 二代直播p2p最大连接数
        boost::uint32_t live_peer_max_connections_;
    };
}
#endif