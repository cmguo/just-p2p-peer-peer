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

using namespace storage;
using namespace p2sp;

namespace statistic
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("dacstatistic");

    DACStatisticModule::p DACStatisticModule::inst_(new DACStatisticModule());

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
        , idle_time_(0)
        , timer_(global_second_timer(), 1000, boost::bind(&DACStatisticModule::OnTimerElapsed, this, &timer_))
    {
    }

    void DACStatisticModule::Start()
    {
        is_running_ = true;
        live_dac_statistic_info_.Clear();
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
#ifdef BOOST_WINDOWS_API
            if (pointer->times() % 60 == 0)
            {
                // 每分钟
                if (AppModule::Inst()->GetIdleTimeInMins() > 1)
                {
                    idle_time_ |= 1;
                }

                idle_time_ <<= 1;
            }
#endif

            if (m_IntervalTime != 0 && pointer->times() > 0 && pointer->times() % (m_IntervalTime * 60) == 0)
            {
#ifdef NEED_TO_POST_MESSAGE
                SendDacUploadMessage();
                SendLiveDacMessage();
#endif
                ResetDacUploadData();
                ResetLiveDacData();
            }
        }
    }

    void DACStatisticModule::SendDacUploadMessage()
    {
#ifdef NEED_TO_POST_MESSAGE
        // 内核提交数据2日志格式
        // A：接口类别，固定为10
        // B: 用户的ID
        // C：peer版本
        // D：统计时长(Min)
        // E：P2P下载字节数(B)
        // F：HTTP 下载字节数(B)
        // G: P2P上传字节数(B)
        // H：缓存目录已用大小(MB)
        // I：缓存目录设置大小(MB
        // J：最大上传速度(B)
        // K：IDLE时长(分钟)
        // L：上传限速字节数 (B)
        // M：上传限速丢弃字节数 (B)

        // 统计数据没有意义，不用提交
//         if (p2p_download_byte_ == 0 && http_download_byte_ == 0 && p2p_upload_byte_ == 0)
//         {
//             return;
//         }

        // TODO(herain):2011-1-4:这个为了兼容旧的SOP模块而保留了以前的代码
        // 在sop全部升级后发布的新内核可以删除这些代码
#ifndef CLIENT_NEW_DAC_LOG
        // 分配内存 初始化
        LPPERIOD_DAC_STATISTIC_INFO_STRUCT lpPeriodDACStatisticInfo =
            MessageBufferManager::Inst()->NewStruct<PERIOD_DAC_STATISTIC_INFO_STRUCT>();
        memset(lpPeriodDACStatisticInfo, 0, sizeof(PERIOD_DAC_STATISTIC_INFO_STRUCT));
        lpPeriodDACStatisticInfo->uSize = sizeof(PERIOD_DAC_STATISTIC_INFO_STRUCT);

        // 华哥ID
        lpPeriodDACStatisticInfo->gPeerID = AppModule::Inst()->GetUniqueGuid();
        // 内核版本：major, minor, micro, extra
        lpPeriodDACStatisticInfo->aPeerVersion[0] = AppModule::GetKernelVersionInfo().Major;
        lpPeriodDACStatisticInfo->aPeerVersion[1] = AppModule::GetKernelVersionInfo().Minor;
        lpPeriodDACStatisticInfo->aPeerVersion[2] = AppModule::GetKernelVersionInfo().Micro;
        lpPeriodDACStatisticInfo->aPeerVersion[3] = AppModule::GetKernelVersionInfo().Extra;

        // 统计时长（分钟）
        lpPeriodDACStatisticInfo->uP2PUploadKBytesByNomal = p2p_upload_byte_by_normal_ / 1024;
        // P2P下载字节数
        lpPeriodDACStatisticInfo->uP2PDownloadBytes = p2p_download_byte_;
        // HTTP下载字节数
        lpPeriodDACStatisticInfo->uHTTPDownloadBytes = http_download_byte_;
        // P2P上传字节数
        lpPeriodDACStatisticInfo->uP2PUploadKBytesByPush = p2p_upload_byte_by_push_ / 1024;

        Storage::p p_storage = Storage::Inst_Storage();

#if DISK_MODE
        // 缓存目录已用大小
        lpPeriodDACStatisticInfo->uUsedDiskSizeInMB = (boost::uint32_t)(p_storage->GetUsedDiskSpace() / (1024 * 1024.0) + 0.5);
        // 缓存目录设置大小
        lpPeriodDACStatisticInfo->uTotalDiskSizeInMB = (boost::uint32_t)(p_storage->GetStoreSize() / (1024 * 1024.0) + 0.5);
#endif  // #if DISK_MODE
        // 上传带宽
        lpPeriodDACStatisticInfo->uUploadBandWidthInBytes = p2sp::P2PModule::Inst()->GetUploadBandWidthInBytes();
        // IDLE时长(分钟)
        //lpPeriodDACStatisticInfo->uIdleTimeInMins = idle_time_;
        lpPeriodDACStatisticInfo->uNeedUseUploadPingPolicy = p2sp::P2PModule::Inst()->NeedUseUploadPingPolicy();

        // 上传限速字节数
        upload_limit_KBytes_ += upload_limit_counter_.elapsed() * upload_speed_limit_KBps_ / 1000;
        lpPeriodDACStatisticInfo->uUploadLimitInKBytes = upload_limit_KBytes_;

        // 上传丢弃字节数
        lpPeriodDACStatisticInfo->uUploadDiscardBytes = upload_discard_byte_;

        LOGX(__DEBUG, "msg", "  Size = " << lpPeriodDACStatisticInfo->uSize);
        LOGX(__DEBUG, "msg", "  gPeerID = " << lpPeriodDACStatisticInfo->gPeerID.to_string());
        LOGX(__DEBUG, "msg", "  aPeerVersion = " <<
            (uint32_t)lpPeriodDACStatisticInfo->aPeerVersion[0] << "," << (uint32_t)lpPeriodDACStatisticInfo->aPeerVersion[1] << "," <<
            (uint32_t)lpPeriodDACStatisticInfo->aPeerVersion[2] << "," << (uint32_t)lpPeriodDACStatisticInfo->aPeerVersion[3]);
        LOGX(__DEBUG, "msg", "  uP2PUploadKBytesByPush = " << lpPeriodDACStatisticInfo->uP2PUploadKBytesByPush);
        LOGX(__DEBUG, "msg", "  uP2PDownloadBytes = " << lpPeriodDACStatisticInfo->uP2PDownloadBytes);
        LOGX(__DEBUG, "msg", "  uHTTPDownloadBytes = " << lpPeriodDACStatisticInfo->uHTTPDownloadBytes);
        LOGX(__DEBUG, "msg", "  uP2PUploadKBytesByNomal = " << lpPeriodDACStatisticInfo->uP2PUploadKBytesByNomal);
        LOGX(__DEBUG, "msg", "  uUsedDiskSizeInMB = " << lpPeriodDACStatisticInfo->uUsedDiskSizeInMB);
        LOGX(__DEBUG, "msg", "  uTotalDiskSizeInMB = " << lpPeriodDACStatisticInfo->uTotalDiskSizeInMB);
        LOGX(__DEBUG, "msg", "  uHasGateWay = " << lpPeriodDACStatisticInfo->uHasGateWay);
        LOGX(__DEBUG, "msg", "  uUploadLimitInKBytes = " << lpPeriodDACStatisticInfo->uUploadLimitInKBytes);
        LOGX(__DEBUG, "msg", "  uUploadDiscardBytes = " << lpPeriodDACStatisticInfo->uUploadDiscardBytes);

        WindowsMessage::Inst().PostWindowsMessage(UM_PERIOD_DAC_STATISTIC, (WPARAM)0, (LPARAM)lpPeriodDACStatisticInfo);
#else

        PERIOD_DAC_STATISTIC_INFO_STRUCT info;
        info.aPeerVersion[0] = AppModule::GetKernelVersionInfo().Major;
        info.aPeerVersion[1] = AppModule::GetKernelVersionInfo().Minor;
        info.aPeerVersion[2] = AppModule::GetKernelVersionInfo().Micro;
        info.aPeerVersion[3] = AppModule::GetKernelVersionInfo().Extra;
        info.uPeriodInMins = m_IntervalTime;
        info.uP2PDownloadBytes = p2p_download_byte_;
        info.uHTTPDownloadBytes = http_download_byte_;
        //TODO(herain):2011-5-25:upload_byte_要按照push和非push区分统计
#if DISK_MODE
        Storage::p p_storage = Storage::Inst_Storage();
        info.uUsedDiskSizeInMB = p_storage->GetUsedDiskSpace() / (1024 * 1024);
        info.uTotalDiskSizeInMB = p_storage->GetStoreSize() / (1024 * 1024);
#else
        info.uUsedDiskSizeInMB  = 0;
        info.uTotalDiskSizeInMB = 0;
#endif
        info.uUploadBandWidthInBytes = p2sp::P2PModule::Inst()->GetUploadBandWidthInBytes();
        info.uIdleTimeInMins = idle_time_;

        upload_limit_KBytes_ += upload_limit_counter_.elapsed() * upload_speed_limit_KBps_ / 1000;
        info.uUploadLimitInBytes = upload_limit_KBytes_ * 1024;
        info.uUploadDiscardBytes = upload_discard_byte_;

        // herain:2010-12-31:创建提交DAC的日志字符串
        ostringstream log_stream;

        log_stream << "C=" << info.aPeerVersion[0] << "." << info.aPeerVersion[1] << "."
            << info.aPeerVersion[2] << "." << info.aPeerVersion[3];
        log_stream << "&D=" << info.uPeriodInMins;
        log_stream << "&E=" << info.uP2PDownloadBytes;
        log_stream << "&F=" << info.uHTTPDownloadBytes;
        log_stream << "&G=" << info.uP2PUploadKBytes;
        log_stream << "&H=" << info.uUsedDiskSizeInMB;
        log_stream << "&I=" << info.uTotalDiskSizeInMB;
        log_stream << "&J=" << info.uUploadBandWidthInBytes;
        log_stream << "&K=" << info.uIdleTimeInMins;
        log_stream << "&L=" << info.uUploadLimitInBytes;
        log_stream << "&M=" << info.uUploadDiscardBytes;

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
        idle_time_ = 0;
    }

    void DACStatisticModule::SendLiveDacMessage()
    {
#ifdef NEED_TO_POST_MESSAGE

        // 内核周期性提交直播数据日志的格式
        // A：接口类别，固定为*(TODO(emma): 确定下来固定为几)
        // B: 用户的ID
        // C：peer版本
        // D: P2P下载速度
        // E: Http下载速度
        // F：P2P下载字节数(B)
        // G：HTTP下载字节数(B)
        // H: P2P下载的时间
        // I: Http下载的时间
        // J: P2P上传速度
        // K: P2P上传字节数(B)
        // L: 上传带宽(B)
        // M: 码流率等级
        // N: 播放点
        // O: 直播点
        // P: 剩余时间
        // Q: 连接上的节点数
        // R: 查询到的节点数
        // S: 频道ID
        // T: 从UdpServer下载的字节数(B)

        live_dac_statistic_info_.PeerVersion[0] = AppModule::GetKernelVersionInfo().Major;
        live_dac_statistic_info_.PeerVersion[1] = AppModule::GetKernelVersionInfo().Minor;
        live_dac_statistic_info_.PeerVersion[2] = AppModule::GetKernelVersionInfo().Micro;
        live_dac_statistic_info_.PeerVersion[3] = AppModule::GetKernelVersionInfo().Extra;
        live_dac_statistic_info_.UploadBandWidthInBytes = p2sp::P2PModule::Inst()->GetUploadBandWidthInBytes();
        live_dac_statistic_info_.P2PDownloadSpeed = live_dac_statistic_info_.P2PDownloadTimeInSecond == 0 ?
            0 : live_dac_statistic_info_.P2PDownloadBytes / live_dac_statistic_info_.P2PDownloadTimeInSecond;
        live_dac_statistic_info_.HttpDownloadSpeed = live_dac_statistic_info_.HttpDownloadTimeInsecond == 0 ?
            0 : live_dac_statistic_info_.HTTPDownloadBytes / live_dac_statistic_info_.HttpDownloadTimeInsecond;
        live_dac_statistic_info_.P2PUploadSpeed = live_dac_statistic_info_.P2PUploadBytes / (m_IntervalTime * 60);

        // 创建提交DAC的日志字符串
        ostringstream log_stream;

        log_stream << "C=" << live_dac_statistic_info_.PeerVersion[0] << "." << live_dac_statistic_info_.PeerVersion[1] << "."
            << live_dac_statistic_info_.PeerVersion[2] << "." << live_dac_statistic_info_.PeerVersion[3];
        log_stream << "&D=" << live_dac_statistic_info_.P2PDownloadSpeed;
        log_stream << "&E=" << live_dac_statistic_info_.HttpDownloadSpeed;
        log_stream << "&F=" << live_dac_statistic_info_.P2PDownloadBytes;
        log_stream << "&G=" << live_dac_statistic_info_.HTTPDownloadBytes;
        log_stream << "&H=" << live_dac_statistic_info_.P2PDownloadTimeInSecond;
        log_stream << "&I=" << live_dac_statistic_info_.HttpDownloadTimeInsecond;
        log_stream << "&J=" << live_dac_statistic_info_.P2PUploadSpeed;
        log_stream << "&K=" << live_dac_statistic_info_.P2PUploadBytes;
        log_stream << "&L=" << live_dac_statistic_info_.UploadBandWidthInBytes;
        log_stream << "&M=" << live_dac_statistic_info_.DataRateLevel;
        log_stream << "&N=" << live_dac_statistic_info_.PlayingPosition;
        log_stream << "&O=" << live_dac_statistic_info_.LivePosition;
        log_stream << "&P=" << live_dac_statistic_info_.RestPlayTime;
        log_stream << "&Q=" << live_dac_statistic_info_.ConnectedPeers;
        log_stream << "&R=" << live_dac_statistic_info_.QueryedPeers;
        log_stream << "&S=" << live_dac_statistic_info_.ChannelID;
        log_stream << "&T=" << live_dac_statistic_info_.UdpServerDownloadBytes;
        string log = log_stream.str();

        DebugLog("%s", log.c_str());

        LPPERIOD_DAC_STATISTIC_INFO upload_data =
            MessageBufferManager::Inst()->NewStruct<PERIOD_DAC_STATISTIC_INFO>();
        memset(upload_data, 0, sizeof(PERIOD_DAC_STATISTIC_INFO));

        upload_data->uSize = sizeof(PERIOD_DAC_STATISTIC_INFO);
        strncpy(upload_data->szLog, log.c_str(), sizeof(upload_data->szLog) - 1);

        WindowsMessage::Inst().PostWindowsMessage(UM_PERIOD_DAC_LIVE_STATISTIC, (WPARAM)0, (LPARAM)upload_data);
#endif
    }

    void DACStatisticModule::ResetLiveDacData()
    {
        live_dac_statistic_info_.Clear();
    }

}
