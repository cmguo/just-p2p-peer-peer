//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// RangeProxySender.h

#ifndef _P2SP_PROXY_RANGE_PROXY_SENDER_H_
#define _P2SP_PROXY_RANGE_PROXY_SENDER_H_

#include "p2sp/proxy/ProxySender.h"

namespace p2sp
{
    class RangeInfo;
    typedef boost::shared_ptr<RangeInfo> RangeInfo__p;

    class RangeProxySender
        : public boost::noncopyable
        , public boost::enable_shared_from_this<RangeProxySender>
        , public ProxySender
#ifdef DUMP_OBJECT
        , public count_object_allocate<RangeProxySender>
#endif
    {
    public:
        typedef boost::shared_ptr<RangeProxySender> p;
        static p create(network::HttpServer::pointer http_server_socket) { return p(new RangeProxySender(http_server_socket)); }
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
        // 下载器
        virtual void OnDownloadDriverError(uint32_t error_code);
        // 播放数据
        virtual void OnAsyncGetSubPieceSucced(uint32_t start_position, base::AppBuffer buffer);
        // 获得Contentlength
        virtual void OnNoticeGetContentLength(uint32_t content_length, network::HttpResponse::p http_response);
        // 失败
        virtual void OnNotice403Header();

        virtual void OnNoticeOpenServiceHeadLength(uint32_t head_length);
        virtual uint32_t GetStartOffset();
        virtual void OnRecvSubPiece(uint32_t position, std::vector<base::AppBuffer> const & buffers);

    private:
        void SendHttpHeader(network::HttpResponse::p response);

    private:
        network::HttpServer::pointer http_server_socket_;
        network::HttpRequest::p http_request_;
        ProxyConnection__p proxy_connection_;

        volatile bool is_running_;
        uint32_t playing_position_;
        bool is_response_header_;
        uint32_t file_length_;
        RangeInfo__p range_info_;

    private:
        RangeProxySender(network::HttpServer::pointer http_server_socket)
            : http_server_socket_(http_server_socket), is_running_(false), playing_position_(0)
            , is_response_header_(false), file_length_(0)
        {
        }
    };
}

#endif  // _P2SP_PROXY_RANGE_PROXY_SENDER_H_
