#include "Common.h"
#include "network/Uri.h"
#include "LiveHttpDownloader.h"
#include "p2sp/download/LiveDownloadDriver.h"
#include "statistic/DACStatisticModule.h"

using network::HttpClient;
using namespace base;
using namespace storage;

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("live_p2s");

    LiveHttpDownloader::LiveHttpDownloader(
        boost::asio::io_service & io_svc, 
        const protocol::UrlInfo &url_info, 
        const RID & rid,
        LiveDownloadDriver__p live_download_driver)
        : io_svc_(io_svc)
        , rid_(rid.to_string())
        , live_download_driver_(live_download_driver)
        , status_(closed)
        , sleep_timer_(global_second_timer(), 1000, boost::bind(&LiveHttpDownloader::OnTimerElapsed, this, &sleep_timer_))
        , http_status_(0)
        , connect_failed_times_(0)
        , is_pms_status_good_(true)
        , is_http_pausing_(true)
    {
        network::Uri uri(url_info.url_);
        pms_url_domain_ = uri.getdomain();
        pms_url_path_ = uri.getpath();
        boost::system::error_code ec = framework::string::parse2(uri.getport(), pms_url_port_);
        if (ec)
        {
            LOG(__DEBUG, "", "get port failed. use dafault port 80.");
            pms_url_port_ = 80;
        }

        transform(rid_.begin(), rid_.end(), rid_.begin(), tolower);
    }

    void LiveHttpDownloader::Start()
    {
        is_running_ = true;
        http_speed_info_.Start();
    }

    void LiveHttpDownloader::Stop()
    {
        is_running_ = false;
        live_download_driver_.reset();
        http_speed_info_.Stop();
    }

    void LiveHttpDownloader::Pause()
    {
        if (!is_http_pausing_)
        {
            is_http_pausing_ = true;

            if (status_ == sleeping)
            {
                sleep_timer_.stop();
            }

            while (!block_tasks_.empty())
            {
                live_download_driver_->OnBlockTimeout(block_tasks_.front());
            }

            if (http_client_)
            {
                http_client_->Close();
            }

            status_ = closed;
        }
    }

    void LiveHttpDownloader::Resume()
    {
        if (is_http_pausing_)
        {
            // 不在下载，申请Piece下载
            is_http_pausing_ = false;

            status_ = closed;
            RequestNextBlock();
        }
    }

    void LiveHttpDownloader::OnTimerElapsed(framework::timer::Timer * timer)
    {
        if (timer = &sleep_timer_)
        {
            sleep_timer_.stop();

            if (!is_running_)
            {
                return;
            }

            // 状态机暂停
            if (is_http_pausing_)
            {
                return;
            }

            assert(status_ == sleeping);

            status_ = closed;
            
            if (!block_tasks_.empty())
            {
                DoConnect();
            }
            else
            {
                RequestNextBlock();
            }
        }
    }

    void LiveHttpDownloader::PutBlockTask(const protocol::LiveSubPieceInfo & live_block)
    {
        block_tasks_.push_back(live_block);

        if (status_ == closed)
        {
            assert(!block_tasks_.empty());
            DoConnect();
        }
    }

    void LiveHttpDownloader::DoConnect()
    {
        if (!is_running_)
        {
            return;
        }

        // 当前有下载任务则直接下载，如果没有则向DownloadDriver请求任务
        if(!block_tasks_.empty())
        {
            if (http_client_)
            {
                http_client_->Close();
            }

            http_client_ = HttpClient<protocol::LiveSubPieceContent>::create(io_svc_, pms_url_domain_,
                pms_url_port_, MakeRequstPath(block_tasks_.front().GetBlockId()), "", 0, 0, false);
            http_client_->SetHandler(shared_from_this());

            // 二代直播 PMS请求 加入版本号
            // 用于后面添加防盗链
            http_client_->AddPragma("1.0", "");

            http_client_->Connect();

            status_ = connecting;
        }
        else
        {
            RequestNextBlock();
        }
    }

    string LiveHttpDownloader::MakeRequstPath(uint32_t start_block_id)
    {
        // 拼接请求PMS的HTTP串
        using framework::string::format;
        return pms_url_path_ + rid_ + "/" + format(start_block_id) + ".block";
    }

    void LiveHttpDownloader::OnConnectSucced()
    {
        connect_failed_times_ = 0;
        is_pms_status_good_ = true;

        assert(status_ == connecting);

        http_client_->HttpGet();
        status_ = sending_request_head;
    }

    void LiveHttpDownloader::OnConnectFailed(uint32_t error_code)
    {
        if (!is_running_)
            return;

        // 连接失败，重连
        connect_failed_times_++;
        status_ = closed;
        if (connect_failed_times_ > 20)
        {
            // 如果连接失败，每秒连接一次，持续20次连接失败
            // 默认PMS不可用，停止尝试连接
            assert(false);
            is_pms_status_good_ = false;
            return;
        }

        SleepForConnect();
    }

    void LiveHttpDownloader::OnConnectTimeout()
    {
        OnConnectFailed(0);
    }

    void LiveHttpDownloader::OnRecvHttpHeaderSucced(network::HttpResponse::p http_response)
    {
        if (!is_running_)
        {
            return;
        }

        LOG(__DEBUG, "", "OnRecvHttpHeaderSucced StatusCode " << http_response->GetStatusCode());

        http_status_ = http_response->GetStatusCode();

        switch(http_response->GetStatusCode())
        {
        case 200:
        case 206:
            // RequestSubPiece()当中可能调用DoConnect()把status_重置为closed;
            // 这里设置status_状态必须在调用RequestSubPiece()之前
            status_ = recving_response_data;
            RequestSubPiece();
            break;
        default:
            // 重试
            DoClose();
            SleepForConnect();
            break;
        }
    }

    void LiveHttpDownloader::OnRecvHttpHeaderFailed(uint32_t error_code)
    {
        LOG(__ERROR, "", "OnRecvHttpHeaderFailed!");

        if (!is_running_)
        {
            return;
        }

        DoClose();
        SleepForConnect();
    }

    void LiveHttpDownloader::OnRecvHttpDataSucced(protocol::LiveSubPieceBuffer const & buffer, uint32_t file_offset, uint32_t content_offset, bool is_gzip)
    {
        if (!is_running_)
        {
            return;
        }

        LOG(__DEBUG, "", "OnRecvHttpDataSucced! content_offset:" << content_offset << ", buff size=" << buffer.Length());

        uint16_t subpiece_index = file_offset / LIVE_SUB_PIECE_SIZE;

        live_download_driver_->GetInstance()->AddSubPiece(protocol::LiveSubPieceInfo(block_tasks_.front().GetBlockId(), subpiece_index), buffer);

        http_speed_info_.SubmitDownloadedBytes(buffer.Length());

        statistic::DACStatisticModule::Inst()->SubmitLiveHttpDownloadBytes(buffer.Length());

        LOG(__DEBUG, "", "Receive subpiece from http, block id = " << block_tasks_.front().GetBlockId()
            << ", subpiece index = " << subpiece_index);

        RequestSubPiece();
    }

    void LiveHttpDownloader::OnRecvHttpDataPartial(protocol::LiveSubPieceBuffer const & buffer, uint32_t file_offset, uint32_t content_offset)
    {
        LOG(__ERROR, "", "OnRecvHttpDataPartial!");
        OnError();
    }

    void LiveHttpDownloader::OnRecvHttpDataFailed(uint32_t error_code)
    {
        LOG(__ERROR, "", "OnRecvHttpDataFailed!");
        OnError();
    }

    void LiveHttpDownloader::OnRecvTimeout()
    {
        LOG(__ERROR, "", "OnRecvHttpHeaderFailed!");
        OnError();
    }
    
    void LiveHttpDownloader::OnComplete()
    {
        LOG(__ERROR, "", "OnComplete!");

        if (!is_running_)
        {
            return;
        }

        // 删除当前任务
        status_ = closed;

        live_download_driver_->OnBlockComplete(block_tasks_.front());

        if (!block_tasks_.empty())
        {
            // 任务队列不空,继续下载
            DoConnect();
        }
        else
        {
            // 任务队列为空,请求下一片任务
            RequestNextBlock();
        }
    }


    void LiveHttpDownloader::DoClose()
    {
        http_client_->Close();
        status_ = closed;
        
        while (!block_tasks_.empty())
        {
            live_download_driver_->OnBlockTimeout(block_tasks_.front());
        }
    }

    void LiveHttpDownloader::SleepForConnect()
    {
        http_client_->Close();
        status_ = sleeping;

        if (live_download_driver_->GetRestPlayableTime() > 2 * live_download_driver_->GetLiveInterval())
        {
            sleep_timer_.interval(live_download_driver_->GetLiveInterval()*1000);
        }
        else
        {
            sleep_timer_.interval(1000);
        }

        sleep_timer_.start();
    }

    void LiveHttpDownloader::RequestNextBlock()
    {
        live_download_driver_->RequestNextBlock(shared_from_this());
    }

    void LiveHttpDownloader::RequestSubPiece()
    {
        assert(!block_tasks_.empty());

        // 当前block 已经完成，无需再使用HTTP下载当前block
        if (live_download_driver_->GetInstance()->HasCompleteBlock(block_tasks_.begin()->GetBlockId()))
        {
            OnComplete();
            return;
        }

        http_client_->HttpRecvSubPiece();
        LOG(__DEBUG, "", "RequestSubPiece");
    }

    statistic::SPEED_INFO_EX LiveHttpDownloader::GetSpeedInfoEx()
    {
        return http_speed_info_.GetSpeedInfoEx();
    }

    boost::uint32_t LiveHttpDownloader::GetCurrentDownloadSpeed() 
    {
        if (false == is_running_)
            return 0;
        LOGX(__DEBUG, "live_http_download", "speed = " << GetSpeedInfoEx().NowDownloadSpeed);
        return GetSpeedInfoEx().NowDownloadSpeed;
    }

    boost::uint32_t LiveHttpDownloader::GetSecondDownloadSpeed() 
    {
        if (false == is_running_)
            return 0;
        return GetSpeedInfoEx().SecondDownloadSpeed;
    }

    uint32_t LiveHttpDownloader::GetMinuteDownloadSpeed()
    {
        if (false == is_running_)
            return 0;
        return GetSpeedInfoEx().MinuteDownloadSpeed;
    }

    uint32_t LiveHttpDownloader::GetRecentDownloadSpeed() 
    {
        if (false == is_running_)
            return 0;
        return GetSpeedInfoEx().RecentDownloadSpeed;
    }

    void LiveHttpDownloader::OnBlockTimeout(boost::uint32_t block_id)
    {
        std::deque<protocol::LiveSubPieceInfo>::iterator iter = block_tasks_.begin();
        for (; iter != block_tasks_.end(); ++iter)
        {
            if ((*iter).GetBlockId() == block_id)
            {
                block_tasks_.erase(iter);

                if (!is_http_pausing_)
                {
                    if (status_ != closed)
                    {
                        Pause();
                        global_io_svc().post(boost::bind(&LiveHttpDownloader::Resume, shared_from_this()));
                    }
                }

                return;
            }
        }

        assert(false);
    }

    void LiveHttpDownloader::OnDataRateChanged(const RID & rid)
    {
        if (is_http_pausing_)
        {
            rid_ = rid.to_string();
        }
        else
        {
            Pause();
            rid_ = rid.to_string();
            Resume();
        }
    }

    void LiveHttpDownloader::SetSpeedLimitInKBps(boost::int32_t speed_limit_in_KBps)
    {
    }

    void LiveHttpDownloader::OnError()
    {
        if (!is_running_)
        {
            return;
        }

        DoClose();
        SleepForConnect();
    }
}