//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// OpenServiceProxySender.h

#ifndef _P2SP_PROXY_OPEN_SERVICE_PROXY_SENDER_H_
#define _P2SP_PROXY_OPEN_SERVICE_PROXY_SENDER_H_

#include "p2sp/proxy/ProxySender.h"

namespace p2sp
{
    /**
     *
     */
    class OpenServiceProxySender
        : public boost::noncopyable
        , public boost::enable_shared_from_this<OpenServiceProxySender>
        , public ProxySender
#ifdef DUMP_OBJECT
        , public count_object_allocate<OpenServiceProxySender>
#endif
    {
    public:
        typedef boost::shared_ptr<OpenServiceProxySender> p;

    public:
        static OpenServiceProxySender::p create(network::HttpServer::pointer http_server_socket_, ProxyConnection__p proxy_connection);

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
        // 下载器
        virtual void OnDownloadDriverError(uint32_t error_code);
        // 播放数据
        virtual void OnAsyncGetSubPieceSucced(uint32_t start_position, base::AppBuffer buffer);
        // 获得Contentlength
        virtual void OnNoticeGetContentLength(uint32_t content_length, network::HttpResponse::p http_response);
        // 失败
        virtual void OnNotice403Header();

        virtual void OnNoticeOpenServiceHeadLength(uint32_t head_length);
        virtual void OnRecvSubPiece(uint32_t position, std::vector<base::AppBuffer> const & buffers);
        virtual uint32_t GetStartOffset();

    private:
        network::HttpServer::pointer http_server_socket_;
        ProxyConnection__p proxy_connection_;

        volatile bool is_running_;
        volatile bool is_response_header_;
        volatile bool is_sent_first_piece_;

        uint32_t file_length_;
        uint32_t header_offset_in_first_piece_;
        uint32_t key_frame_position_;
        volatile uint32_t playing_position_;
        uint32_t start_offset_;
        volatile uint32_t head_length_;

        base::AppBuffer head_buffer_;
        network::HttpResponse::p header_response_template_;

    private:
        OpenServiceProxySender(network::HttpServer::pointer http_server_socket, ProxyConnection__p proxy_connection);
    };

}

#endif  // _P2SP_PROXY_OPEN_SERVICE_PROXY_SENDER_H_
