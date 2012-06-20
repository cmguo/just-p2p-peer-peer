#ifndef LIVE_STREAM_H
#define LIVE_STREAM_H

#include "p2sp/download/LiveBlockRequestManager.h"
#include "p2sp/download/LiveDownloadDriver.h"

namespace storage
{
    class LiveInstance;
    typedef boost::shared_ptr<LiveInstance> LiveInstance__p;
}

namespace p2sp
{
    class LiveDownloadDriver;
    typedef boost::shared_ptr<LiveDownloadDriver> LiveDownloadDriver__p;

    class LiveHttpDownloader;
    typedef boost::shared_ptr<LiveHttpDownloader> LiveHttpDownloader__p;

    class LiveP2PDownloader;
    typedef boost::shared_ptr<LiveP2PDownloader> LiveP2PDownloader__p;

    class ILiveStream
    {
    public:
        virtual const storage::LivePosition & GetStartPosition() = 0;
        virtual storage::LivePosition & GetPlayingPosition() = 0;
        virtual bool OnRecvLivePiece(uint32_t block_id, std::vector<protocol::LiveSubPieceBuffer> const & buffs, uint8_t progress_percentage) = 0;
        virtual ~ILiveStream(){ }
    };

    class LiveStream
        : public boost::enable_shared_from_this<LiveStream>
        , public ILiveStream
    {
    public:
        static boost::shared_ptr<LiveStream> Create(LiveDownloadDriver__p live_downloaddriver,
            const string & url, const RID & rid, boost::uint32_t live_interval, 
            boost::uint32_t default_data_rate)
        {
            return boost::shared_ptr<LiveStream>(new LiveStream(live_downloaddriver,
                url, rid, live_interval, default_data_rate));
        }

        void Start(boost::uint32_t start_position);

        void Stop();

        LiveHttpDownloader__p GetHttpDownloader() const
        {
            return live_http_downloader_;
        }

        LiveP2PDownloader__p GetP2PDownloader() const
        {
            return live_p2p_downloader_;
        }

        storage::LiveInstance__p GetInstance() const
        {
            return live_instance_;
        }

        boost::uint32_t GetRestPlayableTimeInSecond() const
        {
            return live_download_driver_->GetRestPlayableTime();
        }

        bool RequestNextBlock(LiveDownloader__p downloader);

        virtual bool OnRecvLivePiece(uint32_t block_id, std::vector<protocol::LiveSubPieceBuffer> const & buffs,
            uint8_t progress_percentage);

        virtual const storage::LivePosition & GetStartPosition()
        {
            return live_download_driver_->GetStartPosition();
        }

        virtual storage::LivePosition & GetPlayingPosition()
        {
            return live_download_driver_->GetPlayingPosition();
        }

        void RemoveBlockTask(const protocol::LiveSubPieceInfo & live_block)
        {
            live_block_request_manager_.RemoveBlockTask(live_block.GetBlockId());
        }

        JumpBWType GetBWType() const
        {
            return live_download_driver_->GetBWType();
        }

        bool GetReplay() const
        {
            return live_download_driver_->GetReplay();
        }

        boost::uint32_t GetSourceType() const
        {
            return live_download_driver_->GetSourceType();
        }

        bool IsUploadSpeedLargeEnough() const
        {
            return live_download_driver_->IsUploadSpeedLargeEnough();
        }

        bool IsUploadSpeedSmallEnough() const
        {
            return live_download_driver_->IsUploadSpeedSmallEnough();
        }

        boost::uint32_t GetDownloadTime() const
        {
            return live_download_driver_->GetDownloadTime();
        }

        boost::uint32_t GetDataRateInBytes() const
        {
            if (live_instance_->GetDataRate() == 0)
            {
                return default_data_rate_in_kbps_ / 8 * 1024;
            }
            return live_instance_->GetDataRate();
        }

        LiveDownloadDriver__p GetDownloadDriver() const
        {
            return live_download_driver_;
        }

        bool IsSavingMode() const
        {
            return live_download_driver_->IsSavingMode();
        }

        void SetUseCdnBecauseOfLargeUpload();
        void SetUseP2P();
        void UpdateCdnAccelerationHistory();
        void CalcCdnAccelerationStatusWhenStop();
        boost::uint32_t GetTimesOfUseCdnBecauseOfLargeUpload() const;
        boost::uint32_t GetTimeElapsedUseCdnBecauseOfLargeUpload() const;
        boost::uint32_t GetDownloadBytesUseCdnBecauseOfLargeUpload() const;
        boost::uint32_t GetTotalUploadBytesWhenUsingCdnBecauseOfLargeUpload() const;

    private:
        LiveStream(LiveDownloadDriver__p live_download_driver,
            const string & url,
            const RID & rid,
            boost::uint32_t live_interval,
            boost::uint32_t default_data_rate);

        bool HaveUsedCdnToAccelerateLongEnough() const;
        bool IsPopular() const;

    private:
        bool is_running_;

        string url_;

        RID rid_;

        boost::uint32_t live_interval_;

        boost::uint32_t default_data_rate_in_kbps_;

        JumpBWType bwtype_;

        LiveDownloadDriver__p live_download_driver_;

        LiveHttpDownloader__p live_http_downloader_;

        LiveP2PDownloader__p live_p2p_downloader_;

        LiveBlockRequestManager live_block_request_manager_;

        storage::LiveInstance__p live_instance_;

        boost::uint32_t times_of_use_cdn_because_of_large_upload_;
        boost::uint32_t time_elapsed_use_cdn_because_of_large_upload_;
        framework::timer::TickCounter use_cdn_tick_counter_;
        boost::uint32_t http_download_bytes_when_changed_to_cdn_;
        bool using_cdn_because_of_large_upload_;
        boost::uint32_t upload_bytes_when_changed_to_cdn_because_of_large_upload_;
        boost::uint32_t download_bytes_use_cdn_because_of_large_upload_;
        boost::uint32_t total_upload_bytes_when_using_cdn_because_of_large_upload_;

        framework::timer::TickCounter download_time_;
    };
}

#endif