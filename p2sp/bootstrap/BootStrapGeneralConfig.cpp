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
        , use_cdn_when_large_upload_in_saving_mode_(true)
        , desirable_vod_ippool_size_(500)
        , desirable_live_ippool_size_(1000)
        , rest_play_time_delim_(25)
        , rest_play_time_delim_in_saving_mode_(30)
        , ratio_delim_of_upload_speed_to_datarate_(200)
        , ratio_delim_of_upload_speed_to_datarate_in_saving_mode_(900)
        , limit_upload_speed_for_live2_(true)
        , send_peer_info_packet_interval_in_second_(5)
        , safe_enough_rest_playable_time_delim_under_http_(20)
        , safe_enough_rest_playable_time_delim_under_http_in_saving_mode_(0)
        , http_running_long_enough_time_when_start_(3 * 60 * 1000)
        , http_running_long_enough_time_when_start_in_saving_mode_(0)
        , http_protect_time_when_start_(10 * 1000)
        , http_protect_time_when_start_in_saving_mode_(10000)
        , http_protect_time_when_urgent_switched_(20 * 1000)
        , http_protect_time_when_urgent_switched_in_saving_mode_(15000)
        , http_running_long_enough_time_when_urgent_switched_(60 * 1000)
        , http_running_long_enough_time_when_urgent_switched_in_saving_mode_(45000)
        , safe_rest_playable_time_delim_when_use_http_(5)
        , safe_rest_playable_time_delim_when_use_http_in_saving_mode_(5)
        , http_protect_time_when_large_upload_(10 * 1000)
        , http_protect_time_when_large_upload_in_saving_mode_(10000)
        , p2p_rest_playable_time_delim_when_switched_with_large_time_(6)
        , p2p_rest_playable_time_delim_when_switched_with_large_time_in_saving_mode_(0)
        , p2p_rest_playable_time_delim_(5)
        , p2p_rest_playable_time_delim_in_saving_mode_(0)
        , p2p_protect_time_when_switched_with_not_enough_time_(15 * 1000)
        , p2p_protect_time_when_switched_with_not_enough_time_in_saving_mode_(20000)
        , p2p_protect_time_when_switched_with_buffering_(30 * 1000)
        , p2p_protect_time_when_switched_with_buffering_in_saving_mode_(35000)
        , time_to_ignore_http_bad_(3 * 60 * 1000)
        , time_to_ignore_http_bad_in_saving_mode_(180000)
        , urgent_rest_playable_time_delim_(10)
        , urgent_rest_playable_time_delim_in_saving_mode_(12)
        , safe_rest_playable_time_delim_(15)
        , safe_rest_playable_time_delim_in_saving_mode_(13)
        , safe_enough_rest_playable_time_delim_(20)
        , safe_enough_rest_playable_time_delim_in_saving_mode_(18)
        , using_udpserver_time_in_second_delim_(60)
        , using_udpserver_time_in_second_delim_in_saving_mode_(45)
        , small_ratio_delim_of_upload_speed_to_datarate_(100)
        , small_ratio_delim_of_upload_speed_to_datarate_in_saving_mode_(600)
        , using_cdn_or_udpserver_time_at_least_when_large_upload_(30)
        , using_cdn_or_udpserver_time_at_least_when_large_upload_in_saving_mode_(30)
        , use_udpserver_count_(3)
        , p2p_protect_time_when_start_(30 * 1000)
        , p2p_protect_time_when_start_in_saving_mode_(30000)
        , should_use_bw_type_(true)
        , enhanced_announce_threshold_in_millseconds_(4000)
        , enhanced_announce_copies_(4)
        , udpserver_protect_time_when_start_(15 * 1000)
        , udpserver_protect_time_when_start_in_saving_mode_(25000)
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
        , p2p_speed_threshold_in_saving_mode_(0)
        , time_of_advancing_switching_to_http_when_p2p_slow_(3)
        , time_of_advancing_switching_to_http_when_p2p_slow_in_saving_mode_(3)
        , p2p_protect_time_if_start_and_speed_is_0_(10 * 1000)
        , p2p_protect_time_if_start_and_speed_is_0_in_saving_mode_(1000000)
        , p2p_protect_time_if_speed_is_0_(5 * 1000)
        , p2p_protect_time_if_speed_is_0_in_saving_mode_(1000000)
        , fall_behind_seconds_threshold_(30)
        , max_rest_playable_time_(120)
        , min_rest_playable_time_(60)
        , prevent_http_predownload(true)
        , max_ratio_of_upload_to_download_delim_(800)
        , max_ratio_of_upload_to_download_delim_in_saving_mode_(900)
        , large_ratio_of_upload_to_download_delim_(600)
        , large_ratio_of_upload_to_download_delim_in_saving_mode_(700)
        , ratio_of_large_upload_times_to_total_times_delim_(60)
        , ratio_of_large_upload_times_to_total_times_delim_in_saving_mode_(60)
        , upload_connection_count_delim_(10)
        , upload_connection_count_delim_in_saving_mode_(10)
        , not_strict_ratio_delim_of_upload_speed_to_datarate_(600)
        , not_strict_ratio_delim_of_upload_speed_to_datarate_in_saving_mode_(600)
        , min_interval_of_cdn_acceleration_delim_(300)
        , min_interval_of_cdn_acceleration_delim_in_saving_mode_(300)
        , use_cdn_to_accelerate_based_on_history_(true)
        , use_cdn_to_accelerate_based_on_history_in_saving_mode_(false)
        , max_times_of_record_(50)
        , min_times_of_record_(3)
        , interval_of_requesting_announce_from_udpserver_(5)
        , p2p_download_max_connect_count_bound(40)
        , p2p_download_min_connect_count_bound(5)
        , udp_server_usage_history_enabled_(true)
        , auto_switch_stream_(false)
        , max_sn_list_size_(2)
        , max_sn_list_size_for_vip_(3)
        , sn_request_number_(20)
        , should_judge_switching_datarate_manually_(true)
        , interval_of_two_vv_delim_(5 * 1000)
        , rest_playable_time_delim_when_switching_(10)
        , max_material_do_list_count_(2)
        , rest_time_enough_lauch_P2P_2300_0(30)
        , rest_time_enough_launch_P2P_2300_10(20)
        , rest_time_need_check_P2P(50)
        , vip_download_min_p2p_speed_(50)
        , write_block_when_full_(false)
        , write_block_when_verified_(false)
        , max_upload_speed_used_in_network_check_(100)
        , ping_time_net_check_big_upload_speed_(20)
        , ping_time_net_check_small_upload_speed_(80)
        , minus_value_when_upload_speed_overlarge_(5)
        , load_sn_on_cdn_(false)
        , time_wait_for_tinydrag_(1)
        , sn_port_on_cdn_(19001)
        , seconds_wait_for_tinydrag_in_download_mode_(2)
        , use_http_when_connected_none_peers_(true)
        , use_http_when_connected_none_peers_in_saving_mode_(false)
        , rest_play_time_delim_when_none_peers_(50)
        , rest_play_time_delim_when_none_peers_in_saving_mode_(10)
        , enough_rest_play_time_delim_when_none_peers_(100)
        , enough_rest_play_time_delim_when_none_peers_in_saving_mode_(15)
        , use_http_when_urgent_(true)
        , use_http_when_urgent_in_saving_mode_(false)
        , http_running_longest_time_when_urgent_switched_(60000)
        , http_running_longest_time_when_urgent_switched_in_saving_mode_(0)
        , config_string_length_(0)
        , open_request_subpiece_packet_old_(false)
        , open_rid_info_request_response_(true)
        , udpserver_udp_port_(19001)
        , use_udpserver_from_cdn_(true)
        , is_vip_use_sn_all_time_(true)
        , nat_check_server_ip_("202.102.68.38@202.102.68.208@115.238.166.151@115.238.166.211@124.160.184.157@124.160.184.217@122.141.230.143@122.141.230.173")
        , nat_need_check_min_days_(3)
        , need_use_stunclient_nat_check_(false)
        , factor_used_on_cdn_sn_speed_(2)
        , need_protect_cdn_sn_(false)
        , second_delay_for_check_sn_speed_(8)
        , http_protect_time2_when_start_(3000)
        , http_protect_time2_when_start_in_saving_mode_(3000)
        , http_download_bytes_delim_when_start_(0)
        , http_download_bytes_delim_when_start_in_saving_mode_(0)
    {
    }

    void BootStrapGeneralConfig::Start(string const & config_path)
    {
        SetConfigString(DEFAULT_CONFIG_STRING, false);

        local_config_file_path_ = config_path;

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
                ("config.usecdnpolicysave", po::value<bool>()->default_value(use_cdn_when_large_upload_in_saving_mode_))
                ("config.vps", po::value<uint32_t>()->default_value(desirable_vod_ippool_size_))
                ("config.lps", po::value<uint32_t>()->default_value(desirable_live_ippool_size_))
                ("config.restplaytime", po::value<uint32_t>()->default_value(rest_play_time_delim_))
                ("config.restplaytimesave", po::value<uint32_t>()->default_value(rest_play_time_delim_in_saving_mode_))
                ("config.ratiodelim", po::value<uint32_t>()->default_value(ratio_delim_of_upload_speed_to_datarate_))
                ("config.ratiodelimsave", po::value<uint32_t>()->default_value(ratio_delim_of_upload_speed_to_datarate_in_saving_mode_))
                ("config.limitlive2upload", po::value<bool>()->default_value(limit_upload_speed_for_live2_))
                ("config.peerinfointerval", po::value<uint32_t>()->default_value(send_peer_info_packet_interval_in_second_))
                ("config.a", po::value<uint32_t>()->default_value(safe_enough_rest_playable_time_delim_under_http_))
                ("config.asave", po::value<uint32_t>()->default_value(safe_enough_rest_playable_time_delim_under_http_in_saving_mode_))
                ("config.b", po::value<uint32_t>()->default_value(http_running_long_enough_time_when_start_))
                ("config.bsave", po::value<uint32_t>()->default_value(http_running_long_enough_time_when_start_in_saving_mode_))
                ("config.c", po::value<uint32_t>()->default_value(http_protect_time_when_start_))
                ("config.csave", po::value<uint32_t>()->default_value(http_protect_time_when_start_in_saving_mode_))
                ("config.d", po::value<uint32_t>()->default_value(http_protect_time_when_urgent_switched_))
                ("config.dsave", po::value<uint32_t>()->default_value(http_protect_time_when_urgent_switched_in_saving_mode_))
                ("config.e", po::value<uint32_t>()->default_value(http_running_long_enough_time_when_urgent_switched_))
                ("config.esave", po::value<uint32_t>()->default_value(http_running_long_enough_time_when_urgent_switched_in_saving_mode_))
                ("config.f", po::value<uint32_t>()->default_value(safe_rest_playable_time_delim_when_use_http_))
                ("config.fsave", po::value<uint32_t>()->default_value(safe_rest_playable_time_delim_when_use_http_in_saving_mode_))
                ("config.g", po::value<uint32_t>()->default_value(http_protect_time_when_large_upload_))
                ("config.gsave", po::value<uint32_t>()->default_value(http_protect_time_when_large_upload_in_saving_mode_))
                ("config.h", po::value<uint32_t>()->default_value(p2p_rest_playable_time_delim_when_switched_with_large_time_))
                ("config.hsave", po::value<uint32_t>()->default_value(p2p_rest_playable_time_delim_when_switched_with_large_time_in_saving_mode_))
                ("config.i", po::value<uint32_t>()->default_value(p2p_rest_playable_time_delim_))
                ("config.isave", po::value<uint32_t>()->default_value(p2p_rest_playable_time_delim_in_saving_mode_))
                ("config.j", po::value<uint32_t>()->default_value(p2p_protect_time_when_switched_with_not_enough_time_))
                ("config.jsave", po::value<uint32_t>()->default_value(p2p_protect_time_when_switched_with_not_enough_time_in_saving_mode_))
                ("config.k", po::value<uint32_t>()->default_value(p2p_protect_time_when_switched_with_buffering_))
                ("config.ksave", po::value<uint32_t>()->default_value(p2p_protect_time_when_switched_with_buffering_in_saving_mode_))
                ("config.l", po::value<uint32_t>()->default_value(time_to_ignore_http_bad_))
                ("config.lsave", po::value<uint32_t>()->default_value(time_to_ignore_http_bad_in_saving_mode_))
                ("config.m", po::value<uint32_t>()->default_value(p2p_protect_time_when_start_))
                ("config.msave", po::value<uint32_t>()->default_value(p2p_protect_time_when_start_in_saving_mode_))
                ("config.n", po::value<bool>()->default_value(should_use_bw_type_))
                ("config.o", po::value<uint32_t>()->default_value(udpserver_protect_time_when_start_))
                ("config.osave", po::value<uint32_t>()->default_value(udpserver_protect_time_when_start_in_saving_mode_))
                ("config.peerinfointerval", po::value<uint32_t>()->default_value(send_peer_info_packet_interval_in_second_))
                ("config.rpt1", po::value<uint32_t>()->default_value(urgent_rest_playable_time_delim_))
                ("config.rpt1save", po::value<uint32_t>()->default_value(urgent_rest_playable_time_delim_in_saving_mode_))
                ("config.rpt2", po::value<uint32_t>()->default_value(safe_rest_playable_time_delim_))
                ("config.rpt2save", po::value<uint32_t>()->default_value(safe_rest_playable_time_delim_in_saving_mode_))
                ("config.rpt3", po::value<uint32_t>()->default_value(safe_enough_rest_playable_time_delim_))
                ("config.rpt3save", po::value<uint32_t>()->default_value(safe_enough_rest_playable_time_delim_in_saving_mode_))
                ("config.ut1", po::value<uint32_t>()->default_value(using_udpserver_time_in_second_delim_))
                ("config.ut1save", po::value<uint32_t>()->default_value(using_udpserver_time_in_second_delim_in_saving_mode_))
                ("config.ut2", po::value<uint32_t>()->default_value(using_cdn_or_udpserver_time_at_least_when_large_upload_))
                ("config.ut2save", po::value<uint32_t>()->default_value(using_cdn_or_udpserver_time_at_least_when_large_upload_in_saving_mode_))
                ("config.sr", po::value<uint32_t>()->default_value(small_ratio_delim_of_upload_speed_to_datarate_))
                ("config.srsave", po::value<uint32_t>()->default_value(small_ratio_delim_of_upload_speed_to_datarate_in_saving_mode_))
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
                ("config.p2pstsave", po::value<uint32_t>()->default_value(p2p_speed_threshold_in_saving_mode_))
                ("config.ahttp", po::value<uint32_t>()->default_value(time_of_advancing_switching_to_http_when_p2p_slow_))
                ("config.ahttpsave", po::value<uint32_t>()->default_value(time_of_advancing_switching_to_http_when_p2p_slow_in_saving_mode_))
                ("config.pp1", po::value<uint32_t>()->default_value(p2p_protect_time_if_start_and_speed_is_0_))
                ("config.pp1save", po::value<uint32_t>()->default_value(p2p_protect_time_if_start_and_speed_is_0_in_saving_mode_))
                ("config.pp2", po::value<uint32_t>()->default_value(p2p_protect_time_if_speed_is_0_))
                ("config.pp2save", po::value<uint32_t>()->default_value(p2p_protect_time_if_speed_is_0_in_saving_mode_))
                ("config.fbt", po::value<uint32_t>()->default_value(fall_behind_seconds_threshold_))
                ("config.maxlive2t", po::value<uint32_t>()->default_value(max_rest_playable_time_))
                ("config.minlive2t", po::value<uint32_t>()->default_value(min_rest_playable_time_))
                ("config.phpd", po::value<bool>()->default_value(prevent_http_predownload))
                ("config.maxrutd", po::value<uint32_t>()->default_value(max_ratio_of_upload_to_download_delim_))
                ("config.maxrutdsave", po::value<uint32_t>()->default_value(max_ratio_of_upload_to_download_delim_in_saving_mode_))
                ("config.lrutd", po::value<uint32_t>()->default_value(large_ratio_of_upload_to_download_delim_))
                ("config.lrutdsave", po::value<uint32_t>()->default_value(large_ratio_of_upload_to_download_delim_in_saving_mode_))
                ("config.rlut", po::value<uint32_t>()->default_value(ratio_of_large_upload_times_to_total_times_delim_))
                ("config.rlutsave", po::value<uint32_t>()->default_value(ratio_of_large_upload_times_to_total_times_delim_in_saving_mode_))
                ("config.ucc", po::value<uint32_t>()->default_value(upload_connection_count_delim_))
                ("config.uccsave", po::value<uint32_t>()->default_value(upload_connection_count_delim_in_saving_mode_))
                ("config.nsrdutd", po::value<uint32_t>()->default_value(not_strict_ratio_delim_of_upload_speed_to_datarate_))
                ("config.nsrdutdsave", po::value<uint32_t>()->default_value(not_strict_ratio_delim_of_upload_speed_to_datarate_in_saving_mode_))
                ("config.minica", po::value<uint32_t>()->default_value(min_interval_of_cdn_acceleration_delim_))
                ("config.minicasave", po::value<uint32_t>()->default_value(min_interval_of_cdn_acceleration_delim_in_saving_mode_))
                ("config.uca", po::value<bool>()->default_value(use_cdn_to_accelerate_based_on_history_))
                ("config.ucasave", po::value<bool>()->default_value(use_cdn_to_accelerate_based_on_history_in_saving_mode_))
                ("config.maxtr", po::value<uint32_t>()->default_value(max_times_of_record_))
                ("config.mintr", po::value<uint32_t>()->default_value(min_times_of_record_))
                ("config.ira", po::value<uint32_t>()->default_value(interval_of_requesting_announce_from_udpserver_))
                ("config.maxcon", po::value<uint32_t>()->default_value(p2p_download_max_connect_count_bound))
                ("config.mincon", po::value<uint32_t>()->default_value(p2p_download_min_connect_count_bound))
                ("config.usuhe", po::value<bool>()->default_value(udp_server_usage_history_enabled_))
                ("config.ass", po::value<bool>()->default_value(auto_switch_stream_))
                ("config.maxsnls", po::value<uint32_t>()->default_value(max_sn_list_size_))
                ("config.snfvip", po::value<uint32_t>()->default_value(max_sn_list_size_for_vip_))
                ("config.snrc", po::value<uint32_t>()->default_value(sn_request_number_))
                ("config.jsd", po::value<bool>()->default_value(should_judge_switching_datarate_manually_))
                ("config.i2vv", po::value<uint32_t>()->default_value(interval_of_two_vv_delim_))
                ("config.rpts", po::value<uint32_t>()->default_value(rest_playable_time_delim_when_switching_))
                ("config.mmdc", po::value<uint32_t>()->default_value(max_material_do_list_count_))
                ("config.rel10", po::value<uint32_t>()->default_value(rest_time_enough_launch_P2P_2300_10))
                ("config.rel0", po::value<uint32_t>()->default_value(rest_time_enough_lauch_P2P_2300_0))
                ("config.rncp", po::value<uint32_t>()->default_value(rest_time_need_check_P2P))
                ("config.vdmps", po::value<uint32_t>()->default_value(vip_download_min_p2p_speed_))
                ("config.wbwf", po::value<uint32_t>()->default_value(write_block_when_full_))
                ("config.wbwv", po::value<uint32_t>()->default_value(write_block_when_verified_))
                ("config.musnc", po::value<uint32_t>()->default_value(max_upload_speed_used_in_network_check_))
                ("config.pbus", po::value<uint32_t>()->default_value(ping_time_net_check_big_upload_speed_))
                ("config.psus", po::value<uint32_t>()->default_value(ping_time_net_check_small_upload_speed_))
                ("config.mvul", po::value<uint32_t>()->default_value(minus_value_when_upload_speed_overlarge_))
                ("config.twtd", po::value<uint32_t>()->default_value(time_wait_for_tinydrag_))
                ("config.lsc", po::value<bool>()->default_value(load_sn_on_cdn_))
                ("config.spc", po::value<uint32_t>()->default_value(sn_port_on_cdn_))
                ("config.swtdm", po::value<uint32_t>()->default_value(seconds_wait_for_tinydrag_in_download_mode_))
                ("config.uh0p", po::value<bool>()->default_value(use_http_when_connected_none_peers_))
                ("config.uh0psave", po::value<bool>()->default_value(use_http_when_connected_none_peers_in_saving_mode_))
                ("config.rptd0p", po::value<uint32_t>()->default_value(rest_play_time_delim_when_none_peers_))
                ("config.rptd0psave", po::value<uint32_t>()->default_value(rest_play_time_delim_when_none_peers_in_saving_mode_))
                ("config.erptd0p", po::value<uint32_t>()->default_value(enough_rest_play_time_delim_when_none_peers_))
                ("config.erptd0psave", po::value<uint32_t>()->default_value(enough_rest_play_time_delim_when_none_peers_in_saving_mode_))
                ("config.uswu", po::value<bool>()->default_value(use_http_when_urgent_))
                ("config.uswusave", po::value<bool>()->default_value(use_http_when_urgent_in_saving_mode_))
                ("config.hrlt", po::value<uint32_t>()->default_value(http_running_longest_time_when_urgent_switched_))
                ("config.hrltsave", po::value<uint32_t>()->default_value(http_running_longest_time_when_urgent_switched_in_saving_mode_))
                ("config.spold", po::value<bool>()->default_value(open_request_subpiece_packet_old_))
                ("config.ridinfo", po::value<bool>()->default_value(open_rid_info_request_response_))
                ("config.uup", po::value<uint16_t>()->default_value(udpserver_udp_port_))
                ("config.uufc", po::value<bool>()->default_value(use_udpserver_from_cdn_))
                ("config.vusat", po::value<bool>()->default_value(is_vip_use_sn_all_time_))
                ("config.nsip", po::value<string>()->default_value(nat_check_server_ip_))
                ("config.nnmd", po::value<uint32_t>()->default_value(nat_need_check_min_days_))
                ("config.nsnc", po::value<bool>()->default_value(need_use_stunclient_nat_check_))
                ("config.fcss", po::value<uint32_t>()->default_value(factor_used_on_cdn_sn_speed_))
                ("config.npcs", po::value<bool>()->default_value(need_protect_cdn_sn_))
                ("config.sdss", po::value<uint32_t>()->default_value(second_delay_for_check_sn_speed_))
                ("config.hpt2", po::value<uint32_t>()->default_value(http_protect_time2_when_start_))
                ("config.hpt2save", po::value<uint32_t>()->default_value(http_protect_time2_when_start_in_saving_mode_))
                ("config.hdbd", po::value<uint32_t>()->default_value(http_download_bytes_delim_when_start_))
                ("config.hdbdsave", po::value<uint32_t>()->default_value(http_download_bytes_delim_when_start_in_saving_mode_))
                ;

            std::istringstream config_stream(config_string);

            po::variables_map vm;
            po::store(po::parse_config_file(config_stream, config_desc, true), vm);
            po::notify(vm);

            hou_server_list_ = vm["config.hl"].as<string>();
            use_push_ = vm["config.usepush"].as<bool>();
            upload_policy_ = (UploadPolicy)(vm["config.uploadpolicy"].as<uint32_t>());
            connection_policy_enable_ = vm["config.connectionpolicy"].as<bool>();
            use_cdn_when_large_upload_ = vm["config.usecdnpolicy"].as<bool>();
            use_cdn_when_large_upload_in_saving_mode_ = vm["config.usecdnpolicysave"].as<bool>();
            desirable_live_ippool_size_ = vm["config.lps"].as<uint32_t>();
            desirable_vod_ippool_size_ = vm["config.vps"].as<uint32_t>();
            rest_play_time_delim_ = vm["config.restplaytime"].as<uint32_t>();
            rest_play_time_delim_in_saving_mode_ = vm["config.restplaytimesave"].as<uint32_t>();
            ratio_delim_of_upload_speed_to_datarate_ = vm["config.ratiodelim"].as<uint32_t>();
            ratio_delim_of_upload_speed_to_datarate_in_saving_mode_ = vm["config.ratiodelimsave"].as<uint32_t>();
            data_collection_server_list_ = vm["config.dc_servers"].as<string>();
            limit_upload_speed_for_live2_ = vm["config.limitlive2upload"].as<bool>();
            send_peer_info_packet_interval_in_second_ = vm["config.peerinfointerval"].as<uint32_t>();
            safe_enough_rest_playable_time_delim_under_http_ = vm["config.a"].as<uint32_t>();
            safe_enough_rest_playable_time_delim_under_http_in_saving_mode_ = vm["config.asave"].as<uint32_t>();
            http_running_long_enough_time_when_start_ = vm["config.b"].as<uint32_t>();
            http_running_long_enough_time_when_start_in_saving_mode_ = vm["config.bsave"].as<uint32_t>();
            http_protect_time_when_start_ = vm["config.c"].as<uint32_t>();
            http_protect_time_when_start_in_saving_mode_ = vm["config.csave"].as<uint32_t>();
            http_protect_time_when_urgent_switched_ = vm["config.d"].as<uint32_t>();
            http_protect_time_when_urgent_switched_in_saving_mode_ = vm["config.dsave"].as<uint32_t>();
            http_running_long_enough_time_when_urgent_switched_ = vm["config.e"].as<uint32_t>();
            http_running_long_enough_time_when_urgent_switched_in_saving_mode_ = vm["config.esave"].as<uint32_t>();
            safe_rest_playable_time_delim_when_use_http_ = vm["config.f"].as<uint32_t>();
            safe_rest_playable_time_delim_when_use_http_in_saving_mode_ = vm["config.fsave"].as<uint32_t>();
            http_protect_time_when_large_upload_ = vm["config.g"].as<uint32_t>();
            http_protect_time_when_large_upload_in_saving_mode_ = vm["config.gsave"].as<uint32_t>();
            p2p_rest_playable_time_delim_when_switched_with_large_time_ = vm["config.h"].as<uint32_t>();
            p2p_rest_playable_time_delim_when_switched_with_large_time_in_saving_mode_ = vm["config.hsave"].as<uint32_t>();
            p2p_rest_playable_time_delim_ = vm["config.i"].as<uint32_t>();
            p2p_rest_playable_time_delim_in_saving_mode_ = vm["config.isave"].as<uint32_t>();
            p2p_protect_time_when_switched_with_not_enough_time_ = vm["config.j"].as<uint32_t>();
            p2p_protect_time_when_switched_with_not_enough_time_in_saving_mode_ = vm["config.jsave"].as<uint32_t>();
            p2p_protect_time_when_switched_with_buffering_ = vm["config.k"].as<uint32_t>();
            p2p_protect_time_when_switched_with_buffering_in_saving_mode_ = vm["config.ksave"].as<uint32_t>();
            time_to_ignore_http_bad_ = vm["config.l"].as<uint32_t>();
            time_to_ignore_http_bad_in_saving_mode_ = vm["config.lsave"].as<uint32_t>();
            p2p_protect_time_when_start_ = vm["config.m"].as<uint32_t>();
            p2p_protect_time_when_start_in_saving_mode_ = vm["config.msave"].as<uint32_t>();
            should_use_bw_type_ = vm["config.n"].as<bool>();
            udpserver_protect_time_when_start_ = vm["config.o"].as<uint32_t>();
            udpserver_protect_time_when_start_in_saving_mode_ = vm["config.osave"].as<uint32_t>();
            urgent_rest_playable_time_delim_ = vm["config.rpt1"].as<uint32_t>();
            urgent_rest_playable_time_delim_in_saving_mode_ = vm["config.rpt1save"].as<uint32_t>();
            safe_rest_playable_time_delim_ = vm["config.rpt2"].as<uint32_t>();
            safe_rest_playable_time_delim_in_saving_mode_ = vm["config.rpt2save"].as<uint32_t>();
            safe_enough_rest_playable_time_delim_ = vm["config.rpt3"].as<uint32_t>();
            safe_enough_rest_playable_time_delim_in_saving_mode_ = vm["config.rpt3save"].as<uint32_t>();
            using_udpserver_time_in_second_delim_ = vm["config.ut1"].as<uint32_t>();
            using_udpserver_time_in_second_delim_in_saving_mode_ = vm["config.ut1save"].as<uint32_t>();
            using_cdn_or_udpserver_time_at_least_when_large_upload_ = vm["config.ut2"].as<uint32_t>();
            using_cdn_or_udpserver_time_at_least_when_large_upload_in_saving_mode_ = vm["config.ut2save"].as<uint32_t>();
            small_ratio_delim_of_upload_speed_to_datarate_ = vm["config.sr"].as<uint32_t>();
            small_ratio_delim_of_upload_speed_to_datarate_in_saving_mode_ = vm["config.srsave"].as<uint32_t>();
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
            p2p_speed_threshold_in_saving_mode_ = vm["config.p2pstsave"].as<uint32_t>();
            time_of_advancing_switching_to_http_when_p2p_slow_ = vm["config.ahttp"].as<uint32_t>();
            time_of_advancing_switching_to_http_when_p2p_slow_in_saving_mode_ = vm["config.ahttpsave"].as<uint32_t>();
            p2p_protect_time_if_start_and_speed_is_0_ = vm["config.pp1"].as<uint32_t>();
            p2p_protect_time_if_start_and_speed_is_0_in_saving_mode_ = vm["config.pp1save"].as<uint32_t>();
            p2p_protect_time_if_speed_is_0_ = vm["config.pp2"].as<uint32_t>();
            p2p_protect_time_if_speed_is_0_in_saving_mode_ = vm["config.pp2save"].as<uint32_t>();
            fall_behind_seconds_threshold_ = vm["config.fbt"].as<uint32_t>();
            max_rest_playable_time_ = vm["config.maxlive2t"].as<uint32_t>();
            min_rest_playable_time_ = vm["config.minlive2t"].as<uint32_t>();
            prevent_http_predownload = vm["config.phpd"].as<bool>();
            max_ratio_of_upload_to_download_delim_ = vm["config.maxrutd"].as<uint32_t>();
            max_ratio_of_upload_to_download_delim_in_saving_mode_ = vm["config.maxrutdsave"].as<uint32_t>();
            large_ratio_of_upload_to_download_delim_ = vm["config.lrutd"].as<uint32_t>();
            large_ratio_of_upload_to_download_delim_in_saving_mode_ = vm["config.lrutdsave"].as<uint32_t>();
            ratio_of_large_upload_times_to_total_times_delim_ = vm["config.rlut"].as<uint32_t>();
            ratio_of_large_upload_times_to_total_times_delim_in_saving_mode_ = vm["config.rlutsave"].as<uint32_t>();
            upload_connection_count_delim_ = vm["config.ucc"].as<uint32_t>();
            upload_connection_count_delim_in_saving_mode_ = vm["config.uccsave"].as<uint32_t>();
            not_strict_ratio_delim_of_upload_speed_to_datarate_ = vm["config.nsrdutd"].as<uint32_t>();
            not_strict_ratio_delim_of_upload_speed_to_datarate_in_saving_mode_ = vm["config.nsrdutdsave"].as<uint32_t>();
            min_interval_of_cdn_acceleration_delim_ = vm["config.minica"].as<uint32_t>();
            min_interval_of_cdn_acceleration_delim_in_saving_mode_ = vm["config.minicasave"].as<uint32_t>();
            use_cdn_to_accelerate_based_on_history_ = vm["config.uca"].as<bool>();
            use_cdn_to_accelerate_based_on_history_in_saving_mode_ = vm["config.ucasave"].as<bool>();
            max_times_of_record_ = vm["config.maxtr"].as<uint32_t>();
            min_times_of_record_ = vm["config.mintr"].as<uint32_t>();
            interval_of_requesting_announce_from_udpserver_ = vm["config.ira"].as<uint32_t>();
            p2p_download_max_connect_count_bound = vm["config.maxcon"].as<uint32_t>();
            p2p_download_min_connect_count_bound = vm["config.mincon"].as<uint32_t>();
            udp_server_usage_history_enabled_ = vm["config.usuhe"].as<bool>();
            auto_switch_stream_ = vm["config.ass"].as<bool>();
            max_sn_list_size_ = vm["config.maxsnls"].as<uint32_t>();
            max_sn_list_size_for_vip_ = vm["config.snfvip"].as<uint32_t>();
            sn_request_number_ = vm["config.snrc"].as<uint32_t>();
            should_judge_switching_datarate_manually_ = vm["config.jsd"].as<bool>();
            interval_of_two_vv_delim_ = vm["config.i2vv"].as<uint32_t>();
            rest_playable_time_delim_when_switching_ = vm["config.rpts"].as<uint32_t>();
            max_material_do_list_count_ = vm["config.mmdc"].as<uint32_t>();
            rest_time_enough_launch_P2P_2300_10 = vm["config.rel10"].as<uint32_t>();
            rest_time_enough_lauch_P2P_2300_0 = vm["config.rel0"].as<uint32_t>();
            rest_time_need_check_P2P = vm["config.rncp"].as<uint32_t>();
            vip_download_min_p2p_speed_ = vm["config.vdmps"].as<uint32_t>();
            write_block_when_full_ = vm["config.wbwf"].as<uint32_t>();
            write_block_when_verified_ = vm["config.wbwv"].as<uint32_t>();
            max_upload_speed_used_in_network_check_ = vm["config.musnc"].as<uint32_t>();
            ping_time_net_check_big_upload_speed_ = vm["config.pbus"].as<uint32_t>();
            ping_time_net_check_small_upload_speed_ = vm["config.psus"].as<uint32_t>();
            minus_value_when_upload_speed_overlarge_ = vm["config.mvul"].as<uint32_t>();
            load_sn_on_cdn_ = vm["config.lsc"].as<bool>();
            time_wait_for_tinydrag_ = vm["config.twtd"].as<uint32_t>();
            sn_port_on_cdn_ = vm["config.spc"].as<uint32_t>();
            seconds_wait_for_tinydrag_in_download_mode_ = vm["config.swtdm"].as<uint32_t>();
            use_http_when_connected_none_peers_ = vm["config.uh0p"].as<bool>();
            use_http_when_connected_none_peers_in_saving_mode_ = vm["config.uh0psave"].as<bool>();
            rest_play_time_delim_when_none_peers_ = vm["config.rptd0p"].as<uint32_t>();
            rest_play_time_delim_when_none_peers_in_saving_mode_ = vm["config.rptd0psave"].as<uint32_t>();
            enough_rest_play_time_delim_when_none_peers_ = vm["config.erptd0p"].as<uint32_t>();
            enough_rest_play_time_delim_when_none_peers_in_saving_mode_ = vm["config.erptd0psave"].as<uint32_t>();
            use_http_when_urgent_ = vm["config.uswu"].as<bool>();
            use_http_when_urgent_in_saving_mode_ = vm["config.uswusave"].as<bool>();
            http_running_longest_time_when_urgent_switched_ = vm["config.hrlt"].as<uint32_t>();
            http_running_longest_time_when_urgent_switched_in_saving_mode_ = vm["config.hrltsave"].as<uint32_t>();
            open_request_subpiece_packet_old_ = vm["config.spold"].as<bool>();
            open_rid_info_request_response_ = vm["config.ridinfo"].as<bool>();
            udpserver_udp_port_ = vm["config.uup"].as<uint16_t>();
            use_udpserver_from_cdn_ = vm["config.uufc"].as<bool>();
            is_vip_use_sn_all_time_ = vm["config.vusat"].as<bool>();
            nat_check_server_ip_ = vm["config.nsip"].as<string>();
            nat_need_check_min_days_ = vm["config.nnmd"].as<uint32_t>();
            need_use_stunclient_nat_check_ = vm["config.nsnc"].as<bool>();
            factor_used_on_cdn_sn_speed_ = vm["config.fcss"].as<uint32_t>();
            need_protect_cdn_sn_ = vm["config.npcs"].as<bool>();
            second_delay_for_check_sn_speed_ = vm["config.sdss"].as<uint32_t>();
            http_protect_time2_when_start_ = vm["config.hpt2"].as<uint32_t>();
            http_protect_time2_when_start_in_saving_mode_ = vm["config.hpt2save"].as<uint32_t>();
            http_download_bytes_delim_when_start_ = vm["config.hdbd"].as<uint32_t>();
            http_download_bytes_delim_when_start_in_saving_mode_ = vm["config.hdbdsave"].as<uint32_t>();

            if (save_to_disk)
            {
                SaveLocalConfig(config_string);
            }

            NotifyConfigUpdateEvent();

            config_string_length_ = config_string.length();
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