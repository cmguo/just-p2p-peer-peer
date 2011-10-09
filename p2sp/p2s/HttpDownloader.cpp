//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/p2s/HttpDownloader.h"
#include "p2sp/download/DownloadDriver.h"
#include "p2sp/p2s/HttpConnection.h"
#include "p2sp/p2s/HttpDetecter.h"
#include "p2sp/p2p/P2SPConfigs.h"
#include "statistic/DownloadDriverStatistic.h"

#define HTTP_DEBUG(s)    LOG(__DEBUG, "http", s)
#define HTTP_INFO(s)    LOG(__INFO, "http", s)
#define HTTP_EVENT(s)    LOG(__EVENT, "http", s)
#define HTTP_WARN(s)    LOG(__WARN, "http", s)
#define HTTP_ERROR(s)    LOG(__ERROR, "http", s)

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("http");

    HttpDownloader::HttpDownloader(
        boost::asio::io_service & io_svc,
        const protocol::UrlInfo& url_info,
        DownloadDriver::p download_driver,
        bool is_open_service)
        : io_svc_(io_svc)
        , url_info_(url_info)
        , is_to_get_header_(false)
        , is_open_service_(is_open_service)
        , download_driver_(download_driver)
        , http_speed_limiter_(1000)
    {
    }

    HttpDownloader::HttpDownloader(
        boost::asio::io_service & io_svc,
        const network::HttpRequest::p http_request_demo,
        const protocol::UrlInfo& url_info,
        DownloadDriver::p download_driver,
        bool is_to_get_header,
        bool is_open_service)
        : io_svc_(io_svc)
        , url_info_(url_info)
        , is_to_get_header_(is_to_get_header)
        , is_open_service_(is_open_service)
        , download_driver_(download_driver)
        , http_request_demo_(http_request_demo)
        , http_speed_limiter_(1000)
    {
    }

    void HttpDownloader::Start(bool is_support_start)
    {
        if (is_running_ == true)
            return;

        is_running_ = true;

        is_detecting_ = true;

        string refer(url_info_.refer_url_);
        assert(download_driver_->GetStatistic());

        statistic_ = download_driver_->GetStatistic()->AttachHttpDownloaderStatistic(url_info_.url_);

        assert(statistic_);
        statistic_->SetReferUrl(refer);
        statistic_->SetIsDeath(false);

        is_support_start_ = is_support_start;

        if (false == is_to_get_header_)
        {
            if (http_request_demo_)
                http_detecter_ = HttpDetecter::Create(io_svc_, http_request_demo_, shared_from_this());
            else
                http_detecter_ = HttpDetecter::Create(io_svc_, shared_from_this());

            http_detecter_->Start();
        }

        if (http_request_demo_)
            http_connection_ = HttpConnection::Create(io_svc_, http_request_demo_, shared_from_this(), url_info_, is_to_get_header_);
        else
            http_connection_ = HttpConnection::Create(io_svc_, shared_from_this(), url_info_);

        if (is_original_)
        {
            http_connection_->Start(is_support_start, is_open_service_, download_driver_->GetOpenServiceHeadLength());
        }
        else
        {
            if (false == is_to_get_header_ && false == is_open_service_)
            {
                is_detecting_ = http_detecter_->DoDetect(http_connection_, url_info_);
            }
        }
    }

    void HttpDownloader::Stop()
    {
        if (is_running_ == false)
            return;

        if (http_detecter_)
        {
            http_detecter_->Stop();
            http_detecter_.reset();
        }
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

    bool HttpDownloader::IsSupportRange()
    {
        if (false == is_running_)
            return false;
        return http_connection_->IsSupportRange();
    }

    void HttpDownloader::DoDetecter(HttpConnection::p http_connection, protocol::UrlInfo url_info)
    {
        if (is_running_ == false)
            return;

        HTTP_EVENT("HttpDownloader::DoDetecter url:" << url_info.url_);
        if (false == is_to_get_header_)
        {
            is_detecting_ = http_detecter_->DoDetect(http_connection, url_info);
        }
    }

    void HttpDownloader::DetectorReport(HttpConnection__p http_connection, bool is_support_range)
    {
        if (is_running_ == false)
            return;
        HTTP_EVENT("HttpDownloader::DetectorReport is_support_range:" << is_support_range);
        if (is_original_)
        {
            http_connection->DetectorReport(is_support_range);
        }
        else
        {
            if (is_support_range)
            {
                http_connection_->Start(is_support_start_);
                http_connection_->DetectorReport(is_support_range);
            }
            else
            {
                download_driver_->SetDownloaderToDeath(shared_from_this());
            }
        }
        is_detecting_ = false;
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

    bool HttpDownloader::IsConnected()
    {
        if (is_running_ == false)
            return false;

        return http_connection_->IsConnected();
    }

    void HttpDownloader::StopPausing()
    {
        if (false == is_running_)
        {
            return;
        }

        if (true == http_connection_->IsPausing())
        {
            http_connection_->StopPausing();
        }
    }

    void HttpDownloader::SetPausing()
    {
        if (false == is_running_)
        {
            return;
        }

        if (false == http_connection_->IsPausing())
        {
            http_connection_->SetPausing();
        }
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
        SetPausing();
    }
    void HttpDownloader::Resume()
    {
        statistic_->SetIsPause(false);
        StopPausing();
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
    uint32_t HttpDownloader::GetRecentDownloadSpeed()
    {
        if (false == is_running_)
            return 0;
        return GetSpeedInfoEx().RecentDownloadSpeed;
    }
    uint32_t HttpDownloader::GetMinuteDownloadSpeed()
    {
        if (false == is_running_)
            return 0;
        return GetSpeedInfoEx().MinuteDownloadSpeed;
    }

    bool HttpDownloader::IsDetecting()
    {
        if (false == is_running_)
            return false;
        return is_detecting_;
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
}
