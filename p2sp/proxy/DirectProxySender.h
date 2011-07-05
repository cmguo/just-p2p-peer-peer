//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// DirectProxySender.h

#ifndef _P2SP_PROXY_DIRECT_PROXY_SENDER_H_
#define _P2SP_PROXY_DIRECT_PROXY_SENDER_H_

#include "statistic/DownloadDriverStatistic.h"
#include "statistic/HttpDownloaderStatistic.h"

#include "p2sp/proxy/ProxySender.h"

namespace p2sp
{
    class ProxyConnection;
    typedef boost::shared_ptr<ProxyConnection> ProxyConnection__p;

    class DirectProxySender
        : public boost::noncopyable
        , public boost::enable_shared_from_this<DirectProxySender>
        , public ProxySender
        , public network::IHttpClientListener<protocol::SubPieceBuffer>
#ifdef DUMP_OBJECT
        , public count_object_allocate<DirectProxySender>
#endif
    {
    public:
        typedef boost::shared_ptr<DirectProxySender> p;
        static p create(
            boost::asio::io_service & io_svc,
            network::HttpServer::pointer http_server_socket,
            bool need_bubble)
        {
            return p(new DirectProxySender(io_svc, http_server_socket, need_bubble));
        }
    public:
        // 方法
        virtual void Start();
        virtual void Start(network::HttpRequest::p http_request, ProxyConnection__p proxy_connection);
        virtual void Start(uint32_t start_possition);
        virtual void Stop();
    public:
        // 属性
        virtual uint32_t GetPlayingPosition() const { return 0; }
        virtual void SendHttpRequest();
        virtual void ResetPlayingPosition() {}
        virtual bool IsHeaderResopnsed() const { return is_response_header_; }

    public:
        // 消息
        // network::HttpServer
        virtual void OnTcpSendSucced(uint32_t length);
        // HttpClient
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
        // 下载器
        virtual void OnDownloadDriverError(uint32_t error_code);
        // 播放数据
        virtual void OnAsyncGetSubPieceSucced(uint32_t start_position, base::AppBuffer buffer);
        // 获得Contentlength
        virtual void OnNoticeGetContentLength(uint32_t content_length, network::HttpResponse::p http_response);
        // 失败
        virtual void OnNotice403Header();

        virtual void OnNoticeOpenServiceHeadLength(uint32_t head_length);
        virtual void OnRecvSubPiece(uint32_t position, std::vector<base::AppBuffer> const & buffers);
        virtual uint32_t GetStartOffset();

    private:
        boost::asio::io_service & io_svc_;
        network::HttpServer::pointer http_server_socket_;
        network::HttpClient<protocol::SubPieceContent>::p http_client_;
        network::HttpRequest::p http_request_;
        ProxyConnection__p proxy_connection_;

        statistic::DownloadDriverStatistic::p downloaddriver_statistic_;
        statistic::HttpDownloaderStatistic::p httpdownloader_statistic_;
        int s_id_;

        volatile bool is_running_;
        bool is_response_header_;
        bool have_send_http_header_;
        bool need_bubble_;

    private:
        DirectProxySender(
            boost::asio::io_service & io_svc,
            network::HttpServer::pointer http_server_socket,
            bool need_bubble)
            : io_svc_(io_svc)
            , http_server_socket_(http_server_socket)
            , is_running_(false)
            , is_response_header_(false)
            , have_send_http_header_(false)
            , need_bubble_(need_bubble)
        {
        }
    };
}

#endif  // _P2SP_PROXY_DIRECT_PROXY_SENDER_H_
