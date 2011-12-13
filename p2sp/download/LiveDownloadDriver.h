#ifndef _LIVE_DOWNLOADDRIVER_H_
#define _LIVE_DOWNLOADDRIVER_H_

#ifdef DUMP_OBJECT
#include "count_object_allocate.h"
#endif

#include "p2sp/p2s/LiveHttpDownloader.h"
#include "p2sp/p2p/LiveP2PDownloader.h"
#include "p2sp/download/LiveBlockRequestManager.h"
#include "p2sp/download/SwitchController.h"
#include "network/HttpResponse.h"
#include "storage/LiveInstance.h"
#include "p2sp/download/LiveRestTimeTracker.h"
#include "statistic/LiveDownloadDriverStatistic.h"
#include "p2sp/bootstrap/BootStrapGeneralConfig.h"

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
    class Downloader;
    typedef boost::shared_ptr<Downloader> Downloader__p;

    class LiveDownloader;
    typedef boost::shared_ptr<LiveDownloader> LiveDownloader__p;

    class ProxyConnection;
    typedef boost::shared_ptr<ProxyConnection> ProxyConnection__p;

    class LiveHttpDownloader;
    typedef boost::shared_ptr<LiveHttpDownloader> LiveHttpDownloader__p;

    class LiveP2PDownloader;
    typedef boost::shared_ptr<LiveP2PDownloader> LiveP2PDownloader__p;

    typedef struct _LIVE_DOWNLOADDRIVER_STOP_DAC_DATA_STRUCT
    {
        vector<RID>             ResourceIDs;            // 资源ID
        boost::uint32_t         PeerVersion[4];         // 内核版本：major, minor, micro, extra
        vector<boost::uint32_t> DataRates;              // 码流率
        string                  OriginalUrl;            // Url
        boost::uint32_t         P2PDownloadBytes;       // P2P下载字节数(不包括UdpServer)
        boost::uint32_t         HttpDownloadBytes;      // Http下载字节数
        boost::uint32_t         TotalDownloadBytes;     // 总下载字节数
        boost::uint32_t         AvgP2PDownloadSpeed;    // P2P平均速度
        boost::uint32_t         MaxP2PDownloadSpeed;    // P2P最大速度
        boost::uint32_t         MaxHttpDownloadSpeed;   // Http最大速度
        boost::uint32_t         ConnectedPeerCount;     // 连接上的节点数目
        boost::uint32_t         QueriedPeerCount;       // 查询到的节点数目
        boost::uint32_t         StartPosition;          // 开始播放点
        boost::uint32_t         JumpTimes;              // 跳跃次数
        boost::uint32_t         NumOfCheckSumFailedPieces;// 校验失败的piece个数
        boost::uint32_t         SourceType;             //
        RID                     ChannelID;              // 频道ID
        boost::uint32_t         UdpDownloadBytes;       // 从UdpServer下载的字节数
        boost::uint32_t         MaxUdpServerDownloadSpeed;  // 从UdpServer下载的最大速度
        boost::uint32_t         UploadBytes;            // 上传字节数
        boost::uint32_t         DownloadTime;           // 下载时间
        boost::uint32_t         TimesOfUseCdnBecauseLargeUpload;
        boost::uint32_t         TimeElapsedUseCdnBecauseLargeUpload;
        boost::uint32_t         DownloadBytesUseCdnBecauseLargeUpload;
        boost::uint32_t         TimesOfUseUdpServerBecauseUrgent;
        boost::uint32_t         TimeElapsedUseUdpServerBecauseUrgent;
        boost::uint32_t         DownloadBytesUseUdpServerBecauseUrgent;
        boost::uint32_t         TimesOfUseUdpServerBecauseLargeUpload;
        boost::uint32_t         TimeElapsedUseUdpServerBecauseLargeUpload;
        boost::uint32_t         DownloadBytesUseUdpServerBecauseLargeUpload;
        boost::uint32_t         MaxUploadSpeedIncludeSameSubnet;
        boost::uint32_t         MaxUploadSpeedExcludeSameSubnet;
        boost::uint32_t         MaxUnlimitedUploadSpeedInRecord;
        boost::uint8_t          ChangeToP2PConditionWhenStart;
        boost::uint32_t         ChangedToHttpTimesWhenUrgent;
        boost::uint32_t         BlockTimesWhenUseHttpUnderUrgentSituation;
        boost::uint32_t         MaxUploadSpeedDuringThisConnection;
        boost::uint32_t         AverageUploadConnectionCount;
        boost::uint32_t         TimeOfReceivingFirstConnectRequest;
        boost::uint32_t         TimeOfSendingFirstSubPiece;
        boost::uint32_t         TimeOfNonblankUploadConnections;
        boost::uint8_t          NatType;
        boost::uint32_t         HttpDownloadBytesWhenStart;
        boost::uint32_t         UploadBytesDuringThisConnection;
    } LIVE_DOWNLOADDRIVER_STOP_DAC_DATA_STRUCT;

    class ILiveDownloadDriver
    {
    public:
        virtual const storage::LivePosition & GetStartPosition() = 0;
        virtual storage::LivePosition & GetPlayingPosition() = 0;
        virtual bool OnRecvLivePiece(uint32_t block_id, std::vector<protocol::LiveSubPieceBuffer> const & buffs, uint8_t progress_percentage) = 0;
        virtual ~ILiveDownloadDriver(){ }
    };

    class DataRateManager
    {
    public:
        void Start(const vector<RID>& rids, const vector<boost::uint32_t>& data_rate_s)
        {
            assert(rids.size() > 0);
            assert(rids.size() == data_rate_s.size());
            rid_s_ = rids;
            current_data_rate_pos_ = 0;
            data_rate_s_ = data_rate_s;
            timer_.start();
        }
        RID GetCurrentRID()
        {
            assert(current_data_rate_pos_ < rid_s_.size());
            return rid_s_[current_data_rate_pos_];
        }

        // TODO: 跳转中心拿到的播放点与PMS最前的播放点的差距是否能超过 16 + 4 * current_data_rate_pos_
        bool SwitchToHigherDataRateIfNeeded(uint32_t rest_time_in_seconds)
        {
            boost::uint32_t last_pos = current_data_rate_pos_;
            if (timer_.elapsed() > 30*1000 && rest_time_in_seconds > 16 + 4 * current_data_rate_pos_)
            {
                current_data_rate_pos_++;
                if (current_data_rate_pos_ > rid_s_.size() - 1)
                {
                    current_data_rate_pos_ = rid_s_.size() - 1;
                }
            }

            if (last_pos != current_data_rate_pos_)
            {
                timer_.reset();
                return true;
            }

            return false;
        }

        bool SwitchToLowerDataRateIfNeeded(uint32_t rest_time_in_seconds)
        {
            boost::uint32_t last_pos = current_data_rate_pos_;
            if (timer_.elapsed() > 20*1000 && rest_time_in_seconds < 8 + 4 * current_data_rate_pos_)
            {
                if (current_data_rate_pos_ != 0)
                {
                    current_data_rate_pos_--;
                }
            }

            if (last_pos != current_data_rate_pos_)
            {
                timer_.reset();
                return true;
            }

            return false;
        }

        boost::uint32_t GetCurrentDataRatePos() const
        {
            return current_data_rate_pos_;
        }

        boost::uint32_t GetCurrentDefaultDataRate() const
        {
            assert(current_data_rate_pos_ < data_rate_s_.size());
            return data_rate_s_[current_data_rate_pos_];
        }

        const vector<boost::uint32_t>& GetDataRates() const
        {
            return data_rate_s_;
        }

        const vector<RID>& GetRids() const
        {
            return rid_s_;
        }

    private:
        vector<RID> rid_s_;
        framework::timer::TickCounter timer_;
        boost::uint32_t current_data_rate_pos_;
        // 码流率
        vector<boost::uint32_t> data_rate_s_;
    };

    class LiveDownloadDriver
        : public IGlobalControlTarget
        , public ILiveDownloadDriver
        , public boost::enable_shared_from_this<LiveDownloadDriver>
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
        void Start(const protocol::UrlInfo& url_info, const vector<RID>& rids, uint32_t start_position, uint32_t live_interval, bool replay,
            const vector<boost::uint32_t>& data_rate, const RID& channel_id, uint32_t source_type, JumpBWType bwtype, uint32_t unique_id);
        void Stop();

        bool RequestNextBlock(LiveDownloader__p downloader);

        bool OnRecvLivePiece(uint32_t block_id, std::vector<protocol::LiveSubPieceBuffer> const & buffs,
            uint8_t progress_percentage);

        void OnBlockComplete(const protocol::LiveSubPieceInfo & live_block);
        void OnBlockTimeout(const protocol::LiveSubPieceInfo & live_block);

        storage::LiveInstance__p GetInstance();

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

        boost::uint32_t GetLiveInterval() const
        {
            return live_instance_->GetLiveInterval();
        }

        virtual void OnConfigUpdated();

        boost::uint8_t GetLostRate() const;
        boost::uint8_t GetRedundancyRate() const;
        boost::uint32_t GetTotalRequestSubPieceCount() const;
        boost::uint32_t GetTotalRecievedSubPieceCount() const;

        void SetReceiveConnectPacket();
        void SetSendSubPiecePacket();

        boost::uint32_t GetDownloadTime() const;

    public:
        //IGlobalControlTarget
        virtual uint32_t GetBandWidth();
        virtual uint32_t GetFileLength(){return 0;}
        virtual uint32_t GetPlayElapsedTimeInMilliSec() {return 0;}
        virtual uint32_t GetDownloadingPosition() {return 0;}
        virtual uint32_t GetDownloadedBytes() {return 0;}
        virtual uint32_t GetDataDownloadSpeed() {return 0;}
        virtual bool IsStartFromNonZero() {return true;}
        virtual bool IsDrag() {return true;}
        virtual bool IsHeadOnly() {return true;}
        virtual bool HasRID() {return true;}

        virtual IHTTPControlTarget::p GetHTTPControlTarget();
        virtual IP2PControlTarget::p GetP2PControlTarget();
        virtual void OnStateMachineType(uint32_t state_machine_type) {};
        virtual void OnStateMachineState(const string& state_machine_state) {};
        virtual void SetSpeedLimitInKBps(boost::int32_t speed_in_KBps) {};
        virtual void SetSwitchState(boost::int32_t h, boost::int32_t p, boost::int32_t tu, boost::int32_t t);
        virtual boost::uint32_t GetRestPlayableTime();
        virtual void SetDragMachineState(boost::int32_t state) {};
        virtual bool IsDragLocalPlayForSwitch() {return true;}
        virtual boost::int32_t GetDownloadMode() {return true;}
        virtual void SetAcclerateStatus(boost::int32_t status) {};
        virtual JumpBWType GetBWType() {return bwtype_;}
        virtual void SetHttpHungry() {};
        virtual boost::uint32_t GetDataRate();
        virtual bool IsPPLiveClient() {return true;}
        virtual void NoticeLeave2300() {}
        virtual void SetDragHttpStatus(int32_t status) {}
        virtual std::vector<IHTTPControlTarget::p> GetAllHttpControlTargets() 
        {
            assert(false);
            std::vector<IHTTPControlTarget::p> v;
            return v;
        }

        virtual void ReportUseBakHost() {}
        virtual void ReportBakHostFail() {}

        virtual bool ShouldUseCDNWhenLargeUpload() const;
        virtual boost::uint32_t GetRestPlayTimeDelim() const;
        virtual bool IsUploadSpeedLargeEnough();
        virtual bool IsUploadSpeedSmallEnough();
        virtual bool GetUsingCdnTimeAtLeastWhenLargeUpload() const;

        virtual void SetUseCdnBecauseOfLargeUpload();
        virtual void SetUseP2P();
        virtual void SubmitChangedToP2PCondition(boost::uint8_t condition);
        virtual void SubmitChangedToHttpTimesWhenUrgent(boost::uint32_t times = 1);
        virtual void SubmitBlockTimesWhenUseHttpUnderUrgentCondition(boost::uint32_t times = 1);

        virtual bool GetReplay() const
        {
            return replay_;
        }

        virtual boost::uint32_t GetSourceType() const
        {
            return source_type_;
        }

    private:
        void OnTimerElapsed(framework::timer::Timer * pointer);

        void JumpTo(const storage::LivePosition & new_playing_position);

        void OnDataRateChanged();

        void SendDacStopData();

        void StartBufferringMonitor();

        void JumpOrSwitchIfNeeded();

    private:
        boost::shared_ptr<statistic::BufferringMonitor> bufferring_monitor_;
        boost::asio::io_service & io_svc_;
        ProxyConnection__p proxy_connection_;

        storage::LiveInstance__p live_instance_;

        LiveHttpDownloader__p live_http_downloader_;
        LiveP2PDownloader__p live_p2p_downloader_;
        LiveBlockRequestManager live_block_request_manager_;
        SwitchController::p switch_controller_;
        SwitchController::ControlModeType switch_control_mode_;

        DataRateManager data_rate_manager_;

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

        protocol::UrlInfo url_info_;

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

        bool use_cdn_when_large_upload_;
        boost::uint32_t rest_play_time_delim_;
        boost::uint32_t ratio_delim_of_upload_speed_to_datarate_;
        boost::uint32_t small_ratio_delim_of_upload_speed_to_datarate_;
        boost::uint32_t using_cdn_time_at_least_when_large_upload_;

        framework::timer::TickCounter download_time_;

        boost::uint32_t times_of_use_cdn_because_of_large_upload_;
        boost::uint32_t time_elapsed_use_cdn_because_of_large_upload_;
        boost::uint32_t download_bytes_use_cdn_because_of_large_upload_;

        framework::timer::TickCounter use_cdn_tick_counter_;
        boost::uint32_t http_download_bytes_when_changed_to_cdn_;

        bool using_cdn_because_of_large_upload_;

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

        boost::uint32_t p2p_protect_time_when_start_;

    private:
        // statistic

        statistic::SPEED_INFO_EX GetP2PSpeedInfoEx() const;
        statistic::SPEED_INFO_EX GetP2PSubPieceSpeedInfoEx() const;
        statistic::SPEED_INFO_EX GetHttpSpeedInfoEx() const;

        const std::map<boost::asio::ip::udp::endpoint, boost::shared_ptr<LivePeerConnection> > & GetPeerConnectionInfo() const;

        boost::uint8_t IsPlayingPositionBlockFull() const;

#ifndef STATISTIC_OFF
        void UpdateStatisticInfo();
#endif

#ifndef STATISTIC_OFF
    public:
        const statistic::LIVE_DOWNLOADDRIVER_STATISTIC_INFO & GetLiveDownloadDirverStatisticInfo() const;
#endif
    };

    inline boost::uint32_t LiveDownloadDriver::GetDataRate()
    {
        if (live_instance_->GetDataRate() == 0)
        {
            return data_rate_manager_.GetCurrentDefaultDataRate();
        }

        return live_instance_->GetDataRate();
    }

    inline boost::uint8_t LiveDownloadDriver::IsPlayingPositionBlockFull() const
    {
        return live_instance_->HasCompleteBlock(playing_position_.GetBlockId());
    }

    inline const std::map<boost::asio::ip::udp::endpoint, boost::shared_ptr<LivePeerConnection> > & LiveDownloadDriver::GetPeerConnectionInfo() const
    {
        return live_p2p_downloader_->GetPeerConnectionInfo();
    }

#ifndef STATISTIC_OFF
    inline const statistic::LIVE_DOWNLOADDRIVER_STATISTIC_INFO & LiveDownloadDriver::GetLiveDownloadDirverStatisticInfo() const
    {
        return live_download_driver_statistic_info_;
    }
#endif
}

#endif