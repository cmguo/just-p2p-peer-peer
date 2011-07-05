//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _DACSTATISTIC_MODULE_H_
#define _DACSTATISTIC_MODULE_H_

#include "message.h"

namespace statistic
{
    // TODO(herain):2011-1-4:这个为了兼容旧的SOP模块而保留了以前的代码
    // 在sop全部升级后发布的新内核可以删除这些代码
#ifndef CLIENT_NEW_DAC_LOG
    typedef struct _PERIOD_DAC_STATISTIC_INFO_STRUCT
    {
        uint32_t       uSize;
        Guid                  gPeerID;                              // 华哥ID
        boost::uint8_t        aPeerVersion[4];                      // 内核版本：major, minor, micro, extra
        boost::uint32_t       uP2PUploadKBytesByNomal;              // 统计时长（分钟）
        boost::uint32_t       uP2PDownloadBytes;                    // P2P下载字节数
        boost::uint32_t       uHTTPDownloadBytes;                   // HTTP下载字节数
        boost::uint32_t       uP2PUploadKBytesByPush;               // P2P上传字节数
        boost::uint32_t       uUsedDiskSizeInMB;                    // 缓存目录已用大小
        boost::uint32_t       uTotalDiskSizeInMB;                   // 缓存目录设置大小
        boost::uint32_t       uUploadBandWidthInBytes;              // 上传带宽
        boost::uint32_t       uIdleTimeInMins;                      // IDLE时长(分钟)
        boost::uint32_t       uUploadLimitInKBytes;                 // p2p上传限速字节数
        boost::uint32_t       uUploadDiscardBytes;                  // p2p上传限速导致被丢弃的报文字节数
    } PERIOD_DAC_STATISTIC_INFO_STRUCT, *LPPERIOD_DAC_STATISTIC_INFO_STRUCT;

#else
    typedef struct _PERIOD_DAC_STATISTIC_INFO_STRUCT
    {
        boost::uint32_t       aPeerVersion[4];                      // 内核版本：major, minor, micro, extra
        boost::uint32_t       uPeriodInMins;                        // 统计时长（分钟）
        boost::uint32_t       uP2PDownloadBytes;                    // P2P下载字节数
        boost::uint32_t       uHTTPDownloadBytes;                   // HTTP下载字节数
        boost::uint32_t       uP2PUploadKBytes;                     // P2P上传字节数
        boost::uint32_t       uUsedDiskSizeInMB;                    // 缓存目录已用大小
        boost::uint32_t       uTotalDiskSizeInMB;                   // 缓存目录设置大小
        boost::uint32_t       uUploadBandWidthInBytes;              // 上传带宽
        boost::uint32_t       uIdleTimeInMins;                      // IDLE时长(分钟)
        boost::uint32_t       uUploadLimitInBytes;                  // p2p上传限速字节数
        boost::uint32_t       uUploadDiscardBytes;                  // p2p上传限速导致被丢弃的报文字节数
    } PERIOD_DAC_STATISTIC_INFO_STRUCT;

#endif

    typedef struct _PERIOD_LIVE_DAC_STATISTIC_INFO_STRUCT
    {
        boost::uint32_t       PeerVersion[4];                      // 内核版本：major, minor, micro, extra
        boost::uint32_t       P2PDownloadSpeed;                    // P2P下载速度
        boost::uint32_t       HttpDownloadSpeed;                   // Http下载速度
        boost::uint32_t       P2PDownloadBytes;                    // P2P下载字节数
        boost::uint32_t       HTTPDownloadBytes;                   // HTTP下载字节数
        boost::uint32_t       P2PDownloadTimeInSecond;             // P2P下载的时间
        boost::uint32_t       HttpDownloadTimeInsecond;            // Http下载的时间
        boost::uint32_t       P2PUploadSpeed;                      // P2P上传速度
        boost::uint32_t       P2PUploadBytes;                      // P2P上传字节数
        boost::uint32_t       UploadBandWidthInBytes;              // 上传带宽
        boost::uint32_t       DataRateLevel;                       // 码流率等级
        boost::uint32_t       PlayingPosition;                     // 播放点
        boost::uint32_t       LivePosition;                        // 直播点
        boost::uint32_t       RestPlayTime;                        // 剩余时间
        boost::uint32_t       ConnectedPeers;                      // 连接上的节点数
        boost::uint32_t       QueryedPeers;                        // 查询到的节点数

        void Clear()
        {
            memset(this, 0, sizeof(_PERIOD_LIVE_DAC_STATISTIC_INFO_STRUCT));
        }

    } PERIOD_LIVE_DAC_STATISTIC_INFO_STRUCT;

    class DACStatisticModule
        : public boost::noncopyable
        , public boost::enable_shared_from_this<DACStatisticModule>
    {
    public:
        typedef boost::shared_ptr<DACStatisticModule> p;
        void Start();
        void Stop();

        void SubmitHttpDownloadBytes(uint32_t http_download_bytes);
        void SubmitP2PDownloadBytes(uint32_t p2p_download_bytes);
        void SubmitP2PUploadBytes(uint32_t p2p_upload_bytes, bool is_push);
        void SubmitP2PUploadSpeedInKBps(uint32_t p2p_upload_KBps);
        void SubmitP2PUploadSpeedLimitInKBps(boost::uint32_t p2p_upload_limit_KBps);
        void SubmitP2PUploadDisCardBytes(uint32_t p2p_upload_discard_bytes);

        void SubmitLiveHttpDownloadBytes(uint32_t live_http_download_bytes);
        void SubmitLiveP2PDownloadBytes(uint32_t live_p2p_download_bytes);
        void SubmitLiveP2PUploadBytes(uint32_t live_p2p_upload_bytes);
        void SubmitDataRateLevel(uint32_t data_rate_level);
        void SubmitPlayingPosition(uint32_t playing_position);
        void SubmitLivePosition(uint32_t live_position);
        void SubmitConnectedPeers(uint32_t connect_peers);
        void SubmitQueryedPeers(uint32_t queryed_peers);
        void SubmitP2PDownloadTime(uint32_t p2p_download_time);
        void SubmitHttpDownloadTime(uint32_t http_download_time);
        void SubmitRestPlayTime(uint32_t rest_play_time);

        void SetIntervalTime(boost::uint8_t interval_time);
        boost::uint8_t GetIntervalTime();

        void OnTimerElapsed(
            framework::timer::Timer * pointer);

        static DACStatisticModule::p Inst()
        {
            if (!inst_)
            {
                inst_.reset(new DACStatisticModule());
            }
            return inst_;
        }

    private:
        DACStatisticModule();

        void SendDacUploadMessage();
        void ResetDacUploadData();

        void SendLiveDacMessage();
        void ResetLiveDacData();

        bool is_running_;
        // 定时器
        framework::timer::PeriodicTimer timer_;
        static DACStatisticModule::p inst_;

        // 间隔时间
        boost::uint32_t m_IntervalTime;

        // 上传限速统计相关字段
        boost::uint32_t upload_speed_limit_KBps_;
        framework::timer::TickCounter upload_limit_counter_;
        boost::uint32_t upload_limit_KBytes_;

        // 上传限速丢弃字节数
        boost::uint32_t upload_discard_byte_;

        // HTTP下载字节
        uint32_t http_download_byte_;
        // P2P下载字节
        uint32_t p2p_download_byte_;
        // P2P上传字节
        boost::uint64_t p2p_upload_byte_by_normal_;
        boost::uint64_t p2p_upload_byte_by_push_;
        // P2P上传最大速度
        uint32_t max_peer_upload_kbps_;

        u_int idle_time_;

        PERIOD_LIVE_DAC_STATISTIC_INFO_STRUCT live_dac_statistic_info_;
    };

    inline void DACStatisticModule::SubmitHttpDownloadBytes(uint32_t http_download_kbps)
    {
        http_download_byte_ += http_download_kbps;
    }

    inline void DACStatisticModule::SubmitP2PDownloadBytes(uint32_t p2p_download_kbps)
    {
        p2p_download_byte_ += p2p_download_kbps;
    }

    inline void DACStatisticModule::SubmitP2PUploadBytes(uint32_t p2p_upload_bytes, bool is_push)
    {
        if (is_push)
        {
            p2p_upload_byte_by_push_ += p2p_upload_bytes;
        }
        else
        {
            p2p_upload_byte_by_normal_ += p2p_upload_bytes;
        }
    }

    inline void DACStatisticModule::SubmitP2PUploadSpeedInKBps(uint32_t p2p_upload_KBps)
    {
        if (max_peer_upload_kbps_ < p2p_upload_KBps)
        {
            max_peer_upload_kbps_ = p2p_upload_KBps;
        }
    }

    inline void DACStatisticModule::SubmitP2PUploadSpeedLimitInKBps(boost::uint32_t p2p_upload_limit_KBps)
    {
        upload_limit_KBytes_ += upload_limit_counter_.elapsed() * upload_speed_limit_KBps_ / 1000;
        upload_speed_limit_KBps_ = p2p_upload_limit_KBps;
        upload_limit_counter_.reset();
    }

    inline void DACStatisticModule::SubmitP2PUploadDisCardBytes(uint32_t p2p_upload_discard_bytes)
    {
        upload_discard_byte_ += p2p_upload_discard_bytes;
    }

    inline void DACStatisticModule::SubmitLiveHttpDownloadBytes(uint32_t live_http_download_bytes)
    {
        live_dac_statistic_info_.HTTPDownloadBytes += live_http_download_bytes;
    }

    inline void DACStatisticModule::SubmitLiveP2PDownloadBytes(uint32_t live_p2p_download_bytes)
    {
        live_dac_statistic_info_.P2PDownloadBytes += live_p2p_download_bytes;
    }

    inline void DACStatisticModule::SubmitLiveP2PUploadBytes(uint32_t live_p2p_upload_bytes)
    {
        live_dac_statistic_info_.P2PUploadBytes += live_p2p_upload_bytes;
    }

    inline void DACStatisticModule::SubmitDataRateLevel(uint32_t data_rate_level)
    {
        live_dac_statistic_info_.DataRateLevel = data_rate_level;
    }

    inline void DACStatisticModule::SubmitPlayingPosition(uint32_t playing_position)
    {
        live_dac_statistic_info_.PlayingPosition = playing_position;
    }

    inline void DACStatisticModule::SubmitLivePosition(uint32_t live_position)
    {
        live_dac_statistic_info_.LivePosition = live_position;
    }

    inline void DACStatisticModule::SubmitConnectedPeers(uint32_t connect_peers)
    {
        live_dac_statistic_info_.ConnectedPeers = connect_peers;
    }

    inline void DACStatisticModule::SubmitQueryedPeers(uint32_t queryed_peers)
    {
        live_dac_statistic_info_.QueryedPeers = queryed_peers;
    }

    inline void DACStatisticModule::SubmitP2PDownloadTime(uint32_t p2p_download_time)
    {
        live_dac_statistic_info_.P2PDownloadTimeInSecond += p2p_download_time;
    }

    inline void DACStatisticModule::SubmitHttpDownloadTime(uint32_t http_download_time)
    {
        live_dac_statistic_info_.HttpDownloadTimeInsecond += http_download_time;
    }

    inline void DACStatisticModule::SubmitRestPlayTime(uint32_t rest_play_time)
    {
        live_dac_statistic_info_.RestPlayTime = rest_play_time;
    }
}

#endif

