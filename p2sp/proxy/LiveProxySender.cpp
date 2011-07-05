//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/proxy/LiveProxySender.h"
#include "p2sp/proxy/ProxySender.h"
#include "p2sp/proxy/ProxyConnection.h"
#include "p2sp/AppModule.h"

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("proxy");

    void LiveProxySender::Start()
    {
        if (is_running_ == true)
        {
            assert(false);
            return;
        }

        LOG(__EVENT, "proxy", "LiveProxySender::Strat");
        is_running_ = true;
    }

    void LiveProxySender::Start(network::HttpRequest::p http_request, ProxyConnection__p proxy_connection)
    {
        assert(false);
    }

    void LiveProxySender::Start(uint32_t start_position)
    {
        assert(false);
    }

    void LiveProxySender::Stop()
    {
        if (is_running_ == false)
        {
            assert(false);
            return;
        }

        LOG(__EVENT, "proxy", "LiveProxySender::Stop");

        if (http_server_socket_)
        {
            http_server_socket_.reset();
        }

        is_running_ = false;
    }

    void LiveProxySender::OnTcpSendSucced(uint32_t length)
    {
        
    }

    void LiveProxySender::OnClose()
    {
        assert(false);
    }

    void LiveProxySender::OnDownloadDriverError(uint32_t error_code)
    {
        assert(false);
    }

    void LiveProxySender::OnAsyncGetSubPieceSucced(uint32_t start_position, base::AppBuffer buffer)
    {
        assert(false);
    }

    void LiveProxySender::OnRecvSubPiece(uint32_t start_position, std::vector<base::AppBuffer> const & buffers)
    {
        OnNoticeGetContentLength(0, network::HttpResponse::p());

        for (uint32_t i = 0; i < buffers.size(); ++i) 
        {
            http_server_socket_->HttpSendBuffer(buffers[i]);
        }
    }

    void LiveProxySender::OnNoticeGetContentLength(uint32_t content_length, network::HttpResponse::p http_response)
    {
        if (is_running_ == false )
        {
            return;
        }

        if (is_response_header_ == true)
        {
            return;
        }

        // 向播放器回复 http 200 头部，仅仅回一次
        string header("HTTP/1.0 200 OK\r\nCache-Control: no-store, no-cache, must-revalidate\r\nContent-Type: application/octet-stream\r\n\r\n");
        http_server_socket_->HttpSendHeader(header);

        is_response_header_ = true;
    }

    void LiveProxySender::OnNotice403Header()
    {
        assert(false);
    }

    void LiveProxySender::OnNoticeOpenServiceHeadLength(uint32_t head_length)
    {
        assert(false);
    }

    uint32_t LiveProxySender::GetStartOffset()
    {
        assert(false);
        return 0;
    }
}