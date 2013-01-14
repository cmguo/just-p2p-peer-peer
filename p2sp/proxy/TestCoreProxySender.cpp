//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#if 0
#include <boost/format.hpp>

#include "p2sp/proxy/ProxyModule.h"
#include "p2sp/proxy/TestCoreProxySender.h"
#include "p2sp/proxy/ProxySender.h"
#include "p2sp/proxy/ProxyConnection.h"
#include "p2sp/AppModule.h"


namespace p2sp
{
    void TestCoreProxySender::Start()
    {
        assert(0);
    }

    void TestCoreProxySender::Start(network::HttpRequest::p http_request, ProxyConnection::p proxy_connection)
    {
        if (is_running_ == true) return;

        LOG(__EVENT, "proxy", "TestCoreProxySender::Start");

        is_running_ = true;
        http_request_ = http_request;
        proxy_connection_ = proxy_connection;

    }

    void TestCoreProxySender::Start(boost::uint32_t start_possition)
    {
        assert(0);
    }


    void TestCoreProxySender::Stop()
    {
        if (is_running_ == false) return;

        LOG(__EVENT, "proxy", "TestCoreProxySender::Stop");
        if (http_server_socket_)
        {
            // http_server_socket_->WillClose();
            http_server_socket_.reset();
        }

        if (http_client_)
        {
            http_client_->Close();
            http_client_.reset();
        }

        if (http_request_)
        {
            http_request_.reset();
        }

        if (proxy_connection_)
        {
            proxy_connection_.reset();
        }

        is_running_ = false;
    }

    void TestCoreProxySender::SendHttpRequest()
    {
        if (is_running_ == false) return;

        assert(http_request_);
        if (have_send_http_header_) return;
        have_send_http_header_ = true;

        LOG(__INFO, "proxy", "TestCoreProxySender::SendHttpRequest test port:" << ProxyModule::Inst()->GetTestCoreHttpPort());
        http_client_ = network::HttpClient::create("localhost", ProxyModule::Inst()->GetTestCoreHttpPort(), "/");

        http_client_->SetHandler(shared_from_this());
        http_client_->Connect();
    }


    // HttpClient ******************************
    void TestCoreProxySender::OnHttpRecvSucced(network::HttpRequest::p http_request)
    {
        assert(0);
    }

    void TestCoreProxySender::OnHttpRecvFailed(boost::uint32_t error_code)
    {
        assert(0);
    }

    void TestCoreProxySender::OnHttpRecvTimeout()
    {
        assert(0);
    }

    void TestCoreProxySender::OnTcpSendSucced(boost::uint32_t length)
    {
        if (is_running_ == false) return;
        // LOG(__INFO, "proxy", "OnTcpSendSucced " << http_server_socket_->GetEndPoint() << " length=" << length);
    }

    void TestCoreProxySender::OnTcpSendFailed()
    {
        if (is_running_ == false) return;
        LOG(__WARN, "proxy", "OnTcpSendFailed " << http_server_socket_->GetEndPoint());

    }

    void TestCoreProxySender::OnClose()
    {
        if (is_running_ == false) return;
        assert(0);
    }

    void TestCoreProxySender::OnDownloadDriverError(boost::uint32_t error_code)
    {
        if (is_running_ == false) return;
        assert(0);
    }


    void TestCoreProxySender::OnAsyncGetSubPieceSucced(boost::uint32_t start_position, base::SubPieceContent& buffer)
    {
        if (is_running_ == false) return;
        assert(0);
    }

    void TestCoreProxySender::OnAsyncGetSubPieceFailed(boost::uint32_t start_position, int failed_code)
    {
        if (is_running_ == false) return;
        assert(0);
    }

    void TestCoreProxySender::OnNoticeGetContentLength(boost::uint32_t content_length, network::HttpResponse::p http_response)
    {
        if (is_running_ == false) return;
        assert(0);
    }

    // HttpClient ******************************
    void TestCoreProxySender::OnConnectSucced()
    {
        if (is_running_ == false) return;
        LOG(__INFO, "proxy", "TestCoreProxySender::OnConnectSucced");
        http_client_->HttpGetByString(http_request_->GetRequestString());
    }

    void TestCoreProxySender::OnConnectFailed(boost::uint32_t error_code)
    {
        if (is_running_ == false) return;

        LOG(__INFO, "proxy", "TestCoreProxySender::OnConnectFailed");
//        WillStop();
        proxy_connection_->WillStop();

    }

    void TestCoreProxySender::OnConnectTimeout()
    {
        if (is_running_ == false) return;
        LOG(__INFO, "proxy", "TestCoreProxySender::OnConnectTimeout");
//        WillStop();
        proxy_connection_->WillStop();

    }

    void TestCoreProxySender::OnRecvHttpHeaderSucced(network::HttpResponse::p http_response)
    {
        if (is_running_ == false) return;
        LOG(__INFO, "proxy", "TestCoreProxySender::OnRecvHttpHeaderSucced");

        http_server_socket_->HttpSendHeader(http_response->GetContentLength(), "video/x-flv");

        http_client_->HttpRecvSubPiece();
    }

    void TestCoreProxySender::OnRecvHttpHeaderFailed(boost::uint32_t error_code)
    {
        if (is_running_ == false) return;
        LOG(__INFO, "proxy", "TestCoreProxySender::OnRecvHttpHeaderFailed");

//        WillStop();
        proxy_connection_->WillStop();


    }

    void TestCoreProxySender::OnRecvHttpDataSucced(base::SubPieceContent buffer, boost::uint32_t file_offset, boost::uint32_t content_offset)
    {
        if (is_running_ == false) return;
        LOG(__INFO, "proxy", "TestCoreProxySender::OnRecvHttpDataSucced");

        http_server_socket_->HttpSendBuffer(buffer);

        http_client_->HttpRecvSubPiece();
    }

    void TestCoreProxySender::OnRecvHttpDataPartial(base::SubPieceContent buffer, boost::uint32_t file_offset, boost::uint32_t content_offset)
    {
        if (is_running_ == false) return;
        LOG(__INFO, "proxy", "TestCoreProxySender::OnRecvHttpDataPartial");

        http_server_socket_->HttpSendBuffer(buffer);

    }

    void TestCoreProxySender::OnRecvHttpDataFailed(boost::uint32_t error_code)
    {
        if (is_running_ == false) return;
        LOG(__INFO, "proxy", "TestCoreProxySender::OnRecvHttpDataFailed");

        proxy_connection_->WillStop();
//        WillStop();
    }

    void TestCoreProxySender::OnRecvTimeout()
    {
        if (is_running_ == false) return;
        LOG(__INFO, "proxy", "TestCoreProxySender::OnRecvTimeout");

        proxy_connection_->WillStop();
//        WillStop();
    }

    void TestCoreProxySender::OnComplete()
    {
        if (is_running_ == false) return;
        LOG(__INFO, "proxy", "TestCoreProxySender::OnComplete");
        proxy_connection_->WillStop();

//        WillStop();
    }
    void TestCoreProxySender::OnNotice403Header()
    {
        if (false == is_running_)
            return;
        if (true == is_response_header_)
            return;

        LOG(__EVENT, "proxy", __FUNCTION__ << ": Notice 403 header");

        http_server_socket_->HttpSend403Header();
    }

    void TestCoreProxySender::OnNoticeOpenServiceHeadLength(boost::uint32_t head_length)
    {
        if (false == is_running_)
            return;
    }
}
#endif  // 0

