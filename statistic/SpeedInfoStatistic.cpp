//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "statistic/SpeedInfoStatistic.h"
#include "statistic/StatisticUtil.h"

namespace statistic
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_statistic = log4cplus::Logger::getInstance("[speed_info_statistic]");
#endif
    SpeedInfoStatistic::SpeedInfoStatistic()
        : is_running_(false)
    {

    }

    void SpeedInfoStatistic::Start()
    {
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "SpeedInfoStatistic::Start [IN]");
        if (is_running_ == true)
        {
            LOG4CPLUS_WARN_LOG(logger_statistic, "SpeedInfoStatistic is running, return.");
            return;
        }

        is_running_ = true;
        Clear();

        start_time_ = GetTickCountInMilliSecond();
        download_speed_meter_.Start();
        upload_speed_meter_.Start();

        LOG4CPLUS_DEBUG_LOG(logger_statistic, "SpeedInfoStatistic::Start [OUT]");
    }

    void SpeedInfoStatistic::Stop()
    {
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "SpeedInfoStatistic::Stop [IN]");
        if (is_running_ == false)
        {
            LOG4CPLUS_WARN_LOG(logger_statistic, "SpeedInfoStatistic is not running, return.");
            return;
        }

        Clear();

        download_speed_meter_.Stop();
        upload_speed_meter_.Stop();
        start_time_ = 0;

        is_running_ = false;
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "SpeedInfoStatistic::Stop [OUT]");
    }

    void SpeedInfoStatistic::Clear()
    {
        if (is_running_ == false)
            return;

        start_time_ = 0;
        download_speed_meter_.Clear();
        upload_speed_meter_.Clear();
    }

    bool SpeedInfoStatistic::IsRunning() const
    {
        return is_running_;
    }

    const SPEED_INFO & SpeedInfoStatistic::GetSpeedInfo()
    {
        if (is_running_ == false)
            return speed_info_;

        speed_info_.StartTime = start_time_;

        speed_info_.AvgDownloadSpeed = download_speed_meter_.AverageByteSpeed();
        speed_info_.AvgUploadSpeed = upload_speed_meter_.AverageByteSpeed();

        speed_info_.TotalDownloadBytes = download_speed_meter_.TotalBytes();
        speed_info_.TotalUploadBytes = upload_speed_meter_.TotalBytes();

        speed_info_.NowDownloadSpeed = download_speed_meter_.CurrentByteSpeed();
        speed_info_.NowUploadSpeed = upload_speed_meter_.CurrentByteSpeed();

        speed_info_.MinuteDownloadSpeed = download_speed_meter_.RecentMinuteByteSpeed();
        speed_info_.MinuteUploadSpeed = upload_speed_meter_.RecentMinuteByteSpeed();

        return speed_info_;
    }

    const SPEED_INFO_EX & SpeedInfoStatistic::GetSpeedInfoEx()
    {
        if (is_running_ == false)
            return speed_info_ex_;

        speed_info_ex_.StartTime = start_time_;

        speed_info_ex_.AvgDownloadSpeed = download_speed_meter_.AverageByteSpeed();
        speed_info_ex_.AvgUploadSpeed = upload_speed_meter_.AverageByteSpeed();

        speed_info_ex_.TotalDownloadBytes = download_speed_meter_.TotalBytes();
        speed_info_ex_.TotalUploadBytes = upload_speed_meter_.TotalBytes();

        speed_info_ex_.NowDownloadSpeed = download_speed_meter_.CurrentByteSpeed();
        speed_info_ex_.NowUploadSpeed = upload_speed_meter_.CurrentByteSpeed();

        speed_info_ex_.MinuteDownloadSpeed = download_speed_meter_.RecentMinuteByteSpeed();
        speed_info_ex_.MinuteUploadSpeed = upload_speed_meter_.RecentMinuteByteSpeed();

        speed_info_ex_.RecentDownloadSpeed = download_speed_meter_.RecentByteSpeed();
        speed_info_ex_.RecentUploadSpeed = upload_speed_meter_.RecentByteSpeed();

        speed_info_ex_.SecondDownloadSpeed = download_speed_meter_.SecondByteSpeed();
        speed_info_ex_.SecondUploadSpeed = upload_speed_meter_.SecondByteSpeed();

        return speed_info_ex_;
    }

    //////////////////////////////////////////////////////////////////////////
    // Online Time Info

    uint32_t SpeedInfoStatistic::GetElapsedTimeInMilliSeconds() const
    {
        return download_speed_meter_.GetElapsedTimeInMilliSeconds();
    }
}
