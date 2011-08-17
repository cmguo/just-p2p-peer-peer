//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _LIVE_PROXY_SENDER_H_
#define _LIVE_PROXY_SENDER_H_

#include "p2sp/proxy/ProxySender.h"
#include "network/HttpServer.h"

namespace p2sp
{
    class LiveProxySender
        : public boost::noncopyable
        , public boost::enable_shared_from_this<LiveProxySender>
        , public ProxySender
#ifdef DUMP_OBJECT
        , public count_object_allocate<LiveProxySender>
#endif
    {
    public:
        typedef boost::shared_ptr<LiveProxySender> p;
        static p create(network::HttpServer::pointer http_server_socket)
        {
            return p(new LiveProxySender(http_server_socket));
        }

    public:
        virtual void Start();
        virtual void Start(network::HttpRequest::p http_request, ProxyConnection__p proxy_connection);
        virtual void Start(uint32_t start_possition);
        virtual void Stop();

    public:
        virtual uint32_t GetPlayingPosition() const { assert(false); return 0; }
        virtual void SendHttpRequest() {assert(0);}
        virtual void ResetPlayingPosition() { assert(false); }
        virtual bool IsHeaderResopnsed() const { return is_response_header_; }

    public:
        virtual void OnClose();
        virtual void OnRecvSubPiece(uint32_t position, std::vector<base::AppBuffer> const & buffers);
        virtual void OnNoticeGetContentLength(uint32_t content_length, network::HttpResponse::p http_response);
        virtual void OnNotice403Header();
        virtual void OnNoticeOpenServiceHeadLength(uint32_t head_length);

    private:
        network::HttpServer::pointer http_server_socket_;
        volatile bool is_running_;
        bool is_response_header_;

    private:
        LiveProxySender(network::HttpServer::pointer http_server_socket) 
            : http_server_socket_(http_server_socket)
            , is_running_(false)
            , is_response_header_(false)
        {
        }
    };
}

#endif // _P2SP_PROXY_COMMON_PROXY_SENDER_H_