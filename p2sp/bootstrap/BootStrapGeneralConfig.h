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

        boost::uint32_t GetRestPlayTimeDelim() const
        {
            return rest_play_time_delim_;
        }

        boost::uint32_t GetRatioDelimOfUploadSpeedToDatarate() const
        {
            return ratio_delim_of_upload_speed_to_datarate_;
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

        // 当上传速度很大，并且正在下比较靠前的块时，切换到cdn下载，让peer充当udpserver来快速分发
        bool use_cdn_when_large_upload_;

        // 当剩余时间大于这个值时，认为下载的足够靠前，可以用cdn来下载以达到快速分发的目的
        boost::uint32_t rest_play_time_delim_;

        // 1分钟内的平均上传速度大于码流率的ratio_delim_of_upload_speed_to_datarate_ / 100倍时，认为上传速度足够大
        boost::uint32_t ratio_delim_of_upload_speed_to_datarate_;
    };
}
#endif