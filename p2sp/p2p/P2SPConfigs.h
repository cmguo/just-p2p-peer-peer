//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// P2SPConfigs.h

#ifndef _P2SP_P2P_P2SP_CONFIGS_H_
#define _P2SP_P2P_P2SP_CONFIGS_H_

namespace p2sp
{
    class P2SPConfigs
    {
    public:
        static void LoadConfig();
    public:

        //////////////////////////////////////////////////////////////////////////
        //

        //
        static uint32_t EXCHANGE_PROTECT_TIME_IN_MILLISEC;
        //
        static uint32_t CONNECT_PROTECT_TIME_IN_MILLISEC;
        //
        static uint32_t CONNECT_INITIAL_PROTECT_TIME_IN_MILLISEC;
        //
        static uint32_t CONNECT_FINAL_PROTECT_TIME_IN_MILLISEC;

        //
        static uint32_t ASSIGN_SUBPIECE_COUNT_PER_PEER;
        //
        static uint32_t ASSIGN_SUBPIECE_MAX_COUNT_PER_PEER;
        //
        static uint32_t ASSIGN_MAX_SUBPIECE_TIMEOUT_IN_MILLISEC;
        //
        static uint32_t ASSIGN_RESEND_PIECE_DISTANCE_A;
        //
        static uint32_t ASSIGN_RESEND_PIECE_DISTANCE_B;
        //
        // static boost::uint32_t ASSIGN_RESEND_REST_COUNT;
        //
        // static boost::uint32_t ASSIGN_RESEND_DETECT_COUNT;

        //
        static uint32_t ASSIGN_REDUNTANT_DECISION_SIZE;
        //
        static uint32_t ASSIGN_REDUNTANT_DECISION_SIZE_INTERVAL;
        //
        static uint32_t ASSIGN_REDUNTANT_DECISION_TIMEOUT;
        //
        static uint32_t ASSIGN_REDUNTANT_PIECE_COUNT;
        //
        static uint32_t ASSIGN_REDUNTANT_TIMES_NORMAL;
        //
        static uint32_t ASSIGN_REDUNTANT_TIMES_URGENT;

        //
        static boost::uint32_t ASSIGN_MAX_SUBPIECE_COUNT;
        //
        static uint32_t ASSIGN_CONTINUOUS_REDUNTANT_DECISION_SIZE;
        //
        static uint32_t ASSIGN_CONTINUOUS_REDUNTANT_DECISION_TIMEOUT;

        //
        static uint32_t HTTP_DOWNLOAD_SPEED_LIMIT;
        //
        static uint32_t HTTP_DOWNLOAD_SPEED_LIMIT_ENABEL;

        //
        static uint32_t P2P_DOWNLOAD_CLOSE_TIME_IN_MILLISEC;
        //
        static uint32_t P2P_DOWNLOAD_INIT_MAX_CONNECT_COUNT_PER_SEC;
        //
        static uint32_t P2P_DOWNLOAD_INIT_TIMEOUT;
        //
        static uint32_t P2P_DOWNLOAD_MAX_CONNECT_COUNT;
        //
        static uint32_t P2P_DOWNLOAD_MIN_CONNECT_COUNT;
        //
        static uint32_t P2P_DOWNLOAD_MAX_SCHEDULE_COUNT;
        //
        static uint32_t P2P_DOWNLOAD_MAX_CONNECT_COUNT_PER_SEC;

        //
        static uint32_t P2P_DOWNLOAD_NEED_INCREASE_MAX_POOLED_PEER_COUNT;
        //
        static uint32_t P2P_DOWNLOAD_NEED_INCREASE_MAX_CONNECTED_PEER_COUNT;

        //
        static uint32_t P2P_DOWNLOAD_MULTI_SUBPIECES_COUNT;
        //
        static uint32_t P2P_DOWNLOAD_MULTI_SUBPIECES_REQUEST_COUNT;
        //
        static uint32_t P2P_DOWNLOAD_SPEED_LIMIT;
        //
        static uint32_t P2P_DOWNLOAD_SPEED_LIMIT_TEST;
        //
        static uint32_t P2P_DOWNLOAD_SPEED_LIMIT_ENABEL;

        //
        static uint32_t P2P_MAX_EXCHANGE_PEER_COUNT;
        //
        static uint32_t P2P_MAX_IPPOOL_PEER_COUNT;
        //
        static uint32_t P2P_MAX_TOTAL_WINDOW_SIZE;
        //
        static uint32_t P2P_MIN_TOTAL_WINDOW_SIZE;

        //
        static uint32_t PEERCONNECTION_CAN_KICK_TIME_IN_MILLISEC;
        //
        static uint32_t PEERCONNECTION_CAN_KICK_SPEED_IN_BPS;
        //
        static uint32_t PEERCONNECTION_NORESPONSE_KICK_TIME_IN_MILLISEC;
        //
        static uint32_t PEERCONNECTION_MAX_WINDOW_SIZE;
        //
        static uint32_t PEERCONNECTION_MIN_WINDOW_SIZE;
        //
        static uint32_t PEERCONNECTION_INIT_MAX_WINDOW_SIZE;
        //
        static uint32_t PEERCONNECTION_MIN_REQUEST_SIZE;
        //
        static uint32_t PEERCONNECTION_MAX_REQUEST_SIZE;
        //
        static uint32_t PEERCONNECTION_RTT_MODIFICATION_TIME_IN_MILLISEC;
        //
        static uint32_t PEERCONNECTION_LONGEST_RTT_MODIFICATION_TIME_IN_MILLISEC;
        //
        static uint32_t PEERCONNECTION_RTT_RANGE_SIZE;
        //
        static uint32_t PEERCONNECTION_MIN_PACKET_COPY_COUNT;
        //
        static uint32_t PEERCONNECTION_MIN_DELTA_SIZE;

        //
        static uint32_t PIECE_TIME_OUT_IN_MILLISEC;

        //
        static uint32_t UPLOAD_LIMITER_MAX_DATA_QUEUE_SIZE;
        //
        static uint32_t UPLOAD_LIMITER_MAX_PACKET_LIFE_TIME_IN_MILLISEC;
        //
        static uint32_t UPLOAD_MAX_CACHE_LENGTH;
        //
        static uint32_t UPLOAD_CACHE_BLOCK_EXPIRE_TIME_IN_MILLISEC;
        //
        static uint32_t UPLOAD_MAX_CONNECT_PEER_COUNT;
        //
        static uint32_t UPLOAD_MAX_UPLOAD_PEER_COUNT;
        //
        static uint32_t UPLOAD_MIN_UPLOAD_BANDWIDTH;
        //
        static uint32_t UPLOAD_BOOL_CONTROL_MODE;
        //
        static uint32_t UPLOAD_SPEED_LIMIT;

        //
        static uint32_t SWITCH_CONTROLLER_SIMPLE_MAX_P2P_DOWNLOAD_TIME_IN_SEC;
        //
        static uint32_t SWITCH_CONTROLLER_DRAG_HTTP_DOWNLOAD_DURATION;

        //
        static uint32_t PUSH_MAX_DOWNLOAD_SPEED_WHEN_IDLE_IN_KBPS;
        //
        static uint32_t PUSH_MAX_DOWNLOAD_SPEED_WHEN_NORMAL_IN_KBPS;
        //
        static uint32_t PUSH_MIN_DOWNLOAD_SPEED_IN_KBPS;
        //
        static uint32_t PUSH_IDLE_TIME_IN_SEC;
        //
        static uint32_t PUSH_AWAY_TIME_IN_SEC;
        //
        static uint32_t PUSH_PROTECTION_INTERVAL_IN_SEC;
        //
        static uint32_t PUSH_BANDWIDTH_RATIO_WHEN_IDLE;
        //
        static uint32_t PUSH_BANDWIDTH_RATIO_WHEN_NORMAL;

    };
}
#endif  // _P2SP_P2P_P2SP_CONFIGS_H_
