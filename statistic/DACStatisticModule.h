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
        std::string           stun_handshake_statistic_info_;       // E1:统计到stun发送handshake的次数和成功返回次数
        int                   nat_check_time_cost_in_ms;            // F1:统计Nat Check检测耗费时间,单位：毫秒
        boost::uint8_t        upnp_stat_;                           //G1:统计upnp的映射状态
        std::string           upnp_port_mapping_;                   //H1: 统计upnp成功和失败的次数，格式为:tcp成功数:tcp失败数:udp成功数:udp失败数
        std::string           nat_name_;                        //I1:从upnp模块获取到的路由器信息
        std::string           invalid_ip_count_;                // J1: 非法IP发送包的个数
        boost::int8_t         upnp_check_result;                       //K1:upnp成功之后，会触发一次natcheck的检测，这里记录检测的结果。0成功，1失败。默认值-1
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
        void SubmitStunHandShakeRequestCount(boost::uint32_t stun_ip);
        void SubmitStunHandShakeResponseCount(boost::uint32_t stun_ip);
        std::string GetStunHandShakeInfoString() const;
        void SubmitUpnpStat(boost::uint8_t stat);
        void SubmitUpnpPortMapping(bool isTcp,bool isSucc);
        std::string GetUpnpPortMappingString() const;
        void SetNatName(std::string &natName);
        void SetUpnpCheckResult(boost::int8_t upnp_check_result);

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

        void SubmitNatCheckTimeCost(uint32_t time_cost)
        {
            nat_check_time_cost_in_ms_ = time_cost;
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

        //<stun的ip，<发送的请求数，收到的回复数> >
        std::map<boost::uint32_t, std::pair<boost::uint32_t, boost::uint32_t> > stun_handshake_statistic_; 
        int nat_check_time_cost_in_ms_;

        //标记进行upnp到了哪个步骤
        boost::uint8_t       upnp_stat_; 

        //tcpport映射成功，失败，udpport映射成功，失败
        std::pair<std::pair<boost::uint32_t,boost::uint32_t>,std::pair<boost::uint32_t,boost::uint32_t> > upnp_port_mapping_;

        //nat的名称
        std::string           nat_name_;

        //upnp的natcheck的结果
        boost::int8_t        upnp_check_result_;
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

    inline void DACStatisticModule::SubmitStunHandShakeRequestCount(boost::uint32_t stun_ip)
    {
        if (stun_handshake_statistic_.find (stun_ip) != stun_handshake_statistic_.end())
        {
            stun_handshake_statistic_[stun_ip].first++;
        }
        else
        {
            stun_handshake_statistic_[stun_ip] = std::make_pair(1, 0);
        }
    }

    inline void DACStatisticModule::SubmitStunHandShakeResponseCount(boost::uint32_t stun_ip)
    {
        stun_handshake_statistic_[stun_ip].second++;
    }

    inline std::string DACStatisticModule::GetStunHandShakeInfoString() const
    {
        std::ostringstream os;
        std::map<boost::uint32_t, std::pair<boost::uint32_t, boost::uint32_t> >::const_iterator iter
            = stun_handshake_statistic_.begin();
        for (iter; iter != stun_handshake_statistic_.end(); )
        {
            in_addr add;
            add.s_addr = htonl(iter->first);           
            os <<inet_ntoa(add) <<":"
                << (uint32_t)iter->second.first << ":"
                << (uint32_t)iter->second.second;

            if (++iter != stun_handshake_statistic_.end())
            {
                os << ",";
            }
        }
        return os.str();
    }

    inline void DACStatisticModule::SubmitUpnpStat(boost::uint8_t stat)
    {
        upnp_stat_ = stat;
    }
    inline void DACStatisticModule::SubmitUpnpPortMapping(bool isTcp,bool isSucc)
    {
        if(isTcp){
            if(isSucc){
                ++upnp_port_mapping_.first.first;
            }
            else{
                ++upnp_port_mapping_.first.second;
            }
        }
        else{
            if(isSucc){
                ++upnp_port_mapping_.second.first;
            }
            else{
                ++upnp_port_mapping_.second.second;
            }
        }
    }

    inline std::string DACStatisticModule::GetUpnpPortMappingString() const
    {
       std::ostringstream os;
       os<<upnp_port_mapping_.first.first<<":"<<upnp_port_mapping_.first.second<<":"
           <<upnp_port_mapping_.second.first<<":"<<upnp_port_mapping_.second.second;
       return os.str();
    }   

    inline void DACStatisticModule::SetNatName(std::string &natName) 
    {
        nat_name_ = natName;
    }

    inline void DACStatisticModule::SetUpnpCheckResult(boost::int8_t upnp_check_result)
    {
        upnp_check_result_ = upnp_check_result;
    }

}

#endif

