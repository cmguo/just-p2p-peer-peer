//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// CommonProxySender.h

#ifndef _P2SP_PROXY_COMMON_PROXY_SENDER_H_
#define _P2SP_PROXY_COMMON_PROXY_SENDER_H_

#include "p2sp/proxy/ProxySender.h"
#include "network/HttpServer.h"

namespace p2sp
{
    class CommonProxySender
        : public boost::noncopyable
        , public boost::enable_shared_from_this<CommonProxySender>
        , public ProxySender
#ifdef DUMP_OBJECT
        , public count_object_allocate<CommonProxySender>
#endif
    {
    public:
        typedef boost::shared_ptr<CommonProxySender> p;
        static p create(
            boost::asio::io_service & io_svc,
            network::HttpServer::pointer http_server_socket)
        {
            return p(new CommonProxySender(io_svc, http_server_socket));
        }
    public:
        // 方法
        virtual void Start();
        virtual void Start(network::HttpRequest::p http_request, ProxyConnection__p proxy_connection);
        virtual void Start(uint32_t start_possition);
        virtual void Stop();  // do not stop http_server_socket
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
        virtual void OnRecvSubPiece(uint32_t position, std::vector<base::AppBuffer> const & buffers);
        // 获得Contentlength
        virtual void OnNoticeGetContentLength(uint32_t content_length, network::HttpResponse::p http_response);
        // 失败
        virtual void OnNotice403Header();

        virtual void OnNoticeOpenServiceHeadLength(uint32_t head_length);
    private:
        boost::asio::io_service & io_svc_;
        network::HttpServer::pointer http_server_socket_;
        volatile bool is_running_;
        uint32_t playing_position_;
        bool is_response_header_;
        uint32_t file_length_;

        // FILE* fp_;
    private:
        CommonProxySender(
            boost::asio::io_service & io_svc,
            network::HttpServer::pointer http_server_socket)
            : io_svc_(io_svc_)
            , http_server_socket_(http_server_socket)
            , is_running_(false)
            , playing_position_(0)
            , is_response_header_(false)
            , file_length_(0)
        {
        }
    };
}

#endif  // _P2SP_PROXY_COMMON_PROXY_SENDER_H_
