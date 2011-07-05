//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/proxy/CommonProxySender.h"
#include "p2sp/proxy/ProxySender.h"
#include "p2sp/proxy/ProxyConnection.h"
#include "p2sp/AppModule.h"

// #define COUT(msg) std::cout << msg << "\r"

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("proxy");

    void CommonProxySender::Start()
    {
        if (is_running_ == true) return;

        LOG(__EVENT, "proxy", "IProxySender::Strat");
        is_running_ = true;
    }

    void CommonProxySender::Start(network::HttpRequest::p http_request, ProxyConnection__p proxy_connection)
    {
        assert(!"CommonProxySender::Start(network::HttpRequest::p http_request, ProxyConnection__p proxy_connection)");
    }

    void CommonProxySender::Start(uint32_t start_possition)
    {
        assert(!"CommonProxySender::Start(uint32_t start_possition)");
    }

    void CommonProxySender::Stop()
    {
        if (is_running_ == false) return;

        LOG(__EVENT, "proxy", "IProxySender::Stop");

        if (http_server_socket_)
        {
            // http_server_socket_->WillClose();
            http_server_socket_.reset();
        }

        // if (fp_) {
        //    fclose(fp_);
        //    fp_ = NULL;
        // }

        is_running_ = false;
    }

    void CommonProxySender::OnTcpSendSucced(uint32_t length)
    {
        if (is_running_ == false) return;
        // LOG(__INFO, "proxy", "OnTcpSendSucced " << http_server_socket_->GetEndPoint() << " length=" << length);
    }

    void CommonProxySender::OnDownloadDriverError(uint32_t error_code)
    {
        if (is_running_ == false) return;

        string proxy_script_text = "not find";
        http_server_socket_->HttpSendContent(proxy_script_text);

    }


    void CommonProxySender::OnAsyncGetSubPieceSucced(uint32_t start_position, base::AppBuffer buffer)
    {
        if (is_running_ == false) return;
        assert(file_length_ > 0);
        assert(playing_position_ < file_length_);
        // ??????
        //     ??????? http_server_->SendBuffer(buffer);

        assert(playing_position_ == start_position);

        // if (false == is_response_header_)
        // {
        //    // http_server_socket_->HttpSendHeader(content_length, "video/x-flv");

        // } else
        // {
        //    http_server_socket_->HttpSendBuffer(buffer);

        // }

        // LOG(__INFO, "proxy", "CommonProxySender::OnAsyncGetSubPieceSucced Send protocol::SubPieceContent to: " << http_server_socket_->GetEndPoint() << " start_possition: " << start_position << " buffer_length: " << buffer.Length());
        // LOG(__DEBUG, "proxy", "write buffer @" << buffer.GetSubPieceBuffer()->get_buffer_address());
        http_server_socket_->HttpSendBuffer(buffer);

        //     playing_position ??????????? buffer.Length()
        playing_position_ += buffer.Length();

        if (playing_position_ == file_length_)
        {
            LOG(__WARN, "proxy", "CommonProxySender::OnAsyncGetSubPieceSucced playing_position_ == file_length_ send \\r\\n\\r\\n");
//            http_server_socket_->HttpSendBuffer(protocol::SubPieceContent("\\r\\n"));
//            http_server_socket_->HttpSendBuffer((const boost::uint8_t *)"\r\n\r\n", sizeof("\r\n\r\n"));
        }

        // if (http_server_socket_->GetSendPendingCount() > 200)
        // {
        //    LOG(__WARN, "proxy", "ProxyConnection::OnAsyncGetSubPieceSucced but GetSendPendingCount" << http_server_socket_->GetSendPendingCount() << " > 200 So Close It");
        //    WillStop();
        // }
    }

    void CommonProxySender::OnRecvSubPiece(uint32_t start_position, std::vector<base::AppBuffer> const & buffers)
    {
        if (is_running_ == false) return;
        // assert(file_length_ > 0);
        // assert(playing_position_ < file_length_);
        // ??????
        //     ??????? http_server_->SendBuffer(buffer);

        // assert(playing_position_ == start_position);
        LOG(__DEBUG, "proxy", ">> playing_position_ = " << playing_position_ << ", file_length_ = " << file_length_ << ", buffers.count = " << buffers.size());

        for (uint32_t i = 0; i < buffers.size(); ++i) {
            // if (fp_) {
            //    fwrite(buffers[i].Data(), sizeof(boost::uint8_t), buffers[i].Length(), fp_);
            // }
            // avoid proxy connection stopped in the send process @herain
            if (is_running_ == false)
                return;
            // LOG(__DEBUG, "proxy", "write buffer @" << buffers[i].GetSubPieceBuffer()->get_buffer_address());
            http_server_socket_->HttpSendBuffer(buffers[i]);
            playing_position_ += buffers[i].Length();
        }

        if (playing_position_ == file_length_)
        {
            LOG(__WARN, "proxy", "CommonProxySender::OnRecvSubPiece playing_position_ == file_length_ send \\r\\n\\r\\n");
        }
    }

    void CommonProxySender::OnNoticeGetContentLength(uint32_t content_length, network::HttpResponse::p http_response)
    {
        if (is_running_ == false) return;
        // ???Http???
        // assert(is_response_header_ == false);
        if (is_response_header_ == true)
            return;

        LOG(__INFO, "proxy", "CommonProxySender::OnNoticeGetContentLength " << http_server_socket_->GetEndPoint() << " content_length: " << content_length);

        // string tudouheader("HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nETag: \"812255599\"\r\nAccept-Ranges: bytes\r\nLast-Modified: Thu, 24 Jul 2008 09:33:25 GMT\r\nContent-Length: 4916020\r\nDate: Fri, 25 Jul 2008 01:23:10 GMT\r\nServer: WS CDN Server\r\n\r\n");

        // http_server_socket_->HttpSendHeader(content_length, "video/x-flv");

        if (!http_response)
        {
            http_server_socket_->HttpSendHeader(content_length, "video/x-flv");
            // http_server_socket_->HttpSendKeepAliveHeader(content_length, "video/x-flv");
        }
        else if (http_response->GetStatusCode() == 206)
        {
            http_server_socket_->HttpSendHeader(content_length, "video/x-flv");
        }
        else
        {
            http_response->SetProperty("Connection", "close");
            LOG(__EVENT, "proxy", "CommonProxySender::OnNoticeGetContentLength Send response string: \n" << http_response->ToString());
            http_server_socket_->HttpSendHeader(http_response->ToString());
            // http_server_socket_->HttpSendHeader(content_length, "video/x-flv");
        }
        // http_server_socket_->HttpSendHeader(tudouheader);

        is_response_header_ = true;

        file_length_ = content_length;

        // ??????????????
        //        timer_ = OnceTimer::create(250, shared_from_this());
        //        timer_ = framework::timer::PeriodicTimer::create(250, shared_from_this());
        //        timer_->Start();
    }

    // ???
    void CommonProxySender::OnNotice403Header()
    {
        if (false == is_running_)
            return;
        if (true == is_response_header_)
            return;

        LOG(__EVENT, "proxy", __FUNCTION__ << ": Notice 403 header");

        http_server_socket_->HttpSend403Header();
    }

    void CommonProxySender::OnNoticeOpenServiceHeadLength(uint32_t head_length)
    {
        if (false == is_running_)
            return;
    }

    uint32_t CommonProxySender::GetStartOffset()
    {
        return 0;
    }
}
