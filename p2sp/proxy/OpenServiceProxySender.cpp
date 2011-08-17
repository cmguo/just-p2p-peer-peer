//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/proxy/OpenServiceProxySender.h"
#include "p2sp/proxy/ProxySender.h"
#include "p2sp/proxy/ProxyConnection.h"

#include "p2sp/AppModule.h"
#include "p2sp/download/DownloadDriver.h"

#define OPEN_DEBUG(message) LOG(__WARN, "openservice", __FUNCTION__ << " " << message)

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("proxy");

    OpenServiceProxySender::p OpenServiceProxySender::create(network::HttpServer::pointer http_server_socket, ProxyConnection::p proxy_connection)
    {
        return OpenServiceProxySender::p(new OpenServiceProxySender(http_server_socket, proxy_connection));
    }

    //
    OpenServiceProxySender::OpenServiceProxySender(network::HttpServer::pointer http_server_socket, ProxyConnection::p proxy_connection)
        : http_server_socket_(http_server_socket)
        , proxy_connection_(proxy_connection)
        , is_running_(false)
        , is_response_header_(false)
        , file_length_(0)
        , header_offset_in_first_piece_(0)
        , playing_position_(0)
    {
    }

    // 锟斤拷锟斤拷
    void OpenServiceProxySender::Start()
    {
        assert(0);
    }
    void OpenServiceProxySender::Start(network::HttpRequest::p http_request, ProxyConnection__p proxy_connection)
    {
        assert(0);
    }
    void OpenServiceProxySender::Start(uint32_t start_offset)  // should be a correct key_frame_position
    {
        if (true == is_running_)
            return;

        is_running_ = true;

        is_sent_first_piece_ = false;

        is_response_header_ = false;

        playing_position_ = 0;

        start_offset_ = start_offset;

        head_length_ = (uint32_t)-1;  // init

        file_length_ = 0;

        OPEN_DEBUG("start_offset=" << start_offset_);
    }

    void OpenServiceProxySender::Stop()
    {
        if (false == is_running_)
            return;

        if (http_server_socket_)
        {
            http_server_socket_.reset();
        }
        if (proxy_connection_)
        {
            proxy_connection_.reset();
        }

        head_buffer_ = base::AppBuffer();

        if (header_response_template_)
        {
            header_response_template_.reset();
        }

        is_running_ = false;
    }

    void OpenServiceProxySender::OnRecvSubPiece(uint32_t position, std::vector<base::AppBuffer> const & buffers)
    {
        for (uint32_t i = 0; i < buffers.size(); ++i)
        {
            if (playing_position_ == position)
            {
                OnAsyncGetSubPieceSucced(position, buffers[i]);
                position += buffers[i].Length();
            }
        }
    }
    // 播放数据
    void OpenServiceProxySender::OnAsyncGetSubPieceSucced(uint32_t start_position, base::AppBuffer buffer)
    {
        if (false == is_running_)
            return;

        assert(file_length_ > 0);
        assert(playing_position_ < file_length_);
        assert(playing_position_ == start_position);

        if (head_length_ == (uint32_t)-1)
        {
            OPEN_DEBUG("head_length==-1 start_position=" << start_position << " buffer_length=" << buffer.Length());
            // wait for header response
            return;
        }
        else if (false == is_response_header_)
        {
            if (head_length_ > start_position + buffer.Length())  // copy head buffer
            {
                base::util::memcpy2(head_buffer_.Data() + start_position, head_buffer_.Length() - start_position, buffer.Data(), buffer.Length());
                playing_position_ += buffer.Length();
            }
            else if (head_length_ >= start_position && head_length_ <= start_position + buffer.Length())
            {
                // store rest of the head
                uint32_t remain_length = (head_length_ - start_position);
                if (remain_length > 0) {
                    base::util::memcpy2(head_buffer_.Data() + start_position, head_buffer_.Length() - start_position, buffer.Data(), remain_length);
                }

                base::AppBuffer media_header = proxy_connection_->GetHeader(key_frame_position_, head_buffer_);
                header_offset_in_first_piece_ = (key_frame_position_ % 1024);

                // calc content length
                uint32_t content_length = file_length_ - key_frame_position_ + media_header.Length();

                OPEN_DEBUG("media_header_length=" << media_header.Length() << " content_length=" << content_length);

                // send http header
                if (!header_response_template_) {
                    http_server_socket_->HttpSendHeader(content_length, "video/x-flv");
                }
                else if (header_response_template_->GetStatusCode() != 200) {
                    http_server_socket_->HttpSendHeader(content_length, "video/x-flv");
                }
                else {
                    // remove Content-Range & Accept-Ranges properties
                    header_response_template_->RemoveProperty("Content-Range");
                    header_response_template_->RemoveProperty("Accept-Ranges");
                    // add no cache
                    header_response_template_->SetPragma("no-cache", "");
                    header_response_template_->SetProperty("Cache-Control", "no-cache");
                    header_response_template_->SetProperty("Expires", "0");
                    // connection close
                    header_response_template_->SetProperty("Connection", "close");
                    // std::set content length
                    header_response_template_->SetContentLength(content_length);
                    // send head
                    OPEN_DEBUG("mock response:\n" << header_response_template_->ToString());
                    // send
                    http_server_socket_->HttpSendHeader(header_response_template_->ToString());
                }

                // send media header
                if (media_header.Length() > 0) {
                    http_server_socket_->HttpSendBuffer(media_header);
                }

                is_response_header_ = true;

                // re-calc playing position
                playing_position_ = (key_frame_position_ / 1024) * 1024;

                // check whether send this piece
                if (playing_position_ == start_position) {
                    http_server_socket_->HttpSendBuffer(buffer.Data() + header_offset_in_first_piece_, buffer.Length() - header_offset_in_first_piece_);
                    is_sent_first_piece_ = true;
                    playing_position_ += buffer.Length();
                }
            }
            // todo!!!!!
        }
        else
        {
            if (false == is_sent_first_piece_) {
                http_server_socket_->HttpSendBuffer(buffer.Data() + header_offset_in_first_piece_, buffer.Length() - header_offset_in_first_piece_);
                is_sent_first_piece_ = true;
            } else {
                http_server_socket_->HttpSendBuffer(buffer);
            }
            // incr
            playing_position_ += buffer.Length();
        }
    }

    // 获得Contentlength
    void OpenServiceProxySender::OnNoticeGetContentLength(uint32_t content_length, network::HttpResponse::p http_response)
    {
        if (false == is_running_)
            return;

        // KKK("file_length = ", content_length);
        // original file length from openservice rid info
        if (file_length_ == 0) {
            file_length_ = content_length;
            header_response_template_ = http_response;
        }
        // not a range response
        else if (content_length == file_length_) {
            header_response_template_ = http_response;
        }
        else {
            OPEN_DEBUG(" rid_content_length=" << file_length_ << " server_content_length=" << content_length);
            // uint32_t i = file_length_;
        }

        OPEN_DEBUG("file_length=" << file_length_);
    }
    // 失败
    void OpenServiceProxySender::OnNotice403Header()
    {
        if (false == is_running_)
            return;

        if (true == is_response_header_)
            return;

        OPEN_DEBUG("");

        http_server_socket_->HttpSend403Header();
    }

    void OpenServiceProxySender::OnNoticeOpenServiceHeadLength(uint32_t head_length)
    {
        if (false == is_running_)
            return;

        if (head_length <= 16 * 1024 * 1024)
        {
            head_length_ = head_length;
            key_frame_position_ = start_offset_ /*+ head_length_*/;
            header_offset_in_first_piece_ = (key_frame_position_ % 1024);

            // to be fix
            // head_buffer_.Malloc(head_length_);
            head_buffer_.Malloc(head_length_);

            OPEN_DEBUG("head_length=" << head_length << " key_frame=" << key_frame_position_ << " head_offset=" << header_offset_in_first_piece_);
        }
        else
        {
            OPEN_DEBUG("head_length=" << head_length << "(Too Large)");
        }
    }
}
