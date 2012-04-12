#ifndef _LIVE_DOWNLOADDRIVER_H_
#define _LIVE_DOWNLOADDRIVER_H_

#ifdef DUMP_OBJECT
#include "count_object_allocate.h"
#endif

#include "p2sp/p2s/LiveHttpDownloader.h"
#include "p2sp/p2p/LiveP2PDownloader.h"
#include "p2sp/download/LiveBlockRequestManager.h"
#include "p2sp/download/LiveSwitchController.h"
#include "network/HttpResponse.h"
#include "storage/LiveInstance.h"
#include "p2sp/download/LiveRestTimeTracker.h"
#include "statistic/LiveDownloadDriverStatistic.h"
#include "p2sp/bootstrap/BootStrapGeneralConfig.h"
#include "UdpServersScoreHistory.h"
#include "p2sp/download/LiveDataRateManager.h"

namespace storage
{
    class LiveInstance;
    typedef boost::shared_ptr<LiveInstance> LiveInstance__p;
}

namespace statistic
{
    class BufferringMonitor;
}

namespace p2sp
{
    class LiveDownloader;
    typedef boost::shared_ptr<LiveDownloader> LiveDownloader__p;

    class ProxyConnection;
    typedef boost::shared_ptr<ProxyConnection> ProxyConnection__p;

    class LiveStream;
    typedef boost::shared_ptr<LiveStream> LiveStream__p;

    class LiveDownloadDriver
        : public boost::enable_shared_from_this<LiveDownloadDriver>
        , public ConfigUpdateListener
#ifdef DUMP_OBJECT
        , public count_object_allocate<LiveDownloadDriver>
#endif
    {
    public:
        typedef boost::shared_ptr<LiveDownloadDriver> p;
        static p create(boost::asio::io_service & io_svc, ProxyConnection__p proxy_connetction)
        {
            return p(new LiveDownloadDriver(io_svc, proxy_connetction));
        }

    private:
        LiveDownloadDriver(boost::asio::io_service & io_svc, ProxyConnection__p proxy_connetction);

    public:
        void Start(const string & url, const vector<RID>& rids, uint32_t start_position, uint32_t live_interval, bool replay,
            const vector<boost::uint32_t>& data_rate, const RID& channel_id, uint32_t source_type, JumpBWType bwtype, uint32_t unique_id);
        void Stop();

        bool OnRecvLivePiece(uint32_t block_id, std::vector<protocol::LiveSubPieceBuffer> const & buffs,
            uint8_t progress_percentage);

        storage::LiveInstance__p GetInstance() const;

        // 获得起始播放点
        const storage::LivePosition & GetStartPosition();
        // 获得当前数据推送点
        storage::LivePosition & GetPlayingPosition();
        
        const RID & GetChannelId()
        {
            return channel_id_;
        }

        void OnPause(bool pause);

        boost::uint32_t GetUniqueID() const
        {
            return unique_id_;
        }

        virtual void OnConfigUpdated();

        boost::uint8_t GetLostRate() const;
        boost::uint8_t GetRedundancyRate() const;
        boost::uint32_t GetTotalRequestSubPieceCount() const;
        boost::uint32_t GetTotalRecievedSubPieceCount() const;

        void SetReceiveConnectPacket();
        void SetSendSubPiecePacket();

        boost::uint32_t GetDownloadTime() const;

        void SetRestTimeInSecond(boost::uint32_t rest_time_in_second);

    public:
        LiveHttpDownloader__p GetHTTPControlTarget() const;
        LiveP2PDownloader__p GetP2PControlTarget() const;
        void SetSwitchState(boost::int32_t h, boost::int32_t p);
        boost::uint32_t GetRestPlayableTime();
        JumpBWType GetBWType() {return bwtype_;}
        boost::uint32_t GetDataRate() const;

        bool IsUploadSpeedLargeEnough();
        bool IsUploadSpeedSmallEnough();
        bool GetUsingCdnTimeAtLeastWhenLargeUpload() const;

        void SetUseCdnBecauseOfLargeUpload();
        void SetUseP2P();
        void SubmitChangedToP2PCondition(boost::uint8_t condition);
        void SubmitChangedToHttpTimesWhenUrgent(boost::uint32_t times = 1);
        void SubmitBlockTimesWhenUseHttpUnderUrgentCondition(boost::uint32_t times = 1);

        bool GetReplay() const
        {
            return replay_;
        }

        boost::uint32_t GetSourceType() const
        {
            return source_type_;
        }

        bool DoesFallBehindTooMuch() const;

        bool ShouldUseCdnToAccelerate();

        void UpdateUdpServerServiceScore(const boost::asio::ip::udp::endpoint& udp_server, int service_score);

        const std::map<boost::uint32_t, boost::uint32_t> GetUdpServerServiceScore() const;

        void AddCdnAccelerationHistory(boost::uint32_t ratio_of_upload_to_download);

    private:
        void OnTimerElapsed(framework::timer::Timer * pointer);

        void JumpTo(const storage::LivePosition & new_playing_position);

        void OnDataRateChanged();

        void SendDacStopData();

        void StartBufferringMonitor();

        void JumpOrSwitchIfNeeded();

        void LoadConfig();
        void CalcHistoryUploadStatus();

        void SaveHistoryConfig();
        void CalcCdnAccelerationStatusWhenStop(boost::uint32_t data_rate_pos);

    private:
        boost::shared_ptr<statistic::BufferringMonitor> bufferring_monitor_;
        boost::asio::io_service & io_svc_;
        ProxyConnection__p proxy_connection_;

        vector<LiveStream__p> live_streams_;
        
        LiveSwitchController live_switch_controller_;

        LiveDataRateManager data_rate_manager_;

        framework::timer::PeriodicTimer timer_;
        uint32_t elapsed_seconds_since_started_;

        // 起始播放点
        storage::LivePosition start_position_;
        // 数据推送点
        storage::LivePosition playing_position_;

        // 剩余时间的计算类
        RestTimeTracker rest_time_tracker_;

        // 是否回放
        bool replay_;

        boost::uint8_t switch_state_http_;
        boost::uint8_t switch_state_p2p_;

        static boost::uint32_t s_id_;
        boost::uint32_t id_;

        string url_;

        boost::uint32_t jump_times_;

        boost::uint32_t checksum_failed_times_;

#ifndef STATISTIC_OFF
        statistic::LiveDownloadDriverStatistic::p statistic_;
        statistic::LIVE_DOWNLOADDRIVER_STATISTIC_INFO live_download_driver_statistic_info_;
#endif
        boost::uint32_t http_download_max_speed_;
        boost::uint32_t p2p_download_max_speed_;
        boost::uint32_t udp_server_max_speed_;

        boost::uint32_t source_type_;
        JumpBWType bwtype_;
        RID channel_id_;
        boost::uint32_t unique_id_;

#if ((defined _DEBUG || defined DEBUG) && (defined CHECK_DOWNLOADED_FILE))
        FILE *fp_;
#endif

        boost::uint32_t ratio_delim_of_upload_speed_to_datarate_;
        boost::uint32_t small_ratio_delim_of_upload_speed_to_datarate_;
        boost::uint32_t using_cdn_time_at_least_when_large_upload_;

        framework::timer::TickCounter download_time_;

        boost::uint32_t total_times_of_use_cdn_because_of_large_upload_;
        boost::uint32_t total_time_elapsed_use_cdn_because_of_large_upload_;
        boost::uint32_t total_download_bytes_use_cdn_because_of_large_upload_;

        boost::uint8_t changed_to_p2p_condition_when_start_;
        boost::uint32_t changed_to_http_times_when_urgent_;
        boost::uint32_t block_times_when_use_http_under_urgent_situation_;

        boost::uint32_t max_upload_speed_during_this_connection_;
        // 为了计算平均上传连接数引入了总连接数，该值等于在连接存在期间每秒上传连接数之和
        boost::uint32_t total_upload_connection_count_;
        boost::uint32_t time_of_receiving_first_connect_request_;
        boost::uint32_t time_of_sending_first_subpiece_;
        // 上传连接数不为0的时间
        boost::uint32_t time_of_nonblank_upload_connections_;

        bool has_received_connect_packet_;
        bool has_sended_subpiece_packet_;

        boost::uint32_t http_download_bytes_when_start_;

        boost::uint32_t upload_bytes_when_start_;

        static const boost::uint8_t InitialChangedToP2PConditionWhenStart;

        framework::timer::TickCounter tick_count_since_last_recv_subpiece_;

        bool is_notify_restart_;

        boost::uint32_t max_push_data_interval_;

        boost::uint32_t p2p_protect_time_when_start_;

        std::vector<boost::uint32_t> rest_playable_times_;

        boost::uint32_t total_upload_bytes_when_using_cdn_because_of_large_upload_;

        std::vector<boost::uint32_t> ratio_of_upload_to_download_on_history_;
        UdpServersScoreHistory udpservers_score_history_;

        bool is_history_upload_good_;
        framework::timer::TickCounter tick_counter_since_last_advance_using_cdn_;
        boost::uint32_t history_record_count_;

        class LiveHistorySettings
        {
        public:
            static const std::string RatioOfUploadToDownload;
            static const std::string UdpServerScore;
            static const std::string UdpServerIpAddress;
        };

    private:
        // statistic

        statistic::SPEED_INFO_EX GetP2PSpeedInfoEx() const;
        statistic::SPEED_INFO_EX GetP2PSubPieceSpeedInfoEx() const;
        statistic::SPEED_INFO_EX GetHttpSpeedInfoEx() const;

        boost::uint8_t IsPlayingPositionBlockFull() const;

#ifndef STATISTIC_OFF
        void UpdateStatisticInfo();
#endif

        boost::uint32_t CalcAverageOfRestPlayableTime();
        boost::uint32_t CalcVarianceOfRestPlayableTime(boost::uint32_t average_of_rest_playable_time);

#ifndef STATISTIC_OFF
    public:
        const statistic::LIVE_DOWNLOADDRIVER_STATISTIC_INFO & GetLiveDownloadDirverStatisticInfo() const;
#endif
    };

    inline boost::uint8_t LiveDownloadDriver::IsPlayingPositionBlockFull() const
    {
        return GetInstance()->HasCompleteBlock(playing_position_.GetBlockId());
    }

#ifndef STATISTIC_OFF
    inline const statistic::LIVE_DOWNLOADDRIVER_STATISTIC_INFO & LiveDownloadDriver::GetLiveDownloadDirverStatisticInfo() const
    {
        return live_download_driver_statistic_info_;
    }
#endif
}

#endif