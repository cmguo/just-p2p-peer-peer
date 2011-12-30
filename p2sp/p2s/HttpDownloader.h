//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// HttpDownloader.h

#ifndef _P2SP_P2S_HTTP_DOWNLOADER_H_
#define _P2SP_P2S_HTTP_DOWNLOADER_H_

#include "p2sp/download/Downloader.h"
#include "p2sp/download/SwitchController.h"
#include "statistic/HttpDownloaderStatistic.h"
#include "statistic/StatisticStructs.h"
#include "p2sp/p2s/HttpDownloadSpeedLimiter.h"

namespace p2sp
{
    class HttpConnection;
    typedef boost::shared_ptr<HttpConnection> HttpConnection__p;
    class HttpDetecter;
    typedef boost::shared_ptr<HttpDetecter> HttpDetecter__p;
    class DownloadDriver;
    typedef boost::shared_ptr<DownloadDriver> DownloadDriver__p;
    class HttpDownloadSpeedLimiter;
    typedef boost::shared_ptr<HttpDownloadSpeedLimiter> HttpDownloadSpeedLimiter__p;

    class HttpDownloader
        : public VodDownloader
        , public IHTTPControlTarget
        , public boost::enable_shared_from_this<HttpDownloader>
#ifdef DUMP_OBJECT
        , public count_object_allocate<HttpDownloader>
#endif
    {
    public:
        typedef boost::shared_ptr<HttpDownloader> p;
        static p Create(
            boost::asio::io_service & io_svc, const protocol::UrlInfo& url_info,
            DownloadDriver__p download_driver, bool is_open_service, bool is_head_only)
        {
            return p(new HttpDownloader(io_svc, url_info, download_driver, is_open_service, is_head_only));
        }

        static p Create(
            boost::asio::io_service & io_svc, const network::HttpRequest::p http_request_demo,
            const protocol::UrlInfo& url_info, DownloadDriver__p download_driver,
            bool is_to_get_header, bool is_open_service, bool is_head_only)
        {
            return p(new HttpDownloader(io_svc, http_request_demo, url_info, download_driver,
                is_to_get_header, is_open_service, is_head_only));
        }

        virtual void Start(bool is_support_start);
        virtual void Stop();

        virtual ~HttpDownloader();

    public:

        virtual void PutPieceTask(const std::deque<protocol::PieceInfoEx> & piece_info_ex_s, DownloadDriver__p downloader_driver);
        virtual bool IsConnected();
        virtual bool GetUrlInfo(protocol::UrlInfo& url_info) { url_info = url_info_; return true;}
        virtual bool IsSupportRange();
        virtual bool HasPieceTask() const { assert(0); return false; }

        virtual void HttpConnectComplete(HttpConnection__p http_connection);

        // HttpConnection下达命令做detecter
        virtual void DoDetecter(HttpConnection__p http_connection, protocol::UrlInfo url_info);
        virtual void DetectorReport(HttpConnection__p http_connection, bool is_support_range);

        virtual void StopPausing();
        virtual void SetPausing();
        virtual bool IsPausing();

        virtual void OnPieceTimeout(DownloadDriver__p download_driver_, const protocol::PieceInfoEx & piece);
        virtual boost::int32_t GetPieceTaskNum();
        DownloadDriver__p GetDownloadDriver() const {return download_driver_;}
        statistic::HttpDownloaderStatistic::p GetStatistics() const {return statistic_;}

        virtual statistic::SPEED_INFO GetSpeedInfo()
        {
            if (false == is_running_)
                return statistic::SPEED_INFO();
            assert(statistic_);
            return statistic_->GetSpeedInfo();
        }

        virtual statistic::SPEED_INFO_EX GetSpeedInfoEx()
        {
            if (false == is_running_)
                return statistic::SPEED_INFO_EX();
            assert(statistic_);
            return statistic_->GetSpeedInfoEx();
        }

        // notice multi connection
        virtual void SetSpeedLimitInKBps(boost::int32_t speed_limit_in_KBps);

        virtual void DoRequestSubPiece(HttpConnection__p http_connection);

        void OnSecondTimer();

        boost::uint32_t GetDownloadingTimeInSeconds() const {return downloading_time_in_seconds_;}

        void SubmitHttpDownloadBytesInConnection(boost::uint32_t bytes);

        boost::uint32_t GetHttpAvgDownloadBytes();

    public:
        //////////////////////////////////////////////////////////////////////////
        // IHTTPControlTarget

        virtual void Pause();
        virtual void Resume();

        virtual boost::uint32_t GetSecondDownloadSpeed();
        virtual boost::uint32_t GetCurrentDownloadSpeed();
        virtual boost::uint32_t GetRecentDownloadSpeed();
        virtual boost::uint32_t GetMinuteDownloadSpeed();
        virtual bool IsDetecting();
        // virtual bool IsSupportRange();

    protected:
        boost::asio::io_service & io_svc_;

        protocol::UrlInfo url_info_;
        bool is_support_start_;
        bool is_to_get_header_;
        bool is_need_detect_;
        bool is_detecting_;
        bool is_open_service_;

        HttpConnection__p http_connection_;
        HttpDetecter__p http_detecter_;
        DownloadDriver__p download_driver_;
        statistic::HttpDownloaderStatistic::p statistic_;
        network::HttpRequest::p http_request_demo_;
        HttpDownloadSpeedLimiter http_speed_limiter_;

        boost::uint32_t downloading_time_in_seconds_;

        std::deque<boost::uint32_t> http_download_bytes_deque_;

        bool is_head_only_;

    protected:

        HttpDownloader(
            boost::asio::io_service & io_svc,
            const protocol::UrlInfo& url_info,
            DownloadDriver__p download_driver,
            bool is_open_service,
            bool is_head_only);

        HttpDownloader(
            boost::asio::io_service & io_svc,
            const network::HttpRequest::p http_request_demo,
            const protocol::UrlInfo& url_info,
            DownloadDriver__p download_driver,
            bool is_to_get_header,
            bool is_open_service,
            bool is_head_only);

    };
}

#endif  // _P2SP_P2S_HTTP_DOWNLOADER_H_
