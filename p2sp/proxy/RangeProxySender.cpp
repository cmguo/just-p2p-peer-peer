//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/proxy/RangeProxySender.h"
#include "p2sp/proxy/ProxySender.h"
#include "p2sp/proxy/ProxyConnection.h"
#include "p2sp/AppModule.h"
#include "p2sp/proxy/RangeInfo.h"
#include "p2sp/download/DownloadDriver.h"

#define RANGE_DEBUG(msg) LOGX(__DEBUG, "proxy", msg)

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("proxy");

    void RangeProxySender::Start()
    {
        RANGE_DEBUG("Invalid Start Function Call!!!");
        assert(!"RangeProxySender::Start()");
    }

    void RangeProxySender::Start(network::HttpRequest::p http_request, ProxyConnection__p proxy_connection)
    {
        if (is_running_ == true) return;

        RANGE_DEBUG("");

        http_request_ = http_request;
        proxy_connection_ = proxy_connection;
        playing_position_ = 0;
        is_response_header_ = false;
        file_length_ = 0;

        range_info_ = RangeInfo::Parse(http_request->GetProperty("Range"));

        if (!range_info_) {
            RANGE_DEBUG("RangeInfo Should not be NULL!");
        }
        else {
            playing_position_ = (range_info_->GetRangeBegin() / 1024) * 1024;
            RANGE_DEBUG("playing_position = " << playing_position_);
        }

        is_running_ = true;
    }

    void RangeProxySender::Start(uint32_t start_possition)
    {
        assert(!"RangeProxySender::Start(uint32_t start_possition)");
    }

    void RangeProxySender::Stop()
    {
        if (is_running_ == false) return;

        RANGE_DEBUG("");

        if (http_server_socket_)
        {
            http_server_socket_.reset();
        }
        if (proxy_connection_)
        {
            proxy_connection_.reset();
        }
        if (http_request_)
        {
            http_request_.reset();
        }
        if (range_info_)
        {
            range_info_.reset();
        }

        is_running_ = false;
    }

    void RangeProxySender::OnTcpSendSucced(uint32_t length)
    {
        if (is_running_ == false) return;
    }

    void RangeProxySender::OnDownloadDriverError(uint32_t error_code)
    {
        if (is_running_ == false) return;

        string proxy_script_text = "not find";
        http_server_socket_->HttpSendContent(proxy_script_text);
    }

    void RangeProxySender::OnAsyncGetSubPieceSucced(uint32_t start_position, base::AppBuffer buffer)
    {
        if (is_running_ == false) return;
        assert(file_length_ > 0);
        assert(playing_position_ <= file_length_);
        // assert(playing_position_ == start_position);

        if (playing_position_ >= start_position + buffer.Length())
            return;

        // check
        if (range_info_)
        {
            uint32_t range_begin = range_info_->GetRangeBegin();
            uint32_t range_end = range_info_->GetRangeEnd();
            RANGE_DEBUG("RangeInfo, begin = " << range_begin << ", end = " << range_end << ", start_position = " << start_position << ", buffer.length = " << buffer.Length());
            if (start_position <= range_begin && range_begin < start_position + buffer.Length())
            {
                RANGE_DEBUG("start_position <= range_begin && range_begin < start_position + buffer.Length()");
                if (range_end + 1 < start_position + buffer.Length())
                {
                    RANGE_DEBUG("range_end + 1 < start_position + buffer.Length(), jump to end of file");
                    http_server_socket_->HttpSendBuffer(buffer.Data() + (range_begin - start_position), range_end - range_begin + 1);
                    playing_position_ = file_length_;
                }
                else
                {
                    http_server_socket_->HttpSendBuffer(buffer.Data() + (range_begin - start_position), start_position + buffer.Length() - range_begin);
                    playing_position_ += buffer.Length();
                }
            }
            else if (start_position >= range_begin && start_position <= range_end)
            {
                RANGE_DEBUG("start_position >= range_begin && start_position <= range_end");
                if (range_end + 1 < start_position + buffer.Length())
                {
                    RANGE_DEBUG("range_end + 1 < start_position + buffer.Length(), jump to end of file");
                    http_server_socket_->HttpSendBuffer(buffer.Data(), range_end - start_position + 1);
                    playing_position_ = file_length_;
                }
                else
                {
                    http_server_socket_->HttpSendBuffer(buffer);
                    playing_position_ += buffer.Length();
                }
            }
            else if (start_position > range_end)
            {
                RANGE_DEBUG("start_position > range_end, jump to end of file");
                playing_position_ = file_length_;
            }
        }
        else
        {
            RANGE_DEBUG("Send protocol::SubPieceContent to: " << http_server_socket_->GetEndPoint() << " start_possition: " << start_position << " buffer_length: " << buffer.Length());

            http_server_socket_->HttpSendBuffer(buffer);

            // playing_position ??????????? buffer.Length()
            playing_position_ += buffer.Length();
        }

        if (playing_position_ == file_length_)
        {
            RANGE_DEBUG("playing_position_ == file_length_ send \\r\\n\\r\\n");
        }
    }

    void RangeProxySender::OnNoticeGetContentLength(uint32_t content_length, network::HttpResponse::p http_response)
    {
        if (is_running_ == false) return;

        if (is_response_header_ == true)
        {
            return;
        }

        RANGE_DEBUG("Endpoint = " << http_server_socket_->GetEndPoint() << " content_length: " << content_length);

        file_length_ = content_length;

        if (range_info_)
        {
            if (range_info_->GetRangeEnd() == RangeInfo::npos)
            {
                range_info_->SetRangeEnd(file_length_ - 1);
                RANGE_DEBUG("RangeEnd == npos, SetRangeEnd = " << range_info_->GetRangeEnd());
            }
            else if (range_info_->GetRangeEnd() >= file_length_)
            {
                range_info_->SetRangeEnd(file_length_ - 1);
                RANGE_DEBUG("RangeEnd >= file_length_, SetRangeEnd = " << range_info_->GetRangeEnd());
            }
        }

        if (http_response)
        {
            SendHttpHeader(http_response);
        }
    }

    // ???
    void RangeProxySender::OnNotice403Header()
    {
        if (false == is_running_)
            return;
        if (true == is_response_header_)
            return;

        RANGE_DEBUG("Notice 403 header");

        http_server_socket_->HttpSend403Header();
    }

    void RangeProxySender::OnNoticeOpenServiceHeadLength(uint32_t head_length)
    {
        if (false == is_running_)
            return;
    }

    void RangeProxySender::OnRecvSubPiece(uint32_t position, std::vector<base::AppBuffer> const & buffers)
    {
        if (!is_response_header_)
        {
            SendHttpHeader(network::HttpResponse::p());
        }

        for (uint32_t i = 0; i < buffers.size(); ++i)
        {
            OnAsyncGetSubPieceSucced(position, buffers[i]);
            position += buffers[i].Length();
        }
    }

    uint32_t RangeProxySender::GetStartOffset()
    {
        if (false == is_running_)
            return 0;
        if (range_info_) {
            return range_info_->GetRangeBegin();
        }
        return 0;
    }

    void RangeProxySender::SendHttpHeader(network::HttpResponse::p http_response)
    {
        if (range_info_)
        {
            // range
            std::ostringstream oss_range;
            oss_range << "bytes " << range_info_->GetRangeBegin() << "-"
                << range_info_->GetRangeEnd() << "/" << file_length_;

            if (!http_response || http_response->GetStatusCode() == 200)
            {
                //
                std::ostringstream response_str;
                response_str << "HTTP/1.1 206 Partial Content\r\n";
                if (!http_response || http_response->GetContentType().length() == 0)
                {
                    response_str << "Content-Type: video/flv\r\n";
                }
                else
                {
                    response_str << "Content-Type: " << http_response->GetContentType() << "\r\n";
                }
                response_str << "\r\n";
                uint32_t header_length = 0;
                http_response = network::HttpResponse::ParseFromBuffer(response_str.str(), header_length);
            }

            http_response->SetContentLength(range_info_->GetRangeEnd() - range_info_->GetRangeBegin() + 1);
            http_response->SetProperty("Connection", "close");
            http_response->SetProperty("Content-Range", oss_range.str());
            
            assert(proxy_connection_->GetDownloadDriver());
            http_response->SetProperty("LocalPlay", proxy_connection_->GetDownloadDriver()->IsDragLocalPlay() ? "yes" : "no");
            
            // send
            RANGE_DEBUG("Send response string: \n" << http_response->ToString());
            http_server_socket_->HttpSendHeader(http_response->ToString());
        }
        else
        {
            if (!http_response)
            {
                http_server_socket_->HttpSendHeader(file_length_, "video/flv");
            }
            else
            {
                http_response->SetProperty("Connection", "close");
                RANGE_DEBUG("Send response string: \n" << http_response->ToString());
                http_server_socket_->HttpSendHeader(http_response->ToString());
            }
        }

        is_response_header_ = true;
    }
}
