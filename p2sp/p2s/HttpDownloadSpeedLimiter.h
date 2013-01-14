//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _HTTP_DOWNLOAD_SPEED_LIMITER_H_
#define _HTTP_DOWNLOAD_SPEED_LIMITER_H_

#include "p2sp/p2s/HttpConnection.h"

namespace p2sp
{
    class HttpDownloadSpeedLimiter
    {
    public:
        HttpDownloadSpeedLimiter(boost::uint32_t max_data_queue_length);

        ~HttpDownloadSpeedLimiter();

        void DoRequestSubPiece(HttpConnection::p http_connection);

        void SetSpeedLimitInKBps(boost::int32_t speed_limit_in_KBps);

        boost::int32_t GetSpeedLimitInKBps() const;

    private:

        void OnTimerElapsed(framework::timer::Timer * pointer);

    private:

        boost::uint32_t max_data_queue_length_;

        boost::int32_t speed_limit_in_KBps_;

        boost::uint32_t packet_number_cur_tick_;

        boost::uint32_t packet_send_count_per_tick_;

        framework::timer::PeriodicTimer tick_timer_;

        std::list<HttpConnection::p> data_queue_;
    };
}

#endif