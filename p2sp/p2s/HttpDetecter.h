//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// HttpDetecter.h

#ifndef _P2SP_P2S_HTTP_DETECTER_H_
#define _P2SP_P2S_HTTP_DETECTER_H_

#include "network/HttpRequest.h"
#include "network/HttpResponse.h"
#include "network/HttpClient.h"

namespace p2sp
{
    class HttpDownloader;
    typedef boost::shared_ptr<HttpDownloader> HttpDownloader__p;
    class HttpConnection;
    typedef boost::shared_ptr<HttpConnection> HttpConnection__p;

    class HttpDetecter
        : public boost::noncopyable
        , public boost::enable_shared_from_this<HttpDetecter>
        , public network::IHttpClientListener<protocol::SubPieceBuffer>
#ifdef DUMP_OBJECT
        , public count_object_allocate<HttpDetecter>
#endif
    {
    public:
        typedef boost::shared_ptr<HttpDetecter> p;
        static p Create(
            boost::asio::io_service & io_svc,
            HttpDownloader__p downloader)
        {
            return p(new HttpDetecter(io_svc, downloader));
        }

        static p Create(
            boost::asio::io_service & io_svc,
            const network::HttpRequest::p http_request_demo, HttpDownloader__p downloader)
        {
            return p(new HttpDetecter(io_svc, http_request_demo, downloader));
        }

    public:
        void Start();

        void Stop();

        bool DoDetect(HttpConnection__p http_connection, protocol::UrlInfo url_info);  // whether do detect

    private:
        void SleepForConnect();
        void SendDemoRequest();

    public:
        // 消息
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
        boost::asio::io_service & io_svc_;
        HttpDownloader__p downloader_;
        HttpConnection__p http_connection_;
        network::HttpClient<protocol::SubPieceContent>::p http_client_;
        protocol::UrlInfo url_info_;

        volatile bool is_running_;

        // 只被启动一次
        bool is_detected_;

        network::HttpRequest::p http_request_demo_;

        framework::timer::OnceTimer sleep_once_timer_;

    private:
        HttpDetecter(
            boost::asio::io_service & io_svc,
            HttpDownloader__p downloader);

        HttpDetecter(
            boost::asio::io_service & io_svc,
            const network::HttpRequest::p http_request_demo,
            HttpDownloader__p downloader);
    };
}

#endif  // _P2SP_P2S_HTTP_DETECTER_H_
