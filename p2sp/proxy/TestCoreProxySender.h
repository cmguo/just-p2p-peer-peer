//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// TestCoreProxySender.h

#ifndef _P2SP_PROXY_TEST_CORE_PROXY_SENDER_H_
#define _P2SP_PROXY_TEST_CORE_PROXY_SENDER_H_

#if 0

#include "framework/network/network::HttpServer.h"
#include "framework/network/HttpClient.h"

#include "protocol/Base.h"

#include "p2sp/proxy/ProxySender.h"

using namespace framework::network;

namespace p2sp
{
    class ProxyConnection;
    typedef boost::shared_ptr<ProxyConnection> ProxyConnection__p;

    class TestCoreProxySender
        : public boost::noncopyable
        , public boost::enable_shared_from_this<TestCoreProxySender>
        , public ProxySender
        , public framework::network::IHttpClientListener
#ifdef DUMP_OBJECT
        , public count_object_allocate<TestCoreProxySender>
#endif
    {
    public:
        typedef boost::shared_ptr<TestCoreProxySender> p;
        static p create(network::HttpServer::pointer http_server_socket) { return p(new TestCoreProxySender(http_server_socket)); }
    public:
        virtual void Start();
        virtual void Start(network::HttpRequest::p http_request, ProxyConnection__p proxy_connection);
        virtual void Start(uint32_t start_possition);
        virtual void Stop();
    public:
        virtual uint32_t GetPlayingPosition() const { return 0; }
        virtual void SendHttpRequest();
        virtual void ResetPlayingPosition() {}
        virtual bool IsHeaderResopnsed() const { return is_response_header_; }

    public:
        // network::HttpServer
        virtual void OnHttpRecvSucced(network::HttpRequest::p http_request);
        virtual void OnHttpRecvFailed(uint32_t error_code);
        virtual void OnHttpRecvTimeout();
        virtual void OnTcpSendSucced(uint32_t length);
        virtual void OnTcpSendFailed();
        virtual void OnClose();
        // HttpClient
        virtual void OnConnectSucced();
        virtual void OnConnectFailed(uint32_t error_code);
        virtual void OnConnectTimeout();
        virtual void OnRecvHttpHeaderSucced(network::HttpResponse::p http_response);
        virtual void OnRecvHttpHeaderFailed(uint32_t error_code);
        virtual void OnRecvHttpDataSucced(base::SubPieceContent buffer, uint32_t file_offset, uint32_t content_offset);
        virtual void OnRecvHttpDataPartial(base::SubPieceContent buffer, uint32_t file_offset, uint32_t content_offset);
        virtual void OnRecvHttpDataFailed(uint32_t error_code);
        virtual void OnRecvTimeout();
        virtual void OnComplete();
        virtual void OnDownloadDriverError(uint32_t error_code);
        virtual void OnAsyncGetSubPieceSucced(uint32_t start_position, base::SubPieceContent& buffer);
        virtual void OnAsyncGetSubPieceFailed(uint32_t start_position, int failed_code);
        virtual void OnNoticeGetContentLength(uint32_t content_length, network::HttpResponse::p http_response);
        virtual void OnNotice403Header();

        virtual void OnNoticeOpenServiceHeadLength(uint32_t head_length);
    private:
        network::HttpServer::pointer http_server_socket_;
        network::HttpClient::p http_client_;
        network::HttpRequest::p http_request_;
        ProxyConnection__p proxy_connection_;
        volatile bool is_running_;
        bool is_response_header_;
        bool have_send_http_header_;

    private:
        TestCoreProxySender(network::HttpServer::pointer http_server_socket)
            : http_server_socket_(http_server_socket), is_running_(false), is_response_header_(false), have_send_http_header_(false) { is_test_sender_ = true;}
    };
}

#endif  // 0

#endif  // _P2SP_PROXY_TEST_CORE_PROXY_SENDER_H_
