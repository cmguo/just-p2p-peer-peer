//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// NullProxySender.h

#ifndef _P2SP_PROXY_NULL_PROXY_SENDER_H
#define _P2SP_PROXY_NULL_PROXY_SENDER_H

#include "ProxySender.h"

namespace p2sp
{
    /**
     *
     */
    class NullProxySender
        : public boost::noncopyable
        , public boost::enable_shared_from_this<NullProxySender>
        , public ProxySender
#ifdef DUMP_OBJECT
        , public count_object_allocate<NullProxySender>
#endif
    {
    public:
        typedef boost::shared_ptr<NullProxySender> p;

    public:
        static NullProxySender::p create(ProxyConnection__p proxy_connection);

    public:
        // 方法
        virtual void Start();
        virtual void Start(network::HttpRequest::p http_request, ProxyConnection__p proxy_connection);
        virtual void Start(boost::uint32_t start_possition);
        virtual void Stop();

    public:
        // 属性
        virtual boost::uint32_t GetPlayingPosition() const { return playing_position_; }
        virtual void SendHttpRequest() {assert(0);}
        virtual void ResetPlayingPosition() { playing_position_ = 0; }
        virtual bool IsHeaderResopnsed() const { return is_response_header_; }

    public:
        // 获得Contentlength
        virtual void OnNoticeGetContentLength(boost::uint32_t content_length, network::HttpResponse::p http_response);
        // 失败
        virtual void OnNotice403Header();

        virtual void OnNoticeOpenServiceHeadLength(boost::uint32_t head_length);

        virtual void OnRecvSubPiece(boost::uint32_t position, std::vector<base::AppBuffer> const & buffers);

    private:
        ProxyConnection__p proxy_connection_;

        volatile bool is_running_;
        volatile bool is_response_header_;

        boost::uint32_t file_length_;
        volatile boost::uint32_t playing_position_;

    private:
        NullProxySender(ProxyConnection__p proxy_connection);
    };

}

#endif  // _P2SP_PROXY_NULL_PROXY_SENDER_H
