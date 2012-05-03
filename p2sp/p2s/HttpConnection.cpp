//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "statistic/StatisticModule.h"
#include "statistic/DACStatisticModule.h"
#include "p2sp/p2s/HttpConnection.h"
#include "p2sp/download/DownloadDriver.h"
#include "p2sp/p2s/HttpDownloader.h"
#include "storage/Instance.h"
#ifdef AUTO_SVN_VERSION
#include "autopeerversion.hpp"
#else
#include "PeerVersion.h"
#endif

#include "network/Uri.h"

#define HTTP_DEBUG(s)    LOG(__DEBUG, "http", s)
#define HTTP_INFO(s)    LOG(__INFO, "http", s)
#define HTTP_EVENT(s)    LOG(__EVENT, "http", s)
#define HTTP_WARN(s)    LOG(__WARN, "http", s)
#define HTTP_ERROR(s)    LOG(__ERROR, "http", s)

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("p2s");

    uint32_t DELAY_CONNECT_TIME = 1000;
    const uint32_t HTTP_SLEEP_TIME = 1*1000;
    const uint32_t HTTP_PAUSING_SLEEP_TIME = 8*1000;

    HttpConnection::HttpConnection(
        boost::asio::io_service & io_svc,
        HttpDownloader__p downloader,
        protocol::UrlInfo url_info,
        bool is_head_only)
        : io_svc_(io_svc)
        , downloader_(downloader)
        , url_info_(url_info)
        , is_running_(false)
        , is_connected_(false)
        , is_open_service_(false)
        , status(NONE)
        , have_piece_(false)
        , is_support_range_(true)
        , is_to_get_header_(false)
        , is_detected_(false)
        , no_notice_header_ (false)
        , pausing_sleep_timer_(global_second_timer(), HTTP_PAUSING_SLEEP_TIME, boost::bind(&HttpConnection::OnTimerElapsed, this, &pausing_sleep_timer_))
        , sleep_once_timer_(global_second_timer(), HTTP_SLEEP_TIME, boost::bind(&HttpConnection::OnTimerElapsed, this, &sleep_once_timer_))
        , delay_once_timer_(global_second_timer(), DELAY_CONNECT_TIME, boost::bind(&HttpConnection::OnTimerElapsed, this, &delay_once_timer_))
        , retry_count_403_header_(0)
        , connect_fail_count_(0)
        , download_bytes_(0)
        , is_head_only_(is_head_only)
    {}

    HttpConnection::HttpConnection(
        boost::asio::io_service & io_svc,
        const network::HttpRequest::p http_request_demo,
        HttpDownloader__p downloader,
        protocol::UrlInfo url_info,
        bool is_to_get_header,
        bool is_head_only)
        : io_svc_(io_svc)
        , downloader_(downloader)
        , url_info_(url_info)
        , is_running_(false)
        , is_connected_(false)
        , is_open_service_(false)
        , status(NONE)
        , have_piece_(false)
        , is_support_range_(true)
        , is_to_get_header_(is_to_get_header)
        , is_detected_(false)
        , no_notice_header_ (false)
        , pausing_sleep_timer_(global_second_timer(), HTTP_PAUSING_SLEEP_TIME, boost::bind(&HttpConnection::OnTimerElapsed, this, &pausing_sleep_timer_))
        , sleep_once_timer_(global_second_timer(), HTTP_SLEEP_TIME, boost::bind(&HttpConnection::OnTimerElapsed, this, &sleep_once_timer_))
        , delay_once_timer_(global_second_timer(), DELAY_CONNECT_TIME, boost::bind(&HttpConnection::OnTimerElapsed, this, &delay_once_timer_))
        , http_request_demo_(http_request_demo)
        , retry_count_403_header_(0)
        , connect_fail_count_(0)
        , download_bytes_(0)
        , is_head_only_(is_head_only)
    {}

    void HttpConnection::Start(bool is_support_start, bool is_open_service, uint32_t head_length)
    {
        if (is_running_ == true)
            return;

        HTTP_EVENT("HttpConnection Start" << shared_from_this());
        LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " is_open_service = " << is_open_service);

        is_running_ = true;

        is_support_start_ = is_support_start;

        is_open_service_ = is_open_service;

        is_detected_ = false;

        is_support_range_ = true;

        is_pausing_ = true;

        head_length_ = -1;
        if (is_open_service_)
        {
            head_length_ = head_length;
        }
        LOG(__DEBUG, "http", "HttpConnection::Start head_length = " << head_length_);

        if (is_to_get_header_)
        {
            have_piece_ = true;
            piece_info_ex_.block_index_ = piece_info_ex_.piece_index_ = piece_info_ex_.subpiece_index_ = 0;
        }

        connect_fail_count_ = 0;

        is_connected_ = false;

        is_downloading_ = false;

        retry_count_500_header_ = 0;

        if (!is_pausing_)
        {
            LOG(__DEBUG, "http", "HttpConnection::Start is_pausing = true, do connect");
            DoConnect();
        }

        if (is_open_service_ && is_head_only_)
        {
            gzip_decompresser_.Start(shared_from_this());
        }
    }

    void HttpConnection::Stop()
    {
        if (is_running_ == false) return;

        HTTP_EVENT("HttpConnection::Stop" << shared_from_this());
        LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << "");

        sleep_once_timer_.stop();
        pausing_sleep_timer_.stop();

        gzip_decompresser_.Stop();

        if (http_client_)
        {
            http_client_->Close();
            http_client_.reset();
        }

        if (downloader_)
        {
            downloader_.reset();
        }

        is_running_ = false;
    }

    void HttpConnection::DetectorReport(bool is_support_range)
    {
        if (is_running_ == false)
            return;

        is_support_range_ = is_support_range;
        is_detected_ = true;

        HTTP_EVENT("HttpConnection::DecetecterReport is_support_range_=" << is_support_range_ << " is_detected_=" << is_detected_);
    }
    void HttpConnection::DoConnect()
    {
        if (is_running_ == false)
            return;

        if (true == no_notice_header_ && false == downloader_->GetDownloadDriver()->HasNextPiece(downloader_))
        {
            // bug!!最后一片给httpConnection后，这里HasNextPiece会返回false, 但是这里又不去连接！
            LOG(__DEBUG, "ppbug", "DO NOT has Next Piece!");
            status = SLEEPING;
            SleepForConnect();
            return;
        }

        HTTP_EVENT("HttpConnection::DoConnect" << shared_from_this());
        LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << "");

        assert(downloader_->GetStatistics());
        downloader_->GetStatistics()->SubmitRetry();

        if (status == NONE)
        {
            status = CONNECTING;

            if (http_client_)
            {
                downloader_->SubmitHttpDownloadBytesInConnection(download_bytes_);
                download_bytes_ = 0;
                http_client_->Close();
            }
            if (is_head_only_)
            {
                boost::uint32_t gzip_range_end = ((head_length_ - 1) / SUB_PIECE_SIZE + 1) * SUB_PIECE_SIZE - 1;
                http_client_ = network::HttpClient<protocol::SubPieceContent>::create(io_svc_, http_request_demo_, url_info_.url_, url_info_.refer_url_, 0, gzip_range_end, true, url_info_.user_agent_);
            }
            else
            {
                http_client_ = network::HttpClient<protocol::SubPieceContent>::create(io_svc_, http_request_demo_, url_info_.url_, url_info_.refer_url_, 0, 0, false, url_info_.user_agent_);
            }
            
            LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " create http_client = " << http_client_);

            http_client_->SetHandler(shared_from_this());
            http_client_->Connect();
        }
        else
        {
            assert(0);
        }
    }

    void HttpConnection::SleepForConnect()
    {
        if (is_running_ == false) return;

        HTTP_EVENT("HttpConnection::SleepForConnect ");

        for (uint32_t i = 0; i < piece_task.size(); ++i)
        {
            downloader_->GetDownloadDriver()->OnPieceFaild(piece_task[i].GetPieceInfo(), downloader_);
        }
        if (http_client_)
        {
            http_client_->Close();
        }
        piece_task.clear();
        have_piece_ = false;
        is_downloading_ = false;
        if (status == SLEEPING)
        {
            sleep_once_timer_.start();
        }
    }

    void HttpConnection::DelayForConnect()
    {
        LOG(__DEBUG, "ppbug", "DelayForConnect DELAY_CONNECT_TIME  = " << DELAY_CONNECT_TIME);
        delay_once_timer_.start();
    }

    void HttpConnection::OnTimerElapsed(framework::timer::Timer * pointer)
    {
        if (false == is_running_)
            return;

        if (pointer == &delay_once_timer_)
        {
            LOG(__DEBUG, "ppbug", "bingo! ReConnect");
            status = NONE;
            DoConnect();
        }

        if (pointer == &sleep_once_timer_)
        {
            if (status == SLEEPING)
            {
                status = NONE;
                DoConnect();
            }
        }
        else if (pointer == &pausing_sleep_timer_)
        {
            // receive subpiece
            // if (status == PIECING && have_piece_ == true)
            assert(is_pausing_);
            HttpRecvSubPiece();
            // stop and reset timer
            pausing_sleep_timer_.stop();
        }
    }

    // HttpDownloader调用的唯一接口
    void HttpConnection::PutPieceTask(const std::deque<protocol::PieceInfoEx> & piece_info_ex_s)
    {
        piece_task.insert(piece_task.end(), piece_info_ex_s.begin(), piece_info_ex_s.end());
        LOG(__DEBUG, "ppbug", "PutPieceTask = " << is_downloading_);

        if (!is_downloading_)
        {
            // 没有在下载, 设置为正在下载
            is_downloading_ = true;
            PutPieceTask();
        }
    }

    void HttpConnection::PutPieceTask()
    {
        if (is_running_ == false) return;

        protocol::PieceInfoEx piece_info_ex = piece_task.front();
        LOG(__DEBUG, "ppbug", "piece_task = " << piece_info_ex.GetPieceInfo());
        LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " protocol::PieceInfo = " << piece_info_ex);

        if (status == CONNECTED && have_piece_ == false && is_support_range_  == true)
        {
            piece_info_ex_ = piece_info_ex;
            have_piece_ = true;
            LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " SendHttpRequest");
            SendHttpRequest();
        }
        else if (status == PIECED && have_piece_ == false && is_support_range_  == true)
        {
            uint32_t block_size_ = downloader_->GetDownloadDriver()->GetInstance()->GetBlockSize();
            uint32_t current_position = piece_info_ex.GetPosition(block_size_);

            // 开放服务
            if (true == is_open_service_)
            {
                if (piece_info_ex_.GetEndPosition(block_size_) == current_position ||
                    // head length 未获得
                    head_length_ == (uint32_t)-1 ||

                    current_position < head_length_)
                {
                    // 继续
                    piece_info_ex_ = piece_info_ex;
                    have_piece_ = true;
                    status = PIECEING;
                    LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " HttpRecvSubPiece");
                    HttpRecvSubPiece();
                }
                else
                {
                    // 重连
                    piece_info_ex_ = piece_info_ex;
                    have_piece_ = true;
                    status = NONE;
                    http_client_->Close();
                    LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " DoConnect");
                    DoConnect();
                }
            }
            // 普通模式
            else
            {
                if (piece_info_ex_.GetEndPosition(block_size_) == current_position)
                {
                    piece_info_ex_ = piece_info_ex;
                    have_piece_ = true;
                    status = PIECEING;
                    LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " HttpRecvSubPiece");
                    HttpRecvSubPiece();
                }
                else
                {
                    piece_info_ex_ = piece_info_ex;
                    have_piece_ = true;
                    status = NONE;
                    http_client_->Close();
                    LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " DoConnect");
                    DoConnect();
                }
            }
        }
        else if (status == NONE && have_piece_ == false)
        {
            piece_info_ex_ = piece_info_ex;
            status = CONNECTING;
            have_piece_ = true;
            LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " DoConnect");
            DoConnect();
        }
        else if (status == CONNECTING && have_piece_ == false)
        {
            piece_info_ex_ = piece_info_ex;
            have_piece_ = true;
            LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " Nothing");
        }
        else if (status == CONNECTED && have_piece_ == false && is_support_range_  == false)
        {
            piece_info_ex_ = piece_info_ex;
            have_piece_ = true;
            LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " SendHttpRequest");
            SendHttpRequest();
        }
        else if (status == PIECED && have_piece_ == false && is_support_range_  == false)
        {
            //  不论给的什么  继续下！
            status = PIECEING;
            have_piece_ = true;
            piece_info_ex_ = piece_info_ex;
            LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " HttpRecvSubPiece");
            HttpRecvSubPiece();
        }
        else
        {
            LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " UnknownState " << status);
        }
    }

    void HttpConnection::OnConnectSucced()
    {
        if (is_running_ == false) return;

        LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " ");

        connect_fail_count_ = 0;

        is_connected_ = true;
        HTTP_EVENT("HttpConnection::OnConnectSucced " << shared_from_this() << " status=" << status);

        assert(downloader_->GetStatistics());
        downloader_->GetStatistics()->SubmitHttpConnected();
        downloader_->GetStatistics()->ClearRetry();

        if (is_to_get_header_)
        {
            status = CONNECTED;
            SendHttpRequest();
        }
        else if (status == CONNECTING && have_piece_ == false && (is_support_range_  == true || is_open_service_ == true))
        {
            status = CONNECTED;
            LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " CONNECTED downloader_->GetDownloadDriver()->RequestNextPiece(downloader_)");
            if (false == downloader_->GetDownloadDriver()->RequestNextPiece(downloader_))
            {
                LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " RequestNextPiece = FALSE");

                if (downloader_->GetDownloadDriver()->GetInstance()->IsComplete())
                {
                    LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << "Instance Complete, GetHeader!");
                    have_piece_ = true;
                    piece_info_ex_.block_index_ = piece_info_ex_.piece_index_ = piece_info_ex_.subpiece_index_ = 0;
                    SendHttpRequest();
                }
                else
                {
                    LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << "Instance not complete, Wait for connect!");
                    status = SLEEPING;
                    have_piece_ = false;
                    http_client_->Close();
                    SleepForConnect();
                }
            }
            else
            {
                LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " RequestNextPiece = TRUE");
            }
        }
        else if (status == CONNECTING && have_piece_ == true && is_support_range_  == true)
        {
            status = CONNECTED;
            SendHttpRequest();
        }
        else if (status == CONNECTING && have_piece_ == true && is_support_range_  == false)
        {
            status = CONNECTED;
            SendHttpRequest();
        }
        else if (status == CONNECTING && have_piece_ == false && is_support_range_  == false)
        {
            status = CONNECTED;
            bool result = downloader_->GetDownloadDriver()->RequestNextPiece(downloader_);
            (void)result;
            LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " RequestNextPiece = " << result);
        }
        else
        {
            // assert(!"HttpConnection::OnConnectSucced: No Such State");
            LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " status=" << status << " have_piece=" << have_piece_ << " range=" << is_support_range_);
            // restart
            if ((is_support_range_  == true || is_open_service_ == true) && false == downloader_->GetDownloadDriver()->GetInstance()->IsComplete())
            {
                status = SLEEPING;
                http_client_->Close();
                have_piece_ = false;
                SleepForConnect();
            }
        }
    }

    void HttpConnection::SendHttpRequest()
    {
        if (false == is_running_)
            return;

        HTTP_EVENT("HttpConnection::SendHttpRequest" << shared_from_this());
        LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " ");

        if (is_to_get_header_)
        {
            status = HEADERING;

            HTTP_INFO("HttpGet: downloader is_to_get_header_:" << is_to_get_header_);

            LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " is_to_get_header_");
            if (http_request_demo_)
                http_client_->HttpGet(http_request_demo_, 0);
            else
                http_client_->HttpGet(0);
        }
        else if (status == CONNECTED && have_piece_ == true && is_support_range_  == true && false == is_open_service_)
        {
            status = HEADERING;
            uint32_t block_size = downloader_->GetDownloadDriver()->GetInstance()->GetBlockSize();
            HTTP_INFO("HttpGet: downloader:" << downloader_ << " protocol::PieceInfo: " << piece_info_ex_ << " block_size_: " << block_size);
            LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " ");
            if (http_request_demo_)
                http_client_->HttpGet(http_request_demo_, piece_info_ex_.GetPosition(block_size));
            else
            {
                // 开放服务的CDN请求会经过这里
                boost::uint32_t range_end = GetTaskRangeEnd(block_size);
                if (range_end == 0)
                {
                    // 不知道分配任务的终点,range_end不填，CDN会一直发送
                    http_client_->HttpGet(piece_info_ex_.GetPosition(block_size));
                }
                else
                {
                    // 已经知道range_end
                    //DebugLog("[%d - %d]\n", piece_info_ex_.GetPosition(block_size), range_end);
                    http_client_->HttpGet(piece_info_ex_.GetPosition(block_size), range_end);
                }
            }

            assert(downloader_->GetStatistics());
            downloader_->GetStatistics()->SetStartPieceInfo(piece_info_ex_.block_index_, piece_info_ex_.piece_index_, piece_info_ex_.subpiece_index_);
            downloader_->GetStatistics()->SubmitRequestPiece();
        }
        else if (status == CONNECTED && have_piece_ == true && (is_support_range_  == true || true == is_open_service_))
        {
            uint32_t block_size = downloader_->GetDownloadDriver()->GetInstance()->GetBlockSize();
            uint32_t piece_position = piece_info_ex_.GetPosition(block_size);

            HTTP_INFO("block size = " << block_size << " piece_position = " << piece_position << " head length = " << head_length_);

            if ((head_length_ != (uint32_t)-1 && piece_position > head_length_) || piece_position >= 2*1024*1024)
            {
                status = HEADERING;
                HTTP_INFO("HttpGet: downloader:" << downloader_ << " protocol::PieceInfo: " << piece_info_ex_ << " block_size_: " << block_size);
                LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " head_length = " << head_length_ << ", piece_position = " << piece_position);
                if (http_request_demo_)
                    http_client_->HttpGet(http_request_demo_, piece_info_ex_.GetPosition(block_size));
                else
                {
                    boost::uint32_t range_end = GetTaskRangeEnd(block_size);
                    if (range_end == 0)
                    {
                        // 不知道分配任务的终点,range_end不填，CDN会一直发送
                        http_client_->HttpGet(piece_position);
                    }
                    else
                    {
                        // 已经知道range_end
                        //DebugLog("[%d - %d]\n", piece_info_ex_.GetPosition(block_size), range_end);
                        http_client_->HttpGet(piece_position, range_end);
                    }
                }

                assert(downloader_->GetStatistics());
                downloader_->GetStatistics()->SetStartPieceInfo(piece_info_ex_.block_index_, piece_info_ex_.piece_index_, piece_info_ex_.subpiece_index_);
                downloader_->GetStatistics()->SubmitRequestPiece();
            }
            else
            {
                status = HEADERING;
                HTTP_INFO("HttpGet: downloader:" << downloader_ << " protocol::PieceInfo: " << piece_info_ex_);

                LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " ");
                if (http_request_demo_)
                    http_client_->HttpGet(http_request_demo_, 0);
                else
                    http_client_->HttpGet(0);

                assert(downloader_->GetStatistics());
                downloader_->GetStatistics()->SetStartPieceInfo(piece_info_ex_.block_index_, piece_info_ex_.piece_index_, piece_info_ex_.subpiece_index_);
                downloader_->GetStatistics()->SubmitRequestPiece();
            }
        }
        else if (status == CONNECTED && have_piece_ == true && is_support_range_  == false)
        {
            status = HEADERING;
            HTTP_INFO("HttpGet: downloader:" << downloader_ << " protocol::PieceInfo: " << piece_info_ex_);

            LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " ");
            if (http_request_demo_)
                http_client_->HttpGet(http_request_demo_, 0);
            else
                http_client_->HttpGet(0);

            assert(downloader_->GetStatistics());
            downloader_->GetStatistics()->SetStartPieceInfo(piece_info_ex_.block_index_, piece_info_ex_.piece_index_, piece_info_ex_.subpiece_index_);
            downloader_->GetStatistics()->SubmitRequestPiece();
        }
    }

    void HttpConnection::OnConnectFailed(uint32_t error_code)
    {
        if (is_running_ == false) return;
        HTTP_ERROR("HttpConnection::OnConnectFailed " << url_info_ << " ErrorCode=" << error_code);
        LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " Url = " << url_info_ << " Error = " << error_code << " FailCount = " << connect_fail_count_);

        if (connect_fail_count_ >= 5)
        {
            LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " OnNotice403Header");
            downloader_->GetDownloadDriver()->OnNotice403Header(downloader_, network::HttpResponse::p());
            return;
        }

        ++connect_fail_count_;

        if (status == CONNECTING)
        {
            status = SLEEPING;
            http_client_->Close();
            LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " SleepForConnect");
            SleepForConnect();
        }
        else if (status == SLEEPING)
        {
            // 什么都不做
            LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " Nothing");
        }
        else
        {
            LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " UnknownState = " << status);
        }
    }

    void HttpConnection::OnConnectTimeout()
    {
        if (is_running_ == false) return;
        HTTP_ERROR("OnConnectTimeout " << url_info_);
        LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " Url = " << url_info_ << " FailCount = " << connect_fail_count_);

        if (connect_fail_count_ >= 5)
        {
            LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " OnNotice403Header");
            downloader_->GetDownloadDriver()->OnNotice403Header(downloader_, network::HttpResponse::p());
            return;
        }

        ++connect_fail_count_;

        if (status == CONNECTING)
        {
            status = NONE;
            http_client_->Close();
            LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " ");
            DoConnect();
        }
        else if (status == HEADERING)
        {
            status = NONE;
            http_client_->Close();
            LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " ");
            DoConnect();
        }
        else if (status == SLEEPING)
        {
            // 什么都不做
            LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " ");
        }
        else
        {
            LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " ");
        }
    }

    void HttpConnection::OnRecvHttpHeaderSucced(network::HttpResponse::p http_response)
    {
        if (is_running_ == false) return;
        HTTP_EVENT("HttpConnection::OnRecvHttpHeaderSucced Response: " << http_response);
        LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " HeaderResponse = \n" << http_response->ToString());

        assert(downloader_->GetStatistics());
        downloader_->GetStatistics()->SetHttpStatusCode(http_response->GetStatusCode());

        HTTP_EVENT("HttpConnection::OnRecvHttpHeaderSucced downloader: " << downloader_ << " protocol::UrlInfo: " << url_info_ << "\r\nResponse:\n" << *http_response);
        HTTP_DEBUG(__FUNCTION__ << " status=" << status << " have_piece_=" << have_piece_ << " is_support_range_=" << is_support_range_);
        if (status == HEADERING && have_piece_ == true && (is_support_range_  == true || is_open_service_ == true))
        {
            if (http_response->GetStatusCode() == 304)
            {
                downloader_->GetDownloadDriver()->OnNotice304Header(downloader_, http_response);
            }
            else if (http_response->GetStatusCode() == 200)
            {
                // Support Range
                if (http_client_->IsBogusAcceptRange() == false || is_open_service_ == true)
                {
                    HTTP_EVENT("HttpConnection::OnRecvHttpHeaderSucced, Accept Range");
                    if (! no_notice_header_)
                    {
                        HTTP_EVENT("HttpConnection::OnRecvHttpHeaderSucced no_notice_header == false");
                        no_notice_header_ = true;

                        DownloadDriver::p download_driver = downloader_->GetDownloadDriver();

                        // notice pragma
                        string pragma_mod = http_response->GetPragma("Mod");
                        string head = http_response->GetPragma("head");
                        if (pragma_mod.length() != 0 && head.length() != 0)
                        {
                            boost::system::error_code ec = framework::string::parse2(head, head_length_);
                            if (!ec)
                            {
                                download_driver->OnNoticePragmaInfo(pragma_mod, head_length_);
                                HTTP_WARN("HttpConnection::OnRecvHttpHeaderSucced_HEAD " << downloader_->GetDownloadDriver()->GetDownloadDriverID() << " " << head);
                            }
                            else
                            {
                                HTTP_WARN("HttpConnection::OnRecvHttpHeaderSucced bad: mod=" << pragma_mod << " head=" << head);
                            }
                        }

                        // notice content length
                        download_driver->OnNoticeConnentLength(http_response->GetFileLength(), downloader_, http_response);
                        // 当文件长度为0的时候，会自动切换为Direct模式，会把DownloadDriver关闭，造成下面的代码Crash
                        if (http_response->GetFileLength() == 0)
                        {
                            return;
                        }
                    }
                    status = PIECEING;

                    // 如果不带start的 进行探测
                    if (true == is_open_service_)
                    {
                        if (head_length_ != (uint32_t)-1) {
                            if (downloader_)
                            {
                                downloader_->DetectorReport(shared_from_this(), true);
                            }
                        }
                        else {
                            HTTP_ERROR("HttpConnection::OnRecvHttpHeaderSucced Invalid OpenService Response");
                        }
                    }
                    else if (is_support_start_ == false)
                    {
                        HTTP_EVENT("HttpConnection::OnRecvHttpHeaderSucced NoStart");
                        if (downloader_->IsOriginal())
                        {
                            HTTP_EVENT("HttpConnection::OnRecvHttpHeaderSucced Detect");
                            downloader_->DoDetecter(shared_from_this(), url_info_);
                        }
                    }

                    HTTP_EVENT("HttpConnection::OnRecvHttpHeaderSucced http_client_->RecvSubPiece() ");
                    HttpRecvSubPiece();
                }
                // Not Support Range
                else
                {
                    HTTP_EVENT(__FUNCTION__ << " BogusAcceptRange");
                    // 假支持range
                    is_support_range_ = false;
                    status = NONE;
                    http_client_->Close();
                    DoConnect();
                }
            }
            else if (http_response->GetStatusCode() == 206)
            {
                if (! no_notice_header_)
                {
                    no_notice_header_ = true;
                    downloader_->GetDownloadDriver()->OnNoticeConnentLength(http_response->GetFileLength(), downloader_, http_response);
                }
                status = PIECEING;

                HttpRecvSubPiece();
            }
            else if (http_response->GetStatusCode() / 100 == 4)  // 400~415
            {
                // forbidden
                status = NONE;
                http_client_->Close();
                // notice
                downloader_->GetDownloadDriver()->OnNotice403Header(downloader_, http_response);
                if (is_support_range_)
                {
                    is_support_range_ = false;
                    DoConnect();
                }
            }
            else if (http_response->GetStatusCode() == 301 ||http_response->GetStatusCode() == 302||http_response->GetStatusCode() == 303)
            {
                Redirect(http_response);
            }
            else if (http_response->GetStatusCode() == 500 && retry_count_500_header_ < 9)
            {
                // 500
                LOG(__DEBUG, "ppbug", "500! DelayForConnect retry_count_500_header_ " << retry_count_500_header_);
                ++retry_count_500_header_;
                DelayForConnect();
            }
            else
            {
                status = SLEEPING;
                http_client_->Close();
                SleepForConnect();
            }
        }
        else if (status == HEADERING && have_piece_ == true && is_support_range_  == false)
        {
            if (http_response->GetStatusCode() == 304)
            {
                downloader_->GetDownloadDriver()->OnNotice304Header(downloader_, http_response);
            }
            else if (http_response->GetStatusCode() == 200 || http_response->GetStatusCode() == 206)
            {
                if (! no_notice_header_)
                {
                    no_notice_header_ = true;
                    downloader_->GetDownloadDriver()->OnNoticeConnentLength(http_response->GetFileLength(), downloader_, http_response);
                }
                status = PIECEING;

                HttpRecvSubPiece();
            }
            else if (http_response->GetStatusCode() == 301 ||http_response->GetStatusCode() == 302||http_response->GetStatusCode() == 303)
            {
                Redirect(http_response);
            }
            else if (http_response->GetStatusCode() / 100 == 4)
            {
                // forbidden
                status = NONE;
                http_client_->Close();
                // notice
                downloader_->GetDownloadDriver()->OnNotice403Header(downloader_, http_response);
            }
            else
            {
                status = SLEEPING;
                http_client_->Close();
                SleepForConnect();
            }
        }
        else
        {
            HTTP_ERROR(__FUNCTION__ << " No Such State: status=" << status << " have_piece_=" << have_piece_ << " is_support_range_=" << is_support_range_);
            // restart
            if ((is_support_range_  == true || is_open_service_ == true) && false == downloader_->GetDownloadDriver()->GetInstance()->IsComplete())
            {
                status = SLEEPING;
                http_client_->Close();
                have_piece_ = false;
                SleepForConnect();
            }
            // assert(!"HttpConnection::OnRecvHttpHeaderSucced: No Such State");
        }

    }

    void HttpConnection::OnRecvHttpHeaderFailed(uint32_t error_code)
    {
        if (is_running_ == false) return;
        HTTP_ERROR("OnRecvHttpHeaderFailed downloader:" << downloader_ << " protocol::UrlInfo: " << url_info_ << " ErrorCode=" << error_code);
        LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " Url = " << url_info_ << " ErrorCode = " << error_code);

        if (status == CONNECTING && have_piece_ == true)
        {
            status = SLEEPING;
            http_client_->Close();
            SleepForConnect();
        }
        else if (status == CONNECTED && have_piece_ == true)
        {
            status = SLEEPING;
            http_client_->Close();
            SleepForConnect();
        }
        else if (status == HEADERING && have_piece_ == true)
        {
            status = SLEEPING;
            http_client_->Close();
            SleepForConnect();
        }
        else if (status == PIECEING && have_piece_ == true)
        {
            status = SLEEPING;
            http_client_->Close();
            SleepForConnect();
        }
        else if (status == PIECED && have_piece_ == false)
        {
            status = SLEEPING;
            http_client_->Close();
            SleepForConnect();
        }
        else if (status == SLEEPING)
        {
            // 什么都不做
        }
        else
        {
            // assert(0);
            HTTP_WARN(__FUNCTION__ << " status=" << status << " have_piece=" << have_piece_ << " range=" << is_support_range_);
        }
    }

    void HttpConnection::OnRecvHttpDataSucced(protocol::SubPieceBuffer const & buffer, uint32_t file_offset, uint32_t content_offset, bool is_gzip)
    {
        HTTP_EVENT("OnRecvHttpDataSucced " << url_info_ << " file_offset=" << file_offset << " content_offset=" << content_offset);

        if (is_running_ == false) return;

        if (is_gzip)
        {
            assert(is_head_only_);

            if (!gzip_decompresser_.OnRecvData(buffer, file_offset, content_offset))
            {
                // 请求MP4头部，并且服务器返回的MP4头部被GZIP压缩
                // 统计压缩后的网络传输流量
                downloader_->GetDownloadDriver()->GetStatistic()->SubmitHttpDataBytes(buffer.Length());
                HttpRecvSubPiece();
            }
            else
            {
                http_client_->Close();
            }

            return;
        }

        uint32_t block_size_ = downloader_->GetDownloadDriver()->GetInstance()->GetBlockSize();
        protocol::SubPieceInfo sub_piece_info_;
        protocol::SubPieceInfo::MakeByPosition(file_offset, block_size_, sub_piece_info_);
            
        RecvHttpData(buffer, sub_piece_info_);

        bool piece_complete = false;
        // 已经下载到该片piece的终点
        if ((piece_task.empty() && sub_piece_info_.subpiece_index_ % 128 == 127) 
            || (!piece_task.empty() && sub_piece_info_.subpiece_index_ % 128 == piece_task.front().subpiece_index_end_))
        {
            piece_complete = true;

            // LOG(__DEBUG, "ppbug", "piece_complete pop_front = " << piece_task.front());
            if (!piece_task.empty())
            {
                piece_task.pop_front();
            }

            // downloader_->GetDownloadDriver()->OnPieceComplete(piece_info_ex_, downloader_);
        }
        LOG(__EVENT, "P2P", "OnRecvHttpDataSucced::OnSubPiece "
            << downloader_->GetDownloadDriver()->GetDownloadDriverID()
            << " " << 0 << " " << 1 << " " << shared_from_this() << " " << sub_piece_info_
            << " is_pausing_=" << is_pausing_);

        if (!is_pausing_)
        {
            // if (true == downloader_->GetDownloadDriver()->GetInstance()->HasPiece(piece_info_ex_.Getstorage::PieceInfo()))
            if (true == piece_complete)
            {
                if (status == PIECEING && have_piece_ == true && is_support_range_  == true)
                {
                    LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " PieceComplete = " << piece_info_ex_ << " RequestNext");
                    have_piece_ = false;
                    status = PIECED;
                    HTTP_EVENT("OnRecvPieceSucced downloader:" << downloader_ << " protocol::UrlInfo: " << url_info_ << " piece_info: " << piece_info_ex_);
                    downloader_->GetDownloadDriver()->OnPieceComplete(piece_info_ex_, downloader_);

                    if (piece_task.size() == 0)
                    {
                        // 没有任务了，设置为不在下载
                        is_downloading_ = false;
                        LOG(__DEBUG, "ppbug", "piece_task flag = " << is_downloading_);
                        if (false == downloader_->GetDownloadDriver()->RequestNextPiece(downloader_))
                        {
                            if (false == downloader_->GetDownloadDriver()->GetInstance()->IsComplete())
                            {
                                LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " RequestNextPiece FALSE -> SleepForConnect");
                                status = SLEEPING;
                                SleepForConnect();
                            }
                            else
                            {
                                LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " RequestNextPiece FALSE -> InstanceComplete");
                            }
                        }
                    }
                    else
                    {
                        PutPieceTask();
                    }
                }
                else if (status == PIECEING && have_piece_ == true && is_support_range_  == false)
                {
                    LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " PieceComplete = " << piece_info_ex_ << " RequestNext");
                    have_piece_ = false;
                    status = PIECED;
                    HTTP_EVENT("OnRecvPieceSucced downloader:" << downloader_ << " protocol::UrlInfo: " << url_info_ << " piece_info: " << piece_info_ex_);
                    downloader_->GetDownloadDriver()->OnPieceComplete(piece_info_ex_, downloader_);
                    if (piece_task.size() == 0)
                    {
                        // 没有任务了，设置为不在下载
                        is_downloading_ = false;
                        LOG(__DEBUG, "ppbug", "piece_task flag = " << is_downloading_);
                        if (false == downloader_->GetDownloadDriver()->RequestNextPiece(downloader_))
                        {
                            if (false == downloader_->GetDownloadDriver()->GetInstance()->IsComplete())
                            {
                                LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " RequestNextPiece FALSE -> SleepForConnect");
                                status = SLEEPING;
                                SleepForConnect();
                            }
                            else
                            {
                                LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " RequestNextPiece FALSE -> InstanceComplete");
                            }
                        }
                    }
                    else
                    {
                        PutPieceTask();
                    }
                }
                else
                {
                    HTTP_ERROR("HttpConnection::OnRecvHttpDataSucced Invalid State, status = " << status << ", have piece = " << have_piece_);
                    assert(0);
                }
            }
            else
            {
                HttpRecvSubPiece();
            }
        }
        else
        {
            HTTP_EVENT("HttpConnection::OnRecvHttpDataSucced is_pausing=" << is_pausing_ << " is_support_range=" << is_support_range_ << " is_detected_=" << is_detected_);
            if ((false == is_support_range_ || false == is_detected_) && !is_open_service_)
            {
                pausing_sleep_timer_.start();
            }
            else
            {
                status = NONE;
                have_piece_ = false;
                http_client_->Close();
                pausing_sleep_timer_.stop();
                // notice time out
                if (false == piece_complete)
                {
                    downloader_->GetDownloadDriver()->NoticePieceTaskTimeOut(piece_info_ex_, downloader_);
                }
            }
        }
    }

    void HttpConnection::Pause()
    {
        if (false == is_running_)
            return;

        LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " Pausing = " << is_pausing_);

        is_pausing_ = true;

        if (is_open_service_)
        {
            if (http_client_)
                http_client_->Close();

            status = NONE;
            // piece_task.clear();

            while (!piece_task.empty())
            {
                downloader_->GetDownloadDriver()->OnPieceFaild(piece_task.front(), downloader_);
                piece_task.pop_front();
            }
            have_piece_ = false;
        }

        HTTP_DEBUG(__FUNCTION__ << " http_connection_=" << shared_from_this() << " is_support_range_=" << is_support_range_ << " is_pausing_=" << is_pausing_);
    }

    void HttpConnection::Resume()
    {
        if (is_running_ == false)
            return;

        is_pausing_ = false;

        is_downloading_ = false;
        LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " Pausing = " << is_pausing_);

        HTTP_DEBUG(__FUNCTION__ << " is_pausing_=" << is_pausing_ << " http_connection_=" << shared_from_this()
            << " is_support_range_=" << is_support_range_
            << " is_detected_=" << is_detected_);

        LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__
            << " is_support_range_ = " << is_support_range_ << " is_detected_ = " << is_detected_
            << " status = " << status << " have_piece_ = " << have_piece_);

        pausing_sleep_timer_.stop();

        if (is_open_service_)
        {
            if (status == CONNECTING)
            {
                LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " CONNECTING");
            }
            else if (status == HEADERING)
            {
                LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " HEADERING");
            }
            else
            {
                LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " DoConnect");
                status = NONE;
                have_piece_ = false;
                is_to_get_header_ = false;
                DoConnect();
            }
        }
        else if (false == is_support_range_ || false == is_detected_)
        {
            if (status == PIECEING && have_piece_ == true)
            {
                LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " PIECEING HttpRecvSubPiece");
                HttpRecvSubPiece();
            }
            else if (status == PIECED && have_piece_ == false)
            {
                LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " PIECED RequestNextPiece");
                // request next piece
                if (false == downloader_->GetDownloadDriver()->RequestNextPiece(downloader_))
                {
                    LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " PIECED RequestNextPiece FALSE Reconnect");

                    status = NONE;
                    have_piece_ = false;
                    is_to_get_header_ = false;
                    DoConnect();
                }
                else
                {
                    LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " PIECED RequestNextPiece TRUE");
                }
            }
            else if (status == CONNECTING)
            {
                LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " CONNECTING Ignore");
            }
            else
            {
                LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " assert");
                status = NONE;
                have_piece_ = false;
                DoConnect();
            }
        }
        else if (true == is_support_range_ && true == is_detected_)
        {
            LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " DoConnect");
            status = NONE;
            have_piece_ = false;
            is_to_get_header_ = false;
            DoConnect();
        }
        else
        {
            HTTP_ERROR(__FUNCTION__ << " status = " << status << " is_support_range_ = " << is_support_range_ << " is_detected_ = " << is_detected_);
            if (status == PIECEING && have_piece_ == true)
            {
                LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " HttpRecvSubPiece");
                HttpRecvSubPiece();
            }
            else
            {
                LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " DoConnect");
                status = NONE;
                have_piece_ = false;
                DoConnect();
            }
        }
    }

    void HttpConnection::OnRecvHttpDataPartial(
        protocol::SubPieceBuffer const & buffer,
        uint32_t file_offset,
        uint32_t content_offset)
    {
        HTTP_EVENT("OnRecvHttpDataPartial " << url_info_ << " Length=" << buffer.Length());
        if (is_running_ == false) return;
    }

    void HttpConnection::OnRecvHttpDataFailed(uint32_t error_code)
    {
        HTTP_ERROR("OnRecvHttpDataFailed " << url_info_ << " ErrorCode=" << error_code);
        if (is_running_ == false) return;

        LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " Url = " << url_info_ << " ErrorCode = " << error_code);

        if (status == CONNECTING)
        {
            is_support_range_ = false;
            status = NONE;
            http_client_->Close();
            downloader_->DetectorReport(shared_from_this(), false);
            DoConnect();
        }
        else if (status == CONNECTED && have_piece_ == true)
        {
            status = SLEEPING;
            http_client_->Close();
            SleepForConnect();
        }
        else if (status == HEADERING && have_piece_ == true)
        {
            status = SLEEPING;
            http_client_->Close();
            SleepForConnect();
        }
        else if (status == PIECEING && have_piece_ == true && is_support_range_  == true)
        {
            if (error_code == 2)
            {
                is_support_range_ = false;
                status = NONE;
                http_client_->Close();
                downloader_->DetectorReport(shared_from_this(), false);
                DoConnect();
            }
            else
            {
                status = SLEEPING;
                http_client_->Close();
                SleepForConnect();
            }
        }
        else if (status == PIECED && have_piece_ == false)
        {
            status = SLEEPING;
            http_client_->Close();
            SleepForConnect();
        }
        else if (status == PIECEING && have_piece_ == true && is_support_range_  == false)
        {
            status = SLEEPING;
            http_client_->Close();
            SleepForConnect();
        }
        else if (status == SLEEPING)
        {
            // 什么都不做
        }
        else
        {
            assert(0);
        }
    }

    void HttpConnection::OnRecvTimeout()
    {
        HTTP_ERROR("OnRecvTimeout " << url_info_);
        if (is_running_ == false) return;
        LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " Url = " << url_info_);

        if (status == CONNECTED && have_piece_ == true)
        {
            status = SLEEPING;
            http_client_->Close();
            SleepForConnect();
        }
        else if (status == HEADERING && have_piece_ == true)
        {
            status = SLEEPING;
            http_client_->Close();
            SleepForConnect();
        }
        else if (status == PIECEING && have_piece_ == true)
        {
            status = SLEEPING;
            http_client_->Close();
            SleepForConnect();
        }
        else if (status == PIECED && have_piece_ == false && is_support_range_  == false)
        {
            status = SLEEPING;
            http_client_->Close();
            SleepForConnect();
        }
        else if (status == SLEEPING)
        {
            // 什么都不做
        }
        else
        {
            assert(0);
        }
    }

    void HttpConnection::OnComplete()
    {
        HTTP_EVENT("OnComplete " << url_info_);
        if (is_running_ == false) return;
        LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " Url = " << url_info_);

        if (is_support_range_ == false && is_open_service_ == false)
        {
            LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " NoRange Complete");
            downloader_->HttpConnectComplete(shared_from_this());
        }
        else
        {
            if (downloader_->GetDownloadDriver()->GetInstance()->IsComplete())
            {
                LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " Complete");
                downloader_->HttpConnectComplete(shared_from_this());
            }
            else
            {
                LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " NotComplete SleepForConnect");
                status = SLEEPING;
                have_piece_ = false;
                http_client_->Close();
                SleepForConnect();
            }
        }
    }

    void HttpConnection::HttpRecvSubPiece()
    {
        if (false == is_running_) {
            return;
        }

        HTTP_DEBUG("HttpRecvSubPiece");
        downloader_->DoRequestSubPiece(shared_from_this());
    }

    network::HttpClient<protocol::SubPieceContent>::p HttpConnection::GetHttpClient()
    {
        if (false == is_running_) {
            return network::HttpClient<protocol::SubPieceContent>::p();
        }
        return http_client_;
    }
    void HttpConnection::PieceTimeout()
    {
        LOG(__DEBUG, "ppbug", "PieceTimeout");
        if (false == is_running_)
        {
            return;
        }
        if (is_pausing_)
        {
            return;
        }
        while (!piece_task.empty())
        {
            downloader_->GetDownloadDriver()->OnPieceFaild(piece_task.front(), downloader_);
            piece_task.pop_front();
        }
        http_client_->Close();
        status = NONE;
        have_piece_ = false;
        is_to_get_header_ = false;
        is_downloading_ = false;
        DoConnect();
    }
    boost::int32_t HttpConnection::GetPieceTaskNum()
    {
        if (false == is_running_)
        {
            return 0;
        }
        return piece_task.size();
    }

    boost::uint32_t HttpConnection::GetTaskRangeEnd(boost::uint32_t block_size)
    {
        if (false == is_running_)
        {
            return 0;
        }

        boost::uint32_t pos = piece_info_ex_.GetPosition(block_size);

        for (std::deque<protocol::PieceInfoEx>::iterator iter = piece_task.begin();
            iter != piece_task.end(); ++iter)
        {
            if ((*iter).GetPosition(block_size) == pos)
            {
                pos = (*iter).GetEndPosition(block_size);
                continue;
            }

            // 不连续
            return pos;
        }

        // 都是连续的，不知道明确的终点
        return 0;
    }

    void HttpConnection::Redirect(network::HttpResponse::p http_response) 
    {
        string location_url = http_response->GetProperty("Location");
        if (location_url.find("http") == string::npos)
        {
            if (location_url[0] == '/')
            {
                network::Uri uri_(url_info_.url_);
                uri_.replacepath(location_url);
                url_info_.url_ = uri_.geturl();
            } 
            else
            {
                network::Uri uri_(url_info_.url_);
                uri_.replacefile(location_url);
                url_info_.url_ = uri_.geturl();
            }
        }
        else 
        {
            url_info_.url_ = http_response->GetProperty("Location");
        }
        status = NONE;
        http_client_->Close();
        DoConnect();
    }

    void HttpConnection::RecvHttpData(const protocol::SubPieceBuffer & buffer, const protocol::SubPieceInfo & sub_piece_info)
    {
        download_bytes_ += buffer.Length();

        downloader_->GetStatistics()->SubmitDownloadedBytes(buffer.Length());
        if (downloader_->IsOriginal())
        {
            statistic::StatisticModule::Inst()->SubmitTotalHttpOriginalDataBytes(buffer.Length());
        }
        else
            statistic::StatisticModule::Inst()->SubmitTotalHttpNotOriginalDataBytes(buffer.Length());

        //  HTTP请求不支持range重下的情况
        //  以前下过的不再往storage里送 减小开销
        // if (sub_piece_info_.GetPosition(block_size_) >= piece_info_ex_.GetPosition(block_size_))
        if (false == downloader_->GetDownloadDriver()->GetInstance()->HasSubPiece(sub_piece_info))
        {
            // 只有请求MP4头部才会有可能被压缩
            // 只有MP4头部被解压成功，这里才不统计下载字节数（因为之前已经统计过）
            if (!gzip_decompresser_.IsDecompressComplete())
            {
                downloader_->GetDownloadDriver()->GetStatistic()->SubmitHttpDataBytes(buffer.Length());
            }

            if (downloader_->GetDownloadDriver()->IsOpenService())
            {
                statistic::DACStatisticModule::Inst()->SubmitHttpDownloadBytes(buffer.Length());
            }
        }
        LOG(__DEBUG, "ppbug", __FUNCTION__ << ":" << __LINE__ << " SubPieceComplete " << sub_piece_info_);
        downloader_->GetDownloadDriver()->GetInstance()->AsyncAddSubPiece(sub_piece_info, buffer);
    }

    void HttpConnection::OnDecompressComplete(std::deque<boost::shared_ptr<protocol::SubPieceBuffer> > & sub_piece_buffer_deque)
    {
        boost::uint32_t offset = 0;

        for (std::deque<boost::shared_ptr<protocol::SubPieceBuffer> >::iterator iter = sub_piece_buffer_deque.begin();
            iter != sub_piece_buffer_deque.end(); ++iter)
        {
            protocol::SubPieceBuffer & buffer = *((*iter).get());
            OnRecvHttpDataSucced(buffer, offset, offset, false);
            offset += (*iter)->Length();
        }
    }
}