#include "Common.h"
#include "BootStrapGeneralConfig.h"

#include <util/archive/BinaryIArchive.h>
#include <util/archive/BinaryOArchive.h>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/program_options.hpp>

#include <fstream>

namespace p2sp
{
    boost::shared_ptr<BootStrapGeneralConfig> BootStrapGeneralConfig::inst_(new BootStrapGeneralConfig());

    static const string DEFAULT_CONFIG_STRING(
        "[config]");

    BootStrapGeneralConfig::BootStrapGeneralConfig()
        : hou_server_list_("220.165.14.10@119.167.233.56")
        , data_collection_server_list_("")
        , use_push_(false)
        , upload_policy_(policy_default)
        , connection_policy_enable_(true)
        , use_cdn_when_large_upload_(false)
        , desirable_vod_ippool_size_(500)
        , desirable_live_ippool_size_(1000)
        , rest_play_time_delim_(25)
        , ratio_delim_of_upload_speed_to_datarate_(200)
        , limit_upload_speed_for_live2_(true)
        , send_peer_info_packet_interval_in_second_(5)
    {
    }

    void BootStrapGeneralConfig::Start(string const & config_path)
    {
        SetConfigString(DEFAULT_CONFIG_STRING, false);

        if (config_path.length() == 0)
        {
            string szPath;
#ifdef DISK_MODE
            if (base::util::GetAppDataPath(szPath)) 
            {
                local_config_file_path_ = szPath;
            }
#endif  // #ifdef DISK_MODE
        }
        else
        {
            local_config_file_path_ = config_path;
        }

        boost::filesystem::path temp_path(local_config_file_path_);
        temp_path /= "ppbscf";
        local_config_file_path_ = temp_path.file_string();

        LoadLocalConfig();
    }

    void BootStrapGeneralConfig::LoadLocalConfig()
    {
        std::ifstream ifs(local_config_file_path_.c_str(), std::ios_base::in | std::ios_base::binary);
        if (ifs)
        {
            string config_string;
            util::archive::BinaryIArchive<> ar(ifs);
            ar >> config_string;

            ifs.close();
            SetConfigString(config_string, false);
        }
    }

    void BootStrapGeneralConfig::SaveLocalConfig(string const & config_string)
    {
        std::ofstream ofs(local_config_file_path_.c_str(), std::ios_base::out | std::ios_base::binary);
        if (ofs)
        {
            util::archive::BinaryOArchive<> ar(ofs);
            ar << config_string;   // WatchOut
        }
    }

    void BootStrapGeneralConfig::SetConfigString(string const & config_string, bool save_to_disk)
    {
        try
        {
            namespace po = boost::program_options;

            //
            // 请注意,在添加config options时**不要**在中间加上分号,否则会影响到程序的行为
            //
            po::options_description config_desc("config");
            config_desc.add_options()
                ("config.hl", po::value<string>()->default_value(hou_server_list_))
                ("config.dc_servers", po::value<string>()->default_value(data_collection_server_list_))
                ("config.usepush", po::value<bool>()->default_value(use_push_))
                ("config.uploadpolicy", po::value<uint32_t>()->default_value((uint32_t)upload_policy_))
                ("config.connectionpolicy", po::value<bool>()->default_value(connection_policy_enable_))
                ("config.usecdnpolicy", po::value<bool>()->default_value(use_cdn_when_large_upload_))
                ("config.vps", po::value<uint32_t>()->default_value(desirable_vod_ippool_size_))
                ("config.lps", po::value<uint32_t>()->default_value(desirable_live_ippool_size_))
                ("config.restplaytime", po::value<uint32_t>()->default_value(rest_play_time_delim_))
                ("config.ratiodelim", po::value<uint32_t>()->default_value(ratio_delim_of_upload_speed_to_datarate_))
                ("config.limitlive2upload", po::value<bool>()->default_value(limit_upload_speed_for_live2_))
                ("config.peerinfointerval", po::value<uint32_t>()->default_value(send_peer_info_packet_interval_in_second_));

            std::istringstream config_stream(config_string);

            po::variables_map vm;
            po::store(po::parse_config_file(config_stream, config_desc, true), vm);
            po::notify(vm);

            hou_server_list_ = vm["config.hl"].as<string>();
            use_push_ = vm["config.usepush"].as<bool>();
            upload_policy_ = (UploadPolicy)(vm["config.uploadpolicy"].as<uint32_t>());
            connection_policy_enable_ = vm["config.connectionpolicy"].as<bool>();
            use_cdn_when_large_upload_ = vm["config.usecdnpolicy"].as<bool>();
            desirable_live_ippool_size_ = vm["config.lps"].as<uint32_t>();
            desirable_vod_ippool_size_ = vm["config.vps"].as<uint32_t>();
            rest_play_time_delim_ = vm["config.restplaytime"].as<uint32_t>();
            ratio_delim_of_upload_speed_to_datarate_ = vm["config.ratiodelim"].as<uint32_t>();
            data_collection_server_list_ = vm["config.dc_servers"].as<string>();
            limit_upload_speed_for_live2_ = vm["config.limitlive2upload"].as<bool>();
            send_peer_info_packet_interval_in_second_ = vm["config.peerinfointerval"].as<uint32_t>();

            if (save_to_disk)
            {
                SaveLocalConfig(config_string);
            }

            NotifyConfigUpdateEvent();
        }
        catch (boost::program_options::error & e)
        {
            DebugLog("Exception caught: ", e.what());
            assert(false);
        }
    }

    void BootStrapGeneralConfig::NotifyConfigUpdateEvent()
    {
        if (update_listeners_.size() > 0)
        {
            //把set放到另一个copy中，因为OnConfigUpdate可能会引起set内容的增删
            std::set<boost::shared_ptr<ConfigUpdateListener> > update_listeners_copy(update_listeners_);

            for (std::set<boost::shared_ptr<ConfigUpdateListener> >::iterator listener = update_listeners_copy.begin();
                listener != update_listeners_copy.end();
                ++listener)
            {
                (*listener)->OnConfigUpdated();
            }
        }
    }

    std::vector<string> BootStrapGeneralConfig::GetDataCollectionServers() const
    {
        std::vector<string> data_collection_servers;

        if (data_collection_server_list_.length() > 0)
        {
            boost::algorithm::split(data_collection_servers, data_collection_server_list_,
                boost::algorithm::is_any_of("@"));
        }

        return data_collection_servers;
    }

    void BootStrapGeneralConfig::AddUpdateListener(boost::shared_ptr<ConfigUpdateListener> listener)
    {
        if (listener)
        {
            update_listeners_.insert(listener);
        }
    }

    bool BootStrapGeneralConfig::RemoveUpdateListener(boost::shared_ptr<ConfigUpdateListener> listener)
    {
        if (listener)
        {
            return update_listeners_.erase(listener) > 0;
        }
        return false;
    }
}