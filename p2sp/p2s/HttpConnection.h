//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// HttpConnection.h

#ifndef _P2SP_P2S_HTTPCONNECTION_H_
#define _P2SP_P2S_HTTPCONNECTION_H_

#include "network/HttpClient.h"

namespace p2sp
{
    const uint32_t NONE = 0;
    const uint32_t CONNECTING = 1;
    const uint32_t CONNECTED = 2;
    const uint32_t HEADERING = 3;
    const uint32_t HEADERED = 4;
    const uint32_t PIECEING = 5;
    const uint32_t PIECED = 6;
    const uint32_t SLEEPING = 7;

    class HttpDownloader;
    typedef boost::shared_ptr<HttpDownloader> HttpDownloader__p;

    class HttpConnection
        : public boost::noncopyable
        , public boost::enable_shared_from_this<HttpConnection>
        , public network::IHttpClientListener<protocol::SubPieceBuffer>
#ifdef DUMP_OBJECT
        , public count_object_allocate<HttpConnection>
#endif
    {
    public:
        typedef boost::shared_ptr<HttpConnection> p;
        static p Create(
            boost::asio::io_service & io_svc,
            HttpDownloader__p downloader,
            protocol::UrlInfo url_info)
        {
            return p(new HttpConnection(io_svc, downloader, url_info));
        }

        static p Create(
            boost::asio::io_service & io_svc,
            const network::HttpRequest::p http_request_demo,
            HttpDownloader__p downloader,
            protocol::UrlInfo url_info,
            bool is_to_get_header = false)
        {
            return p(new HttpConnection(io_svc, http_request_demo, downloader, url_info, is_to_get_header));
        }

    public:
        void Start(bool is_support_start, bool open_service = false, uint32_t head_length = (uint32_t)-1);

        void Stop();

        void PutPieceTask();
        void PutPieceTask(const std::deque<protocol::PieceInfoEx> & piece_infos);

        bool IsConnected() const { return is_connected_; }

        bool IsSupportRange() const { return is_support_range_; }

        void DecetecterReport(bool is_support_range);

        void StopPausing();

        void SetPausing();

        bool IsPausing() {return is_pausing_;}

        string GetPragmaClient() const { return pragma_client_; }

        void HttpRecvSubPiece();

        network::HttpClient<protocol::SubPieceContent>::p GetHttpClient();

        void PieceTimeout();
        boost::int32_t GetPieceTaskNum();
    private:
        void DoConnect();

        void SleepForConnect();

        void DelayForConnect();

        void SendHttpRequest();
    public:
        // 锟斤拷息
        virtual void OnConnectSucced();
        virtual void OnConnectFailed(uint32_t error_code);
        virtual void OnConnectTimeout();
        virtual void OnRecvHttpHeaderSucced(network::HttpResponse::p http_response);
        virtual void OnRecvHttpHeaderFailed(uint32_t error_code);
        virtual void OnRecvHttpDataSucced(protocol::SubPieceBuffer const & buffer, uint32_t file_offset, uint32_t content_offset);
        virtual void OnRecvHttpDataPartial(protocol::SubPieceBuffer const & buffer, uint32_t file_offset, uint32_t content_offset);
        virtual void OnRecvHttpDataFailed(uint32_t error_code);
        virtual void OnRecvTimeout();
        virtual void OnComplete();

        void OnTimerElapsed(framework::timer::Timer * pointer);

    private:
        boost::uint32_t GetTaskRangeEnd(boost::uint32_t block_size);

    private:
        boost::asio::io_service & io_svc_;
        // typedef std::set<protocol::PieceInfoEx> TaskQueue;
        protocol::PieceInfoEx piece_info_ex_;
        // TaskQueue task_queue_;
        HttpDownloader__p downloader_;
        network::HttpClient<protocol::SubPieceContent>::p http_client_;
        protocol::UrlInfo url_info_;
        string pragma_client_;

        bool is_support_start_;
        bool is_running_;
        bool is_connected_;
        bool is_open_service_;

        uint32_t head_length_;

        uint32_t status;
        bool have_piece_;
        volatile bool is_support_range_;
        bool is_to_get_header_;
        volatile bool is_detected_;

        // 只OnNoticeHeader一锟斤拷
        bool no_notice_header_;

        // pausing sleep
        volatile bool is_pausing_;
        framework::timer::OnceTimer pausing_sleep_timer_;

        // sleep
        framework::timer::OnceTimer sleep_once_timer_;
        framework::timer::OnceTimer delay_once_timer_;
        network::HttpRequest::p http_request_demo_;

        // 403 header retry
        uint32_t retry_count_403_header_;
        // 500 header retry
        uint32_t retry_count_500_header_;
        uint32_t connect_fail_count_;

        std::deque<protocol::PieceInfoEx> piece_task;
        bool is_downloading_;

    private:
        HttpConnection(
            boost::asio::io_service & io_svc,
            HttpDownloader__p downloader,
            protocol::UrlInfo url_info);

        HttpConnection(
            boost::asio::io_service & io_svc,
            const network::HttpRequest::p http_request_demo,
            HttpDownloader__p downloader,
            protocol::UrlInfo url_info,
            bool is_to_get_header);
    };
}

#endif  // _P2SP_P2S_HTTPCONNECTION_H_
