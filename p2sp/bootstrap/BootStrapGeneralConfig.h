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

        void SetConfigString(string const & config_string);

        string GetHouServerList() const {return hou_server_list_;}
        bool UsePush() const {return use_push_;}

        bool IsDataCollectionOn() const { return GetDataCollectionServers().size() > 0; }

        std::vector<string> GetDataCollectionServers() const;

        void AddUpdateListener(boost::shared_ptr<ConfigUpdateListener> listener);
        bool RemoveUpdateListener(boost::shared_ptr<ConfigUpdateListener> listener);

        enum UploadPolicy
        {
            policy_defalut = 0,
            policy_ping
        };
        UploadPolicy GetUploadPolicy() {return upload_policy_;}

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
    };
}
#endif