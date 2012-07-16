//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "DACStatisticModule.h"
#include "p2sp/proxy/MessageBufferManager.h"
#include "p2sp/AppModule.h"
#include "p2sp/p2p/P2PModule.h"
#include "storage/Storage.h"

#ifdef BOOST_WINDOWS_API
#include "WindowsMessage.h"
#endif
#include "p2sp/stun/StunModule.h"

using namespace storage;
using namespace p2sp;

namespace statistic
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_dac_statistic = log4cplus::Logger::getInstance("[dac_statistic]");
#endif

    DACStatisticModule::p DACStatisticModule::inst_;

    DACStatisticModule::DACStatisticModule()
        : is_running_(false)
        , http_download_byte_(0)
        , p2p_download_byte_(0)
        , p2p_upload_byte_by_normal_(0)
        , p2p_upload_byte_by_push_(0)
        , max_peer_upload_kbps_(0)
        , upload_speed_limit_KBps_(0)
        , upload_limit_KBytes_(0)
        , upload_discard_byte_(0)
        , m_IntervalTime(10)
        , timer_(global_second_timer(), 1000, boost::bind(&DACStatisticModule::OnTimerElapsed, this, &timer_))
        , rid_upload_count_total_(0)
        , rid_upload_count_in_ten_minutes_(0)
        , total_report_request_count_(0)
        , total_report_response_count_(0)
        , query_vod_tracker_for_list_request_count_(0)
        , query_vod_tracker_for_list_response_count_(0)
        , query_vod_tracker_for_report_request_count_(0)
        , query_vod_tracker_for_report_response_count_(0)
        , query_live_tracker_for_list_request_count_(0)
        , query_live_tracker_for_list_response_count_(0)
        , query_live_tracker_for_report_request_count_(0)
        , query_live_tracker_for_report_response_count_(0)
    {
    }

    void DACStatisticModule::Start()
    {
        is_running_ = true;
        timer_.start();
    }

    void DACStatisticModule::Stop()
    {
        is_running_ = false;
        timer_.stop();
        inst_.reset();
    }

    void DACStatisticModule::SetIntervalTime(boost::uint8_t interval_time)
    {
        m_IntervalTime = interval_time;
        LOG4CPLUS_DEBUG_LOG(logger_dac_statistic, "interval_time = " << (int)m_IntervalTime);
    }

    boost::uint8_t DACStatisticModule::GetIntervalTime()
    {
        return m_IntervalTime;
    }

    void DACStatisticModule::OnTimerElapsed(framework::timer::Timer * pointer)
    {
        if (is_running_ == false)
        {
            return;
        }

        if (pointer == &timer_)
        {
            if (m_IntervalTime != 0 && pointer->times() > 0 && pointer->times() % (m_IntervalTime * 60) == 0)
            {
#ifdef NEED_TO_POST_MESSAGE
                SendDacUploadMessage();
#endif
                ResetDacUploadData();
            }
        }
    }

    void DACStatisticModule::SendDacUploadMessage()
    {
#ifdef NEED_TO_POST_MESSAGE
        PERIOD_DAC_STATISTIC_INFO_STRUCT info;

        // C: 内核版本：major, minor, micro, extra
        info.aPeerVersion[0] = AppModule::GetKernelVersionInfo().Major;
        info.aPeerVersion[1] = AppModule::GetKernelVersionInfo().Minor;
        info.aPeerVersion[2] = AppModule::GetKernelVersionInfo().Micro;
        info.aPeerVersion[3] = AppModule::GetKernelVersionInfo().Extra;

        // D: 普通P2P上传字节数，单位kb
        info.uP2PUploadKBytesByNomal = p2p_upload_byte_by_normal_ / 1024;
        // E: P2P下载字节数，单位byte
        info.uP2PDownloadBytes = p2p_download_byte_;
        // F: HTTP下载字节数，单位byte
        info.uHTTPDownloadBytes = http_download_byte_;
        // G: PUSH P2P上传字节数，单位kb
        info.uP2PUploadKBytesByPush = p2p_upload_byte_by_push_ / 1024;
       
#if DISK_MODE
        Storage::p p_storage = Storage::Inst_Storage();
        // H: 缓存目录已用大小，单位MB
        info.uUsedDiskSizeInMB = p_storage->GetUsedDiskSpace() / (1024 * 1024);
        // I: 缓存目录设置大小，单位MB
        info.uTotalDiskSizeInMB = p_storage->GetStoreSize() / (1024 * 1024);
#else
        info.uUsedDiskSizeInMB  = 0;
        info.uTotalDiskSizeInMB = 0;
#endif
        // J: 上传带宽，单位byte
        info.uUploadBandWidthInBytes = p2sp::P2PModule::Inst()->GetUploadBandWidthInBytes();
        // K: 上传使用ping policy
        info.uNeedUseUploadPingPolicy = p2sp::P2PModule::Inst()->NeedUseUploadPingPolicy();
        // L: p2p上传限速字节数，单位byte
        upload_limit_KBytes_ += upload_limit_counter_.elapsed() * upload_speed_limit_KBps_ / 1000;
        info.uUploadLimitInBytes = upload_limit_KBytes_ * 1024;
        // M: p2p上传限速导致被丢弃的报文字节数，单位byte
        info.uUploadDiscardBytes = upload_discard_byte_;
        // N: 本地RID数
        info.uLocalRidCount = storage::Storage::Inst_Storage()->LocalRidCount();
        // O: 上传过的RID数
        info.uRidUploadCountTotal = rid_upload_count_total_;
        // P: NAT节点类型
        info.uNatType = StunModule::Inst()->GetPeerNatType();
        // Q: 十分钟内上传的RID数
        info.uRidUploadCountInTenMinutes = rid_upload_count_in_ten_minutes_;
        // R: 上传平均速度，单位byte/s
        info.uUploadAvgSpeedInBytes = UploadStatisticModule::Inst()->GetUploadAvgSpeed();
        // S: peer guid
        info.PeerGuid = AppModule::Inst()->GetPeerGuid();
        // T: 最大上传速度，单位kb/s
        info.uUploadMaxSpeed = max_peer_upload_kbps_;

        //U: 发给Tracker用于查询Report包的总数
        info.total_report_request_packet_count = total_report_request_count_;

        //V: Tracker返回的Report包总数
        info.total_report_response_packet_count = total_report_response_count_;

        //W: 查询vod_tracker_for_list_request包的总数
        info.query_vod_tracker_for_list_request_count = query_vod_tracker_for_list_request_count_;

        //X: 查询vod_tracker_for_list_response包的总数
        info.query_vod_tracker_for_list_response_count = query_vod_tracker_for_list_response_count_;

        // Y: 查询vod_tracker_for_report_request包的总数
        info.query_vod_tracker_for_report_request_count = query_vod_tracker_for_report_request_count_;

        // Z: 查询vod_tracker_for_report_response包的总数
        info.query_vod_tracker_for_report_response_count = query_vod_tracker_for_report_response_count_;

        // A1: 查询live_tracker_for_list_request包的总数
        info.query_live_tracker_for_list_request_count = query_live_tracker_for_list_request_count_;

        // B1: 查询live_tracker_for_list_response包的总数
        info.query_live_tracker_for_list_response_count = query_live_tracker_for_list_response_count_;

        // C1: 查询live_tracker_for_report_request包的总数
        info.query_live_tracker_for_report_request_count = query_live_tracker_for_report_request_count_;

        // D1: 查询live_tracker_for_report_response包的总数
        info.query_live_tracker_for_report_response_count = query_live_tracker_for_report_response_count_;

        // herain:2010-12-31:创建提交DAC的日志字符串
        ostringstream log_stream;

        log_stream << "C=" << (boost::uint32_t)info.aPeerVersion[0] << "." << 
            (boost::uint32_t)info.aPeerVersion[1] << "." << 
            (boost::uint32_t)info.aPeerVersion[2] << "." << 
            (boost::uint32_t)info.aPeerVersion[3];

        log_stream << "&D=" << (boost::uint32_t)info.uP2PUploadKBytesByNomal;
        log_stream << "&E=" << (boost::uint32_t)info.uP2PDownloadBytes;
        log_stream << "&F=" << (boost::uint32_t)info.uHTTPDownloadBytes;
        log_stream << "&G=" << (boost::uint32_t)info.uP2PUploadKBytesByPush;
        log_stream << "&H=" << (boost::uint32_t)info.uUsedDiskSizeInMB;
        log_stream << "&I=" << (boost::uint32_t)info.uTotalDiskSizeInMB;
        log_stream << "&J=" << (boost::uint32_t)info.uUploadBandWidthInBytes;
        log_stream << "&K=" << (boost::uint32_t)info.uNeedUseUploadPingPolicy;
        log_stream << "&L=" << (boost::uint32_t)info.uUploadLimitInBytes;
        log_stream << "&M=" << (boost::uint32_t)info.uUploadDiscardBytes;
        log_stream << "&N=" << (boost::uint32_t)info.uLocalRidCount;
        log_stream << "&O=" << (boost::uint32_t)info.uRidUploadCountTotal;
        log_stream << "&P=" << (boost::uint16_t)info.uNatType;
        log_stream << "&Q=" << (boost::uint32_t)info.uRidUploadCountInTenMinutes;
        log_stream << "&R=" << (boost::uint32_t)info.uUploadAvgSpeedInBytes;
        log_stream << "&S=" << info.PeerGuid.to_string();
        log_stream << "&T=" << (boost::uint32_t)info.uUploadMaxSpeed;
        log_stream << "&U=" << (uint32_t)info.total_report_request_packet_count;
        log_stream << "&V=" << (uint32_t)info.total_report_response_packet_count;
        log_stream << "&W=" << info.query_vod_tracker_for_list_request_count;
        log_stream << "&X=" << info.query_vod_tracker_for_list_response_count;
        log_stream << "&Y=" << info.query_vod_tracker_for_report_request_count;
        log_stream << "&Z=" << info.query_vod_tracker_for_report_response_count;
        log_stream << "&A1=" << info.query_live_tracker_for_list_request_count;
        log_stream << "&B1=" << info.query_live_tracker_for_list_response_count;
        log_stream << "&C1=" << info.query_live_tracker_for_report_request_count;
        log_stream << "&D1=" << info.query_live_tracker_for_report_response_count;

        string log = log_stream.str();

        LOG4CPLUS_DEBUG_LOG(logger_dac_statistic, "UM_PERIOD_DAC_STATISTIC");
        LOG4CPLUS_DEBUG_LOG(logger_dac_statistic, "  log = " << log);

        // herain:2010-12-31:创建并填充实际发送给客户端的消息结构
        LPPERIOD_DAC_STATISTIC_INFO upload_data =
            MessageBufferManager::Inst()->NewStruct<PERIOD_DAC_STATISTIC_INFO>();
        memset(upload_data, 0, sizeof(PERIOD_DAC_STATISTIC_INFO));

        upload_data->uSize = sizeof(PERIOD_DAC_STATISTIC_INFO);
        strncpy(upload_data->szLog, log.c_str(), sizeof(upload_data->szLog) - 1);

        WindowsMessage::Inst().PostWindowsMessage(UM_PERIOD_DAC_STATISTIC_V1, (WPARAM)0, (LPARAM)upload_data);
#endif
    }

    void DACStatisticModule::ResetDacUploadData()
    {
        // 清零
        http_download_byte_ = 0;
        p2p_download_byte_ = 0;
        p2p_upload_byte_by_normal_ = 0;
        p2p_upload_byte_by_push_ = 0;
        max_peer_upload_kbps_ = 0;
        upload_limit_KBytes_ = 0;
        upload_limit_counter_.reset();
        upload_discard_byte_ = 0;
        rid_upload_count_in_ten_minutes_ = 0;
    }
}
