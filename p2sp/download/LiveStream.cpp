#include "Common.h"
#include "p2sp/download/LiveStream.h"
#include "p2sp/download/LiveDownloadDriver.h"
#include "storage/LiveInstance.h"
#include "p2sp/p2s/LiveHttpDownloader.h"
#include "p2sp/p2p/LiveP2PDownloader.h"
#include "p2sp/p2p/P2PModule.h"

namespace p2sp
{
    LiveStream::LiveStream(LiveDownloadDriver__p live_download_driver,
        const string & url,
        const RID & rid,
        boost::uint32_t live_interval,
        boost::uint32_t default_data_rate)
        : live_download_driver_(live_download_driver)
        , url_(url)
        , rid_(rid)
        , live_interval_(live_interval)
        , default_data_rate_in_kbps_(default_data_rate)
        , is_running_(false)
        , times_of_use_cdn_because_of_large_upload_(0)
        , time_elapsed_use_cdn_because_of_large_upload_(0)
        , http_download_bytes_when_changed_to_cdn_(0)
        , using_cdn_because_of_large_upload_(false)
        , upload_bytes_when_changed_to_cdn_because_of_large_upload_(0)
        , download_bytes_use_cdn_because_of_large_upload_(0)
        , total_upload_bytes_when_using_cdn_because_of_large_upload_(0)
    {
    }

    void LiveStream::Start(boost::uint32_t start_position)
    {
        if (is_running_)
        {
            return;
        }

        // 创建Instance
        live_instance_ = boost::static_pointer_cast<storage::LiveInstance>(
            storage::Storage::Inst()->CreateLiveInstance(rid_, live_interval_));

        live_instance_->AttachStream(shared_from_this());
        live_instance_->SetCurrentLivePoint(storage::LivePosition(start_position));

        // 创建HttpDownloader
        live_http_downloader_ = LiveHttpDownloader::Create(url_, rid_, shared_from_this());

        live_http_downloader_->Start();
        live_http_downloader_->Pause();

        if (live_download_driver_->GetBWType() != JBW_HTTP_ONLY)
        {
            // 创建P2PDownloader
            live_p2p_downloader_ = LiveP2PDownloader::Create(rid_, shared_from_this());
            live_p2p_downloader_->Start();

            p2sp::P2PModule::Inst()->OnLiveP2PDownloaderCreated(live_p2p_downloader_);
        }

        download_time_.start();

        is_running_ = true;
    }

    void LiveStream::Stop()
    {
        if (!is_running_)
        {
            return;
        }

        live_instance_->DetachStream(shared_from_this());

        live_http_downloader_->Stop();
        live_http_downloader_.reset();

        if (live_p2p_downloader_)
        {
            live_p2p_downloader_->Stop();
            P2PModule::Inst()->OnLiveP2PDownloaderDestroyed(live_p2p_downloader_);
            live_p2p_downloader_.reset();
        }

        is_running_ = false;

        download_time_.stop();
    }

    bool LiveStream::RequestNextBlock(LiveDownloader__p downloader)
    {
        assert(is_running_);

        boost::uint32_t start_block_id = live_download_driver_->GetPlayingPosition().GetBlockId();
        while(1)
        {
            // 申请一片Block去下载
            protocol::LiveSubPieceInfo live_block;
            live_instance_->GetNextIncompleteBlock(start_block_id, live_block);

#ifdef USE_MEMORY_POOL
            // TODO: 如果使用内存池增加限制分配的逻辑，防止因为内存不足，卡住不播
            if (protocol::LiveSubPieceContent::get_left_capacity() < 2048 &&
                live_block.GetBlockId() > GetPlayingPosition().GetBlockId())
            {
                return false;
            }
#endif

            if (!live_block_request_manager_.IsRequesting(live_block.GetBlockId()))
            {
                // Block不在请求
                // 可以下载，加入
                LOG(__DEBUG, "live_download", "Not Requesting, Add id = " << live_block.GetBlockId());
                live_block_request_manager_.AddBlockTask(live_block, downloader);
                return true;
            }
            else
            {
                // Block正在请求
                if (live_block_request_manager_.IsTimeout(live_block.GetBlockId(), downloader))
                {
                    // 超时了
                    LOG(__DEBUG, "live_download", "Requesting & timeout Add id = " << live_block.GetBlockId());

                    // 再删除任务记录, 同时删除相应的downloader的block_task_
                    live_block_request_manager_.RemoveBlockTask(live_block.GetBlockId());

                    // 再加入
                    live_block_request_manager_.AddBlockTask(live_block, downloader);
                    return true;
                }
                else
                {
                    // 还没有超时
                    // 不能下载，寻找下一片
                    start_block_id += live_instance_->GetLiveInterval();
                }
            }
        }

        assert(false);
        return false;
    }

    bool LiveStream::OnRecvLivePiece(uint32_t block_id, std::vector<protocol::LiveSubPieceBuffer> const & buffs,
        uint8_t progress_percentage)
    {
        assert(is_running_);

        live_download_driver_->OnRecvLivePiece(block_id, buffs, progress_percentage);
        return true;
    }

    void LiveStream::SetUseCdnBecauseOfLargeUpload()
    {
        ++times_of_use_cdn_because_of_large_upload_;
        use_cdn_tick_counter_.reset();
        http_download_bytes_when_changed_to_cdn_ = GetHttpDownloader()->GetSpeedInfo().TotalDownloadBytes;
        using_cdn_because_of_large_upload_ = true;

        upload_bytes_when_changed_to_cdn_because_of_large_upload_ = statistic::StatisticModule::Inst()->GetUploadDataBytes();
    }

    void LiveStream::SetUseP2P()
    {
        time_elapsed_use_cdn_because_of_large_upload_ += use_cdn_tick_counter_.elapsed();
        download_bytes_use_cdn_because_of_large_upload_ += GetHttpDownloader()->GetSpeedInfo().TotalDownloadBytes - http_download_bytes_when_changed_to_cdn_;
        using_cdn_because_of_large_upload_ = false;

        total_upload_bytes_when_using_cdn_because_of_large_upload_ += statistic::StatisticModule::Inst()->GetUploadDataBytes() -
            upload_bytes_when_changed_to_cdn_because_of_large_upload_;
    }

    bool LiveStream::HaveUsedCdnToAccelerateLongEnough() const
    {
        return times_of_use_cdn_because_of_large_upload_ > 0 &&
            time_elapsed_use_cdn_because_of_large_upload_ > 30 * 1000;
    }

    void LiveStream::UpdateCdnAccelerationHistory()
    {
        bool is_popular = IsPopular();
        bool have_used_cdn_to_accelerate_long_enough = HaveUsedCdnToAccelerateLongEnough();

        if (!is_popular && !have_used_cdn_to_accelerate_long_enough)
        {
            return;
        }

        if (is_popular && download_time_.elapsed() < 60 * 1000 && !have_used_cdn_to_accelerate_long_enough)
        {
            return;
        }

        boost::uint32_t ratio_of_upload_to_download = 0;
        if (have_used_cdn_to_accelerate_long_enough)
        {
            boost::uint32_t total_download_bytes_in_theory = GetDataRateInBytes() * time_elapsed_use_cdn_because_of_large_upload_ / 1000;

            if (total_download_bytes_in_theory == 0)
            {
                return;
            }

            ratio_of_upload_to_download = total_upload_bytes_when_using_cdn_because_of_large_upload_ * 100 /
                total_download_bytes_in_theory;
        }

        live_download_driver_->AddCdnAccelerationHistory(ratio_of_upload_to_download);
    }

    bool LiveStream::IsPopular() const
    {
        return GetP2PDownloader() && GetP2PDownloader()->GetPooledPeersCount() >= BootStrapGeneralConfig::Inst()->GetDesirableLiveIpPoolSize();
    }

    void LiveStream::CalcCdnAccelerationStatusWhenStop()
    {
        if (using_cdn_because_of_large_upload_)
        {
            time_elapsed_use_cdn_because_of_large_upload_ += use_cdn_tick_counter_.elapsed();
            download_bytes_use_cdn_because_of_large_upload_ += GetHttpDownloader()->GetSpeedInfo().TotalDownloadBytes - http_download_bytes_when_changed_to_cdn_;

            total_upload_bytes_when_using_cdn_because_of_large_upload_ += statistic::StatisticModule::Inst()->GetUploadDataBytes() -
                upload_bytes_when_changed_to_cdn_because_of_large_upload_;
        }
    }

    boost::uint32_t LiveStream::GetTimesOfUseCdnBecauseOfLargeUpload() const
    {
        return times_of_use_cdn_because_of_large_upload_;
    }

    boost::uint32_t LiveStream::GetTimeElapsedUseCdnBecauseOfLargeUpload() const
    {
        return time_elapsed_use_cdn_because_of_large_upload_;
    }

    boost::uint32_t LiveStream::GetDownloadBytesUseCdnBecauseOfLargeUpload() const
    {
        return download_bytes_use_cdn_because_of_large_upload_;
    }

    boost::uint32_t LiveStream::GetTotalUploadBytesWhenUsingCdnBecauseOfLargeUpload() const
    {
        return total_upload_bytes_when_using_cdn_because_of_large_upload_;
    }
}