//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "message.h"
#include "statistic/StatisticModule.h"
#include "p2sp/proxy/ProxyModule.h"
#include "p2sp/proxy/DirectProxySender.h"
#include "p2sp/proxy/ProxySender.h"
#include "p2sp/proxy/ProxyConnection.h"
#include "p2sp/proxy/MessageBufferManager.h"
#include "p2sp/AppModule.h"
#include "p2sp/download/DownloadDriver.h"

#ifdef PEER_PC_CLIENT
#include "WindowsMessage.h"
#endif

namespace p2sp
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_proxy = log4cplus::Logger::getInstance("[direct_proxy_sender]");
#endif

    void DirectProxySender::Start()
    {
        assert(!"DirectProxySender::Start()");
    }

    void DirectProxySender::Start(network::HttpRequest::p http_request, ProxyConnection::p proxy_connection)
    {
        if (is_running_ == true)
            return;
        LOG4CPLUS_INFO_LOG(logger_proxy, "DirectProxySender::Start");

        is_running_ = true;

        http_request_ = http_request;
        proxy_connection_ = proxy_connection;

        s_id_ = DownloadDriver::GetDownloaderDriverSID();

        downloaddriver_statistic_ = statistic::StatisticModule::Inst()->AttachDownloadDriverStatistic(s_id_, true);

        if (need_bubble_)
        {
            // 状态显示
            assert(downloaddriver_statistic_);
            downloaddriver_statistic_->SetOriginalUrl(http_request->GetUrl());
            downloaddriver_statistic_->SetOriginalReferUrl(http_request->GetRefererUrl());

            httpdownloader_statistic_ = downloaddriver_statistic_->AttachHttpDownloaderStatistic(http_request->GetUrl());
            httpdownloader_statistic_->SetReferUrl(http_request->GetRefererUrl());
            assert(httpdownloader_statistic_);
            httpdownloader_statistic_->SetIsDeath(false);
        }
        else 
        {
            downloaddriver_statistic_->SetHidden(true);
        }
    }

    void DirectProxySender::Start(boost::uint32_t start_possition)
    {
        assert(0);
    }


    void DirectProxySender::Stop()
    {
        if (is_running_ == false) return;

        LOG4CPLUS_INFO_LOG(logger_proxy, "DirectProxySender::Stop");
        if (http_server_socket_)
        {
            http_server_socket_.reset();
        }

        if (http_client_)
        {
            http_client_->Close();
            http_client_.reset();
        }

        if (need_bubble_)
        {
            downloaddriver_statistic_->DetachHttpDownloaderStatistic(httpdownloader_statistic_);
            httpdownloader_statistic_.reset();
        }

        statistic::StatisticModule::Inst()->DetachDownloadDriverStatistic(downloaddriver_statistic_);
        downloaddriver_statistic_.reset();

        if (http_request_)
        {
            http_request_.reset();
        }
        if (proxy_connection_)
        {
            proxy_connection_.reset();
        }

        is_running_ = false;
    }

    void DirectProxySender::SendHttpRequest()
    {
        if (is_running_ == false)
            return;

        assert(http_request_);
        // 保证头部只发一次
        if (have_send_http_header_)
        {
            return;
        }

        have_send_http_header_ = true;

        LOG4CPLUS_INFO_LOG(logger_proxy, "DirectProxySender::SendHttpRequest ");
        http_client_ = network::HttpClient<protocol::SubPieceContent>::create(
            io_svc_, http_request_, http_request_->GetUrl(), "", 0, 0, false);

        http_client_->SetHandler(shared_from_this());
        http_client_->Connect();
    }

    void DirectProxySender::OnNoticeGetContentLength(boost::uint32_t content_length, network::HttpResponse::p http_response)
    {
        if (is_running_ == false)
        {
            return;
        }

        assert(0);
    }

    // HttpClient ******************************
    void DirectProxySender::OnConnectSucced()
    {
        if (is_running_ == false)
            return;
        LOG4CPLUS_INFO_LOG(logger_proxy, "DirectProxySender::OnConnectSucced");
        http_client_->HttpGetByString(network::ProxyRequestToDirectRequest(http_request_->GetRequestString()));
    }

    void DirectProxySender::OnConnectFailed(boost::uint32_t error_code)
    {
        if (is_running_ == false)
        {
            return;
        }

        LOG4CPLUS_INFO_LOG(logger_proxy, "DirectProxySender::OnConnectFailed");

        proxy_connection_->WillStop();

    }

    void DirectProxySender::OnConnectTimeout()
    {
        if (is_running_ == false)
        {
            return;
        }

        LOG4CPLUS_INFO_LOG(logger_proxy, "DirectProxySender::OnConnectTimeout");

        proxy_connection_->WillStop();

    }

    void DirectProxySender::OnRecvHttpHeaderSucced(network::HttpResponse::p http_response)
    {
        if (is_running_ == false)
        {
            return;
        }

        LOG4CPLUS_INFO_LOG(logger_proxy, "DirectProxySender::OnRecvHttpHeaderSucced");

        if (http_server_socket_)
        {
            http_server_socket_->HttpSendHeader(http_response->ToString());
        }

        http_client_->HttpRecvSubPiece();
    }

    void DirectProxySender::OnRecvHttpHeaderFailed(boost::uint32_t error_code)
    {
        if (is_running_ == false)
        {
            return;
        }

        LOG4CPLUS_INFO_LOG(logger_proxy, "DirectProxySender::OnRecvHttpHeaderFailed");

        proxy_connection_->WillStop();


    }

    void DirectProxySender::OnRecvHttpDataSucced(protocol::SubPieceBuffer const & buffer, boost::uint32_t file_offset, boost::uint32_t content_offset, bool is_gzip)
    {
        if (is_running_ == false)
        {
            return;
        }

        LOG4CPLUS_INFO_LOG(logger_proxy, "DirectProxySender::OnRecvHttpDataSucced");

        if (need_bubble_)
        {
            httpdownloader_statistic_->SubmitDownloadedBytes(buffer.Length());
        }

        if (http_server_socket_)
        {
            http_server_socket_->HttpSendBuffer(buffer);
        }

        http_client_->HttpRecvSubPiece();
    }

    void DirectProxySender::OnRecvHttpDataPartial(protocol::SubPieceBuffer const & buffer, boost::uint32_t file_offset, boost::uint32_t content_offset)
    {
        if (is_running_ == false)
        {
            return;
        }

        LOG4CPLUS_INFO_LOG(logger_proxy, "DirectProxySender::OnRecvHttpDataPartial");

        if (http_server_socket_)
        {
            http_server_socket_->HttpSendBuffer(buffer);
        }
    }

    void DirectProxySender::OnRecvHttpDataFailed(boost::uint32_t error_code)
    {
        if (is_running_ == false)
        {
            return;
        }

        LOG4CPLUS_INFO_LOG(logger_proxy, "DirectProxySender::OnRecvHttpDataFailed");

        proxy_connection_->WillStop();
    }

    void DirectProxySender::OnRecvTimeout()
    {
        if (is_running_ == false)
        {
            return;
        }

        LOG4CPLUS_INFO_LOG(logger_proxy, "DirectProxySender::OnRecvTimeout");

        proxy_connection_->WillStop();
    }

    void DirectProxySender::OnComplete()
    {
        if (is_running_ == false)
        {
            return;
        }

        LOG4CPLUS_INFO_LOG(logger_proxy, "DirectProxySender::OnComplete");
        proxy_connection_->WillStop();
    }

    void DirectProxySender::OnNoticeOpenServiceHeadLength(boost::uint32_t head_length)
    {
        if (false == is_running_)
            return;
    }
    void DirectProxySender::OnRecvSubPiece(boost::uint32_t position, std::vector<base::AppBuffer> const & buffers)
    {
        for (boost::uint32_t i = 0; i < buffers.size(); ++i)
        {
            position += buffers[i].Length();
        }
    }
}
