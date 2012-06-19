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
    FRAMEWORK_LOGGER_DECLARE_MODULE("dacstatistic");

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
        LOG(__DEBUG, "statistic", "interval_time = " << (int)m_IntervalTime);
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

        // D: 普通P2P上传字节数
        info.uP2PUploadKBytesByNomal = p2p_upload_byte_by_normal_ / 1024;
        // E: P2P下载字节数
        info.uP2PDownloadBytes = p2p_download_byte_;
        // F: HTTP下载字节数
        info.uHTTPDownloadBytes = http_download_byte_;
        // G: PUSH P2P上传字节数
        info.uP2PUploadKBytesByPush = p2p_upload_byte_by_push_ / 1024;
       
#if DISK_MODE
        Storage::p p_storage = Storage::Inst_Storage();
        // H: 缓存目录已用大小
        info.uUsedDiskSizeInMB = p_storage->GetUsedDiskSpace() / (1024 * 1024);
        // I: 缓存目录设置大小
        info.uTotalDiskSizeInMB = p_storage->GetStoreSize() / (1024 * 1024);
#else
        info.uUsedDiskSizeInMB  = 0;
        info.uTotalDiskSizeInMB = 0;
#endif
        // J: 上传带宽
        info.uUploadBandWidthInBytes = p2sp::P2PModule::Inst()->GetUploadBandWidthInBytes();
        // K: 上传使用ping policy
        info.uNeedUseUploadPingPolicy = p2sp::P2PModule::Inst()->NeedUseUploadPingPolicy();
        // L: p2p上传限速字节数
        upload_limit_KBytes_ += upload_limit_counter_.elapsed() * upload_speed_limit_KBps_ / 1000;
        info.uUploadLimitInBytes = upload_limit_KBytes_ * 1024;
        // M: p2p上传限速导致被丢弃的报文字节数
        info.uUploadDiscardBytes = upload_discard_byte_;
        // N: 本地RID数
        info.uLocalRidCount = storage::Storage::Inst_Storage()->LocalRidCount();
        // O: 上传过的RID数
        info.uRidUploadCountTotal = rid_upload_count_total_;
        // P: NAT节点类型
        info.uNatType = StunModule::Inst()->GetPeerNatType();
        // Q: 十分钟内上传的RID数
        info.uRidUploadCountInTenMinutes = rid_upload_count_in_ten_minutes_;

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

        string log = log_stream.str();

        LOGX(__DEBUG, "msg", "UM_PERIOD_DAC_STATISTIC");
        LOGX(__DEBUG, "msg", "  log = " << log);

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
