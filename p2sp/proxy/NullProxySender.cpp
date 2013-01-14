//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "NullProxySender.h"
#include "ProxyConnection.h"

namespace p2sp
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_null_proxy_sender = log4cplus::Logger::getInstance("[null_proxy_sender]");
#endif

    NullProxySender::NullProxySender(ProxyConnection__p proxy_connection)
        : proxy_connection_(proxy_connection)
        , is_running_(false)
        , is_response_header_(false)
        , playing_position_(0)
    {
    }

    NullProxySender::p NullProxySender::create(ProxyConnection__p proxy_connection)
    {
        return NullProxySender::p(new NullProxySender(proxy_connection));
    }

    void NullProxySender::Start()
    {
        if (true == is_running_) {
            return;
        }
        is_running_ = true;

        // !
        playing_position_ = 0;
        is_response_header_ = false;
        LOG4CPLUS_DEBUG_LOG(logger_null_proxy_sender, "");
    }

    void NullProxySender::Start(network::HttpRequest::p http_request, ProxyConnection__p proxy_connection)
    {
        assert(0);
    }

    void NullProxySender::Start(boost::uint32_t start_possition)
    {
        assert(0);
    }

    void NullProxySender::Stop()
    {
        if (false == is_running_) {
            return;
        }

        // !
        if (proxy_connection_) {
            proxy_connection_.reset();
        }

        is_running_ = false;
    }

    // 获得Contentlength
    void NullProxySender::OnNoticeGetContentLength(boost::uint32_t content_length, network::HttpResponse::p http_response)
    {
        if (false == is_running_) {
            return;
        }
        is_response_header_ = true;
        file_length_ = content_length;
        LOG4CPLUS_DEBUG_LOG(logger_null_proxy_sender, "response file_length = " << content_length);
    }

    // 失败
    void NullProxySender::OnNotice403Header()
    {
        if (false == is_running_) {
            return;
        }

    }

    void NullProxySender::OnNoticeOpenServiceHeadLength(boost::uint32_t head_length)
    {
        if (false == is_running_) {
            return;
        }

    }

    void NullProxySender::OnRecvSubPiece(boost::uint32_t position, std::vector<base::AppBuffer> const & buffers)
    {
        if (false == is_running_) {
            return;
        }
        LOG4CPLUS_DEBUG_LOG(logger_null_proxy_sender, ">> playing_position_ = " << playing_position_ << 
            ", file_length_ = " << file_length_ << ", buffers.count = " << buffers.size());

        for (boost::uint32_t i = 0; i < buffers.size(); ++i) {
            playing_position_ += buffers[i].Length();
        }

        if (playing_position_ == file_length_)
        {
            LOG4CPLUS_WARN_LOG(logger_null_proxy_sender, 
                "CommonProxySender::OnRecvSubPiece playing_position_ == file_length_ send \\r\\n\\r\\n");
        }
    }
}
