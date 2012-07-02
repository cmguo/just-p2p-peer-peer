//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "statistic/HttpDownloaderStatistic.h"
#include "statistic/StatisticUtil.h"
#include "statistic/StatisticStructs.h"
#include "statistic/SpeedInfoStatistic.h"
#include "statistic/DownloadDriverStatistic.h"

#include <framework/string/Url.h>

namespace statistic
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_statistic = log4cplus::Logger::getInstance("[http_downloader_statistic]");
#endif
    HttpDownloaderStatistic::HttpDownloaderStatistic(const string& url, DownloadDriverStatistic::p download_driver_statistic)
        : download_driver_statistic_(download_driver_statistic)
        , is_running_(false)
        , url_(url)
    {
    }

    HttpDownloaderStatistic::p HttpDownloaderStatistic::Create(const string& url, DownloadDriverStatistic::p download_driver_statistic)
    {
        return p(new HttpDownloaderStatistic(url, download_driver_statistic));
    }

    void HttpDownloaderStatistic::Start()
    {
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "HttpDownloaderStatistic::Start [IN]");
        if (is_running_ == true)
        {
            LOG4CPLUS_DEBUG_LOG(logger_statistic, "HttpDownloaderStatistic is running. Return.");
            return;
        }
        is_running_ = true;

        Clear();

        SetUrl(url_);

        http_downloader_info_.IsPause = true;

        speed_info_.Start();
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "Started Speed Info.");

        LOG4CPLUS_DEBUG_LOG(logger_statistic, "HttpDownloaderStatistic::Start [OUT]");
    }

    void HttpDownloaderStatistic::Stop()
    {
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "HttpDownloaderStatistic::Stop [IN]");
        if (is_running_ == false)
        {
            LOG4CPLUS_DEBUG_LOG(logger_statistic, "HttpDownloaderStatistic is not running. Return.");
            return;
        }

        speed_info_.Stop();
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "Stopped Speed Info.");

        Clear();

        is_running_ = false;
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "HttpDownloaderStatistic::Stop [OUT]");
    }

    void HttpDownloaderStatistic::Clear()
    {
        speed_info_.Clear();
        http_downloader_info_.Clear();
    }

    //////////////////////////////////////////////////////////////////////////
    // HTTP Downloader Info

    HTTP_DOWNLOADER_INFO HttpDownloaderStatistic::GetHttpDownloaderInfo()
    {
        UpdateSpeedInfo();
        return http_downloader_info_;
    }

    //////////////////////////////////////////////////////////////////////////
    // Speed Info

    SPEED_INFO HttpDownloaderStatistic::GetSpeedInfo()
    {
        UpdateSpeedInfo();
        return http_downloader_info_.SpeedInfo;
    }

    SPEED_INFO_EX HttpDownloaderStatistic::GetSpeedInfoEx()
    {
        return speed_info_.GetSpeedInfoEx();
    }

    void HttpDownloaderStatistic::SubmitDownloadedBytes(uint32_t downloaded_bytes)
    {
        speed_info_.SubmitDownloadedBytes(downloaded_bytes);
        if (download_driver_statistic_)
        {
            download_driver_statistic_->SubmitDownloadedBytes(downloaded_bytes);
        }
        else
        {
            LOG4CPLUS_WARN_LOG(logger_statistic, "Download Driver is NULL, HttpDownloaderStatisis: " << url_);
        }
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "HttpDownloaderStatistic::SubmitDownloadedBytes " << downloaded_bytes 
            << " Bytes.");
    }

    void HttpDownloaderStatistic::SubmitUploadedBytes(uint32_t uploaded_bytes)
    {
        speed_info_.SubmitUploadedBytes(uploaded_bytes);
        if (download_driver_statistic_)
        {
            download_driver_statistic_->SubmitUploadedBytes(uploaded_bytes);
        }
        else
        {
            LOG4CPLUS_WARN_LOG(logger_statistic, "Download Driver is NULL, HttpDownloaderStatisis: " << url_);
        }
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "HttpDownloaderStatistic::SubmitUploadedBytes " << uploaded_bytes 
            << " Bytes.");
    }

    void HttpDownloaderStatistic::UpdateSpeedInfo()
    {
        http_downloader_info_.SpeedInfo = speed_info_.GetSpeedInfo();
    }

    //////////////////////////////////////////////////////////////////////////
    // Events

    void HttpDownloaderStatistic::SubmitHttpConnected()
    {
        http_downloader_info_.LastConnectedTime = GetTickCountInMilliSecond();
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "HttpDownloaderStatistic::SubmitHttpConnected " 
            << http_downloader_info_.LastConnectedTime);
    }

    void HttpDownloaderStatistic::SubmitRequestPiece()
    {
        http_downloader_info_.LastRequestPieceTime = GetTickCountInMilliSecond();
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "HttpDownloaderStatistic::SubmitRequestPiece " 
            << http_downloader_info_.LastRequestPieceTime);
    }

    void HttpDownloaderStatistic::SubmitRetry()
    {
        http_downloader_info_.RetryCount++;
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "HttpDownloaderStatistic::SubmitRetry count: " 
            << http_downloader_info_.RetryCount);
    }

    void HttpDownloaderStatistic::ClearRetry()
    {
        http_downloader_info_.RetryCount = 0;
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "HttpDownloaderStatistic::ClearRetry count: " 
            << http_downloader_info_.RetryCount);
    }

    //////////////////////////////////////////////////////////////////////////
    // Status

    void HttpDownloaderStatistic::SetSupportRange(bool is_support_range)
    {
        http_downloader_info_.IsSupportRange = is_support_range;
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "HttpDownloaderStatistic::SetSupportRange " << is_support_range);
    }

    void HttpDownloaderStatistic::SetHttpStatusCode(uint32_t http_status_code)
    {
        http_downloader_info_.LastHttpStatusCode = http_status_code;
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "HttpDownloaderStatistic::SetHttpStatusCode " << http_status_code);
    }

    //////////////////////////////////////////////////////////////////////////
    // Url

    void HttpDownloaderStatistic::SetUrl(const string& url)
    {
        url_ = url;
        framework::string::Url::truncate_to(url_, http_downloader_info_.Url);
    }

    string HttpDownloaderStatistic::GetUrl() const
    {
        return url_;
    }

    void HttpDownloaderStatistic::SetReferUrl(const string& refer_url)
    {
        refer_url_ = refer_url;
        framework::string::Url::truncate_to(refer_url_, http_downloader_info_.ReferUrl);
    }

    string HttpDownloaderStatistic::GetReferUrl() const
    {
        return refer_url_;
    }

    void HttpDownloaderStatistic::SetRedirectUrl(const string& redirect_url)
    {
        redirect_url_ = redirect_url;
        framework::string::Url::truncate_to(redirect_url_, http_downloader_info_.RedirectUrl);
    }

    string HttpDownloaderStatistic::GetRedirectUrl() const
    {
        return redirect_url_;
    }

    //////////////////////////////////////////////////////////////////////////
    // PieceInfo

    void HttpDownloaderStatistic::SetDownloadingPieceInfo(const PIECE_INFO_EX & downloading_piece_info)
    {
        http_downloader_info_.DownloadingPieceEx = downloading_piece_info;
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "HttpDownloaderStatistic::SetDownloadingPieceInfo [" 
            << downloading_piece_info << "].");
    }

    void HttpDownloaderStatistic::SetStartPieceInfo(const PIECE_INFO_EX & start_piece_info)
    {
        http_downloader_info_.StartPieceEx = start_piece_info;
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "HttpDownloaderStatistic::SetStartPieceInfo [" 
            << start_piece_info << "].");
    }

    void HttpDownloaderStatistic::SetDownloadingPieceInfo(
        boost::uint16_t BlockIndex, boost::uint16_t PieceIndexInBlock, boost::uint16_t SubPieceIndexInPiece)
    {
        SetDownloadingPieceInfo(PIECE_INFO_EX(BlockIndex, PieceIndexInBlock, SubPieceIndexInPiece));
    }

    void HttpDownloaderStatistic::SetStartPieceInfo(
        boost::uint16_t BlockIndex, boost::uint16_t PieceIndexInBlock, boost::uint16_t SubPieceIndexInPiece)
    {
        SetStartPieceInfo(PIECE_INFO_EX(BlockIndex, PieceIndexInBlock, SubPieceIndexInPiece));
    }

    //////////////////////////////////////////////////////////////////////////
    // death

    void HttpDownloaderStatistic::SetIsDeath(bool is_death)
    {
        http_downloader_info_.IsDeath = is_death;
    }

    bool HttpDownloaderStatistic::IsDeath() const
    {
        return 0 != http_downloader_info_.IsDeath;
    }

    void HttpDownloaderStatistic::SetIsPause(bool is_pause)
    {
        http_downloader_info_.IsPause = is_pause;
    }
}
