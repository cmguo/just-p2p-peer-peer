//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// Mp4DragProxySender.h

#ifndef _P2SP_PROXY_MP4_DRAG_PROXY_SENDER_H_
#define _P2SP_PROXY_MP4_DRAG_PROXY_SENDER_H_

#include "p2sp/proxy/ProxySender.h"

namespace p2sp
{
    /**
    *
    */
    class Mp4DragProxySender
        : public boost::noncopyable
        , public boost::enable_shared_from_this<Mp4DragProxySender>
        , public ProxySender
#ifdef DUMP_OBJECT
        , public count_object_allocate<Mp4DragProxySender>
#endif
    {
    public:
        typedef boost::shared_ptr<Mp4DragProxySender> p;

    public:
        static Mp4DragProxySender::p create(
            boost::asio::io_service & io_svc,
            network::HttpServer::pointer http_server_socket_,
            ProxyConnection__p proxy_connection);

    public:
        // 方法
        virtual void Start();
        virtual void Start(network::HttpRequest::p http_request, ProxyConnection__p proxy_connection);
        virtual void Start(uint32_t start_possition);
        virtual void Stop();

    public:
        // 属性
        virtual uint32_t GetPlayingPosition() const { return playing_position_; }
        virtual void SendHttpRequest() {assert(0);}
        virtual void ResetPlayingPosition() { playing_position_ = 0; }
        virtual bool IsHeaderResopnsed() const { return is_response_header_; }

    public:
        // 消息
        // network::HttpServer
        virtual void OnTcpSendSucced(uint32_t length);
        // 播放数据
        virtual void OnAsyncGetSubPieceSucced(uint32_t start_position, base::AppBuffer buffer);
        // 获得Contentlength
        virtual void OnNoticeGetContentLength(uint32_t content_length, network::HttpResponse::p http_response);
        // 失败
        virtual void OnNotice403Header();

        virtual void OnNoticeOpenServiceHeadLength(uint32_t head_length);

        virtual void OnRecvSubPiece(uint32_t position, std::vector<base::AppBuffer> const & buffers);

    private:
        boost::asio::io_service & io_svc_;
        network::HttpServer::pointer http_server_socket_;

        volatile bool is_running_;
        volatile bool is_response_header_;
        volatile bool is_sent_first_piece_;
        volatile bool is_received_first_piece_;

        uint32_t file_length_;
        uint32_t header_offset_in_first_piece_;
        uint32_t key_frame_position_;
        volatile uint32_t playing_position_;
        uint32_t start_offset_;
        volatile uint32_t head_length_;

        ProxyConnection__p proxy_connection_;

        base::AppBuffer head_buffer_;
        network::HttpResponse::p header_response_template_;

    private:
        Mp4DragProxySender(
            boost::asio::io_service & io_svc,
            network::HttpServer::pointer http_server_socket,
            ProxyConnection__p proxy_connection);
    };

}

#endif  // _P2SP_PROXY_MP4_DRAG_PROXY_SENDER_H_
