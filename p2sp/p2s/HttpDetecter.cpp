//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/p2s/HttpDownloader.h"
#include "p2sp/p2s/HttpDetecter.h"
#include "p2sp/p2s/HttpConnection.h"

#define HTTP_DEBUG(s)    LOG(__DEBUG, "http", s)
#define HTTP_INFO(s)    LOG(__INFO, "http", s)
#define HTTP_EVENT(s)    LOG(__EVENT, "http", s)
#define HTTP_WARN(s)    LOG(__WARN, "http", s)
#define HTTP_ERROR(s)    LOG(__ERROR, "http", s)

const uint32_t HTTP_SLEEP_TIME = 1*5*1000;

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("http");

    HttpDetecter::HttpDetecter(
        boost::asio::io_service & io_svc,
        HttpDownloader__p downloader)
        : io_svc_(io_svc)
        , downloader_(downloader)
        , is_running_(false)
        , is_detected_(false)
        , sleep_once_timer_(global_second_timer(), HTTP_SLEEP_TIME, boost::bind(&HttpDetecter::OnTimerElapsed, this, &sleep_once_timer_))
    {}

    HttpDetecter::HttpDetecter(
        boost::asio::io_service & io_svc,
        const network::HttpRequest::p http_request_demo,
        HttpDownloader__p downloader)
        : io_svc_(io_svc)
        , downloader_(downloader)
        , is_running_(false)
        , is_detected_(false)
        , http_request_demo_(http_request_demo)
        , sleep_once_timer_(global_second_timer(), HTTP_SLEEP_TIME, boost::bind(&HttpDetecter::OnTimerElapsed, this, &sleep_once_timer_))
    {}

    void HttpDetecter::Start()
    {
        if (is_running_ == true)
            return;
        is_running_ = true;
    }

    void HttpDetecter::Stop()
    {
        if (is_running_ == false)
            return;

        if (http_client_)
        {
            http_client_->Close();
            http_client_.reset();
        }

        sleep_once_timer_.stop();

        if (downloader_)
        {
            downloader_.reset();
        }

        if (http_connection_)
        {
            http_connection_.reset();
        }

        if (http_request_demo_)
        {
            http_request_demo_.reset();
        }

        is_running_ = false;
    }

    bool HttpDetecter::DoDetect(HttpConnection::p http_connection, protocol::UrlInfo url_info)
    {
        if (is_running_ == false)
            return false;
        if (is_detected_ == true)
            return false;

        is_detected_ = true;

        url_info_ = url_info;

        http_connection_ = http_connection;

        http_client_ = network::HttpClient<protocol::SubPieceContent>::create(io_svc_, http_request_demo_, url_info_.url_, url_info_.refer_url_);
        string pragma_client = http_connection_->GetPragmaClient();
        if (pragma_client.length() != 0) http_client_->AddPragma("Client", pragma_client);
        http_client_->SetHandler(shared_from_this());
        http_client_->Connect();

        return true;
    }

    void HttpDetecter::SleepForConnect()
    {
        if (is_running_ == false)
            return;

        http_client_->Close();
        sleep_once_timer_.start();

    }

    void HttpDetecter::OnTimerElapsed(framework::timer::Timer * pointer)
    {
        if (is_running_ == false)
            return;
        if (pointer == &sleep_once_timer_)
        {
            http_client_ = network::HttpClient<protocol::SubPieceContent>::create(io_svc_, http_request_demo_, url_info_.url_, url_info_.refer_url_);
            string pragma_client = http_connection_->GetPragmaClient();
            if (pragma_client.length() != 0) http_client_->AddPragma("Client", pragma_client);
            http_client_->SetHandler(shared_from_this());
            http_client_->Connect();
        }
    }

    // ???
    void HttpDetecter::OnConnectSucced()
    {
        if (is_running_ == false)
            return;

        if (http_request_demo_)
            http_client_->HttpGet(http_request_demo_, 1024);
        else
            http_client_->HttpGet(1024);
    }

    void HttpDetecter::OnConnectFailed(uint32_t error_code)
    {
        if (is_running_ == false)
            return;

        SleepForConnect();
    }

    void HttpDetecter::OnConnectTimeout()
    {
        if (is_running_ == false)
            return;

        SleepForConnect();
    }

    void HttpDetecter::OnRecvHttpHeaderSucced(network::HttpResponse::p http_response)
    {
        if (is_running_ == false)
            return;

        HTTP_EVENT("HttpDetecter::OnRecvHttpHeaderSucced  url:" << url_info_.url_ << " " << " StatusCode " << http_response->GetStatusCode());

        if (http_response->GetStatusCode() == 200 || http_response->GetStatusCode() == 206)
        {
            assert(downloader_);
            assert(http_connection_);
            if (http_client_->IsBogusAcceptRange() == true)
            {
                downloader_->DecetecterReport(http_connection_, false);
            }
            else
            {
                // ?????????????????????range????????ж?????????????
                http_client_->HttpRecvSubPiece();  // ??????????
            }
            // http_client_->Close();
        }
        else
        if (http_response->GetStatusCode() == 403 || http_response->GetStatusCode() == 404)
        {
            assert(downloader_);
            assert(http_connection_);
            if (downloader_)
            {
                downloader_->DecetecterReport(http_connection_, false);
            }
            // downloader_->DecetecterReport(http_connection_, false);
            http_client_->Close();
        }
        else
        {
            assert(downloader_);
            assert(http_connection_);
            // downloader_->DecetecterReport(http_connection_, false);
            if (downloader_)
            {
                downloader_->DecetecterReport(http_connection_, false);
            }
            http_client_->Close();
        }
    }

    void HttpDetecter::OnRecvHttpHeaderFailed(uint32_t error_code)
    {
        if (is_running_ == false)
            return;
        http_client_->Close();
    }

    void HttpDetecter::OnRecvHttpDataSucced(protocol::SubPieceBuffer const & buffer, uint32_t file_offset, uint32_t content_offset)
    {
        if (is_running_ == false)
            return;
        HTTP_EVENT(__FUNCTION__ << " file_offset=" << file_offset << " content_offset=" << content_offset);
        downloader_->DecetecterReport(http_connection_, true);
        http_client_->Close();
    }

    void HttpDetecter::OnRecvHttpDataPartial(protocol::SubPieceBuffer const & buffer, uint32_t file_offset, uint32_t content_offset)
    {
        if (is_running_ == false)
            return;
        http_client_->Close();
    }

    void HttpDetecter::OnRecvHttpDataFailed(uint32_t error_code)
    {
        if (is_running_ == false)
            return;

        // if (error_code == 2)
        {
            HTTP_EVENT(__FUNCTION__ << " ErrorCode=" << error_code << " DecetecterReport range=" << false);
            downloader_->DecetecterReport(http_connection_, false);
        }

        http_client_->Close();
    }

    void HttpDetecter::OnRecvTimeout()
    {
        if (is_running_ == false)
            return;

        SleepForConnect();
    }

    void HttpDetecter::OnComplete()
    {
        if (is_running_ == false)
            return;
        http_client_->Close();
    }
}

