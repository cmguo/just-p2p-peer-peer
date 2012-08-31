//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// ProxySender.h

#ifndef _P2SP_PROXY_PROXY_SENDER_H_
#define _P2SP_PROXY_PROXY_SENDER_H_

#include "storage/IStorage.h"

#include "network/HttpRequest.h"
#include "network/HttpResponse.h"
#include "network/HttpClient.h"
#include "network/HttpServer.h"

namespace p2sp
{
    class DownloadDriver;
    typedef boost::shared_ptr<DownloadDriver> DownloadDriver__p;
    class ProxyConnection;
    typedef boost::shared_ptr<ProxyConnection> ProxyConnection__p;

    class ProxySender
#ifdef DUMP_OBJECT
        : public count_object_allocate<ProxySender>
#endif
    {
public:
        typedef boost::shared_ptr<ProxySender> p;

        virtual void Start() = 0;
        virtual void Start(uint32_t start_possition) = 0;
        virtual void Start(network::HttpRequest::p http_request, ProxyConnection__p proxy_connection) = 0;
        virtual void Stop() = 0;  // do not close http_server_socket

        virtual uint32_t GetPlayingPosition() const = 0;
        virtual void SendHttpRequest() = 0;
        virtual void ResetPlayingPosition() = 0;
        virtual bool IsHeaderResopnsed() const = 0;

        // 播放数据
        virtual void OnRecvSubPiece(uint32_t position, std::vector<base::AppBuffer> const & buffers) = 0;
        // 获得Contentlength
        virtual void OnNoticeGetContentLength(uint32_t content_length, network::HttpResponse::p http_response) = 0;
        // 失败
        virtual void OnNoticeOpenServiceHeadLength(uint32_t head_length) = 0;
    };
}

#endif  // _P2SP_PROXY_PROXY_SENDER_H_
