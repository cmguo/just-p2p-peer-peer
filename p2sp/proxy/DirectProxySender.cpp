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

#ifdef BOOST_WINDOWS_API
#include "WindowsMessage.h"
#endif

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("proxy");

    void DirectProxySender::Start()
    {
        assert(!"DirectProxySender::Start()");
    }

    void DirectProxySender::Start(network::HttpRequest::p http_request, ProxyConnection::p proxy_connection)
    {
        if (is_running_ == true)
            return;
        LOG(__EVENT, "proxy", "DirectProxySender::Start");

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

            // 气泡提示
            DOWNLOADDRIVERSTARTDATA* lpDownloadDriverStartData = MessageBufferManager::Inst()->NewStruct<DOWNLOADDRIVERSTARTDATA>();
            memset(lpDownloadDriverStartData, 0, sizeof(DOWNLOADDRIVERSTARTDATA));
            lpDownloadDriverStartData->uSize = sizeof(DOWNLOADDRIVERSTARTDATA);

            strncpy(lpDownloadDriverStartData->szOriginalUrl, http_request->GetUrl().c_str(), sizeof(lpDownloadDriverStartData->szOriginalUrl)-1);
            strncpy(lpDownloadDriverStartData->szOriginalReferUrl, http_request->GetRefererUrl().c_str(), sizeof(lpDownloadDriverStartData->szOriginalReferUrl)-1);

#ifdef NEED_TO_POST_MESSAGE
            WindowsMessage::Inst().PostWindowsMessage(UM_DONWLOADDRIVER_START, (WPARAM)s_id_, (LPARAM)lpDownloadDriverStartData);
#endif
        }
        else 
        {
            downloaddriver_statistic_->SetHidden(true);
        }

    }

    void DirectProxySender::Start(uint32_t start_possition)
    {
        assert(0);
    }


    void DirectProxySender::Stop()
    {
        if (is_running_ == false) return;

        LOG(__EVENT, "proxy", "DirectProxySender::Stop");
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

        LOG(__INFO, "proxy", "DirectProxySender::SendHttpRequest ");
        http_client_ = network::HttpClient<protocol::SubPieceContent>::create(
            io_svc_, http_request_, http_request_->GetUrl());

        http_client_->SetHandler(shared_from_this());
        http_client_->Connect();
    }

    void DirectProxySender::OnTcpSendSucced(uint32_t length)
    {
        if (is_running_ == false)
            return;
    }

    void DirectProxySender::OnDownloadDriverError(uint32_t error_code)
    {
        if (is_running_ == false)
        {
            return;
        }

        assert(0);
    }


    void DirectProxySender::OnAsyncGetSubPieceSucced(uint32_t start_position, base::AppBuffer buffer)
    {
        if (is_running_ == false)
        {
            return;
        }

        assert(0);
    }

    void DirectProxySender::OnNoticeGetContentLength(uint32_t content_length, network::HttpResponse::p http_response)
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
        LOG(__INFO, "proxy", "DirectProxySender::OnConnectSucced");
        http_client_->HttpGetByString(network::ProxyRequestToDirectRequest(http_request_->GetRequestString()));
    }

    void DirectProxySender::OnConnectFailed(uint32_t error_code)
    {
        if (is_running_ == false)
        {
            return;
        }

        LOG(__INFO, "proxy", "DirectProxySender::OnConnectFailed");

        proxy_connection_->WillStop();

    }

    void DirectProxySender::OnConnectTimeout()
    {
        if (is_running_ == false)
        {
            return;
        }

        LOG(__INFO, "proxy", "DirectProxySender::OnConnectTimeout");

        proxy_connection_->WillStop();

    }

    void DirectProxySender::OnRecvHttpHeaderSucced(network::HttpResponse::p http_response)
    {
        if (is_running_ == false)
        {
            return;
        }

        LOG(__INFO, "proxy", "DirectProxySender::OnRecvHttpHeaderSucced");

        if (http_server_socket_)
        {
            http_server_socket_->HttpSendHeader(http_response->ToString());
        }

        http_client_->HttpRecvSubPiece();
    }

    void DirectProxySender::OnRecvHttpHeaderFailed(uint32_t error_code)
    {
        if (is_running_ == false)
        {
            return;
        }

        LOG(__INFO, "proxy", "DirectProxySender::OnRecvHttpHeaderFailed");

        proxy_connection_->WillStop();


    }

    void DirectProxySender::OnRecvHttpDataSucced(protocol::SubPieceBuffer const & buffer, uint32_t file_offset, uint32_t content_offset)
    {
        if (is_running_ == false)
        {
            return;
        }

        LOG(__INFO, "proxy", "DirectProxySender::OnRecvHttpDataSucced");

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

    void DirectProxySender::OnRecvHttpDataPartial(protocol::SubPieceBuffer const & buffer, uint32_t file_offset, uint32_t content_offset)
    {
        if (is_running_ == false)
        {
            return;
        }

        LOG(__INFO, "proxy", "DirectProxySender::OnRecvHttpDataPartial");

        if (http_server_socket_)
        {
            http_server_socket_->HttpSendBuffer(buffer);
        }
    }

    void DirectProxySender::OnRecvHttpDataFailed(uint32_t error_code)
    {
        if (is_running_ == false)
        {
            return;
        }

        LOG(__INFO, "proxy", "DirectProxySender::OnRecvHttpDataFailed");

        proxy_connection_->WillStop();
    }

    void DirectProxySender::OnRecvTimeout()
    {
        if (is_running_ == false)
        {
            return;
        }

        LOG(__INFO, "proxy", "DirectProxySender::OnRecvTimeout");

        proxy_connection_->WillStop();
    }

    void DirectProxySender::OnComplete()
    {
        if (is_running_ == false)
        {
            return;
        }

        LOG(__INFO, "proxy", "DirectProxySender::OnComplete");
        proxy_connection_->WillStop();
    }

    void DirectProxySender::OnNotice403Header()
    {
        if (false == is_running_)
        {
            return;
        }

        if (true == is_response_header_)
        {
            return;
        }

        LOG(__EVENT, "proxy", __FUNCTION__ << ": Notice 403 header");

        if (http_server_socket_)
        {
            http_server_socket_->HttpSend403Header();
        }
    }

    void DirectProxySender::OnNoticeOpenServiceHeadLength(uint32_t head_length)
    {
        if (false == is_running_)
            return;
    }
    void DirectProxySender::OnRecvSubPiece(uint32_t position, std::vector<base::AppBuffer> const & buffers)
    {
        for (uint32_t i = 0; i < buffers.size(); ++i)
        {
            OnAsyncGetSubPieceSucced(position, buffers[i]);
            position += buffers[i].Length();
        }
    }

    uint32_t DirectProxySender::GetStartOffset()
    {
        return 0;
    }
}
