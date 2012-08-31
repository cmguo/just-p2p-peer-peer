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

namespace p2sp
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_range_proxy = log4cplus::Logger::getInstance("[range_proxy]");
#endif

    void RangeProxySender::Start()
    {
        LOG4CPLUS_DEBUG_LOG(logger_range_proxy, "Invalid Start Function Call!!!");
        assert(!"RangeProxySender::Start()");
    }

    void RangeProxySender::Start(network::HttpRequest::p http_request, ProxyConnection__p proxy_connection)
    {
        if (is_running_ == true) return;

        RangeInfo::p range_info = RangeInfo::Parse(http_request->GetProperty("Range"));
        Start(range_info, proxy_connection);
    }

    void RangeProxySender::Start(RangeInfo::p range_info, ProxyConnection__p proxy_connection)
    {
        if (is_running_ == true) return;

        LOG4CPLUS_DEBUG_LOG(logger_range_proxy, "");
        proxy_connection_ = proxy_connection;
        range_info_ = range_info;
        if (!range_info_) 
        {
            LOG4CPLUS_DEBUG_LOG(logger_range_proxy, "RangeInfo Should not be NULL!");
            assert(false);
        }
        else
        {
            playing_position_ = (range_info_->GetRangeBegin() / 1024) * 1024;
            LOG4CPLUS_DEBUG_LOG(logger_range_proxy, "playing_position = " << playing_position_);
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

        LOG4CPLUS_DEBUG_LOG(logger_range_proxy, "");

        if (http_server_socket_)
        {
            http_server_socket_.reset();
        }
        if (proxy_connection_)
        {
            proxy_connection_.reset();
        }
        if (range_info_)
        {
            range_info_.reset();
        }

        is_running_ = false;
    }

    void RangeProxySender::OnAsyncGetSubPieceSucced(uint32_t start_position, const base::AppBuffer & buffer)
    {
        if (is_running_ == false) return;
        assert(file_length_ > 0);
        assert(playing_position_ <= file_length_);

        if (playing_position_ >= start_position + buffer.Length())
            return;

        // check
        if (range_info_)
        {
            uint32_t range_begin = range_info_->GetRangeBegin();
            uint32_t range_end = range_info_->GetRangeEnd();
            LOG4CPLUS_DEBUG_LOG(logger_range_proxy, "RangeInfo, begin = " << range_begin << ", end = " << range_end 
                << ", start_position = " << start_position << ", buffer.length = " << buffer.Length());
            if (start_position <= range_begin && range_begin < start_position + buffer.Length())
            {
                LOG4CPLUS_DEBUG_LOG(logger_range_proxy, 
                    "start_position <= range_begin && range_begin < start_position + buffer.Length()");
                if (range_end + 1 < start_position + buffer.Length())
                {
                    LOG4CPLUS_DEBUG_LOG(logger_range_proxy, 
                        "range_end + 1 < start_position + buffer.Length(), jump to end of file");
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
                LOG4CPLUS_DEBUG_LOG(logger_range_proxy, "start_position >= range_begin && start_position <= range_end");
                if (range_end + 1 < start_position + buffer.Length())
                {
                    LOG4CPLUS_DEBUG_LOG(logger_range_proxy, 
                        "range_end + 1 < start_position + buffer.Length(), jump to end of file");
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
                LOG4CPLUS_DEBUG_LOG(logger_range_proxy, "start_position > range_end, jump to end of file");
                playing_position_ = file_length_;
            }
        }
        else
        {
            LOG4CPLUS_DEBUG_LOG(logger_range_proxy, "Send protocol::SubPieceContent to: " 
                << http_server_socket_->GetEndPoint() << " start_possition: " << start_position 
                << " buffer_length: " << buffer.Length());

            http_server_socket_->HttpSendBuffer(buffer);
            playing_position_ += buffer.Length();
        }

        if (playing_position_ == file_length_)
        {
            LOG4CPLUS_DEBUG_LOG(logger_range_proxy, "playing_position_ == file_length_ send \\r\\n\\r\\n");
        }
    }

    void RangeProxySender::OnNoticeGetContentLength(uint32_t content_length, network::HttpResponse::p http_response)
    {
        if (is_running_ == false) return;

        if (is_response_header_ == true)
        {
            return;
        }

        LOG4CPLUS_DEBUG_LOG(logger_range_proxy, "Endpoint = " << http_server_socket_->GetEndPoint() 
            << " content_length: " << content_length);
        file_length_ = content_length;
        if (range_info_)
        {
            if (range_info_->GetRangeEnd() == RangeInfo::npos)
            {
                range_info_->SetRangeEnd(file_length_ - 1);
                LOG4CPLUS_DEBUG_LOG(logger_range_proxy, "RangeEnd == npos, SetRangeEnd = " 
                    << range_info_->GetRangeEnd());
            }
            else if (range_info_->GetRangeEnd() >= file_length_)
            {
                range_info_->SetRangeEnd(file_length_ - 1);
                LOG4CPLUS_DEBUG_LOG(logger_range_proxy, "RangeEnd >= file_length_, SetRangeEnd = " 
                    << range_info_->GetRangeEnd());
            }
        }

        if (http_response)
        {
            SendHttpHeader(http_response);
        }
    }

    void RangeProxySender::OnNoticeOpenServiceHeadLength(uint32_t head_length)
    {
        if (false == is_running_)
            return;
    }

    void RangeProxySender::OnRecvSubPiece(uint32_t position, std::vector<base::AppBuffer> const & buffers)
    {
        assert(!buffers.empty());
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
            http_response->SetProperty("LocalPlay", proxy_connection_->GetDownloadDriver()->IsDragLocalPlayForClient() ? "yes" : "no");
            
            // send
            LOG4CPLUS_DEBUG_LOG(logger_range_proxy, "Send response string: \n" << http_response->ToString());
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
                LOG4CPLUS_DEBUG_LOG(logger_range_proxy, "Send response string: \n" << http_response->ToString());
                http_server_socket_->HttpSendHeader(http_response->ToString());
            }
        }

        is_response_header_ = true;
    }
}
