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
        boost::uint32_t       aPeerVersion[4];                      // C: 内核版本：major, minor, micro, extra
        boost::uint32_t       uP2PUploadKBytesByNomal;              // D: 普通P2P上传字节数，单位kb
        boost::uint32_t       uP2PDownloadBytes;                    // E: P2P下载字节数，单位byte
        boost::uint32_t       uHTTPDownloadBytes;                   // F: HTTP下载字节数，单位byte
        boost::uint32_t       uP2PUploadKBytesByPush;               // G: PUSH P2P上传字节数，单位kb
        boost::uint32_t       uUsedDiskSizeInMB;                    // H: 缓存目录已用大小，单位MB
        boost::uint32_t       uTotalDiskSizeInMB;                   // I: 缓存目录设置大小，单位MB
        boost::uint32_t       uUploadBandWidthInBytes;              // J: 上传带宽，单位byte
        boost::uint32_t       uNeedUseUploadPingPolicy;             // K: 上传使用ping policy
        boost::uint32_t       uUploadLimitInBytes;                  // L: p2p上传限速字节数，单位byte
        boost::uint32_t       uUploadDiscardBytes;                  // M: p2p上传限速导致被丢弃的报文字节数，单位byte
        boost::uint32_t       uLocalRidCount;                       // N: 本地RID数
        boost::uint32_t       uRidUploadCountTotal;                 // O: 上传过的RID数
        boost::uint16_t       uNatType;                             // P: NAT节点类型
        boost::uint32_t       uRidUploadCountInTenMinutes;          // Q: 十分钟内上传的RID数
        boost::uint32_t       uUploadDataAvgSpeedInBytes;           // R: 上传数据平均速度，单位byte/s
        Guid                  PeerGuid;                             // S: Peer Guid
        boost::uint32_t       uUploadMaxSpeed;                      // T: 最大上传数据速度，单位kb/s
        boost::uint32_t       total_report_request_packet_count;    // U: 发给Tracker用于查询Report包的总数
        boost::uint32_t       total_report_response_packet_count;   // V: Tracker返回的Report包总数 
        boost::uint32_t       query_vod_tracker_for_list_request_count;    // W: 查询vod_tracker_for_list_request包的总数
        boost::uint32_t       query_vod_tracker_for_list_response_count;   // X: 查询vod_tracker_for_list_response包的总数
        boost::uint32_t       query_vod_tracker_for_report_request_count;  // Y: 查询vod_tracker_for_report_request包的总数
        boost::uint32_t       query_vod_tracker_for_report_response_count; // Z: 查询vod_tracker_for_report_response包的总数
        boost::uint32_t       query_live_tracker_for_list_request_count;   // A1: 查询live_tracker_for_list_request包的总数
        boost::uint32_t       query_live_tracker_for_list_response_count;  // B1: 查询live_tracker_for_list_response包的总数
        boost::uint32_t       query_live_tracker_for_report_request_count; // C1: 查询live_tracker_for_report_request包的总数
        boost::uint32_t       query_live_tracker_for_report_response_count;// D1: 查询live_tracker_for_report_response包的总数
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
        void SubmitReportRequest();
        void SubmitReportResponse();
        void SubmitVodTrackerForListRequest();
        void SubmitVodTrackerForListResponse();
        void SubmitVodTrackerForReportRequest();
        void SubmitVodTrackerForReportResponse();
        void SubmitLiveTrackerForListRequest();
        void SubmitLiveTrackerForListResponse();
        void SubmitLiveTrackerForReportRequest();
        void SubmitLiveTrackerForReportResponse();
        void SubmitRidUploadCount()
        {
            rid_upload_count_total_++;
            rid_upload_count_in_ten_minutes_++;
        }

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

        boost::uint32_t rid_upload_count_total_;
        boost::uint32_t rid_upload_count_in_ten_minutes_;

        boost::uint32_t total_report_request_count_;
        boost::uint32_t total_report_response_count_;
        boost::uint32_t query_vod_tracker_for_list_request_count_;
        boost::uint32_t query_vod_tracker_for_list_response_count_;
        boost::uint32_t query_vod_tracker_for_report_request_count_;
        boost::uint32_t query_vod_tracker_for_report_response_count_;
        boost::uint32_t query_live_tracker_for_list_request_count_;
        boost::uint32_t query_live_tracker_for_list_response_count_;
        boost::uint32_t query_live_tracker_for_report_request_count_;
        boost::uint32_t query_live_tracker_for_report_response_count_;
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

    inline void DACStatisticModule::SubmitReportRequest()
    {
        total_report_request_count_++;
    }

    inline void DACStatisticModule::SubmitReportResponse()
    {
        total_report_response_count_++;
    }

    inline void DACStatisticModule::SubmitVodTrackerForListRequest()
    {
        query_vod_tracker_for_list_request_count_++;
    }

    inline void DACStatisticModule::SubmitVodTrackerForListResponse()
    {
        query_vod_tracker_for_list_response_count_++;
    }

    inline void DACStatisticModule::SubmitVodTrackerForReportRequest()
    {
        query_vod_tracker_for_report_request_count_++;
    }

    inline void DACStatisticModule::SubmitVodTrackerForReportResponse()
    {
        query_vod_tracker_for_report_response_count_++;
    }

    inline void DACStatisticModule::SubmitLiveTrackerForListRequest()
    {
        query_live_tracker_for_list_request_count_++;
    }

    inline void DACStatisticModule::SubmitLiveTrackerForListResponse()
    {
        query_live_tracker_for_list_response_count_++;
    }

    inline void DACStatisticModule::SubmitLiveTrackerForReportRequest()
    {
        query_live_tracker_for_report_request_count_++;
    }

    inline void DACStatisticModule::SubmitLiveTrackerForReportResponse()
    {
        query_live_tracker_for_report_response_count_++;
    }
}

#endif

