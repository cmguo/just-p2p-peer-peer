//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _DACSTATISTIC_MODULE_H_
#define _DACSTATISTIC_MODULE_H_

#include "message.h"

namespace statistic
{
    typedef struct _PERIOD_DAC_STATISTIC_INFO_STRUCT
    {
        boost::uint32_t       aPeerVersion[4];                      // 内核版本：major, minor, micro, extra
        boost::uint32_t       uP2PUploadKBytesByNomal;              // 普通P2P上传字节数
        boost::uint32_t       uP2PDownloadBytes;                    // P2P下载字节数
        boost::uint32_t       uHTTPDownloadBytes;                   // HTTP下载字节数
        boost::uint32_t       uP2PUploadKBytesByPush;               // PUSH P2P上传字节数
        boost::uint32_t       uUsedDiskSizeInMB;                    // 缓存目录已用大小
        boost::uint32_t       uTotalDiskSizeInMB;                   // 缓存目录设置大小
        boost::uint32_t       uUploadBandWidthInBytes;              // 上传带宽
        boost::uint32_t       uNeedUseUploadPingPolicy;             // 上传使用ping policy
        boost::uint32_t       uUploadLimitInBytes;                  // p2p上传限速字节数
        boost::uint32_t       uUploadDiscardBytes;                  // p2p上传限速导致被丢弃的报文字节数
    } PERIOD_DAC_STATISTIC_INFO_STRUCT;

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
}

#endif

