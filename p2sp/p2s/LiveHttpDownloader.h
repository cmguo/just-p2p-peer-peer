#ifndef _LIVE_HTTPDOWNLOADER_H_
#define _LIVE_HTTPDOWNLOADER_H_

#include "p2sp/download/Downloader.h"
#include "p2sp/download/SwitchControllerInterface.h"
#include "statistic/SpeedInfoStatistic.h"
#include "network/HttpClient.h"

namespace p2sp
{
    class LiveDownloadDriver;
    typedef boost::shared_ptr<LiveDownloadDriver> LiveDownloadDriver__p;

    class LiveHttpDownloader
        : public LiveDownloader
        , public network::IHttpClientListener<protocol::LiveSubPieceBuffer>
        , public boost::enable_shared_from_this<LiveHttpDownloader>
#ifdef DUMP_OBJECT
        , public count_object_allocate<LiveHttpDownloader>
#endif
    {
    public:
        typedef boost::shared_ptr<LiveHttpDownloader> p;
        static p Create(boost::asio::io_service & io_svc, 
            const protocol::UrlInfo &url_info, 
            const RID & rid,
            LiveDownloadDriver__p live_download_driver)
        {
            return p(new LiveHttpDownloader(io_svc, url_info, rid, live_download_driver));
        }

    private:
        LiveHttpDownloader(
                boost::asio::io_service & io_svc, 
                const protocol::UrlInfo &url_info, 
                const RID & rid,
                LiveDownloadDriver__p live_download_driver);

        enum StatusEnum
        {
            closed, 
            connecting, 
            sending_request_head, 
            recving_response_data, 
            sleeping
        };

    public:
        void Pause();
        void Resume();

        // Downloader
        virtual void Stop();
        virtual bool IsPausing() {return is_http_pausing_;}

        virtual statistic::SPEED_INFO GetSpeedInfo();
        virtual statistic::SPEED_INFO_EX GetSpeedInfoEx();

        // LiveDownloader
        virtual void OnBlockTimeout(boost::uint32_t block_id);
        virtual void PutBlockTask(const protocol::LiveSubPieceInfo & live_block);

        virtual bool IsP2PDownloader() {return false;}

        // IHttpClientListener
        virtual void OnConnectSucced();
        virtual void OnConnectFailed(uint32_t error_code);
        virtual void OnConnectTimeout();

        virtual void OnRecvHttpHeaderSucced(network::HttpResponse::p http_response);
        virtual void OnRecvHttpHeaderFailed(uint32_t error_code);
        virtual void OnRecvHttpDataSucced(protocol::LiveSubPieceBuffer const & buffer, uint32_t file_offset, uint32_t content_offset, bool is_gzip);
        virtual void OnRecvHttpDataPartial(protocol::LiveSubPieceBuffer const & buffer, uint32_t file_offset, uint32_t content_offset);

        virtual void OnRecvHttpDataFailed(uint32_t error_code);
        virtual void OnRecvTimeout();

        virtual void OnComplete();

    public:
        void Start();
        void OnDataRateChanged(const RID & rid);
        boost::uint32_t GetHttpStatusCode() const;
        bool GetPmsStatus() const;  // for shared memory

    private:
        void OnTimerElapsed(framework::timer::Timer * timer);
        void DoConnect();
        void DoClose();
        void SleepForConnect();
        string MakeRequstPath(uint32_t start_piece_id);
        void RequestNextBlock();
        void RequestSubPiece();
        void OnError(); 

    private:
        boost::asio::io_service & io_svc_;
        LiveDownloadDriver__p live_download_driver_;
        network::HttpClient<protocol::LiveSubPieceContent>::p http_client_;

        string rid_;
        
        string pms_url_domain_;
        uint16_t pms_url_port_;
        string pms_url_path_;

        std::deque<protocol::LiveSubPieceInfo> block_tasks_;
        StatusEnum status_;
        framework::timer::PeriodicTimer sleep_timer_;

        statistic::SpeedInfoStatistic http_speed_info_;
        boost::uint32_t http_status_;

        uint32_t connect_failed_times_;
        bool is_pms_status_good_;  // true代表正常，false代表不正常

        bool is_http_pausing_;
    };

    inline statistic::SPEED_INFO LiveHttpDownloader::GetSpeedInfo()
    {
        return http_speed_info_.GetSpeedInfo();
    }

    inline boost::uint32_t LiveHttpDownloader::GetHttpStatusCode() const
    {
        return http_status_;
    }

    inline bool LiveHttpDownloader::GetPmsStatus() const
    {
        return is_pms_status_good_;
    }
}

#endif
