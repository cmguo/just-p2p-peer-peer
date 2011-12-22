//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/p2p/P2SPConfigs.h"

#if defined(NEED_LOG) || defined(LOG_MONITOR)
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include "boost/algorithm/string.hpp"
#include "boost/lexical_cast.hpp"
#endif  // NEED_LOG || LOG_MONITOR

namespace p2sp
{
    //////////////////////////////////////////////////////////////////////////
    // Load

    void P2SPConfigs::LoadConfig()
    {
        // 在Release版中屏蔽参数设置, ReleaseLog版中可用
#if defined(NEED_LOG) || defined(LOG_MONITOR)
        namespace fs = boost::filesystem;
        fs::path config_path("p2sp_config.txt");
        fs::ifstream fin(config_path.file_string());
        if (fin)
        {
            string line;
            while (std::getline(fin, line))
            {
                boost::algorithm::trim(line);
                if (line.empty() || line[0] == '#')
                    continue;
                uint32_t index = line.find_first_of('=');
                if (index != string::npos)
                {
                    string key = boost::algorithm::trim_copy(line.substr(0, index));
                    string value = boost::algorithm::trim_copy(line.substr(index + 1));
                    try {
                        // assign
                        if (key == "ASSIGN_SUBPIECE_COUNT_PER_PEER")
                            ASSIGN_SUBPIECE_COUNT_PER_PEER = boost::lexical_cast<uint32_t>(value);
                        else if (key == "ASSIGN_SUBPIECE_MAX_COUNT_PER_PEER")
                            ASSIGN_SUBPIECE_MAX_COUNT_PER_PEER = boost::lexical_cast<uint32_t>(value);
                        else if (key == "ASSIGN_MAX_SUBPIECE_TIMEOUT_IN_MILLISEC")
                            ASSIGN_MAX_SUBPIECE_TIMEOUT_IN_MILLISEC = boost::lexical_cast<uint32_t>(value);
                        else if (key == "ASSIGN_RESEND_PIECE_DISTANCE_A")
                            ASSIGN_RESEND_PIECE_DISTANCE_A = boost::lexical_cast<uint32_t>(value);
                        else if (key == "ASSIGN_RESEND_PIECE_DISTANCE_B")
                            ASSIGN_RESEND_PIECE_DISTANCE_B = boost::lexical_cast<uint32_t>(value);
                        else if (key == "ASSIGN_REDUNTANT_DECISION_SIZE")
                            ASSIGN_REDUNTANT_DECISION_SIZE = boost::lexical_cast<boost::uint32_t>(value);
                        else if (key == "ASSIGN_REDUNTANT_DECISION_SIZE_INTERVAL")
                            ASSIGN_REDUNTANT_DECISION_SIZE_INTERVAL = boost::lexical_cast<boost::uint32_t>(value);
                        else if (key == "ASSIGN_REDUNTANT_DECISION_TIMEOUT")
                            ASSIGN_REDUNTANT_DECISION_TIMEOUT = boost::lexical_cast<boost::uint32_t>(value);
                        else if (key == "ASSIGN_REDUNTANT_PIECE_COUNT")
                            ASSIGN_REDUNTANT_PIECE_COUNT = boost::lexical_cast<boost::uint32_t>(value);
                        else if (key == "ASSIGN_REDUNTANT_TIMES_NORMAL")
                            ASSIGN_REDUNTANT_TIMES_NORMAL = boost::lexical_cast<boost::uint32_t>(value);
                        else if (key == "ASSIGN_REDUNTANT_TIMES_URGENT")
                            ASSIGN_REDUNTANT_TIMES_URGENT = boost::lexical_cast<boost::uint32_t>(value);
                        else if (key == "ASSIGN_MAX_SUBPIECE_COUNT")
                            ASSIGN_MAX_SUBPIECE_COUNT = boost::lexical_cast<boost::uint32_t>(value);
                        else if (key == "ASSIGN_CONTINUOUS_REDUNTANT_DECISION_SIZE")
                            ASSIGN_CONTINUOUS_REDUNTANT_DECISION_SIZE = boost::lexical_cast<boost::uint32_t>(value);
                        else if (key == "ASSIGN_CONTINUOUS_REDUNTANT_DECISION_TIMEOUT")
                            ASSIGN_CONTINUOUS_REDUNTANT_DECISION_TIMEOUT = boost::lexical_cast<boost::uint32_t>(value);
                        // peer connection
                        else if (key == "PEERCONNECTION_MAX_WINDOW_SIZE")
                            PEERCONNECTION_MAX_WINDOW_SIZE = boost::lexical_cast<uint32_t>(value);
                        else if (key == "PEERCONNECTION_MIN_WINDOW_SIZE")
                            PEERCONNECTION_MIN_WINDOW_SIZE = boost::lexical_cast<uint32_t>(value);
                        else if (key == "PEERCONNECTION_INIT_MAX_WINDOW_SIZE")
                            PEERCONNECTION_INIT_MAX_WINDOW_SIZE = boost::lexical_cast<uint32_t>(value);
                        else if (key == "PEERCONNECTION_MIN_REQUEST_SIZE")
                                PEERCONNECTION_MIN_REQUEST_SIZE = boost::lexical_cast<uint32_t>(value);
                        else if (key == "PEERCONNECTION_MAX_REQUEST_SIZE")
                                PEERCONNECTION_MAX_REQUEST_SIZE = boost::lexical_cast<uint32_t>(value);
                        else if (key == "PEERCONNECTION_RTT_MODIFICATION_TIME_IN_MILLISEC")
                            PEERCONNECTION_RTT_MODIFICATION_TIME_IN_MILLISEC = boost::lexical_cast<uint32_t>(value);
                        else if (key == "PEERCONNECTION_LONGEST_RTT_MODIFICATION_TIME_IN_MILLISEC")
                            PEERCONNECTION_LONGEST_RTT_MODIFICATION_TIME_IN_MILLISEC = boost::lexical_cast<uint32_t>(value);
                        else if (key == "PEERCONNECTION_RTT_RANGE_SIZE")
                            PEERCONNECTION_RTT_RANGE_SIZE = boost::lexical_cast<uint32_t>(value);
                        else if (key == "PEERCONNECTION_CAN_KICK_TIME_IN_MILLISEC")
                            PEERCONNECTION_CAN_KICK_TIME_IN_MILLISEC = boost::lexical_cast<uint32_t>(value);
                        else if (key == "PEERCONNECTION_CAN_KICK_SPEED_IN_BPS")
                            PEERCONNECTION_CAN_KICK_SPEED_IN_BPS = boost::lexical_cast<uint32_t>(value);
                        else if (key == "PEERCONNECTION_MIN_PACKET_COPY_COUNT")
                            PEERCONNECTION_MIN_PACKET_COPY_COUNT = boost::lexical_cast<uint32_t>(value);
                        else if (key == "PEERCONNECTION_MIN_DELTA_SIZE")
                            PEERCONNECTION_MIN_DELTA_SIZE = boost::lexical_cast<uint32_t>(value);
                        // http
                        else if (key == "HTTP_DOWNLOAD_SPEED_LIMIT")
                            HTTP_DOWNLOAD_SPEED_LIMIT = boost::lexical_cast<uint32_t>(value);
                        else if (key == "HTTP_DOWNLOAD_SPEED_LIMIT_ENABEL")
                            HTTP_DOWNLOAD_SPEED_LIMIT_ENABEL = boost::lexical_cast<uint32_t>(value);
                        // p2p
                        else if (key == "P2P_MAX_TOTAL_WINDOW_SIZE")
                            P2P_MAX_TOTAL_WINDOW_SIZE = boost::lexical_cast<uint32_t>(value);
                        else if (key == "P2P_MIN_TOTAL_WINDOW_SIZE")
                            P2P_MIN_TOTAL_WINDOW_SIZE = boost::lexical_cast<uint32_t>(value);
                        // p2p download
                        else if (key == "P2P_DOWNLOAD_INIT_MAX_CONNECT_COUNT_PER_SEC")
                                P2P_DOWNLOAD_INIT_MAX_CONNECT_COUNT_PER_SEC = boost::lexical_cast<uint32_t>(value);
                        else if (key == "P2P_DOWNLOAD_INIT_TIMEOUT")
                                P2P_DOWNLOAD_INIT_TIMEOUT = boost::lexical_cast<uint32_t>(value);
                        else if (key == "P2P_DOWNLOAD_MAX_CONNECT_COUNT")
                            P2P_DOWNLOAD_MAX_CONNECT_COUNT = boost::lexical_cast<uint32_t>(value);
                        else if (key == "P2P_DOWNLOAD_MIN_CONNECT_COUNT")
                            P2P_DOWNLOAD_MIN_CONNECT_COUNT = boost::lexical_cast<uint32_t>(value);
                        else if (key == "P2P_DOWNLOAD_MAX_SCHEDULE_COUNT")
                            P2P_DOWNLOAD_MAX_SCHEDULE_COUNT = boost::lexical_cast<uint32_t>(value);
                        else if (key == "P2P_DOWNLOAD_NEED_INCREASE_MAX_POOLED_PEER_COUNT")
                            P2P_DOWNLOAD_NEED_INCREASE_MAX_POOLED_PEER_COUNT = boost::lexical_cast<uint32_t>(value);
                        else if (key == "P2P_DOWNLOAD_NEED_INCREASE_MAX_CONNECTED_PEER_COUNT")
                            P2P_DOWNLOAD_NEED_INCREASE_MAX_CONNECTED_PEER_COUNT = boost::lexical_cast<uint32_t>(value);
                        else if (key == "P2P_DOWNLOAD_MULTI_SUBPIECES_COUNT")
                            P2P_DOWNLOAD_MULTI_SUBPIECES_COUNT = boost::lexical_cast<uint32_t>(value);
                        else if (key == "P2P_DOWNLOAD_MULTI_SUBPIECES_REQUEST_COUNT")
                            P2P_DOWNLOAD_MULTI_SUBPIECES_REQUEST_COUNT = boost::lexical_cast<uint32_t>(value);
                        else if (key == "P2P_DOWNLOAD_SPEED_LIMIT")
                            P2P_DOWNLOAD_SPEED_LIMIT = boost::lexical_cast<uint32_t>(value);
                        else if (key == "P2P_DOWNLOAD_SPEED_LIMIT_TEST")
                            P2P_DOWNLOAD_SPEED_LIMIT_TEST = boost::lexical_cast<uint32_t>(value);
                        else if (key == "P2P_DOWNLOAD_SPEED_LIMIT_ENABEL")
                            P2P_DOWNLOAD_SPEED_LIMIT_ENABEL = boost::lexical_cast<uint32_t>(value);

                        // upload limiter
                        else if (key == "UPLOAD_LIMITER_MAX_DATA_QUEUE_SIZE")
                            UPLOAD_LIMITER_MAX_DATA_QUEUE_SIZE = boost::lexical_cast<uint32_t>(value);
                        else if (key == "UPLOAD_LIMITER_MAX_PACKET_LIFE_TIME_IN_MILLISEC")
                            UPLOAD_LIMITER_MAX_PACKET_LIFE_TIME_IN_MILLISEC = boost::lexical_cast<uint32_t>(value);
                        // upload
                        else if (key == "UPLOAD_MAX_CACHE_LENGTH")
                            UPLOAD_MAX_CACHE_LENGTH = boost::lexical_cast<uint32_t>(value);
                        else if (key == "UPLOAD_CACHE_BLOCK_EXPIRE_TIME_IN_MILLISEC")
                            UPLOAD_CACHE_BLOCK_EXPIRE_TIME_IN_MILLISEC = boost::lexical_cast<uint32_t>(value);
                        else if (key == "UPLOAD_MAX_CONNECT_PEER_COUNT")
                            UPLOAD_MAX_CONNECT_PEER_COUNT = boost::lexical_cast<uint32_t>(value);
                        else if (key == "UPLOAD_MAX_UPLOAD_PEER_COUNT")
                            UPLOAD_MAX_UPLOAD_PEER_COUNT = boost::lexical_cast<uint32_t>(value);
                        else if (key == "UPLOAD_MIN_UPLOAD_BANDWIDTH")
                            UPLOAD_MIN_UPLOAD_BANDWIDTH = boost::lexical_cast<uint32_t>(value);
                        else if (key == "UPLOAD_SPEED_LIMIT")
                            UPLOAD_SPEED_LIMIT = boost::lexical_cast<uint32_t>(value);
                        else if (key == "UPLOAD_SPEED_LIMIT_WHEN_URGENT")
                            UPLOAD_SPEED_LIMIT_WHEN_URGENT = boost::lexical_cast<uint32_t>(value);
                        else if (key == "UPLOAD_BOOL_CONTROL_MODE")
                            UPLOAD_BOOL_CONTROL_MODE = boost::lexical_cast<uint32_t>(value);

                        // controller
                        else if (key == "SWITCH_CONTROLLER_SIMPLE_MAX_P2P_DOWNLOAD_TIME_IN_SEC")
                            SWITCH_CONTROLLER_SIMPLE_MAX_P2P_DOWNLOAD_TIME_IN_SEC = boost::lexical_cast<uint32_t>(value);
                        else if (key == "SWITCH_CONTROLLER_DRAG_HTTP_DOWNLOAD_DURATION")
                            SWITCH_CONTROLLER_DRAG_HTTP_DOWNLOAD_DURATION = boost::lexical_cast<uint32_t>(value);
                        // push module
                        else if (key == "PUSH_MAX_DOWNLOAD_SPEED_WHEN_IDLE_IN_KBPS")
                            PUSH_MAX_DOWNLOAD_SPEED_WHEN_IDLE_IN_KBPS = boost::lexical_cast<uint32_t>(value);
                        else if (key == "PUSH_MAX_DOWNLOAD_SPEED_WHEN_NORMAL_IN_KBPS")
                            PUSH_MAX_DOWNLOAD_SPEED_WHEN_NORMAL_IN_KBPS = boost::lexical_cast<uint32_t>(value);
                        else if (key == "PUSH_MIN_DOWNLOAD_SPEED_IN_KBPS")
                            PUSH_MIN_DOWNLOAD_SPEED_IN_KBPS = boost::lexical_cast<uint32_t>(value);
                        else if (key == "PUSH_IDLE_TIME_IN_SEC")
                            PUSH_IDLE_TIME_IN_SEC = boost::lexical_cast<uint32_t>(value);
                        else if (key == "PUSH_AWAY_TIME_IN_SEC")
                            PUSH_AWAY_TIME_IN_SEC = boost::lexical_cast<uint32_t>(value);
                        else if (key == "PUSH_PROTECTION_INTERVAL_IN_SEC")
                            PUSH_PROTECTION_INTERVAL_IN_SEC = boost::lexical_cast<uint32_t>(value);
                        else if (key == "PUSH_BANDWIDTH_RATIO_WHEN_IDLE")
                            PUSH_BANDWIDTH_RATIO_WHEN_IDLE = boost::lexical_cast<uint32_t>(value);
                        else if (key == "PUSH_BANDWIDTH_RATIO_WHEN_NORMAL")
                            PUSH_BANDWIDTH_RATIO_WHEN_NORMAL = boost::lexical_cast<uint32_t>(value);
                    } catch(const boost::bad_lexical_cast&) {
                        // nothing
                    }
                }
            }
        }
#endif  // NEED_LOG
    }

    //////////////////////////////////////////////////////////////////////////
    // Default Values

    //
    uint32_t P2SPConfigs::EXCHANGE_PROTECT_TIME_IN_MILLISEC = 2 * 60 * 1000;
    //
    uint32_t P2SPConfigs::CONNECT_PROTECT_TIME_IN_MILLISEC = 1 * 60 * 1000;
    //
    uint32_t P2SPConfigs::CONNECT_INITIAL_PROTECT_TIME_IN_MILLISEC = 10 * 1000;
    //
    uint32_t P2SPConfigs::CONNECT_FINAL_PROTECT_TIME_IN_MILLISEC = 2 * 60 * 60 * 1000;

    //
    boost::uint32_t P2SPConfigs::ASSIGN_SUBPIECE_COUNT_PER_PEER = 40;
    //
    uint32_t P2SPConfigs::ASSIGN_SUBPIECE_MAX_COUNT_PER_PEER = 30;
    //
    uint32_t P2SPConfigs::ASSIGN_MAX_SUBPIECE_TIMEOUT_IN_MILLISEC = 9000;
    //
    uint32_t P2SPConfigs::ASSIGN_RESEND_PIECE_DISTANCE_A = 1;
    //
    uint32_t P2SPConfigs::ASSIGN_RESEND_PIECE_DISTANCE_B = 2;
    //
    // boost::uint32_t P2SPConfigs::ASSIGN_RESEND_REST_COUNT = 100;
    //
    // boost::uint32_t P2SPConfigs::ASSIGN_RESEND_DETECT_COUNT = 5;

    //
    uint32_t P2SPConfigs::ASSIGN_REDUNTANT_DECISION_SIZE = 10;
    //
    uint32_t P2SPConfigs::ASSIGN_REDUNTANT_DECISION_SIZE_INTERVAL = 2;
    //
    uint32_t P2SPConfigs::ASSIGN_REDUNTANT_DECISION_TIMEOUT = 400;
    //
    uint32_t P2SPConfigs::ASSIGN_REDUNTANT_PIECE_COUNT = 3;
    //
    uint32_t P2SPConfigs::ASSIGN_REDUNTANT_TIMES_NORMAL = 1;
    //
    uint32_t P2SPConfigs::ASSIGN_REDUNTANT_TIMES_URGENT = 3;

    //
    boost::uint32_t P2SPConfigs::ASSIGN_MAX_SUBPIECE_COUNT = 80;
    //
    uint32_t P2SPConfigs::ASSIGN_CONTINUOUS_REDUNTANT_DECISION_SIZE = 2;
    //
    uint32_t P2SPConfigs::ASSIGN_CONTINUOUS_REDUNTANT_DECISION_TIMEOUT = 800;

    //
    uint32_t P2SPConfigs::HTTP_DOWNLOAD_SPEED_LIMIT = 20;
    //
    uint32_t P2SPConfigs::HTTP_DOWNLOAD_SPEED_LIMIT_ENABEL = 0;

    //
    uint32_t P2SPConfigs::P2P_DOWNLOAD_CLOSE_TIME_IN_MILLISEC = 2 * 1000;
    //
    uint32_t P2SPConfigs::P2P_DOWNLOAD_INIT_MAX_CONNECT_COUNT_PER_SEC = 20;
    //
    uint32_t P2SPConfigs::P2P_DOWNLOAD_INIT_TIMEOUT = 1200;
    //
    uint32_t P2SPConfigs::P2P_DOWNLOAD_MAX_CONNECT_COUNT = 40;
    //
    uint32_t P2SPConfigs::P2P_DOWNLOAD_MIN_CONNECT_COUNT = 25;
    //
    uint32_t P2SPConfigs::P2P_DOWNLOAD_MAX_SCHEDULE_COUNT = 25;
    //
    uint32_t P2SPConfigs::P2P_DOWNLOAD_MAX_CONNECT_COUNT_PER_SEC = 20;
    //
    uint32_t P2SPConfigs::P2P_DOWNLOAD_NEED_INCREASE_MAX_POOLED_PEER_COUNT = 16;
    //
    uint32_t P2SPConfigs::P2P_DOWNLOAD_NEED_INCREASE_MAX_CONNECTED_PEER_COUNT = 8;
    //
    uint32_t P2SPConfigs::P2P_DOWNLOAD_MULTI_SUBPIECES_COUNT = 6;
    //
    uint32_t P2SPConfigs::P2P_DOWNLOAD_MULTI_SUBPIECES_REQUEST_COUNT = 3;

    //
    uint32_t P2SPConfigs::P2P_MAX_EXCHANGE_PEER_COUNT = 30;
    //
    uint32_t P2SPConfigs::P2P_MAX_IPPOOL_PEER_COUNT = 500;
    //
    uint32_t P2SPConfigs::P2P_MAX_TOTAL_WINDOW_SIZE = 400;
    //
    uint32_t P2SPConfigs::P2P_MIN_TOTAL_WINDOW_SIZE = 40;
    //
    uint32_t P2SPConfigs::P2P_DOWNLOAD_SPEED_LIMIT = 500;
    uint32_t P2SPConfigs::P2P_DOWNLOAD_SPEED_LIMIT_TEST = 500;
    //
    uint32_t P2SPConfigs::P2P_DOWNLOAD_SPEED_LIMIT_ENABEL = 0;

    //
    uint32_t P2SPConfigs::PEERCONNECTION_CAN_KICK_TIME_IN_MILLISEC = 6 * 1000;
    //
    uint32_t P2SPConfigs::PEERCONNECTION_CAN_KICK_SPEED_IN_BPS = 2048;
    //
    uint32_t P2SPConfigs::PEERCONNECTION_NORESPONSE_KICK_TIME_IN_MILLISEC = 60 * 1000;
    //
    uint32_t P2SPConfigs::PEERCONNECTION_MAX_WINDOW_SIZE = 25;
    //
    uint32_t P2SPConfigs::PEERCONNECTION_MIN_WINDOW_SIZE = 10;
    //
    uint32_t P2SPConfigs::PEERCONNECTION_INIT_MAX_WINDOW_SIZE = 5;
    //
    uint32_t P2SPConfigs::PEERCONNECTION_MIN_REQUEST_SIZE = 8;
    //
    uint32_t P2SPConfigs::PEERCONNECTION_MAX_REQUEST_SIZE = 48;
    //
    uint32_t P2SPConfigs::PEERCONNECTION_RTT_MODIFICATION_TIME_IN_MILLISEC = 200;
    //
    uint32_t P2SPConfigs::PEERCONNECTION_LONGEST_RTT_MODIFICATION_TIME_IN_MILLISEC = 300;
    //
    uint32_t P2SPConfigs::PEERCONNECTION_RTT_RANGE_SIZE = 60;
    //
    uint32_t P2SPConfigs::PEERCONNECTION_MIN_PACKET_COPY_COUNT = 1;
    //
    uint32_t P2SPConfigs::PEERCONNECTION_MIN_DELTA_SIZE = 2;

    //
    uint32_t P2SPConfigs::PIECE_TIME_OUT_IN_MILLISEC = 8 * 1000;

    //
    uint32_t P2SPConfigs::UPLOAD_LIMITER_MAX_DATA_QUEUE_SIZE = 400;
    //
    uint32_t P2SPConfigs::UPLOAD_LIMITER_MAX_PACKET_LIFE_TIME_IN_MILLISEC = 2500;
    //
    uint32_t P2SPConfigs::UPLOAD_MAX_CACHE_LENGTH = 10;
    //
    uint32_t P2SPConfigs::UPLOAD_CACHE_BLOCK_EXPIRE_TIME_IN_MILLISEC = 30 * 1000;
    //
    uint32_t P2SPConfigs::UPLOAD_MAX_CONNECT_PEER_COUNT = 25;
    //
    uint32_t P2SPConfigs::UPLOAD_MAX_UPLOAD_PEER_COUNT = 20;
    //
    uint32_t P2SPConfigs::UPLOAD_MIN_UPLOAD_BANDWIDTH = 64 * 1024;
    //
    uint32_t P2SPConfigs::UPLOAD_BOOL_CONTROL_MODE = 0;
    //
    uint32_t P2SPConfigs::UPLOAD_SPEED_LIMIT = 15 * 1024;
    //
    uint32_t P2SPConfigs::UPLOAD_SPEED_LIMIT_WHEN_URGENT = 10 * 1024;

    //
    uint32_t P2SPConfigs::SWITCH_CONTROLLER_SIMPLE_MAX_P2P_DOWNLOAD_TIME_IN_SEC = 10 * 60;
    //
    uint32_t P2SPConfigs::SWITCH_CONTROLLER_DRAG_HTTP_DOWNLOAD_DURATION = 20000;

    //
    uint32_t P2SPConfigs::PUSH_MAX_DOWNLOAD_SPEED_WHEN_IDLE_IN_KBPS = 200;
    //
    uint32_t P2SPConfigs::PUSH_MAX_DOWNLOAD_SPEED_WHEN_NORMAL_IN_KBPS = 100;
    //
    uint32_t P2SPConfigs::PUSH_MIN_DOWNLOAD_SPEED_IN_KBPS = 5;
    //
    uint32_t P2SPConfigs::PUSH_IDLE_TIME_IN_SEC = 5 * 60;
    //
    uint32_t P2SPConfigs::PUSH_AWAY_TIME_IN_SEC = 1 * 60;
    //
    uint32_t P2SPConfigs::PUSH_PROTECTION_INTERVAL_IN_SEC = 30 * 60;
    //
    uint32_t P2SPConfigs::PUSH_BANDWIDTH_RATIO_WHEN_IDLE = 170;  // 66%
    //
    uint32_t P2SPConfigs::PUSH_BANDWIDTH_RATIO_WHEN_NORMAL = 25;  // 10%
}