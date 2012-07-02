//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/AppModule.h"
#include "p2sp/download/DownloadDriver.h"

#include "p2sp/proxy/Mp4DragProxySender.h"
#include "p2sp/proxy/ProxySender.h"
#include "p2sp/proxy/ProxyConnection.h"
#include "p2sp/proxy/FlvDragProxySender.h"
#include "p2sp/proxy/CommonProxySender.h"

#include "storage/format/Mp4Spliter.h"

namespace p2sp
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_mp4_drag_proxy_sender = log4cplus::Logger::getInstance("[mp4_drag_proxy_sender]");
#endif

    Mp4DragProxySender::p Mp4DragProxySender::create(
        boost::asio::io_service & io_svc,
        network::HttpServer::pointer http_server_socket,
        ProxyConnection__p proxy_connection)
    {
        return Mp4DragProxySender::p(new Mp4DragProxySender(io_svc, http_server_socket, proxy_connection));
    }

    //
    Mp4DragProxySender::Mp4DragProxySender(
        boost::asio::io_service & io_svc,
        network::HttpServer::pointer http_server_socket,
        ProxyConnection__p proxy_connection)
        : io_svc_(io_svc)
        , http_server_socket_(http_server_socket)
        , is_running_(false)
        , is_response_header_(false)
        , is_received_first_piece_(false)
        , file_length_(0)
        , header_offset_in_first_piece_(0)
        , playing_position_(0)
        , proxy_connection_(proxy_connection)
    {
    }

    // 锟斤拷锟斤拷
    void Mp4DragProxySender::Start()
    {
        assert(0);
    }
    void Mp4DragProxySender::Start(network::HttpRequest::p http_request, ProxyConnection__p proxy_connection)
    {
        assert(0);
    }
    void Mp4DragProxySender::Start(uint32_t start_offset)
    {
        if (true == is_running_)
            return;

        is_running_ = true;

        is_sent_first_piece_ = false;

        is_received_first_piece_ = false;

        is_response_header_ = false;

        playing_position_ = 0;

        start_offset_ = start_offset;

        head_length_ = (uint32_t)-1;  // init

        file_length_ = 0;

        LOG4CPLUS_DEBUG_LOG(logger_mp4_drag_proxy_sender, "start_offset=" << start_offset_);

    }

    void Mp4DragProxySender::Stop()
    {
        if (false == is_running_)
            return;

        if (http_server_socket_) {
            http_server_socket_.reset();
        }

        if (proxy_connection_) {
            proxy_connection_.reset();
        }

        head_buffer_ = base::AppBuffer();

        if (header_response_template_) {
            header_response_template_.reset();
        }

        is_running_ = false;
    }

    // 播放数据
    void Mp4DragProxySender::OnAsyncGetSubPieceSucced(uint32_t start_position, base::AppBuffer buffer)
    {
        if (false == is_running_)
            return;

        assert(file_length_ > 0);
        assert(playing_position_ < file_length_);
        assert(playing_position_ == start_position);
        // assert(head_length_ != -1);

        if (false == is_received_first_piece_)
        {
            // the first piece
            uint32_t head_length = Mp4Spliter::Mp4HeadLength(base::AppBuffer(buffer.Data(), buffer.Length()));
            // 解析失败
            if (head_length == 0) {

                // notice change proxy sender
                ProxySender::p next_proxy_sender;
                if (buffer.Data()[0] == 'F' && buffer.Data()[1] == 'L' && buffer.Data()[2] == 'V') {
                    // use flv proxy sender
                    next_proxy_sender = FlvDragProxySender::create(io_svc_, http_server_socket_);
                    next_proxy_sender->Start(start_offset_);
                }
                else {
                    next_proxy_sender = CommonProxySender::create(io_svc_, http_server_socket_);
                    next_proxy_sender->Start();
                }
                // change
                next_proxy_sender->OnNoticeGetContentLength(file_length_, header_response_template_);
                // next_proxy_sender->OnAsyncGetSubPieceSucced(start_position, buffer);
                // no reference
                // proxy_connection_->OnNoticeChangeProxySender(next_proxy_sender);
                if (proxy_connection_)
                {
                    // MainThread::Post(boost::bind(
                    //  &ProxyConnection::OnNoticeChangeProxySender, proxy_connection_, next_proxy_sender));
                    proxy_connection_->OnNoticeChangeProxySender(next_proxy_sender);
                }

                // stop current
                Stop();
                return;
            }
            OnNoticeOpenServiceHeadLength(head_length);
            // copy to head buffer
            //       uint32_t remain_length = min(buffer.Length(), head_buffer_.Length() - start_position);
            //       remain_length = (remain_length / 1024) * 1024;
            //       memcpy(head_buffer_.Data() + start_position, buffer.Data(), remain_length);
            //       playing_position_ += remain_length;
            // received
            is_received_first_piece_ = true;
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

                base::AppBuffer media_header = Mp4Spliter::Mp4HeadParse(head_buffer_, key_frame_position_);
                header_offset_in_first_piece_ = (key_frame_position_ % 1024);

                // calc content length
                uint32_t content_length = file_length_ - key_frame_position_ + media_header.Length();

                LOG4CPLUS_DEBUG_LOG(logger_mp4_drag_proxy_sender, "media_header_length=" << media_header.Length() 
                    << " content_length=" << content_length);

                // send http header
                if (!header_response_template_) {
                    http_server_socket_->HttpSendHeader(content_length, "video/x-mp4");
                }
                else if (header_response_template_->GetStatusCode() != 200) {
                    http_server_socket_->HttpSendHeader(content_length, "video/x-mp4");
                }
                else {
                    // remove Content-Range & Accept-Ranges properties
                    header_response_template_->RemoveProperty("Content-Range");
                    header_response_template_->RemoveProperty("Accept-Ranges");
                    // std::set content length
                    header_response_template_->SetContentLength(content_length);
                    // send head
                    LOG4CPLUS_DEBUG_LOG(logger_mp4_drag_proxy_sender, "mock response:\n" 
                        << header_response_template_->ToString());
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
    void Mp4DragProxySender::OnNoticeGetContentLength(uint32_t content_length, network::HttpResponse::p http_response)
    {
        if (false == is_running_)
            return;

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
            LOG4CPLUS_DEBUG_LOG(logger_mp4_drag_proxy_sender, " rid_content_length=" << file_length_ 
                << " server_content_length=" << content_length);
            // uint32_t i = file_length_;
        }

        LOG4CPLUS_DEBUG_LOG(logger_mp4_drag_proxy_sender, "file_length=" << file_length_);
    }
    // 失败
    void Mp4DragProxySender::OnNotice403Header()
    {
        if (false == is_running_)
            return;

        if (true == is_response_header_)
            return;

        LOG4CPLUS_DEBUG_LOG(logger_mp4_drag_proxy_sender, "");

        http_server_socket_->HttpSend403Header();
    }

    void Mp4DragProxySender::OnNoticeOpenServiceHeadLength(uint32_t head_length)
    {
        if (false == is_running_)
            return;

        if (head_length <= 64 * 1024 * 1024)
        {
            head_length_ = head_length;
            key_frame_position_ = start_offset_;
            header_offset_in_first_piece_ = (key_frame_position_ % 1024);

            // to be fix
            // head_buffer_.Malloc(head_length_);

            LOG4CPLUS_DEBUG_LOG(logger_mp4_drag_proxy_sender, "head_length=" << head_length << " key_frame=" 
                << key_frame_position_ << " head_offset=" << header_offset_in_first_piece_);
        }
        else
        {
            LOG4CPLUS_DEBUG_LOG(logger_mp4_drag_proxy_sender, "head_length=" << head_length << "(Too Large)");
        }
    }

    void Mp4DragProxySender::OnRecvSubPiece(uint32_t position, std::vector<base::AppBuffer> const & buffers)
    {
        for (uint32_t i = 0; i < buffers.size(); ++i)
        {
            OnAsyncGetSubPieceSucced(position, buffers[i]);
            position += buffers[i].Length();
        }
    }
}
