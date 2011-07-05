//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/proxy/FlvDragProxySender.h"
#include "p2sp/proxy/ProxySender.h"
#include "p2sp/proxy/ProxyConnection.h"
#include "p2sp/AppModule.h"


namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("proxy");

    void FlvDragProxySender::Start()
    {
        assert(0);
    }

    void FlvDragProxySender::Start(network::HttpRequest::p http_request, ProxyConnection__p proxy_connection)
    {
        assert(0);
    }

    void FlvDragProxySender::Start(uint32_t start_possition)
    {
        if (is_running_ == true) return;

        LOG(__EVENT, "proxy", "FlvDragProxySender::Start");

        is_running_ = true;

        // 取整  1k
        start_offset_ = start_possition;
        playing_position_ = (start_possition/1024) * 1024;

    }

    void FlvDragProxySender::Stop()
    {
        if (is_running_ == false) return;

        LOG(__EVENT, "proxy", "FlvDragProxySender::Stop");

        if (http_server_socket_)
        {
            // http_server_socket_->WillClose();
            http_server_socket_.reset();
        }

        if (download_driver_)
        {
            download_driver_.reset();
        }

        is_running_ = false;
    }

    void FlvDragProxySender::OnTcpSendSucced(uint32_t length)
    {
        if (is_running_ == false) return;
        // LOG(__INFO, "proxy", "OnTcpSendSucced " << http_server_socket_->GetEndPoint() << " length=" << length);
    }

    void FlvDragProxySender::OnDownloadDriverError(uint32_t error_code)
    {
        if (is_running_ == false) return;

        string proxy_script_text = "not find";
        http_server_socket_->HttpSendContent(proxy_script_text);

    }


    void FlvDragProxySender::OnAsyncGetSubPieceSucced(uint32_t start_position, base::AppBuffer buffer)
    {
        if (is_running_ == false) return;
        assert(file_length_ > 0);
        assert(playing_position_ < file_length_);


        assert(playing_position_ == start_position);
        if (playing_position_ != start_position) {
            return;
        }

        if (false == is_response_header_)
        {
            // http_server_socket_->HttpSendHeader(content_length, "video/x-flv");

            uint32_t i;
            for (i = 0; i < buffer.Length() - 10; i ++)
            {
                if (buffer.Data()[i] == 0x08 || buffer.Data()[i] == 0x09)  // Tag Type
                {
                    // StreamID always zero
                    if (buffer.Data()[i + 8] == 0x00 && buffer.Data()[i + 9] == 0x00 && buffer.Data()[i + 10] == 0x00)
                    {
                        uint32_t content_length = file_length_ - start_position - i + 13;

                        http_server_socket_->HttpSendKeepAliveHeader(content_length, "video/x-flv");
                        // http_server_socket_->HttpSendHeader(content_length, "video/x-flv");

                        boost::uint8_t a[] = {0x46, 0x4C, 0x56, 0x01, 0x05, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00};
                        http_server_socket_->HttpSendBuffer(a, 13);

                        http_server_socket_->HttpSendBuffer(buffer.Data() + i, buffer.Length() - i);

                        is_response_header_ = true;
                        break;
                    }
                }
            }
        }
        else
        {
            http_server_socket_->HttpSendBuffer(buffer);
        }

        //     playing_position 往前面按字节顺推 buffer.length_
        playing_position_ += buffer.Length();


        // LOG(__INFO, "proxy", "ProxyConnection::OnAsyncGetSubPieceSucced GetSendPendingCount = " << http_server_socket_->GetSendPendingCount());
        // if (http_server_socket_->GetSendPendingCount() > 200)
        // {
        //    LOG(__WARN, "proxy", "ProxyConnection::OnAsyncGetSubPieceSucced but GetSendPendingCount" << http_server_socket_->GetSendPendingCount() << " > 200 So Close It");
        //    WillStop();
        // }
    }

    void FlvDragProxySender::OnNoticeGetContentLength(uint32_t content_length, network::HttpResponse::p http_response)
    {
        if (is_running_ == false) return;
        // 回应Http头部
        // assert(is_response_header_ == false);
        // if (is_response_header_ == true)
        //    return;

        // http_server_socket_->HttpSendHeader(content_length, "video/x-flv");

        // is_response_header_ = true;
        // http_server_socket_->HttpSendHeader(http_response->ToString());

        file_length_ = content_length;  // + start_possition_ - 13;

        // 启动获得数据定时器
        //        timer_ = OnceTimer::create(250, shared_from_this());
        //        timer_ = framework::timer::PeriodicTimer::create(250, shared_from_this());
        //        timer_->Start();
    }
    // 失锟斤拷
    void FlvDragProxySender::OnNotice403Header()
    {
        if (false == is_running_)
            return;
        if (true == is_response_header_)
            return;

        LOG(__EVENT, "proxy", __FUNCTION__ << ": Notice 403 header");

        http_server_socket_->HttpSend403Header();
    }


    void FlvDragProxySender::OnNoticeOpenServiceHeadLength(uint32_t head_length)
    {
        if (false == is_running_)
            return;
    }

    void FlvDragProxySender::OnRecvSubPiece(uint32_t position, std::vector<base::AppBuffer> const & buffers)
    {
        for (uint32_t i = 0; i < buffers.size(); ++i)
        {
            OnAsyncGetSubPieceSucced(position, buffers[i]);
            position += buffers[i].Length();
        }
    }


    uint32_t FlvDragProxySender::GetStartOffset()
    {
        if (false == is_running_)
            return 0;
        return start_offset_;
    }
}
