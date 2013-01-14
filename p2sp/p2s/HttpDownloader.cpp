//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/p2s/HttpDownloader.h"
#include "p2sp/download/DownloadDriver.h"
#include "p2sp/p2s/HttpConnection.h"
#include "p2sp/p2p/P2SPConfigs.h"
#include "statistic/DownloadDriverStatistic.h"

namespace p2sp
{
    HttpDownloader::HttpDownloader(
        boost::asio::io_service & io_svc,
        const protocol::UrlInfo& url_info,
        DownloadDriver::p download_driver,
        bool is_open_service,
        bool is_head_only)
        : io_svc_(io_svc)
        , url_info_(url_info)
        , is_to_get_header_(false)
        , is_open_service_(is_open_service)
        , download_driver_(download_driver)
        , http_speed_limiter_(1000)
        , downloading_time_in_seconds_(0)
        , is_head_only_(is_head_only)
    {
    }

    HttpDownloader::HttpDownloader(
        boost::asio::io_service & io_svc,
        const network::HttpRequest::p http_request_demo,
        const protocol::UrlInfo& url_info,
        DownloadDriver::p download_driver,
        bool is_to_get_header,
        bool is_open_service,
        bool is_head_only)
        : io_svc_(io_svc)
        , url_info_(url_info)
        , is_to_get_header_(is_to_get_header)
        , is_open_service_(is_open_service)
        , download_driver_(download_driver)
        , http_request_demo_(http_request_demo)
        , http_speed_limiter_(1000)
        , downloading_time_in_seconds_(0)
        , is_head_only_(is_head_only)
    {
    }

    HttpDownloader::~HttpDownloader()
    {
    }

    void HttpDownloader::Start(bool is_support_start)
    {
        if (is_running_ == true)
            return;

        is_running_ = true;

        string refer(url_info_.refer_url_);
        assert(download_driver_->GetStatistic());

        statistic_ = download_driver_->GetStatistic()->AttachHttpDownloaderStatistic(url_info_.url_);

        assert(statistic_);
        statistic_->SetReferUrl(refer);
        statistic_->SetIsDeath(false);

        is_support_start_ = is_support_start;

        if (http_request_demo_)
            http_connection_ = HttpConnection::Create(io_svc_, http_request_demo_, shared_from_this(), url_info_, is_to_get_header_, is_head_only_);
        else
            http_connection_ = HttpConnection::Create(io_svc_, shared_from_this(), url_info_, is_head_only_);

        if (is_original_)
        {
            http_connection_->Start(is_support_start, is_open_service_, download_driver_->GetOpenServiceHeadLength());
        }
    }

    void HttpDownloader::Stop()
    {
        if (is_running_ == false)
            return;

        if (http_connection_)
        {
            http_connection_->Stop();
            http_connection_.reset();
        }

        assert(download_driver_->GetStatistic());
        assert(statistic_);
        download_driver_->GetStatistic()->DetachHttpDownloaderStatistic(statistic_);
        statistic_.reset();

        if (download_driver_)
        {
            download_driver_.reset();
        }
        if (http_request_demo_)
        {
            http_request_demo_.reset();
        }

        is_running_ = false;
    }

    void HttpDownloader::HttpConnectComplete(HttpConnection::p http_connection)
    {
        if (is_running_ == false)
            return;
        assert(http_connection == http_connection_);
        http_connection_->Stop();
    }

    void HttpDownloader::PutPieceTask(const std::deque<protocol::PieceInfoEx> & piece_info_ex_s, DownloadDriver__p downloader_driver)
    {
        if (is_running_ == false)
            return;

        http_connection_->PutPieceTask(piece_info_ex_s);
    }

    bool HttpDownloader::IsPausing()
    {
        if (false == is_running_)
        {
            return false;
        }

        return http_connection_->IsPausing();
    }

    //////////////////////////////////////////////////////////////////////////
    // IHTTPControlTarget

    void HttpDownloader::Pause()
    {
        statistic_->SetIsPause(true);
        if (false == is_running_)
        {
            return;
        }

        if (false == http_connection_->IsPausing())
        {
            http_connection_->Pause();
        }
    }

    void HttpDownloader::Resume()
    {
        statistic_->SetIsPause(false);

        if (false == is_running_)
        {
            return;
        }

        if (true == http_connection_->IsPausing())
        {
            http_connection_->Resume();
        }
    }

    boost::uint32_t HttpDownloader::GetSecondDownloadSpeed()
    {
        if (false == is_running_)
            return 0;

        return GetSpeedInfoEx().SecondDownloadSpeed;
    }
    boost::uint32_t HttpDownloader::GetCurrentDownloadSpeed()
    {
        if (false == is_running_)
            return 0;

        return GetSpeedInfoEx().NowDownloadSpeed;
    }
    boost::uint32_t HttpDownloader::GetRecentDownloadSpeed()
    {
        if (false == is_running_)
            return 0;
        return GetSpeedInfoEx().RecentDownloadSpeed;
    }
    boost::uint32_t HttpDownloader::GetMinuteDownloadSpeed()
    {
        if (false == is_running_)
            return 0;
        return GetSpeedInfoEx().MinuteDownloadSpeed;
    }

    void HttpDownloader::SetSpeedLimitInKBps(boost::int32_t speed_limit_in_KBps)
    {
        if (false == is_running_) {
            return;
        }

        if (P2SPConfigs::HTTP_DOWNLOAD_SPEED_LIMIT_ENABEL)
        {
            http_speed_limiter_.SetSpeedLimitInKBps(P2SPConfigs::HTTP_DOWNLOAD_SPEED_LIMIT);
            return;
        }

        http_speed_limiter_.SetSpeedLimitInKBps(speed_limit_in_KBps);
    }

    void HttpDownloader::DoRequestSubPiece(HttpConnection::p http_connection)
    {
        if (!is_running_)
        {
            return;
        }

        http_speed_limiter_.DoRequestSubPiece(http_connection);
    }

    void HttpDownloader::OnPieceTimeout(DownloadDriver__p download_driver_, const protocol::PieceInfoEx & piece)
    {
        if (http_connection_)
            http_connection_->PieceTimeout();
    }

    boost::int32_t HttpDownloader::GetPieceTaskNum()
    {
        return http_connection_->GetPieceTaskNum();
    }

    void HttpDownloader::OnSecondTimer()
    {
        if (!http_connection_->IsPausing())
        {
            downloading_time_in_seconds_++;
        }
    }

    void HttpDownloader::SubmitHttpDownloadBytesInConnection(boost::uint32_t bytes)
    {
        http_download_bytes_deque_.push_back(bytes);
    }

    boost::uint32_t HttpDownloader::GetHttpAvgDownloadBytes()
    {
        // 特殊处理最后一次的情况
        boost::uint32_t bytes = http_connection_->GetDownloadByteInConnection();

        if (bytes != 0)
        {
            SubmitHttpDownloadBytesInConnection(bytes);  
        }

        if (!http_download_bytes_deque_.empty())
        {
            boost::uint32_t total_bytes = 0;
            for (std::deque<boost::uint32_t>::const_iterator iter = http_download_bytes_deque_.begin();
                iter != http_download_bytes_deque_.end(); ++iter)
            {
                total_bytes += *iter;
            }

            return total_bytes / http_download_bytes_deque_.size();
        }

        return 0;
    }
}