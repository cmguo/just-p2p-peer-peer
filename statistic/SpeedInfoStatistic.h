//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// SpeedInfoStatistic.h

#ifndef _STATISTIC_SPEED_INFO_STATISTIC_H_
#define _STATISTIC_SPEED_INFO_STATISTIC_H_

#include "statistic/StatisticStructs.h"
#include "measure/ByteSpeedMeter.h"

namespace statistic
{
    using measure::ByteSpeedMeter;

    class SpeedInfoStatistic
    {
    public:

        SpeedInfoStatistic();

        void Start();

        void Stop();

        void Clear();

        bool IsRunning() const;

    public:

        //////////////////////////////////////////////////////////////////////////
        // Speed Info

        void SubmitDownloadedBytes(uint32_t downloaded_bytes);

        void SubmitUploadedBytes(uint32_t uploaded_bytes);

        const SPEED_INFO & GetSpeedInfo();

        const SPEED_INFO_EX & GetSpeedInfoEx();

        //////////////////////////////////////////////////////////////////////////
        // Online Time Info

        uint32_t GetElapsedTimeInMilliSeconds() const;

    private:

        volatile bool is_running_;

        SPEED_INFO speed_info_;

        SPEED_INFO_EX speed_info_ex_;

    private:

        measure::ByteSpeedMeter download_speed_meter_;

        measure::ByteSpeedMeter upload_speed_meter_;

        uint32_t start_time_;
    };

    inline void SpeedInfoStatistic::SubmitDownloadedBytes(uint32_t downloaded_bytes)
    {
        download_speed_meter_.SubmitBytes(downloaded_bytes);
    }

    inline void SpeedInfoStatistic::SubmitUploadedBytes(uint32_t uploaded_bytes)
    {
        upload_speed_meter_.SubmitBytes(uploaded_bytes);
    }
}

#endif  // _STATISTIC_SPEED_INFO_STATISTIC_H_
