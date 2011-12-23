//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// Download.h

#ifndef _P2SP_DOWNLOAD_DOWNLOADER_H_
#define _P2SP_DOWNLOAD_DOWNLOADER_H_

#include "statistic/StatisticStructs.h"
#include "network/HttpRequest.h"

namespace p2sp
{
    class DownloadDriver;
    typedef boost::shared_ptr<DownloadDriver> DownloadDriver__p;
    class HttpDownloader;
    typedef boost::shared_ptr<HttpDownloader> HttpDownloader__p;

    class Downloader
        : public boost::noncopyable
    {
    public:
        typedef boost::shared_ptr<Downloader> p;

        static HttpDownloader__p CreateByUrl(
            boost::asio::io_service & io_svc,
            const protocol::UrlInfo& url_info,
            DownloadDriver__p download_driver,
            bool is_open_service = false);

        static HttpDownloader__p CreateByUrl(
            boost::asio::io_service & io_svc,
            const network::HttpRequest::p http_request_demo,
            const protocol::UrlInfo& url_info,
            DownloadDriver__p download_driver,
            bool is_to_get_header,
            bool is_open_service = false);

        virtual ~Downloader() {}

    public:
        // 启停
        virtual void Stop() = 0;

    public:
        // 操作
        
        virtual bool IsPausing() = 0;
        

    public:
        // 属性
        inline void SetOriginal(bool original = true) { is_original_ = original; }
        inline bool IsOriginal() { return is_original_; }
        inline bool IsRunning() { return is_running_; }

        virtual statistic::SPEED_INFO GetSpeedInfo() = 0;
        virtual statistic::SPEED_INFO_EX GetSpeedInfoEx() = 0;

    protected:
        bool is_original_;            // 是否是初始的URl
        volatile bool is_running_;
    protected:
        Downloader()
            : is_original_(false), is_running_(false) {}
    };

    class VodDownloader
        : public Downloader
    {
    public:
        virtual bool GetUrlInfo(protocol::UrlInfo& url_info) = 0;
        virtual bool CanDownloadPiece(const protocol::PieceInfo& piece_info) { return true; }
        virtual bool IsP2PDownloader() { return false; }

        virtual void SetSpeedLimitInKBps(boost::int32_t speed_limit_in_KBps) = 0;
        virtual boost::int32_t  GetPieceTaskNum() = 0;

        virtual void OnPieceTimeout(DownloadDriver__p download_driver_, const protocol::PieceInfoEx & piece) = 0;
        virtual void PutPieceTask(const std::deque<protocol::PieceInfoEx> & piece_info_ex_s, DownloadDriver__p downloader) = 0;
    };

    class LiveDownloader
        : public Downloader
    {
    public:
        virtual void OnBlockTimeout(boost::uint32_t block_id) = 0;
        virtual void PutBlockTask(const protocol::LiveSubPieceInfo & live_block) = 0;

        virtual bool IsP2PDownloader() = 0;
    };
}

#endif  // _P2SP_DOWNLOAD_DOWNLOADER_H_
