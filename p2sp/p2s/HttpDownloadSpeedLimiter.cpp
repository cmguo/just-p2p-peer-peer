//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "p2sp/p2s/HttpDownloadSpeedLimiter.h"
#include "p2sp/AppModule.h"

#define HTTPDOWNLIMITER_DEBUG(msg) LOG(__DEBUG, "http_speed_limiter", __FUNCTION__ << " " << msg)

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("http_speed_limiter");

    const uint32_t DOWNLIMIT_MIN_INTERVAL_IN_MS = 1000;

    HttpDownloadSpeedLimiter::HttpDownloadSpeedLimiter(boost::uint32_t max_data_queue_length)
        : max_data_queue_length_(max_data_queue_length)
        , speed_limit_in_KBps_(-1)
        , packet_send_count_per_tick_(-1)
        , tick_timer_(global_second_timer(), DOWNLIMIT_MIN_INTERVAL_IN_MS, boost::bind(&HttpDownloadSpeedLimiter::OnTimerElapsed, this, &tick_timer_))
    {
        LIMIT_MIN_MAX(max_data_queue_length, 100, 1000);  // 100K - 1M
        tick_timer_.start();
    }

    HttpDownloadSpeedLimiter::~HttpDownloadSpeedLimiter()
    {
        tick_timer_.stop();
        data_queue_.clear();
    }

    void HttpDownloadSpeedLimiter::OnTimerElapsed(framework::timer::Timer * pointer)
    {
        HTTPDOWNLIMITER_DEBUG("pointer " << pointer << ", times = " << pointer->times());
        if (pointer == &tick_timer_)
        {
            HTTPDOWNLIMITER_DEBUG("data_queue_.size() = " << data_queue_.size());
            for (packet_send_count_per_tick_ = 0; packet_send_count_per_tick_ < packet_number_cur_tick_ && !data_queue_.empty();)
            {
                network::HttpClient<protocol::SubPieceContent>::p http_client = data_queue_.front()->GetHttpClient();
                if (http_client && !http_client->IsRequesting())
                {
                    HTTPDOWNLIMITER_DEBUG("http_client->HttpRecvSubPiece");
                    http_client->HttpRecvSubPiece();
                }
                ++packet_send_count_per_tick_;
                data_queue_.pop_front();
            }
        }
    }

    void HttpDownloadSpeedLimiter::DoRequestSubPiece(HttpConnection::p http_connection)
    {
        network::HttpClient<protocol::SubPieceContent>::p http_client = http_connection->GetHttpClient();

        if (!http_client)
        {
            HTTPDOWNLIMITER_DEBUG("HttpClient is Null!");
            return;
        }

        if (GetSpeedLimitInKBps() < 0)
        {
            // 不限速
            HTTPDOWNLIMITER_DEBUG("unlimit Speed");
            http_client->HttpRecvSubPiece();
            return;
        }
        else
        {
            // 限速
            if (packet_send_count_per_tick_ < packet_number_cur_tick_)
            {
                http_client->HttpRecvSubPiece();
                ++packet_send_count_per_tick_;
            }
            else
            {
                HTTPDOWNLIMITER_DEBUG("push in the queue!");
                data_queue_.push_back(http_connection);
            }
        }
    }

    void HttpDownloadSpeedLimiter::SetSpeedLimitInKBps(boost::int32_t speed_limit_in_KBps)
    {
        if (speed_limit_in_KBps == speed_limit_in_KBps_)
        {
            return;
        }

        speed_limit_in_KBps_ = speed_limit_in_KBps;

        packet_number_cur_tick_ = speed_limit_in_KBps_ * DOWNLIMIT_MIN_INTERVAL_IN_MS / 1000;
    }

    boost::int32_t HttpDownloadSpeedLimiter::GetSpeedLimitInKBps() const
    {
        return speed_limit_in_KBps_;
    }
}
