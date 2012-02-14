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
        , safe_enough_rest_playable_time_delim_under_http_(20)
        , http_running_long_enough_time_when_start_(3 * 60 * 1000)
        , http_protect_time_when_start_(10 * 1000)
        , http_protect_time_when_urgent_switched_(20 * 1000)
        , http_running_long_enough_time_when_urgent_switched_(60 * 1000)
        , safe_rest_playable_time_delim_when_use_http_(5)
        , http_protect_time_when_large_upload_(10 * 1000)
        , p2p_rest_playable_time_delim_when_switched_with_large_time_(6)
        , p2p_rest_playable_time_delim_(5)
        , p2p_protect_time_when_switched_with_not_enough_time_(15 * 1000)
        , p2p_protect_time_when_switched_with_buffering_(30 * 1000)
        , time_to_ignore_http_bad_(3 * 60 * 1000)
        , urgent_rest_playable_time_delim_(10)
        , safe_rest_playable_time_delim_(15)
        , safe_enough_rest_playable_time_delim_(20)
        , using_udpserver_time_in_second_delim_(60)
        , small_ratio_delim_of_upload_speed_to_datarate_(100)
        , using_cdn_or_udpserver_time_at_least_when_large_upload_(30)
        , use_udpserver_count_(3)
        , p2p_protect_time_when_start_(30 * 1000)
        , should_use_bw_type_(true)
        , enhanced_announce_threshold_in_millseconds_(4000)
        , enhanced_announce_copies_(4)
        , udpserver_protect_time_when_start_(15 * 1000)
        , peer_count_when_use_sn_(100)
        , live_peer_max_connections_(25)
        , live_connect_low_normal_threshold_(25)
        , live_connect_normal_high_threshold_(15)
        , live_minimum_window_size_(4)
        , live_maximum_window_size_(25)
        , live_exchange_large_upload_ability_delim_(20 * 1024)
        , live_exchange_large_upload_ability_max_count_(10)
        , live_exchange_large_upload_to_me_delim_(5 * 1024)
        , live_exchange_large_upload_to_me_max_count_(10)
        , should_use_exchange_peers_firstly_(false)
        , live_exchange_interval_in_second_(10)
        , live_extended_connections_(0)
        , live_lost_prejudge_(true)
        , udpserver_maximum_requests_(6)
        , udpserver_maximum_window_size_(25)
        , live_minimum_upload_speed_in_kilobytes_(20)
        , p2p_speed_threshold_(5)
        , time_of_advancing_switching_to_http_when_p2p_slow_(3)
        , p2p_protect_time_if_start_and_speed_is_0_(10 * 1000)
        , p2p_protect_time_if_speed_is_0_(5 * 1000)
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
                ("config.peerinfointerval", po::value<uint32_t>()->default_value(send_peer_info_packet_interval_in_second_))
                ("config.a", po::value<uint32_t>()->default_value(safe_enough_rest_playable_time_delim_under_http_))
                ("config.b", po::value<uint32_t>()->default_value(http_running_long_enough_time_when_start_))
                ("config.c", po::value<uint32_t>()->default_value(http_protect_time_when_start_))
                ("config.d", po::value<uint32_t>()->default_value(http_protect_time_when_urgent_switched_))
                ("config.e", po::value<uint32_t>()->default_value(http_running_long_enough_time_when_urgent_switched_))
                ("config.f", po::value<uint32_t>()->default_value(safe_rest_playable_time_delim_when_use_http_))
                ("config.g", po::value<uint32_t>()->default_value(http_protect_time_when_large_upload_))
                ("config.h", po::value<uint32_t>()->default_value(p2p_rest_playable_time_delim_when_switched_with_large_time_))
                ("config.i", po::value<uint32_t>()->default_value(p2p_rest_playable_time_delim_))
                ("config.j", po::value<uint32_t>()->default_value(p2p_protect_time_when_switched_with_not_enough_time_))
                ("config.k", po::value<uint32_t>()->default_value(p2p_protect_time_when_switched_with_buffering_))
                ("config.l", po::value<uint32_t>()->default_value(time_to_ignore_http_bad_))
                ("config.m", po::value<uint32_t>()->default_value(p2p_protect_time_when_start_))
                ("config.n", po::value<bool>()->default_value(should_use_bw_type_))
                ("config.o", po::value<uint32_t>()->default_value(udpserver_protect_time_when_start_))
                ("config.peerinfointerval", po::value<uint32_t>()->default_value(send_peer_info_packet_interval_in_second_))
                ("config.rpt1", po::value<uint32_t>()->default_value(urgent_rest_playable_time_delim_))
                ("config.rpt2", po::value<uint32_t>()->default_value(safe_rest_playable_time_delim_))
                ("config.rpt3", po::value<uint32_t>()->default_value(safe_enough_rest_playable_time_delim_))
                ("config.ut1", po::value<uint32_t>()->default_value(using_udpserver_time_in_second_delim_))
                ("config.ut2", po::value<uint32_t>()->default_value(using_cdn_or_udpserver_time_at_least_when_large_upload_))
                ("config.sr", po::value<uint32_t>()->default_value(small_ratio_delim_of_upload_speed_to_datarate_))
                ("config.uuc", po::value<uint32_t>()->default_value(use_udpserver_count_))
                ("config.eat", po::value<uint32_t>()->default_value(enhanced_announce_threshold_in_millseconds_))
                ("config.eac", po::value<uint32_t>()->default_value(enhanced_announce_copies_))
                ("config.pc", po::value<uint32_t>()->default_value(peer_count_when_use_sn_))
                ("config.lmc", po::value<uint32_t>()->default_value(live_peer_max_connections_))
                ("config.lcln", po::value<uint32_t>()->default_value(live_connect_low_normal_threshold_))
                ("config.lcnh", po::value<uint32_t>()->default_value(live_connect_normal_high_threshold_))
                ("config.lminw", po::value<uint32_t>()->default_value(live_minimum_window_size_))
                ("config.lmaxw", po::value<uint32_t>()->default_value(live_maximum_window_size_))
                ("config.leuad", po::value<uint32_t>()->default_value(live_exchange_large_upload_ability_delim_))
                ("config.leuac", po::value<uint32_t>()->default_value(live_exchange_large_upload_ability_max_count_))
                ("config.leumd", po::value<uint32_t>()->default_value(live_exchange_large_upload_to_me_delim_))
                ("config.leumc", po::value<uint32_t>()->default_value(live_exchange_large_upload_to_me_max_count_))
                ("config.epf", po::value<bool>()->default_value(should_use_exchange_peers_firstly_))
                ("config.lei", po::value<uint32_t>()->default_value(live_exchange_interval_in_second_))
                ("config.lec", po::value<uint32_t>()->default_value(live_extended_connections_))
                ("config.llp", po::value<bool>()->default_value(live_lost_prejudge_))
                ("config.umr", po::value<uint32_t>()->default_value(udpserver_maximum_requests_))
                ("config.umw", po::value<uint32_t>()->default_value(udpserver_maximum_window_size_))
                ("config.lminu", po::value<uint32_t>()->default_value(live_minimum_upload_speed_in_kilobytes_))
                ("config.p2pst", po::value<uint32_t>()->default_value(p2p_speed_threshold_))
                ("config.ahttp", po::value<uint32_t>()->default_value(time_of_advancing_switching_to_http_when_p2p_slow_))
                ("config.pp1", po::value<uint32_t>()->default_value(p2p_protect_time_if_start_and_speed_is_0_))
                ("config.pp2", po::value<uint32_t>()->default_value(p2p_protect_time_if_speed_is_0_));

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
            safe_enough_rest_playable_time_delim_under_http_ = vm["config.a"].as<uint32_t>();
            http_running_long_enough_time_when_start_ = vm["config.b"].as<uint32_t>();
            http_protect_time_when_start_ = vm["config.c"].as<uint32_t>();
            http_protect_time_when_urgent_switched_ = vm["config.d"].as<uint32_t>();
            http_running_long_enough_time_when_urgent_switched_ = vm["config.e"].as<uint32_t>();
            safe_rest_playable_time_delim_when_use_http_ = vm["config.f"].as<uint32_t>();
            http_protect_time_when_large_upload_ = vm["config.g"].as<uint32_t>();
            p2p_rest_playable_time_delim_when_switched_with_large_time_ = vm["config.h"].as<uint32_t>();
            p2p_rest_playable_time_delim_ = vm["config.i"].as<uint32_t>();
            p2p_protect_time_when_switched_with_not_enough_time_ = vm["config.j"].as<uint32_t>();
            p2p_protect_time_when_switched_with_buffering_ = vm["config.k"].as<uint32_t>();
            time_to_ignore_http_bad_ = vm["config.l"].as<uint32_t>();
            p2p_protect_time_when_start_ = vm["config.m"].as<uint32_t>();
            should_use_bw_type_ = vm["config.n"].as<bool>();
            udpserver_protect_time_when_start_ = vm["config.o"].as<uint32_t>();
            urgent_rest_playable_time_delim_ = vm["config.rpt1"].as<uint32_t>();
            safe_rest_playable_time_delim_ = vm["config.rpt2"].as<uint32_t>();
            safe_enough_rest_playable_time_delim_ = vm["config.rpt3"].as<uint32_t>();
            using_udpserver_time_in_second_delim_ = vm["config.ut1"].as<uint32_t>();
            using_cdn_or_udpserver_time_at_least_when_large_upload_ = vm["config.ut2"].as<uint32_t>();
            small_ratio_delim_of_upload_speed_to_datarate_ = vm["config.sr"].as<uint32_t>();
            use_udpserver_count_ = vm["config.uuc"].as<uint32_t>();
            enhanced_announce_threshold_in_millseconds_ = vm["config.eat"].as<uint32_t>();
            enhanced_announce_copies_ = vm["config.eac"].as<uint32_t>();
            peer_count_when_use_sn_ = vm["config.pc"].as<uint32_t>();
            live_peer_max_connections_ = vm["config.lmc"].as<uint32_t>();
            live_connect_low_normal_threshold_ = vm["config.lcln"].as<uint32_t>();
            live_connect_normal_high_threshold_ = vm["config.lcnh"].as<uint32_t>();
            live_minimum_window_size_ = vm["config.lminw"].as<uint32_t>();
            live_maximum_window_size_ = vm["config.lmaxw"].as<uint32_t>();
            live_exchange_large_upload_ability_delim_ = vm["config.leuad"].as<uint32_t>();
            live_exchange_large_upload_ability_max_count_ = vm["config.leuac"].as<uint32_t>();
            live_exchange_large_upload_to_me_delim_ = vm["config.leumd"].as<uint32_t>();
            live_exchange_large_upload_to_me_max_count_ = vm["config.leumc"].as<uint32_t>();
            should_use_exchange_peers_firstly_ = vm["config.epf"].as<bool>();
            live_exchange_interval_in_second_ = vm["config.lei"].as<uint32_t>();
            live_extended_connections_ = vm["config.lec"].as<uint32_t>();
            live_lost_prejudge_ = vm["config.llp"].as<bool>();
            udpserver_maximum_requests_ = vm["config.umr"].as<uint32_t>();
            udpserver_maximum_window_size_ = vm["config.umw"].as<uint32_t>();
            live_minimum_upload_speed_in_kilobytes_ = vm["config.lminu"].as<uint32_t>();
            p2p_speed_threshold_ = vm["config.p2pst"].as<uint32_t>();
            time_of_advancing_switching_to_http_when_p2p_slow_ = vm["config.ahttp"].as<uint32_t>();
            p2p_protect_time_if_start_and_speed_is_0_ = vm["config.pp1"].as<uint32_t>();
            p2p_protect_time_if_speed_is_0_ = vm["config.pp2"].as<uint32_t>();

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