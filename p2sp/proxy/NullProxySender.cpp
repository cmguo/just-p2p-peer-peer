//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "NullProxySender.h"
#include "ProxyConnection.h"

#define NL_DEBUG(msg) LOG(__DEBUG, "downloadcenter", __FUNCTION__ << ":" << __LINE__ << " " << msg);

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("downloadcenter");

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
        NL_DEBUG("");
    }

    void NullProxySender::Start(network::HttpRequest::p http_request, ProxyConnection__p proxy_connection)
    {
        assert(0);
    }

    void NullProxySender::Start(uint32_t start_possition)
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

    void NullProxySender::OnTcpSendSucced(uint32_t length)
    {
        if (false == is_running_) {
            return;
        }

    }

    // 下载器
    void NullProxySender::OnDownloadDriverError(uint32_t error_code)
    {

    }

    // 播放数据
    void NullProxySender::OnAsyncGetSubPieceSucced(uint32_t start_position, base::AppBuffer buffer)
    {
        if (false == is_running_) {
            return;
        }
        assert(playing_position_ == start_position);
        playing_position_ += buffer.Length();
        if (playing_position_ % (128*1024) == 0 || playing_position_ >= file_length_) {
            NL_DEBUG("file_length_ " << file_length_ << ", playing_position = " << playing_position_);
        }
        // check
        if (playing_position_ >= file_length_) {
            // close
            NL_DEBUG("Post ProxyConnection::WillStop, " << proxy_connection_);
            // just notify, not stop; connection will be stopped in OnProxyTimer
            if (proxy_connection_)
            {
                // MainThread::Post(boost::bind(&ProxyConnection::NotifyStop, proxy_connection_));
                proxy_connection_->NotifyStop();
            }
        }
    }

    // 获得Contentlength
    void NullProxySender::OnNoticeGetContentLength(uint32_t content_length, network::HttpResponse::p http_response)
    {
        if (false == is_running_) {
            return;
        }
        is_response_header_ = true;
        file_length_ = content_length;
        NL_DEBUG("response file_length = " << content_length);
    }

    // 失败
    void NullProxySender::OnNotice403Header()
    {
        if (false == is_running_) {
            return;
        }

    }

    void NullProxySender::OnNoticeOpenServiceHeadLength(uint32_t head_length)
    {
        if (false == is_running_) {
            return;
        }

    }

    void NullProxySender::OnRecvSubPiece(uint32_t position, std::vector<base::AppBuffer> const & buffers)
    {
        if (false == is_running_) {
            return;
        }
        LOG(__DEBUG, "proxy", ">> playing_position_ = " << playing_position_ << ", file_length_ = " << file_length_ << ", buffers.count = " << buffers.size());

        for (uint32_t i = 0; i < buffers.size(); ++i) {
            playing_position_ += buffers[i].Length();
        }

        if (playing_position_ == file_length_)
        {
            LOG(__WARN, "proxy", "CommonProxySender::OnRecvSubPiece playing_position_ == file_length_ send \\r\\n\\r\\n");
        }
    }

    uint32_t NullProxySender::GetStartOffset()
    {
        return 0;
    }
}
