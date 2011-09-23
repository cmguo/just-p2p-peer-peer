//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/download/DownloadDriver.h"
#include "p2sp/p2s/HttpDownloader.h"
#include "p2sp/download/Downloader.h"
#include "p2sp/download/PieceRequestManager.h"
#include "p2sp/proxy/ProxyConnection.h"
#include "p2sp/proxy/MessageBufferManager.h"
#include "p2sp/p2p/P2PModule.h"
#include "p2sp/p2p/P2PDownloader.h"
#include "p2sp/AppModule.h"
#include "p2sp/p2p/IpPool.h"
#include "p2sp/index/IndexManager.h"
#include "p2sp/push/PushModule.h"
#include "p2sp/p2s/HttpDragDownloader.h"
#include "p2sp/proxy/PlayInfo.h"
#include "p2sp/proxy/ProxyModule.h"
#include "random.h"

#include "storage/Storage.h"
#include "storage/Instance.h"
#include "statistic/StatisticModule.h"
#include "statistic/BufferringMonitor.h"
#ifdef NEED_TO_POST_MESSAGE
#include "WindowsMessage.h"
#include "base/wsconvert.h"
#endif

#include "message.h"

#include <boost/algorithm/string/predicate.hpp>

#define DD_DEBUG(s)    LOG(__DEBUG, "P2P", s)
#define DD_INFO(s)    LOG(__INFO, "P2P", s)
#define DD_EVENT(s)    LOG(__EVENT, "P2P", s)
#define DD_WARN(s)    LOG(__WARN, "P2P", s)
#define DD_ERROR(s)    LOG(__ERROR, "P2P", s)

using namespace network;
using namespace statistic;
using namespace storage;
using namespace protocol;

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("download");

    const string SYNACAST_FLV_STREAMING = "synacast_flv_streaming";
    const string SYNACAST_MP4_STREAMING = "synacast_mp4_streaming";

    inline bool IsFileUrl(const string& url)
    {
        Uri uri(url);
        string file_url = uri.getfile();
        return boost::algorithm::icontains(file_url, ".exe")
            || boost::algorithm::icontains(file_url, ".dll")
            || boost::algorithm::icontains(file_url, ".swf")
            || boost::algorithm::icontains(file_url, ".jpg")
            || boost::algorithm::icontains(file_url, ".png")
            || boost::algorithm::icontains(file_url, ".gif")
            || file_url == "/"
           ;
    }

    inline bool IsFileExtension(const string& url, const string& ext)
    {
        Uri uri(url);
        string file_url = uri.getfile();
        return boost::algorithm::iends_with(file_url, ext);
    }

    inline string GenerateOpenServiceMod(const string& url)
    {
        if (IsFileExtension(url, ".flv"))
        {
            return SYNACAST_FLV_STREAMING;
        }
        else if (IsFileExtension(url, ".mp4"))
        {
            return SYNACAST_MP4_STREAMING;
        }
        return "";
    }

    boost::int32_t DownloadDriver::s_id_ = 1;

    DownloadDriver::DownloadDriver(
        boost::asio::io_service & io_svc,
        ProxyConnection::p proxy_connetction)
        : io_svc_(io_svc)
        , is_running_(false)
        , block_check_faild_times_(0)
        , is_complete_(false)
        , proxy_connection_(proxy_connetction)
        , id_(s_id_++)
        , is_pausing_(false)
        , is_open_service_(false)
        , is_http_403_header_(false)
        , is_http_304_header_(false)
        , is_play_by_rid_(false)
        , switch_control_mode_(SwitchController::CONTROL_MODE_NULL)
        , is_drag_local_play(false)
        , drag_machine_state_(SwitchController::MS_WAIT)
        , is_pool_mode_(false)
        , need_bubble_(true)
        , is_push_(false)
        , speed_limit_in_KBps_(P2SPConfigs::P2P_DOWNLOAD_SPEED_LIMIT)
        , is_pragmainfo_noticed_(false)
        , bwtype_(JBW_NORMAL)
        , rest_play_time_(-1)
#ifndef PEER_PC_CLIENT
        // ppbox 版本默认高速模式
        , download_mode_(IGlobalControlTarget::FAST_MODE)
#else
        // pc 版本默认智能限速模式
        , download_mode_(IGlobalControlTarget::SMART_MODE)
#endif
        , accelerate_status_(255)
        , is_got_accelerate_http_speed(false)
        , accelerate_http_speed(0)
        , init_local_data_bytes_(-1)
        , speed_limit_timer_(global_second_timer(), 1000, boost::bind(&DownloadDriver::OnTimerElapsed, this, &speed_limit_timer_))
        , disable_smart_speed_limit_(false)
        , avg_download_speed_before_limit_(-1)
        , avg_http_download_speed_in2300_(-1)
        , drag_http_status_(0)
        , openservice_head_length_(0)
        , drag_fetch_result_(0)
        , bak_host_status_(BAK_HOST_NONE)
        , max_rest_playable_time_(0)
    {
        source_type_ = PlayInfo::SOURCE_DEFAULT;  // default
        is_head_only_ = false;
    }

    DownloadDriver::~DownloadDriver()
    {
    }

    // URL
    void DownloadDriver::Start(const network::HttpRequest::p http_request_demo, const protocol::UrlInfo& origanel_url_info,
        bool is_support_start, boost::int32_t control_mode)
    {
        if (is_running_ == true)
            return;

        // 智能限速定时器
        speed_limit_timer_.start();

        is_running_ = true;

        // check push module
        // if (!IsPush() && origanel_url_info.url_ == PushModule::Inst()->GetCurrentTaskUrl())
        // {
        //    LOG(__DEBUG, "push", "Url Same as push task: " << origanel_url_info.url_);
        //    PushModule::Inst()->StopCurrentTask();
        // }

        switch_controller_ = SwitchController::Create(shared_from_this());

        download_time_counter_.reset();

        is_play_by_rid_ = false;

        is_http_403_header_ = false;
        is_http_304_header_ = false;

        origanel_url_info_ = origanel_url_info;
        is_support_start_ = is_support_start;
        is_pragmainfo_noticed_ = false;

        // check 'www.pp.tv' refer
        if (origanel_url_info_.refer_url_.find("www.pp.tv") != string::npos)
            origanel_url_info_.refer_url_ = "";

        DD_EVENT("Downloader Driver Start" << shared_from_this());

        // 关联 Statistic 模块
        statistic_ = StatisticModule::Inst()->AttachDownloadDriverStatistic(id_, true);
        assert(statistic_);
        statistic_->SetOriginalUrl(origanel_url_info.url_);
        statistic_->SetOriginalReferUrl(origanel_url_info.refer_url_);
        statistic_->SetHidden(!need_bubble_);


        // 包含.exe则不发送消息
        if (false == IsFileUrl(origanel_url_info.url_))
        {
            if (true == need_bubble_)
            {
                // 计算需要发出的消息的参数
                DOWNLOADDRIVERSTARTDATA* lpDownloadDriverStartData = MessageBufferManager::Inst()->NewStruct<
                    DOWNLOADDRIVERSTARTDATA> ();
                memset(lpDownloadDriverStartData, 0, sizeof(DOWNLOADDRIVERSTARTDATA));
                lpDownloadDriverStartData->uSize = sizeof(DOWNLOADDRIVERSTARTDATA);

                strncpy(lpDownloadDriverStartData->szOriginalUrl, origanel_url_info.url_.c_str(),
                    sizeof(lpDownloadDriverStartData->szOriginalUrl)-1);
                strncpy(lpDownloadDriverStartData->szOriginalReferUrl, origanel_url_info.refer_url_.c_str(),
                    sizeof(lpDownloadDriverStartData->szOriginalReferUrl)-1);

                LOG(__EVENT, "pplive", __FUNCTION__ << "PostWindowsMessage(UM_DONWLOADDRIVER_START, (WPARAM)id_, (LPARAM)lpDownloadDriverStartData)");
    #ifdef NEED_TO_POST_MESSAGE
                WindowsMessage::Inst().PostWindowsMessage(UM_DONWLOADDRIVER_START, (WPARAM)id_, (LPARAM)lpDownloadDriverStartData);
    #endif
            }
            else
            {
                statistic_->SetHidden(true);
            }

            switch_control_mode_ = SwitchController::CONTROL_MODE_VIDEO;
        }
        else
        {
            // 下载模式
            switch_control_mode_ = SwitchController::CONTROL_MODE_DOWNLOAD;
        }

        // 在 Storage 里面 创建资源实例
        instance_ = boost::dynamic_pointer_cast<storage::Instance>(Storage::Inst()->CreateInstance(origanel_url_info));
        assert(instance_);
        instance_->SetIsOpenService(is_open_service_);

        if (instance_)
        {
            if (false == instance_->GetRID().is_empty())
            {
                statistic_->SetResourceID(instance_->GetRID());
                statistic_->SetFileLength(instance_->GetFileLength());
                statistic_->SetBlockSize(instance_->GetBlockSize());
                statistic_->SetBlockCount(instance_->GetBlockCount());
            }

            statistic_->SetLocalDataBytes(instance_->GetDownloadBytes());
        }

        piece_request_manager_ = PieceRequestManager::Create(shared_from_this());
        piece_request_manager_->Start();

        // 资源创建成功，资源对象绑定 这个DownloadDriver
        instance_->AttachDownloadDriver(shared_from_this());

        // 如果该 instance 没有 RID, 那么 要让 IndexManager 发出 QueryRidByUrl 请求
        IndexManager::Inst()->DoQueryRidByUrl(origanel_url_info.url_, origanel_url_info.refer_url_);

        // if (instance_->GetRID().IsEmpty())
        // {
        //    if (false == instance_->IsComplete())
        //        instance_->DoMakeContentMd5AndQuery();
        // }

        // 检查本地是否有已下资源，向proxy_connection_发送content length
        if (instance_->HasRID())
        {
            LOG(__DEBUG, "bug", __FUNCTION__ << ":" << __LINE__ << " instance-IsComplete");
            proxy_connection_->OnNoticeGetContentLength(instance_->GetFileLength(), network::HttpResponse::p());
            // check
        }

        if (IsFileExtension(origanel_url_info_.url_, ".mp3") || IsFileExtension(origanel_url_info_.url_, ".wma"))
        {
            if (false == instance_->GetRID().is_empty() || true == instance_->IsComplete())
            {
                DD_DEBUG("instance is music file; rid available or file complete!");
                proxy_connection_->OnNoticeGetContentLength(instance_->GetFileLength(), network::HttpResponse::p());
            }
        }

        assert(downloaders_.size() == 0);

        if (false == instance_->GetRID().is_empty() && false == instance_->IsComplete())
        {
            p2p_downloader_ = P2PModule::Inst()->CreateP2PDownloader(instance_->GetRID());
            if (p2p_downloader_)
            {
                p2p_downloader_-> AttachDownloadDriver(shared_from_this());
                downloaders_.insert(p2p_downloader_);
                p2p_downloader_->SetSpeedLimitInKBps(speed_limit_in_KBps_);
                p2p_downloader_->SetIsOpenService(is_open_service_);
            }
            // attach
            // switch_controller_->SetP2PControlTarget(p2p_downloader_);
        }

        // 创建一个AddHttpDownloader(url_info)

        // 将这个Downloader设置为 原始的Downloader
        HttpDownloader::p downloader = AddHttpDownloader(http_request_demo, origanel_url_info_, true);
        // AddHttpDownloader(http_request_demo, origanel_url_info_, false);

        // control mode
        if (SwitchController::IsValidControlMode(control_mode))
        {
            switch_control_mode_ = static_cast<SwitchController::ControlModeType> (control_mode);
        }

        // switch_controller_->SetHTTPControlTarget(downloader);
        if (downloader)
        {
            switch_controller_->Start(switch_control_mode_);
        }
        
        StartBufferringMonitor();

        if (proxy_connection_->IsHeaderResopnsed())
        {
            io_svc_.post(boost::bind(&DownloadDriver::GetSubPieceForPlay, shared_from_this()));
        }
    }

    // 绿色通道; 可能会有rid_info
    void DownloadDriver::Start(const protocol::UrlInfo& url_info, bool is_support_start, bool open_service,
        boost::int32_t control_mode, boost::int32_t force_mode)
    {
        if (is_running_ == true)
            return;

        is_running_ = true;

        // 智能限速定时器
        speed_limit_timer_.start();

        // if (!IsPush())
        // {
        //    // check push module
        //    if (url_info.url_ == PushModule::Inst()->GetCurrentTaskUrl())
        //    {
        //        LOG(__DEBUG, "push", "Url Same as push task: " << url_info.url_);
        //        PushModule::Inst()->StopCurrentTask();
        //    }
        //    else if (false == rid_info.GetRID().is_empty() && rid_info == PushModule::Inst()->GetCurrentTaskRidInfo())
        //    {
        //        LOG(__DEBUG, "push", "protocol::RidInfo Same as push task: " << rid_info);
        //        PushModule::Inst()->StopCurrentTask();
        //    }
        // }

        switch_controller_ = SwitchController::Create(shared_from_this());

        download_time_counter_.reset();

        is_http_403_header_ = false;
        is_http_304_header_ = false;

        origanel_url_info_ = url_info;
        is_support_start_ = is_support_start;
        is_open_service_ = open_service;
        is_pragmainfo_noticed_ = false;

        // check 'www.pp.tv' refer
        if (false == open_service && origanel_url_info_.refer_url_.find("www.pp.tv") != string::npos)
            origanel_url_info_.refer_url_ = "";

        DD_EVENT("Downloader Driver Start" << shared_from_this());


        statistic_ = StatisticModule::Inst()->AttachDownloadDriverStatistic(id_, true);

        assert(statistic_);
        statistic_->SetOriginalUrl(origanel_url_info_.url_);
        statistic_->SetOriginalReferUrl(origanel_url_info_.refer_url_);
        statistic_->SetHidden(!need_bubble_);


        // 包含.exe则不发送消息
        if (false == IsFileUrl(origanel_url_info_.url_))
        {
            if (true == need_bubble_)
            {
                // 计算需要发出的消息的参数
                DOWNLOADDRIVERSTARTDATA* lpDownloadDriverStartData = MessageBufferManager::Inst()->NewStruct<
                    DOWNLOADDRIVERSTARTDATA> ();
                memset(lpDownloadDriverStartData, 0, sizeof(DOWNLOADDRIVERSTARTDATA));
                lpDownloadDriverStartData->uSize = sizeof(DOWNLOADDRIVERSTARTDATA);

                strncpy(lpDownloadDriverStartData->szOriginalUrl, origanel_url_info_.url_.c_str(),
                    sizeof(lpDownloadDriverStartData->szOriginalUrl)-1);
                strncpy(lpDownloadDriverStartData->szOriginalReferUrl, origanel_url_info_.refer_url_.c_str(),
                    sizeof(lpDownloadDriverStartData->szOriginalReferUrl)-1);

                LOG(__EVENT, "pplive", __FUNCTION__ << "PostWindowsMessage(UM_DONWLOADDRIVER_START, (WPARAM)id_, (LPARAM)lpDownloadDriverStartData)");
                // 向客户端程序发送 UM_DONWLOADDRIVER_START 消息
                // !TestMessage
    #ifdef NEED_TO_POST_MESSAGE
                WindowsMessage::Inst().PostWindowsMessage(UM_DONWLOADDRIVER_START, (WPARAM)id_, (LPARAM)lpDownloadDriverStartData);
    #endif
            }
            else
            {
                statistic_->SetHidden(true);
            }

            if (open_service)
            {
                switch_control_mode_ = SwitchController::CONTROL_MODE_VIDEO_OPENSERVICE;
            }
            else
            {
                switch_control_mode_ = SwitchController::CONTROL_MODE_VIDEO;
            }
        }
        else
        {
            // 下载模式
            switch_control_mode_ = SwitchController::CONTROL_MODE_DOWNLOAD;
        }

        if (is_open_service_ && !rid_info_.HasRID())
        {
            // 使用文件名为索引查询RID
            Instance::p temp_instance = boost::dynamic_pointer_cast<storage::Instance>(
                Storage::Inst()->GetInstanceByFileName(openservice_file_name_));

            if (temp_instance && temp_instance->HasRID())
            {
                temp_instance->GetRidInfo(rid_info_);
            }
        }

        // 在 Storage 里面 创建资源实例
        instance_ = boost::dynamic_pointer_cast<storage::Instance>(Storage::Inst()->CreateInstance(origanel_url_info_, rid_info_));
        assert(instance_);
        instance_->SetIsOpenService(is_open_service_);
        instance_->SetIsPush(is_push_);

        if (instance_)
        {
            if (false == instance_->GetRID().is_empty())
            {
                statistic_->SetResourceID(instance_->GetRID());
                statistic_->SetFileLength(instance_->GetFileLength());
                statistic_->SetBlockSize(instance_->GetBlockSize());
                statistic_->SetBlockCount(instance_->GetBlockCount());
            }
            statistic_->SetLocalDataBytes(instance_->GetDownloadBytes());
        }

        piece_request_manager_ = PieceRequestManager::Create(shared_from_this());
        piece_request_manager_->Start();

        // 资源创建成功，资源对象绑定 这个DownloadDriver
        instance_->AttachDownloadDriver(shared_from_this());

        if (!rid_info_.HasRID())
        {
            if (is_open_service_)
            {
                http_drag_downloader_ = HttpDragDownloader::Create(io_svc_, shared_from_this(), 
                    url_info.url_);
                http_drag_downloader_->Start();
            }
            else
            {
                // 如果该 instance 没有 RID, 那么 要让 IndexManager 发出 QueryRidByUrl 请求
                IndexManager::Inst()->DoQueryRidByUrl(origanel_url_info_.url_, origanel_url_info_.refer_url_);
            }
        }
        else
        {
            LOG(__DEBUG, "downloadcenter", __FUNCTION__ << ":" << __LINE__ << " AttachRidByUrl inst = " << instance_
                << ", url = " << origanel_url_info_.url_ << ", rid = " << rid_info_);
            Storage::Inst()->AttachRidByUrl(origanel_url_info_.url_, rid_info_, MD5(), 0, protocol::RID_BY_PLAY_URL);
            if (instance_->GetRID().is_empty())
            {
                // RID 错误, 使用普通模式
                switch_control_mode_ = SwitchController::CONTROL_MODE_VIDEO;
            }
        }

        // 检查本地是否有已下资源，向proxy_connection_发送content length
        if (true == is_open_service_)
        {
            LOG(__DEBUG, "bug", __FUNCTION__ << ":" << __LINE__ << " IsOpenService = " << is_open_service_ <<
                ", OpenServiceHeadLength = " << openservice_head_length_);
            init_local_data_bytes_ = instance_->GetDownloadBytes();
            if (openservice_head_length_ > 0)
            {
                string server_mod = GenerateOpenServiceMod(origanel_url_info_.url_);
                if (server_mod.length() > 0)
                {
                    OnNoticePragmaInfo(server_mod, openservice_head_length_);
                }
            }
            proxy_connection_->OnNoticeGetContentLength(instance_->GetFileLength(), network::HttpResponse::p());
        }
        else if (instance_->HasRID())
        {
            LOG(__DEBUG, "bug", __FUNCTION__ << ":" << __LINE__ << " instance-IsComplete");
            proxy_connection_->OnNoticeGetContentLength(instance_->GetFileLength(), network::HttpResponse::p());
        }
        else if (IsFileExtension(origanel_url_info_.url_, ".mp3") || IsFileExtension(origanel_url_info_.url_, ".wma"))
        {
            if (false == instance_->GetRID().is_empty())
            {
                DD_DEBUG("instance is music file; rid available or file complete!");
                proxy_connection_->OnNoticeGetContentLength(instance_->GetFileLength(), network::HttpResponse::p());
            }
        }

        if (is_open_service_ && is_drag_)
        {
            is_drag_local_play = true;
            // 按照10秒的码流率来进行判断
            boost::uint32_t has_piece_num = GetDataRate() * 30 / storage::bytes_num_per_piece_g_;
            boost::uint32_t start_position;
            if (IsPPLiveClient())
            {
                start_position = proxy_connection_->GetPlayingPosition();
            }
            else
            {
                start_position = openservice_start_position_;
            }

            for (boost::uint32_t i = 0; i < has_piece_num; ++i)
            {
                // HasPiece内部已经考虑了超过文件尾部的问题，这里简单处理即可
                if (!instance_->HasPiece(start_position + i*storage::bytes_num_per_piece_g_))
                {
                    is_drag_local_play = false;
                    break;
                }
            }

            DebugLog("IsDragLocalPlay: %d", is_drag_local_play);
        }
        // assert(downloaders_.size() == 0);

        if (false == instance_->GetRID().is_empty() &&
            false == instance_->IsComplete() &&
            force_mode != FORCE_MODE_HTTP_ONLY &&
            bwtype_ != p2sp::JBW_HTTP_ONLY)
        {
            p2p_downloader_ = P2PModule::Inst()->CreateP2PDownloader(instance_->GetRID());
            if (p2p_downloader_)
            {
                p2p_downloader_-> AttachDownloadDriver(shared_from_this());
                downloaders_.insert(p2p_downloader_);
                p2p_downloader_->SetSpeedLimitInKBps(speed_limit_in_KBps_);
                p2p_downloader_->SetIsOpenService(is_open_service_);
            }
        }

        // 将这个Downloader设置为 原始的Downloader
        HttpDownloader::p downloader;

        PlayInfo::p play_info = proxy_connection_->GetPlayInfo();
        if (true == is_open_service_ && instance_->IsComplete() && (openservice_head_length_ > 0 || 
            IsPush() || (play_info && play_info->GetStartPosition() == 0)))
        {
            // 文件已经下载完成并且不需要获取headlength
            LOGX(__DEBUG, "proxy", "OpenService Local Play!");
        }
        else
        {
            if (force_mode != FORCE_MODE_P2P_ONLY && force_mode != FORCE_MODE_P2P_TEST)
            {
                LOGX(__DEBUG, "proxy", "Create HttpDownloader!");
                downloader = AddHttpDownloader(network::HttpRequest::p(), origanel_url_info_, true);
                AddBakHttpDownloaders(origanel_url_info_);
            }
            else
            {
                 LOGX(__DEBUG, "proxy", "ForceMode: " << force_mode << ", don't create httpdownloader");
            }
        }

        // control mode
        if (SwitchController::IsValidControlMode(control_mode))
        {
            switch_control_mode_ = static_cast<SwitchController::ControlModeType> (control_mode);
        }

        DD_DEBUG("downloader = " << downloader << ", p2p_downloader = " << p2p_downloader_);
        if (downloader || p2p_downloader_)
        {
            // start
            switch_controller_->Start(switch_control_mode_);
        }

        StartBufferringMonitor();

        io_svc_.post(boost::bind(&DownloadDriver::GetSubPieceForPlay, shared_from_this()));
    }

    // RID 启动
    void DownloadDriver::Start(const network::HttpRequest::p http_request_demo, const protocol::RidInfo& rid_for_play, boost::int32_t control_mode)
    {
        if (is_running_ == true)
            return;

        is_running_ = true;

        // 智能限速定时器
        speed_limit_timer_.start();

        // if (!IsPush())
        // {
        //    // check push module
        //    if (false == rid_for_play.GetRID().is_empty() && rid_for_play == PushModule::Inst()->GetCurrentTaskRidInfo())
        //    {
        //        LOG(__DEBUG, "push", "protocol::RidInfo Same as push task: " << rid_for_play);
        //        PushModule::Inst()->StopCurrentTask();
        //    }
        // }

        // no need switch controller
        switch_controller_ = SwitchController::Create(shared_from_this());

        download_time_counter_.reset();

        is_play_by_rid_ = true;

        is_http_403_header_ = false;
        is_http_304_header_ = false;

        origanel_url_info_.url_ = rid_for_play.GetRID().to_string();
        origanel_url_info_.refer_url_ = "PPVAPlayByRid";
        is_support_start_ = false;
        is_pragmainfo_noticed_ = false;

        DD_EVENT("Downloader Driver Start" << shared_from_this());

        // 关联 Statistic 模块
        statistic_ = StatisticModule::Inst()->AttachDownloadDriverStatistic(id_, true);
        assert(statistic_);
        statistic_->SetOriginalUrl(origanel_url_info_.url_);
        statistic_->SetOriginalReferUrl(origanel_url_info_.refer_url_);
        statistic_->SetHidden(!need_bubble_);
        statistic_->SetSourceType(source_type_);

        if (false == IsFileUrl(origanel_url_info_.url_))
        {
            if (true == need_bubble_)
            {
                // 计算需要发出的消息的参数
                DOWNLOADDRIVERSTARTDATA* lpDownloadDriverStartData = MessageBufferManager::Inst()->NewStruct<
                    DOWNLOADDRIVERSTARTDATA> ();
                memset(lpDownloadDriverStartData, 0, sizeof(DOWNLOADDRIVERSTARTDATA));
                lpDownloadDriverStartData->uSize = sizeof(DOWNLOADDRIVERSTARTDATA);

                strncpy(lpDownloadDriverStartData->szOriginalUrl, origanel_url_info_.url_.c_str(),
                    sizeof(lpDownloadDriverStartData->szOriginalUrl)-1);
                strncpy(lpDownloadDriverStartData->szOriginalReferUrl, origanel_url_info_.refer_url_.c_str(),
                    sizeof(lpDownloadDriverStartData->szOriginalReferUrl)-1);

                LOG(__EVENT, "pplive", __FUNCTION__ << "PostWindowsMessage(UM_DONWLOADDRIVER_START, (WPARAM)id_, (LPARAM)lpDownloadDriverStartData)");
    #ifdef NEED_TO_POST_MESSAGE
                // 向客户端程序发送 UM_DONWLOADDRIVER_START 消息
                WindowsMessage::Inst().PostWindowsMessage(UM_DONWLOADDRIVER_START, (WPARAM)id_, (LPARAM)lpDownloadDriverStartData);
    #endif
            }
            else
            {
                statistic_->SetHidden(true);
            }

            switch_control_mode_ = SwitchController::CONTROL_MODE_NULL;
        }
        else
        {
            // 下载模式
            switch_control_mode_ = SwitchController::CONTROL_MODE_DOWNLOAD;
        }

        // 在 Storage 里面 创建资源实例
        instance_ = boost::dynamic_pointer_cast<storage::Instance>(Storage::Inst()->CreateInstance(origanel_url_info_));
        assert(instance_);
        instance_->SetIsOpenService(is_open_service_);

        if (instance_)
        {
            statistic_->SetResourceID(rid_for_play.GetRID());
            statistic_->SetFileLength(rid_for_play.GetFileLength());
            statistic_->SetBlockSize(rid_for_play.GetBlockSize());
            statistic_->SetBlockCount(rid_for_play.GetBlockCount());
        }

        piece_request_manager_ = PieceRequestManager::Create(shared_from_this());
        piece_request_manager_->Start();

        instance_->AttachDownloadDriver(shared_from_this());

        Storage::Inst()->AttachRidByUrl(rid_for_play.GetRID().to_string(), rid_for_play, RID(), 0, protocol::RID_BY_PLAY_URL);

        // 检查本地是否有已下资源，向proxy_connection_发送content length
        if (instance_->HasRID())
        {
            LOG(__DEBUG, "bug", __FUNCTION__ << ":" << __LINE__ << " instance-IsComplete, RID = " << rid_for_play);
            proxy_connection_->OnNoticeGetContentLength(instance_->GetFileLength(), network::HttpResponse::p());
        }

        assert(downloaders_.size() == 0);

        proxy_connection_->OnNoticeGetContentLength(rid_for_play.GetFileLength(), network::HttpResponse::p());
        if (false == instance_->IsComplete())
        {
            p2p_downloader_ = P2PModule::Inst()->CreateP2PDownloader(instance_->GetRID());
            if (p2p_downloader_)
            {
                p2p_downloader_-> AttachDownloadDriver(shared_from_this());
                downloaders_.insert(p2p_downloader_);
                p2p_downloader_->SetSpeedLimitInKBps(speed_limit_in_KBps_);
                p2p_downloader_->SetIsOpenService(is_open_service_);
            }
        }

        // control mode
        if (SwitchController::IsValidControlMode(control_mode))
        {
            switch_control_mode_ = static_cast<SwitchController::ControlModeType> (control_mode);
        }

        switch_controller_->Start(switch_control_mode_);

        StartBufferringMonitor();

        io_svc_.post(boost::bind(&DownloadDriver::GetSubPieceForPlay, shared_from_this()));
    }

    void DownloadDriver::StartBufferringMonitor()
    {
        if (instance_ && 
            false == instance_->GetRID().is_empty() && 
            false == instance_->IsComplete() &&
            this->is_open_service_ && 
            IsPPLiveClient())
        {
            bufferring_monitor_ = AppModule::Inst()->CreateBufferringMonitor(instance_->GetRID());
        }
        else
        {
            if (!instance_)
            {
                LOG(__DEBUG, "download", __FUNCTION__ << " instance is NULL.");
            }
            else if (instance_->GetRID().is_empty())
            {
                LOG(__DEBUG, "download", __FUNCTION__ << " instance does not have a valid RID.");
            }
        }
    }

    void DownloadDriver::Stop()
    {
        if (is_running_ == false)
            return;

        DD_EVENT("Downloader Driver Stop" << shared_from_this());
        // LOG(__EVENT, "leak", __FUNCTION__ << " p2p_downloader: " << p2p_downloader_);

        assert(statistic_);

        if (false == instance_->IsComplete())
        {
            // 如果没有下载完成，提交下载时间
            boost::uint32_t time_count_in_millisec = download_time_counter_.elapsed();
            StatisticModule::Inst()->SubmitDownloadDurationInSec((boost::uint16_t) (time_count_in_millisec / 1000.0 + 0.5));
        }

        if (switch_controller_)
        {
            switch_controller_->Stop();
            switch_controller_.reset();
        }

         // 发送UM_GOT_RESOURCEID消息
        protocol::RidInfo ridInfo;

        instance_->GetRidInfo(ridInfo);

        Rid_From rid_from = (Rid_From) (instance_->GetRidOriginFlag());
        if (rid_from != protocol::RID_BY_PLAY_URL)  // 通过播放串获得的RID不通知
        {
            protocol::UrlInfo orig = origanel_url_info_;
            string urlExt = AppModule::MakeUrlByRidInfo(ridInfo);
            uint32_t nSize = sizeof(RESOURCEID_DATA) + urlExt.length() + 1;
            const MetaData& meta = instance_->GetMetaData();

            RESOURCEID_DATA * lpResourceData = (RESOURCEID_DATA*) MessageBufferManager::Inst()->NewBuffer(nSize);
            memset(lpResourceData, 0, nSize);

            lpResourceData->uSize = nSize;
            lpResourceData->guidRID = instance_->GetRID();

            strncpy(lpResourceData->szFileType, meta.FileFormat.c_str(), sizeof(lpResourceData->szFileType)-1);

            string orig_url = orig.GetIdentifiableUrl();

            strncpy(lpResourceData->szOriginalUrl, orig_url.c_str(), sizeof(lpResourceData->szOriginalUrl)-1);
            strncpy(lpResourceData->szOriginalReferUrl, orig.refer_url_.c_str(), sizeof(lpResourceData->szOriginalReferUrl)-1);

            lpResourceData->uDuration = meta.Duration;
            lpResourceData->uFileLength = instance_->GetFileLength();
            lpResourceData->bUploadPic = 0;
            lpResourceData->usVAParamLength = urlExt.length();

            assert(nSize - sizeof(RESOURCEID_DATA) >= urlExt.length());

            strncpy(lpResourceData->szVAParam, urlExt.c_str(), nSize - sizeof(RESOURCEID_DATA)-1);

            DD_DEBUG("lpResourceData: Duration=" << meta.Duration << " FileLength=" << lpResourceData->uFileLength << " vaParam=" << urlExt);

    #ifdef NEED_TO_POST_MESSAGE
            WindowsMessage::Inst().PostWindowsMessage(UM_GOT_RESOURCEID, (WPARAM)id_, (LPARAM)lpResourceData);
    #endif
        }

        // 计算需要发出的消息的参数
        DOWNLOADDRIVERSTOPDATA* lpDownloadDriverStopData =
            MessageBufferManager::Inst()->NewStruct<DOWNLOADDRIVERSTOPDATA> ();
        memset(lpDownloadDriverStopData, 0, sizeof(DOWNLOADDRIVERSTOPDATA));
        lpDownloadDriverStopData->uSize = sizeof(DOWNLOADDRIVERSTOPDATA);

        strncpy(lpDownloadDriverStopData->szOriginalUrl, origanel_url_info_.url_.c_str(),
            sizeof(lpDownloadDriverStopData->szOriginalUrl)-1);
        strncpy(lpDownloadDriverStopData->szOriginalReferUrl, origanel_url_info_.refer_url_.c_str(),
            sizeof(lpDownloadDriverStopData->szOriginalReferUrl)-1);

        lpDownloadDriverStopData->bHasRID = !statistic_->GetResourceID().is_empty();
        lpDownloadDriverStopData->guidRID = statistic_->GetResourceID();
        lpDownloadDriverStopData->ulResourceSize = statistic_->GetFileLength();

        SPEED_INFO speed_info = statistic_->GetSpeedInfo();
        if (false == lpDownloadDriverStopData->bHasRID)
        {
            lpDownloadDriverStopData->ulP2pDownloadBytes = 0;
            lpDownloadDriverStopData->ulOtherServerDownloadBytes = 0;
            lpDownloadDriverStopData->ulDownloadBytes = speed_info.TotalDownloadBytes;
        }
        else
        {
            if (p2p_downloader_ && p2p_downloader_->GetStatistic())
            {
                lpDownloadDriverStopData->ulP2pDownloadBytes
                    = p2p_downloader_->GetStatistic()->GetSpeedInfo().TotalDownloadBytes;
            }
            else
            {
                lpDownloadDriverStopData->ulP2pDownloadBytes = 0;
            }

            lpDownloadDriverStopData->ulDownloadBytes = speed_info.TotalDownloadBytes
                + lpDownloadDriverStopData->ulP2pDownloadBytes;
            std::map<string, HttpDownloader::p>::iterator iter = url_indexer_.find(origanel_url_info_.url_);
            if (iter != url_indexer_.end())
            {
                HttpDownloader::p org_http_downloader_ = iter->second;  // boost::shared_dynamic_cast<HttpDownloader>(iter->second);
                lpDownloadDriverStopData->ulOtherServerDownloadBytes = speed_info.TotalDownloadBytes
                    - org_http_downloader_->GetStatistics()->GetSpeedInfo().TotalDownloadBytes;
            }
            else
            {
                lpDownloadDriverStopData->ulOtherServerDownloadBytes = 0;
            }
        }
        StatisticModule::Inst()->SubmitP2PDownloaderDownloadBytes(lpDownloadDriverStopData->ulP2pDownloadBytes);
        StatisticModule::Inst()->SubmitOtherServerDownloadBytes(lpDownloadDriverStopData->ulOtherServerDownloadBytes);

        // lpDownloadDriverStopData->ulOtherServerDownloadBytes;
        lpDownloadDriverStopData->uPlayTime = 0;
        lpDownloadDriverStopData->uDataRate = 0;

        // 停止所有 Downloader
        for (std::map<string, HttpDownloader::p>::iterator iter = url_indexer_.begin(); iter != url_indexer_.end(); iter++)
        {
            iter->second->Stop();
        }
        // 清除 管理所有的 downloader 的 set
        downloaders_.clear();
        // 清除 用 Url 来索引的 map
        url_indexer_.clear();
        // 取消 Statistic 模块 关联

        double rate = 0.0;
        if (lpDownloadDriverStopData->ulDownloadBytes > 0)
        {
            rate = (lpDownloadDriverStopData->ulP2pDownloadBytes * 100.0 / lpDownloadDriverStopData->ulDownloadBytes);
        }

        LOGX(__DEBUG, "msg", "p2p = " << lpDownloadDriverStopData->ulP2pDownloadBytes <<
            ", download = " << lpDownloadDriverStopData->ulDownloadBytes <<
            ", size = " << lpDownloadDriverStopData->ulResourceSize <<
            ", p2p/download = " << rate << "%");

        assert(instance_);
        // 包含.exe则不发送消息
        if (false == IsFileUrl(origanel_url_info_.url_))
        {
    #ifdef NEED_TO_POST_MESSAGE
            // 先往客户端界面发送 UM_DOWNLOADDRIVER_STOP 消息
            WindowsMessage::Inst().PostWindowsMessage(UM_DOWNLOADDRIVER_STOP, (WPARAM)id_, (LPARAM)lpDownloadDriverStopData);
    #endif
            if (instance_->IsComplete() || is_http_304_header_)
            {
                StatisticModule::Inst()->SubmitCompleteCount(1);
            }
        }
        else
        {
            // 不发送消息则自己释放
            MessageBufferManager::Inst()->DeleteBuffer((boost::uint8_t*) lpDownloadDriverStopData);
        }

#ifdef NEED_TO_POST_MESSAGE
        SendDacStopData();
#endif

        if (bufferring_monitor_)
        {
            bufferring_monitor_.reset();
        }

        instance_->DettachDownloadDriver(shared_from_this());
        instance_.reset();

        if (p2p_downloader_)
        {
            p2p_downloader_->DettachDownloadDriver(shared_from_this());
            downloaders_.erase(p2p_downloader_);
            p2p_downloader_.reset();
        }
        assert(statistic_);

        // 再取消共享内存
        StatisticModule::Inst()->DetachDownloadDriverStatistic(statistic_);
        statistic_.reset();

        // PieceRequestManager
        if (piece_request_manager_)
        {
            piece_request_manager_->Stop();
            piece_request_manager_.reset();
        }

        if (proxy_connection_)
        {
            proxy_connection_.reset();
        }

#ifdef DISK_MODE
        if (IsPush())
        {
            PushModule::Inst()->ReportPushTaskCompete(rid_info_);
        }
#endif

        is_running_ = false;
    }

    void DownloadDriver::SendDacStopData()
    {
#ifdef NEED_TO_POST_MESSAGE
        // 内核提交数据（每次加速完毕后提交）
        // A：接口类别，固定为9
        // B：用户的ID
        // C：资源ID (gResourceID)
        // D：peer版本(PeerVersion)
        // E：视频名称（UTF8编码）(VideoName)
        // F：原始链接 (OriginalUrl) 【 UrlEncode(OriginalUrl) 】
        // G：OriginalReferUrl  【 UrlEncode(OriginalReferUrl) 】
        // H：磁盘已有字节数(DiskBytes)
        // I：视频大小(VideoBytes)
        // J：P2P下载字节数(P2PdownloadBytes)
        // K：HTTP下载字节数(HttpDownloadBytes)
        // L：平均下载速度(AvgDownloadSpeed)
        // M：是否是下载模式完成的此次下载（IsSaveMode）
        // N：拖动位置（StartPosition）
        // O：P2P最大下载速度(MaxP2PDownloadSpeed)
        // P：HTTP最大下载速度(MaxHttpDownloadSpeed)
        // Q：最大连接peer数(ConnectedPeerCount)
        // R：满资源peer数(FullPeerCount)
        // S：活跃的peer数(MaxActivePeerCount)
        // T：查询到的节点数(QueriedPeerCount)
        // U：观看来源
        // V：码流率
        // W：P2P开启切换之前的平均速度
        // X：加速状态
        // Y: 操作系统版本 （OSVersion）
        // Z：文件时长 （AccelerateTime） 视频大小I/码流率V  单位s
        // A1:下载时长(download_time) 单位豪秒
        // B1:最后一刻下载速度（last_speed)
        // C1：是否获得RID
        // D1:有效下载字节数（不包括已经下载，不包括冗余）   J + K

        // TODO(herain):2011-1-4:这个为了兼容旧的SOP模块而保留了以前的代码
        // 在sop全部升级后发布的新内核可以删除这些代码
#ifndef CLIENT_NEW_DAC_LOG
        LPDOWNLOADDRIVER_STOP_DAC_DATA_STRUCT dac_data =
            MessageBufferManager::Inst()->NewStruct<DOWNLOADDRIVER_STOP_DAC_DATA_STRUCT> ();
        memset(dac_data, 0, sizeof(DOWNLOADDRIVER_STOP_DAC_DATA_STRUCT));
        dac_data->uSize = sizeof(DOWNLOADDRIVER_STOP_DAC_DATA_STRUCT);
        dac_data->gPeerID = AppModule::Inst()->GetUniqueGuid();  // huage id

        // C: 资源ID
        dac_data->gResourceID = statistic_->GetResourceID();

        // D: Peer版本(PeerVersion)
        dac_data->aPeerVersion[0] = AppModule::GetKernelVersionInfo().Major;
        dac_data->aPeerVersion[1] = AppModule::GetKernelVersionInfo().Minor;
        dac_data->aPeerVersion[2] = AppModule::GetKernelVersionInfo().Micro;
        dac_data->aPeerVersion[3] = AppModule::GetKernelVersionInfo().Extra;

        // E: 视频名称(VideoName)
#ifdef PEER_PC_CLIENT
        wstring w_file_name;
        if (is_open_service_) 
        {
            w_file_name = base::s2ws(openservice_file_name_);
        }
        else 
        {
            w_file_name = base::s2ws(statistic_->GetFileName());
        }
        wcscpy((wchar_t*)dac_data->szVideoName, w_file_name.c_str());
#endif
        // F: OriginalUrl(原始URL)
        string originalUrl(origanel_url_info_.url_);
        u_int pos = originalUrl.find_first_of('/');
        pos = originalUrl.find_first_of('/', pos + 1);
        pos = originalUrl.find_first_of('/', pos + 1);

        if (is_open_service_)
        {
            // 开放服务
            strncpy(dac_data->szOriginalUrl, string(originalUrl, 0, pos).c_str(), sizeof(dac_data->szOriginalUrl)-1);
        }
        else
        {
            // 加速
            strncpy(dac_data->szOriginalUrl, originalUrl.c_str(), sizeof(dac_data->szOriginalUrl));
        }

        // G: OriginalReferUrl
        strncpy(dac_data->szOriginalReferUrl, origanel_url_info_.refer_url_.c_str(), sizeof(dac_data->szOriginalReferUrl));

        if (p2p_downloader_ && p2p_downloader_->GetStatistic()) {
            SPEED_INFO speed = p2p_downloader_->GetStatistic()->GetSpeedInfo();
            dac_data->uAvgP2PDownloadSpeed = speed.AvgDownloadSpeed;
        }

        // I: 视频大小(VideoBytes)
        dac_data->uVideoBytes = statistic_->GetFileLength();
        // H: 磁盘已有字节数(DiskBytes)
        dac_data->uDiskBytes = init_local_data_bytes_;

        // J: P2P下载字节数
        if (p2p_downloader_ && p2p_downloader_->GetStatistic())
        {
            dac_data->uP2PDownloadBytes = p2p_downloader_->GetStatistic()->GetTotalP2PDataBytes();
        }
        // K: HTTP总下载字节数
        dac_data->uHttpDownloadBytes = statistic_->GetTotalHttpDataBytes();

        // L: 平均下载速度(AvgDownloadSpeed)
        dac_data->uAvgDownloadSpeed = (dac_data->uP2PDownloadBytes + dac_data->uHttpDownloadBytes) * 1000.0 / download_time_counter_.elapsed();

        // M: IsSaveMode
        dac_data->bIsSaveMode = proxy_connection_->IsSaveMode();

        dac_data->uStartPosition = (boost::uint32_t)(p2sp::ProxyModule::Inst()->GetHistoryMaxDwonloadSpeed() / 1024.0);  // 鍘嗗彶鏈€澶т笅杞介€熷害
        
        dac_data->uMaxHttpDownloadSpeed = statistic_->GetHttpDownloadMaxSpeed();                 // 鏈€澶TTP涓嬭浇閫熷害

        // Q: 连接上的节点数
        dac_data->uConnectedPeerCount = statistic_->GetConnectedPeerCount();
        // R: 资源全满的节点数
        //dac_data->uFullPeerCount = statistic_->GetFullPeerCount();
        // TODO(herain):2011-3-2:临时用uFullPeerCount表示拖动后http启动的状态
        dac_data->uFullPeerCount = drag_http_status_;

        // S: 活跃节点数
        //dac_data->uMaxActivePeerCount = statistic_->GetMaxActivePeerCount();        
        dac_data->uBakHostStatus = (boost::uint32_t)bak_host_status_;

        // T: 查询到的节点数
        dac_data->uQueriedPeerCount = statistic_->GetQueriedPeerCount();
        // U: 观看来源(SourceType)
        dac_data->uSourceType = source_type_;
        // V: 码流率(DataRate)
        dac_data->uDataRate = GetDataRate();
        // W: P2P开启切换之前的平均速度（新增）

        // TODO(herain):2011-3-1:用uAccelerateHttpSpeed暂时表示2300状态的http平均速度
        if (dac_data->uSourceType == 0 || dac_data->uSourceType == 1)
        {
            if (avg_http_download_speed_in2300_ == -1)
            {
                dac_data->uAccelerateHttpSpeed = dac_data->uAvgDownloadSpeed;
            }
            else
            {
                dac_data->uAccelerateHttpSpeed = avg_http_download_speed_in2300_;
            }
        }
        else
        {
            if (accelerate_http_speed == 0)
            {
                dac_data->uAccelerateHttpSpeed = dac_data->uAvgDownloadSpeed;
            }
            else
            {
                dac_data->uAccelerateHttpSpeed = accelerate_http_speed;
            }
        }

        // X: 加速状态

        // TODO(herain):2011-2-25:临时把uAccelerateStatus替换为不限速时的平均下载速度，在启用新DAC日志系统后
        // 考虑恢复http最大下载速度的统计

        if (dac_data->uSourceType == 0 || dac_data->uSourceType == 1)
        {
            if (avg_download_speed_before_limit_ == -1)
            {
                dac_data->uAccelerateStatus = dac_data->uAvgDownloadSpeed;
            }
            else
            {
                dac_data->uAccelerateStatus = (uint32_t)avg_download_speed_before_limit_;
            }
        }
        else
        {
            dac_data->uAccelerateStatus = accelerate_status_;
        }

        // Y: 下载时间
        dac_data->download_time = download_time_counter_.elapsed();
        // Z: 最后一刻的速度
        // TODO(herain):2011-4-20:使用last_speed来汇报drag获取的结果
        if (dac_data->uSourceType == 0 || dac_data->uSourceType == 1)
        {
            if (http_drag_downloader_)
            {
                dac_data->last_speed = drag_fetch_result_;
            }
            else
            {
                dac_data->last_speed = -1;
            }
        }
        else
        {
            dac_data->last_speed = statistic_->GetSpeedInfo().NowDownloadSpeed;
        }        

        // 是否获得RID
        if (GetP2PControlTarget())
        {
            dac_data->is_got_rid = 1;
        }
        else
        {
            dac_data->is_got_rid = 0;
        }

        // log
        LOGX(__DEBUG, "msg", "+-----------------DOWNLOADDRIVER_STOP_DAC_DATA-----------------+");
        LOGX(__DEBUG, "msg", "|      gPeerID = " << dac_data->gPeerID);
        LOGX(__DEBUG, "msg", "| C :  gResourceID = " << dac_data->gResourceID.to_string());
        LOGX(__DEBUG, "msg", "| D :  PeerVersion = " <<
            (uint32_t)dac_data->aPeerVersion[0] << "," << (uint32_t)dac_data->aPeerVersion[1] << "," <<
            (uint32_t)dac_data->aPeerVersion[2] << "," << (uint32_t)dac_data->aPeerVersion[3]);
        LOGX(__DEBUG, "msg", "| E :  VideoName = " << dac_data->szVideoName);
        LOGX(__DEBUG, "msg", "| F :  Url = " << dac_data->szOriginalUrl);
        LOGX(__DEBUG, "msg", "| G :  Refer = " << dac_data->szOriginalReferUrl);
        LOGX(__DEBUG, "msg", "| H :  DiskBytes = " << dac_data->uDiskBytes);
        LOGX(__DEBUG, "msg", "| I :  VideoBytes = " << dac_data->uVideoBytes);
        LOGX(__DEBUG, "msg", "| J :  P2PDownloadBytes = " << dac_data->uP2PDownloadBytes);
        LOGX(__DEBUG, "msg", "| K :  HttpDownloadBytes = " << dac_data->uHttpDownloadBytes);
        LOGX(__DEBUG, "msg", "| L :  AvgP2PDownloadSpeed = " << dac_data->uAvgDownloadSpeed);
        LOGX(__DEBUG, "msg", "| N :  StartPosition = " << dac_data->uStartPosition);
        LOGX(__DEBUG, "msg", "| O :  MaxP2PDownloadSpeed = " << dac_data->uAvgP2PDownloadSpeed);
        LOGX(__DEBUG, "msg", "| P :  MaxHttpDownloadSpeed = " << dac_data->uMaxHttpDownloadSpeed);
        LOGX(__DEBUG, "msg", "| Q :  ConnectedPeerCount = " << dac_data->uConnectedPeerCount);
        LOGX(__DEBUG, "msg", "| R :  FullPeerCount = " << dac_data->uFullPeerCount);
        LOGX(__DEBUG, "msg", "| S :  BakHostStatus = " << dac_data->uBakHostStatus);
        LOGX(__DEBUG, "msg", "| T :  QueriedPeerCount = " << dac_data->uQueriedPeerCount);
        LOGX(__DEBUG, "msg", "| U :  SourceType = " << dac_data->uSourceType);
        LOGX(__DEBUG, "msg", "| V :  DataRate = " << dac_data->uDataRate);
        LOGX(__DEBUG, "msg", "| W :  uAccelerateHttpSpeed = " << dac_data->uAccelerateHttpSpeed);
        LOGX(__DEBUG, "msg", "| X :  uAccelerateStatus = " << dac_data->uAccelerateStatus);
        LOGX(__DEBUG, "msg", "| Y :  download_time = " << dac_data->download_time);
        LOGX(__DEBUG, "msg", "| Z :  last_speed = " << dac_data->last_speed);
        LOGX(__DEBUG, "msg", "| A1:  is_got_rid = " << dac_data->is_got_rid);

        WindowsMessage::Inst().PostWindowsMessage(UM_DAC_STATISTIC, (WPARAM)id_, (LPARAM)dac_data);
#else
        DOWNLOADDRIVER_STOP_DAC_DATA_STRUCT info;

        info.gResourceID = statistic_->GetResourceID();
        info.aPeerVersion[0] = AppModule::GetKernelVersionInfo().Major;
        info.aPeerVersion[1] = AppModule::GetKernelVersionInfo().Minor;
        info.aPeerVersion[2] = AppModule::GetKernelVersionInfo().Micro;
        info.aPeerVersion[3] = AppModule::GetKernelVersionInfo().Extra;
        info.videoName = is_open_service_ ? openservice_file_name_ : statistic_->GetFileName();

        string originalUrl(origanel_url_info_.url_);
        u_int pos = originalUrl.find_first_of('/');
        pos = originalUrl.find_first_of('/', pos + 1);
        pos = originalUrl.find_first_of('/', pos + 1);

        info.originalUrl = is_open_service_ ? string(originalUrl, 0, pos) : originalUrl;
        info.originalReferUrl = origanel_url_info_.refer_url_;
        info.uDiskBytes = init_local_data_bytes_;
        info.uVideoBytes = statistic_->GetFileLength();
        info.uP2PDownloadBytes = 0;


        if (p2p_downloader_ && p2p_downloader_->GetStatistic())
        {
            info.uP2PDownloadBytes = p2p_downloader_->GetStatistic()->GetTotalP2PDataBytes();
            info.uAvgP2PDownloadSpeed = p2p_downloader_->GetStatistic()->GetSpeedInfo().AvgDownloadSpeed;
        }
        else
        {
            info.uP2PDownloadBytes = 0;
            info.uAvgP2PDownloadSpeed = 0;
        }

        info.uHttpDownloadBytes = statistic_->GetTotalHttpDataBytes();
        info.uTotalDownloadBytes = info.uP2PDownloadBytes + info.uHttpDownloadBytes;

        info.uAvgDownloadSpeed = (download_time_counter_.elapsed() == 0) ? 0
            : (info.uTotalDownloadBytes * 1000.0 / download_time_counter_.elapsed());
        info.bIsSaveMode = proxy_connection_->IsSaveMode();

        info.uMaxHttpDownloadSpeed = statistic_->GetHttpDownloadMaxSpeed();
        info.uConnectedPeerCount = statistic_->GetConnectedPeerCount();
        info.uFullPeerCount = statistic_->GetFullPeerCount();
        info.uMaxActivePeerCount = statistic_->GetMaxActivePeerCount();
        info.uQueriedPeerCount = statistic_->GetQueriedPeerCount();
        info.uSourceType = source_type_;
        info.uDataRate = GetDataRate();

        if (accelerate_http_speed == 0)
        {
            info.uAccelerateHttpSpeed = (download_time_counter_.elapsed() == 0) ? 0 :
                (info.uTotalDownloadBytes * 1000.0 / download_time_counter_.elapsed());
        }
        else
        {
            info.uAccelerateHttpSpeed = accelerate_http_speed;
        }

        info.uAccelerateStatus = accelerate_status_;
        info.uVideoDuration = statistic_->GetFileLength() / GetDataRate();
        info.download_time = download_time_counter_.elapsed();
        info.last_speed = statistic_->GetSpeedInfo().NowDownloadSpeed;
        info.is_got_rid = (GetP2PControlTarget() ? 1 : 0);

        // herain:2010-12-31:创建提交DAC的日志字符串
        std::ostringstream log_stream;

        log_stream << "C=" << info.gResourceID.to_string();
        log_stream << "&D=" << info.aPeerVersion[0] << "." << info.aPeerVersion[1] << "."
            << info.aPeerVersion[2] << "." << info.aPeerVersion[3];
        log_stream << "&E=" << info.videoName;
        log_stream << "&F=" << info.originalUrl;
        log_stream << "&G=" << info.originalReferUrl;
        log_stream << "&H=" << info.uDiskBytes;
        log_stream << "&I=" << info.uVideoBytes;
        log_stream << "&J=" << info.uP2PDownloadBytes;
        log_stream << "&K=" << info.uHttpDownloadBytes;
        log_stream << "&L=" << info.uAvgDownloadSpeed;
        log_stream << "&M=" << info.bIsSaveMode;
        log_stream << "&N=";
        log_stream << "&O=" << info.uAvgP2PDownloadSpeed;
        log_stream << "&P=" << info.uMaxHttpDownloadSpeed;
        log_stream << "&Q=" << info.uConnectedPeerCount;
        log_stream << "&R=" << info.uFullPeerCount;
        log_stream << "&S=" << info.uMaxActivePeerCount;
        log_stream << "&T=" << info.uQueriedPeerCount;
        log_stream << "&U=" << info.uSourceType;
        log_stream << "&V=" << info.uDataRate;
        log_stream << "&W=" << info.uAccelerateHttpSpeed;
        log_stream << "&X=" << info.uAccelerateStatus;
        log_stream << "&Y=";
        log_stream << "&Z=" << info.uVideoDuration;
        log_stream << "&A1=" << info.download_time;
        log_stream << "&B1=" << info.last_speed;
        log_stream << "&C1=" << info.is_got_rid;
        log_stream << "&D1=" << info.uTotalDownloadBytes;

        string log = log_stream.str();

        LOGX(__DEBUG, "msg", "+-----------------DOWNLOADDRIVER_STOP_DAC_DATA-----------------+");
        LOGX(__DEBUG, "msg", "| " << log);

        // herain:2010-12-31:创建并填充实际发送给客户端的消息结构体
        LPDOWNLOADDRIVER_STOP_DAC_DATA dac_data =
            MessageBufferManager::Inst()->NewStruct<DOWNLOADDRIVER_STOP_DAC_DATA> ();
        memset(dac_data, 0, sizeof(DOWNLOADDRIVER_STOP_DAC_DATA));

        dac_data->uSize = sizeof(DOWNLOADDRIVER_STOP_DAC_DATA);
        dac_data->uSourceType = source_type_;
        strncpy(dac_data->szLog, log.c_str(), sizeof(dac_data->szLog)-1);

        WindowsMessage::Inst().PostWindowsMessage(UM_DAC_STATISTIC_V1, (WPARAM)id_, (LPARAM)dac_data);
#endif
#endif
    }

    bool DownloadDriver::HasUrl(const string& url)
    {
        // ! check youtube
        return url_indexer_.find(url) != url_indexer_.end();
    }

    HttpDownloader::p DownloadDriver::AddHttpDownloader(const network::HttpRequest::p http_request, const protocol::UrlInfo& url_info,
        bool is_orginal)
    {
        // 带上HttpRequest, 好对其内容进行判断然后发出请求，(Tudou  Cookie)
        if (is_running_ == false)
            return HttpDownloader::p();

        DD_EVENT("DownloadDriver::AddHttpDownloader " << url_info);

        if (url_indexer_.size() > 5)
            return HttpDownloader::p();

        if (HasUrl(url_info.url_))
        {
            // 如果该 UrlInfo 已经存在，则不创建，直接返回该类对应的Url
            DD_EVENT("HttpDownloader Existed " << url_info);
            HttpDownloader::p downloader = url_indexer_.find(url_info.url_)->second;
            return downloader;
        }

        HttpDownloader::p downloader;
        if (instance_->IsComplete())
        {
            DD_EVENT("instance is complete! ");
            downloader = Downloader::CreateByUrl(io_svc_, http_request, url_info, shared_from_this(), true, is_open_service_);
        }
        else
        {
            downloader = Downloader::CreateByUrl(io_svc_, http_request, url_info, shared_from_this(), false, is_open_service_);
        }

        if (downloader)
        {
            if (is_orginal)
                downloader->SetOriginal();

            downloader->Start(is_support_start_);
            // 在 downloader集合中 添加这个Downloader
            downloaders_.insert(downloader);
            // 在 downloader_indexer索引中 添加这个Downloader
            url_indexer_[url_info.url_] = downloader;

            if (is_pausing_)
                downloader->SetPausing();

            downloader->SetSpeedLimitInKBps(speed_limit_in_KBps_);
        }
        else
        {
            DD_ERROR("Downloader Creation Error!");
        }

        return downloader;
    }

    HttpDownloader::p DownloadDriver::AddHttpDownloader(const protocol::UrlInfo& url_info, bool is_orginal)
    {
        if (is_running_ == false)
            return HttpDownloader::p();

        HttpDownloader::p downloader;

        if (url_indexer_.size() > 5)
            return downloader;DD_EVENT("DownloadDriver::AddHttpDownloader " << url_info);

        if (HasUrl(url_info.url_))
        {
            // 如果该 UrlInfo 已经存在，则不创建，直接返回该类对应的Url
            DD_EVENT("HttpDownloader Existed " << url_info);
            HttpDownloader::p downloader = url_indexer_.find(url_info.url_)->second;
            return downloader;
        }

        if (instance_->IsComplete())
        {
            DD_EVENT("instance is complete! ");
            return HttpDownloader::p();
        }
        // 创建 HttpDownloader(url_info)
        downloader = Downloader::CreateByUrl(io_svc_, url_info, shared_from_this(), is_open_service_);

        if (downloader)
        {
            if (is_orginal)
                downloader->SetOriginal();

            downloader->Start(is_support_start_);
            // 在 downloader集合中 添加这个Downloader
            downloaders_.insert(downloader);
            // 在 downloader_indexer索引中 添加这个Downloader
            url_indexer_[url_info.url_] = downloader;

            if (is_pausing_)
                downloader->SetPausing();

            downloader->SetSpeedLimitInKBps(speed_limit_in_KBps_);
        }
        else
        {
            DD_ERROR("Downloader Creation Error!");
        }

        return downloader;
    }

    void DownloadDriver::OnPieceComplete(protocol::PieceInfo piece_info, VodDownloader__p downloader)
    {
        if (is_running_ == false)
            return;

        LOG(__EVENT, "downloaddriver", "DownloadDriver::OnPieceComplete " << GetDownloadDriverID() << " " << downloader->IsOriginal() << " " << downloader->IsP2PDownloader() << " " << downloader << " " << piece_info);LOG(__DEBUG, "bug", __FUNCTION__ << ":" << __LINE__ << " " << id_ << " " << (downloader->IsP2PDownloader() ? "P2P" : "HTTP") << " " << piece_info);

        piece_request_manager_->RemovePieceTask(piece_info, downloader);

        if (instance_->IsComplete())
        {
            OnNoticeConnentLength(instance_->GetFileLength(), downloader, network::HttpResponse::p());
        }

        // 如果 downloader 不存在
        //        ppassert();
        //
        if (downloaders_.find(downloader) == downloaders_.end())
            assert(0);
    }

    void DownloadDriver::OnPieceFaild(protocol::PieceInfo piece_info_, VodDownloader__p downloader)
    {
        if (is_running_ == false)
            return;

        LOG(__EVENT, "downloader", "DownloadDriver::OnPiecefaild " << downloader << " protocol::PieceInfo: " << piece_info_);

        piece_request_manager_->RemovePieceTask(piece_info_, downloader);

    }
/*
    void DownloadDriver::OnDownloaderConnected(Downloader::p downloader)
    {
        if (is_running_ == false)
            return;
        if (downloaders_.find(downloader) == downloaders_.end())
        {
            assert(0);
            return;
        }
        protocol::PieceInfoEx piece_info_to_download;
        if (false == piece_request_manager_->GetNextPieceForDownload(proxy_connection_->GetPlayingPosition(),
            piece_info_to_download, downloader))
        {

        }
        else
        {
            // downloader->PutPieceTask(piece_info_to_download, shared_from_this());
        }
    }*/

    void DownloadDriver::SetDownloaderToDeath(VodDownloader__p downloader)
    {
        if (is_running_ == false)
            return;

        // 如果downloader不存在
        //        ppassert()
        if (downloaders_.find(downloader) == downloaders_.end())
        {
            return;
        }
        //
        // 如果该 Downloader 是 orignal 的 Downloader,
        //      那么不设置其为死的
        //
        else if (downloader->IsOriginal() || downloader->IsP2PDownloader())
        {
            DD_EVENT("DownloadDriver::SetDownloaderToDeath downloader is Original || P2P Downloader:" << downloader);
            return;
        }
        DD_EVENT("DownloadDriver::SetDownloaderToDeath downloader:" << downloader);
        downloader->Stop();

        // 删除索引
        downloaders_.erase(downloader);

        for (std::map<string, HttpDownloader::p>::iterator iter = url_indexer_.begin(); iter != url_indexer_.end(); iter++)
        {
            if (iter->second == downloader)
            {
                url_indexer_.erase(iter);
                break;
            }
        }

    }

    void DownloadDriver::OnNotice304Header(VodDownloader__p downloader, network::HttpResponse::p http_response)
    {
        if (is_running_ == false)
            return;

        is_http_304_header_ = true;
        proxy_connection_->OnNoticeGetContentLength(0, http_response);
    }

    void DownloadDriver::OnNotice403Header(VodDownloader__p downloader, network::HttpResponse::p http_response)
    {
        if (is_running_ == false)
            return;

        if (true == is_http_403_header_)
            return;

        is_http_403_header_ = true;

        if (instance_->GetFileLength() != 0)
        {
            // notice
            uint32_t max_header_length = 512;  // 2*1024*1024;  // 2MB
            uint32_t default_header_length = instance_->GetFileLength() < max_header_length ? instance_->GetFileLength() / 2 : max_header_length;
            proxy_connection_->OnNoticeGetContentLength(instance_->GetFileLength(), network::HttpResponse::p());
            proxy_connection_->OnNoticeOpenServiceHeadLength(default_header_length);
            statistic_->SetFileLength(instance_->GetFileLength());
        }
    }

    void DownloadDriver::OnNoticeConnentLength(uint32_t file_length, VodDownloader__p downloader, network::HttpResponse::p http_response)
    {
        // if (http_response)
        //    DD_EVENT("DownloadDriver::OnNoticeConnentLength file_length: " << file_length << " " << http_response->ToString());
        // else
        //    DD_EVENT("DownloadDriver::OnNoticeConnentLength file_length: " << file_length);

        if (is_running_ == false)
            return;

        is_http_403_header_ = false;

        statistic_->SetLocalDataBytes(instance_->GetDownloadBytes());
        instance_->GetMetaData().FileLength = file_length;

        if (file_length == 0)
        {
            DD_EVENT(__FUNCTION__ << " file_length=" << file_length << " Change to DirectMode");
            // HTTP Response头部无ContentLength, 使用Direct模式
            proxy_connection_->OnNoticeDirectMode(shared_from_this());
        }
        else if (instance_->GetFileLength() == 0)
        {
            // 如果 IInstance-> FileLength == 0
            //     ppassert(downloader 一定是origanel的
            //     调用 IInstance->SetFileLength(content_length)
            DD_EVENT(__FUNCTION__ << " path 2");
            assert(downloader->IsOriginal());
            instance_->SetFileLength(file_length);
            proxy_connection_->OnNoticeGetContentLength(file_length, http_response);
            statistic_->SetFileLength(file_length);
        }
        else if (instance_->GetFileLength() == file_length)
        {
            // 否则 如果 IInstance-> FileLength == content-length
            //         直接返回
            DD_EVENT(__FUNCTION__ << " path 3");
            proxy_connection_->OnNoticeGetContentLength(file_length, http_response);
            statistic_->SetFileLength(file_length);
            return;
        }
        else if (true == is_open_service_)
        {
            DD_EVENT(__FUNCTION__ << " path 4 is_openservice = true");
            proxy_connection_->OnNoticeGetContentLength(file_length, http_response);
            statistic_->SetFileLength(instance_->GetFileLength());
            return;
        }
        else if (downloader->IsOriginal())
        {
            //     否则 如果 downloader 是 origanel 的，
            //              intance->DettachDownloadDriver();
            //              Storage.RemoveUrl(downloader 的 Url)
            //              intance_->Storage->CreateIntance()
            //              intance_->AttachDownloaddriver();
            //              停掉所有 非origanel的 Httpdownloader 纯下
            DD_EVENT(__FUNCTION__ << " path 5");
            instance_->DettachDownloadDriver(shared_from_this());
            protocol::UrlInfo url_info_;
            downloader->GetUrlInfo(url_info_);
            Storage::Inst()->RemoveUrlInfo(url_info_);
            instance_ = boost::dynamic_pointer_cast<storage::Instance>(Storage::Inst()->CreateInstance(url_info_, true));
            instance_->SetIsOpenService(is_open_service_);
            instance_->AttachDownloadDriver(shared_from_this());
            instance_->SetFileLength(file_length);

            proxy_connection_->OnNoticeGetContentLength(file_length, http_response);
            statistic_->SetFileLength(file_length);

            // stop
            switch_controller_->Stop();
            switch_control_mode_ = SwitchController::CONTROL_MODE_VIDEO;

            for (std::set<VodDownloader__p>::iterator iter = downloaders_.begin(); iter != downloaders_.end(); iter++)
            {
                if (false == (*iter)->IsOriginal())
                {
                    (*iter)->Stop();
                }
            }
            downloaders_.clear();
            url_indexer_.clear();
            downloaders_.insert(downloader);
            p2p_downloader_.reset();
            url_indexer_[url_info_.url_] = boost::shared_dynamic_cast<HttpDownloader>(downloader);
            // start
            switch_controller_ = SwitchController::Create(shared_from_this());
            switch_controller_->Start(switch_control_mode_);
        }
        else
        {
            //
            //         否则 downloader 不是 origanel 的
            //              Storage.RemoveUrl(downloader 的 Url)
            //              downloader 直接变成死的
            protocol::UrlInfo url_info_;
            downloader->GetUrlInfo(url_info_);
            Storage::Inst()->RemoveUrlInfo(url_info_);
        }
    }


    // 调用条件
    //   1. Storage层发生资源归并
    void DownloadDriver::OnNoticeChangeResource(boost::shared_ptr<Instance> instance_old,
        boost::shared_ptr<Instance> instance_new)
    {
        if (is_running_ == false)
            return;
        // assert(instance_old == instance_);
        // assert(instance_old);
        // assert(instance_new);
        assert(instance_old == instance_);
        assert(instance_old);
        assert(instance_new);
        // assert(instance_->GetRID().IsEmpty());

        // instance_old->DettachDownloaddriveer(shared_fromn_this())
        instance_old->DettachDownloadDriver(shared_from_this());
        // instance_ = instance_new;
        instance_ = instance_new;
        // instance_->AttachDownloaddriveer(shared_fromn_this())
        instance_->AttachDownloadDriver(shared_from_this());

        if (instance_->GetFileLength() != 0)
            proxy_connection_->OnNoticeGetContentLength(instance_->GetFileLength(), network::HttpResponse::p());

        assert(false == instance_->GetRID().is_empty());
        if (false == instance_->IsComplete())
        {
            p2p_downloader_ = P2PModule::Inst()->CreateP2PDownloader(instance_->GetRID());  // ! TODO SwitchController
            if (p2p_downloader_)
            {
                p2p_downloader_-> AttachDownloadDriver(shared_from_this());
                downloaders_.insert(p2p_downloader_);
                p2p_downloader_->SetSpeedLimitInKBps(speed_limit_in_KBps_);
                p2p_downloader_->SetIsOpenService(is_open_service_);

                P2PDownloaderStatistic::p p2p_statistic = p2p_downloader_->GetStatistic();
                if (p2p_statistic)
                {
                    p2p_statistic->ClearP2PDataBytes();
                }
            }
        }

        assert(statistic_);
        statistic_->SetResourceID(instance_->GetRID());
        statistic_->SetFileLength(instance_->GetFileLength());
        statistic_->SetBlockSize(instance_->GetBlockSize());
        statistic_->SetBlockCount(instance_->GetBlockCount());
        statistic_->SetLocalDataBytes(instance_->GetDownloadBytes() - statistic_->GetTotalHttpDataBytes());

        GetSubPieceForPlay();
    }

    void DownloadDriver::OnNoticeRIDChange()
    {
        if (is_running_ == false)
            return;

        // 从 IInstance 中查出 对应 的 RID
        //
        // (条件) 调用 IndexManager->QueryHttpServerByRID
        //
        RID rid_;
        rid_ = instance_->GetRID();
        // assert(false == rid_.IsEmpty());
        // if (!IsPush())
        // {
        //    // check push module
        //    protocol::RidInfo rid_info;
        //    instance_->GetRidInfo(rid_info);
        //    if (false == rid_info.GetRID().is_empty() && rid_info == PushModule::Inst()->GetCurrentTaskRidInfo())
        //    {
        //        LOG(__DEBUG, "push", "GetRid, Same as push task.");
        //        PushModule::Inst()->StopCurrentTask();
        //    }
        // }

        assert(statistic_);
        statistic_->SetResourceID(rid_);
        statistic_->SetFileLength(instance_->GetFileLength());
        statistic_->SetBlockSize(instance_->GetBlockSize());
        statistic_->SetBlockCount(instance_->GetBlockCount());

        if (false == instance_->IsComplete() && bwtype_ != JBW_HTTP_ONLY)
        {
            p2p_downloader_ = P2PModule::Inst()->CreateP2PDownloader(instance_->GetRID());  // ! TODO P2PControlTarget
            if (p2p_downloader_)
            {
                p2p_downloader_-> AttachDownloadDriver(shared_from_this());
                downloaders_.insert(p2p_downloader_);
                p2p_downloader_->SetSpeedLimitInKBps(speed_limit_in_KBps_);
                p2p_downloader_->SetIsOpenService(is_open_service_);
            }
        }

        uint32_t content_length = instance_->GetFileLength();
        assert(content_length > 0);

        proxy_connection_->OnNoticeGetContentLength(content_length, network::HttpResponse::p());

        // 判断，如果HTTP是403，才使用本地资源，否则，使用全透明
//         if (true == is_http_403_header_ && content_length > 0)
//         {
//             proxy_connection_->OnNoticeGetContentLength(content_length, network::HttpResponse::p());
//         }
//         // 或者是音乐文件
//         else if (IsFileExtension(origanel_url_info_.url_, ".mp3") || IsFileExtension(origanel_url_info_.url_, ".wma"))
//         {
//             if (false == instance_->GetRID().is_empty() || true == instance_->IsComplete())
//             {
//                 DD_DEBUG("instance is music file; rid available or file complete!");
//                 proxy_connection_->OnNoticeGetContentLength(content_length, network::HttpResponse::p());
//             }
//         }

    }

    void DownloadDriver::OnNoticeDownloadComplete()
    {
        if (is_running_ == false)
            return;
        is_complete_ = true;

        if (p2p_downloader_)
        {
            p2p_downloader_->DettachDownloadDriver(shared_from_this());
            downloaders_.erase(p2p_downloader_);
            p2p_downloader_.reset();
        }

        uint32_t time_count_in_millisec = download_time_counter_.elapsed();
        StatisticModule::Inst()->SubmitDownloadDurationInSec((boost::uint16_t) (time_count_in_millisec / 1000.0 + 0.5));
    }

    void DownloadDriver::OnNoticeMakeBlockSucced(uint32_t block_info)
    {
        if (is_running_ == false)
            return;
    }

    void DownloadDriver::ChangeToPoolModel()
    {
        if (is_running_ == false)
            return;

        LOG(__EVENT, "downloaddriver", "DownloadDriver::ChangeToPoolModel origanel_url_info_:" << origanel_url_info_ << ", inst=" << shared_from_this());

        if (true == is_pool_mode_)
        {
            LOG(__EVENT, "downloaddriver", __FUNCTION__ << " is_pool_mode_ = true");
            return;
        }

        is_pool_mode_ = true;
        // return;
        switch_control_mode_ = SwitchController::CONTROL_MODE_NULL;
        switch_controller_->Stop();

        if (p2p_downloader_)
        {
            p2p_downloader_->DettachDownloadDriver(shared_from_this());
            downloaders_.erase(p2p_downloader_);
            p2p_downloader_ = P2PDownloader__p();
        }
        boost::int32_t flag = instance_->GetRidOriginFlag();
        instance_->DettachDownloadDriver(shared_from_this());

        // herain:这里有潜在问题，发现校验错误后重新创建Instance，没有传入RID参数，这会造成重新
        // 创建一个新的资源文件，老的资源文件将被放弃，至于新资源和老资源会不会进行merge需要继续研究。
        instance_ = boost::dynamic_pointer_cast<storage::Instance>(Storage::Inst()->CreateInstance(origanel_url_info_, true));
        instance_->SetIsOpenService(is_open_service_);
        instance_->AttachDownloadDriver(shared_from_this());
        instance_->SetRidOriginFlag(flag);

        url_indexer_.clear();

        // TODO: 集合清空之前要stop
        for (std::set<VodDownloader__p>::iterator it = downloaders_.begin(); it != downloaders_.end(); it++)
            (*it)->Stop();

        downloaders_.clear();
        piece_request_manager_->ClearTasks();


        // 将这个Downloader设置为 原始的Downloader
        HttpDownloader::p downloader = AddHttpDownloader(origanel_url_info_, true);

        // switch controller
        switch_controller_ = SwitchController::Create(shared_from_this());
        switch_controller_->Start(switch_control_mode_);
    }

    void DownloadDriver::OnNoticeMakeBlockFailed(uint32_t block_info)
    {
        if (is_running_ == false)
            return;

        // Block校验错误次数 + 1
        block_check_faild_times_++;

        // 如果 Block校验错误次数 > 5 , 转用纯下模式下载
        if (block_check_faild_times_ >= 1)
        {
            ChangeToPoolModel();
        }
        //
        // 是否考虑立即重新分配调度
        //     最好 遍历 所有的活的 Downloader, 然后发现其无Piece任务，便分配
        //
        // RequestNextPiece()    [可能 有因为校验异步的原因 导致 空闲Downloader]
        /*
        for (set<Downloader::p>::iterator iter = downloaders_.begin(); iter != downloaders_.end(); iter ++)
        {
        if (false == (*iter)->IsDeath())
        {
        if (!(*iter)->IsDownloading())
        {
        RequestNextPiece(*iter);
        }
        }
        }
        */
    }

    void DownloadDriver::OnNoticeContentHashSucced(string url, MD5 content_md5, uint32_t content_bytes, uint32_t file_length)
    {
        if (false == is_running_)
            return;DD_EVENT("DownloadDriver::OnNoticeContentHashSucced MD5:" << content_md5 << " file_length:" << content_bytes << " file_length:" << file_length);
        IndexManager::Inst()->DoQueryRidByContent(origanel_url_info_.url_, origanel_url_info_.refer_url_, content_md5,
            content_bytes, file_length);
    }

    void DownloadDriver::OnNoticeGetFileName(const string& file_name)
    {
        if (false == is_running_)
        {
            return;
        }
        statistic_->SetFileName(file_name);
    }

    void DownloadDriver::OnNoticeSetWebUrl(const string& web_url)
    {
        if (false == is_running_)
        {
            return;
        }

        LOGX(__DEBUG, "interface", "DownloadDriver::OnNoticeSetWebUrl weburl = " << web_url);
        statistic_->SetWebUrl(web_url);
    }

     // 通知获得文件名
    void DownloadDriver::OnRecvSubPiece(uint32_t position, const protocol::SubPieceBuffer& buffer)
    {
        assert(buffer.Data() && (buffer.Length() <= bytes_num_per_piece_g_) && (buffer.Length()>0));
        if (proxy_connection_ && proxy_connection_->GetPlayingPosition() == position)
        {
            std::vector<protocol::SubPieceBuffer> buffers(1, buffer);
            instance_->GetSubPieceForPlay(shared_from_this(), position + buffer.Length(), buffers);

            std::vector<base::AppBuffer> app_buffers;
            for (std::vector<protocol::SubPieceBuffer>::iterator iter = buffers.begin();
                iter != buffers.end(); ++iter)
            {
                base::AppBuffer app_buffer(*iter);
                app_buffers.push_back(app_buffer);
            }

            proxy_connection_->OnRecvSubPiece(position, app_buffers);
        }
    }

    uint32_t DownloadDriver::GetPlayingPosition() const
    {
        if (proxy_connection_)
        {
            return proxy_connection_->GetPlayingPosition();
        }
        return 0;
    }

    void DownloadDriver::GetSubPieceForPlay()
    {
        std::vector<protocol::SubPieceBuffer> buffers;
        if (instance_)
        {
            instance_->GetSubPieceForPlay(shared_from_this(), proxy_connection_->GetPlayingPosition(), buffers);
        }

        std::vector<base::AppBuffer> app_buffers;
        for (std::vector<protocol::SubPieceBuffer>::iterator iter = buffers.begin();
            iter != buffers.end(); ++iter)
        {
            base::AppBuffer app_buffer(*iter);
            app_buffers.push_back(app_buffer);
        }

        if (!app_buffers.empty())
        {
            if (proxy_connection_)
            {
                proxy_connection_->OnRecvSubPiece(proxy_connection_->GetPlayingPosition(), app_buffers);
            }
        }
    }

    bool DownloadDriver::HasNextPiece(VodDownloader__p downloader)
    {
        if (false == is_running_)
            return false;

        // 调用 bool piece_request_manager_->GetNextPieceForDownload(playing_position) 获得 PieceInfoEx
        // 如果 返回 false
        //      则说明下载可能已经完成

        protocol::PieceInfoEx piece_info_to_download;
        uint32_t request_possition = proxy_connection_->GetPlayingPosition();

        statistic_->SetDataRate(GetDataRate());

        while (true)
        {
            if (false == piece_request_manager_->HasNextPieceForDownload(request_possition, piece_info_to_download, downloader))
            {
                return false;
            }
            else if (downloader->CanDownloadPiece(piece_info_to_download.GetPieceInfo()))
            {
                DD_EVENT("DownloadDriver::RequestNextPiece " << GetDownloadDriverID() << " " << downloader->IsOriginal() << " " << downloader->IsP2PDownloader() << " " << downloader << " " << piece_info_to_download);
                // piece_request_manager_->AddPieceTask(piece_info_to_download, downloader);
                // downloader->PutPieceTask(piece_info_to_download, shared_from_this());
                // MainThread::Post(boost::bind(&Downloader::PutPieceTask, downloader, piece_info_to_download, shared_from_this()));
                return true;
            }
            else
            {
                // 改成跳一个block
                uint32_t block_size = instance_->GetBlockSize();
                if (block_size == 0)
                {
                    break;
                }
                request_possition = piece_info_to_download.GetBlockEndPosition(block_size);
            }
        }
        return false;
    }

    bool DownloadDriver::RequestNextPiece(VodDownloader__p downloader)
    {
        if (false == is_running_)
            return false;

        // 调用 bool piece_request_manager_->GetNextPieceForDownload(playing_position) 获得 PieceInfoEx
        // 如果 返回 false
        //      则说明下载可能已经完成
        std::deque<protocol::PieceInfoEx> piece_info_ex_s;
        protocol::PieceInfoEx piece_info_to_download;
        uint32_t request_possition = proxy_connection_->GetPlayingPosition();

        statistic_->SetDataRate(GetDataRate());

        while (true)
        {
            if (false == piece_request_manager_->GetNextPieceForDownload(request_possition, piece_info_to_download,
                downloader))
            {
                return false;
            }
            else if (downloader->CanDownloadPiece(piece_info_to_download.GetPieceInfo()))
            {
#ifdef USE_MEMORY_POOL
                uint32_t block_size = instance_->GetBlockSize();
                if (block_size == 0)
                {
                    return true;
                }

                uint32_t play_position = proxy_connection_->GetPlayingPosition();
                uint32_t download_position = piece_info_to_download.GetEndPosition(block_size);
                if (protocol::SubPieceContent::get_left_capacity() < 128 || 
                    download_position - play_position > (protocol::SubPieceContent::get_left_capacity() - 128) * 1024)
                {
                    return true;
                }
#endif
                DD_EVENT("DownloadDriver::RequestNextPiece " << GetDownloadDriverID() << " " << downloader->IsOriginal() << " " << downloader->IsP2PDownloader() << " " << downloader << " " << piece_info_to_download);
                piece_request_manager_->AddPieceTask(piece_info_to_download, downloader);

                // 将任务放入任务队列
                piece_info_ex_s.push_back(piece_info_to_download);

                if (downloader->IsP2PDownloader())
                {
                    // P2PDownloader
                    LOG(__DEBUG, "dbg", "RequestNextPiece p2p add " << piece_info_to_download);
                    downloader->PutPieceTask(piece_info_ex_s, shared_from_this());
                }
                else
                {
                    // HTTP
                    LOG(__DEBUG, "dbg", "RequestNextPiece http add " << piece_info_to_download);
                    boost::int32_t num = 0;
                    if (downloader->GetSpeedInfo().NowDownloadSpeed > 25*1024)
                    {
                        num = downloader->GetSpeedInfo().NowDownloadSpeed * P2SPConfigs::PIECE_TIME_OUT_IN_MILLISEC / (128 * 1024) - downloader->GetPieceTaskNum() - 2;
                    }
                    LIMIT_MIN_MAX(num, 0, 7);

                    LOG(__DEBUG, "ppbug", "RequestPiecesssssss num = " << num);

                    for (boost::int32_t i = 0; i<num; i++)
                    {
                        if (piece_request_manager_->GetNextPieceForDownload(request_possition, piece_info_to_download, downloader))
                        {
                            if (downloader->CanDownloadPiece(piece_info_to_download.GetPieceInfo()))
                            {
#ifdef USE_MEMORY_POOL
                                uint32_t block_size = instance_->GetBlockSize();
                                if (block_size == 0)
                                {
                                    return true;
                                }

                                uint32_t play_position = proxy_connection_->GetPlayingPosition();
                                uint32_t download_position = piece_info_to_download.GetEndPosition(block_size);
                                if (protocol::SubPieceContent::get_left_capacity() < 128 || 
                                    download_position - play_position > (protocol::SubPieceContent::get_left_capacity() - 128) * 1024)
                                {
                                    downloader->PutPieceTask(piece_info_ex_s, shared_from_this());
                                    return true;
                                }
#endif
                                piece_request_manager_->AddPieceTask(piece_info_to_download, downloader);
                                // 将任务放入任务队列
                                LOG(__DEBUG, "dbg", "RequestNextPiece http add " << piece_info_to_download);
                                piece_info_ex_s.push_back(piece_info_to_download);
                            }
                        }
                    }


                    downloader->PutPieceTask(piece_info_ex_s, shared_from_this());
                }

                return true;
            }
            else
            {
            // 改成跳一个block
                uint32_t block_size = instance_->GetBlockSize();
                if (block_size == 0)
                {
                    break;
                }
                request_possition = piece_info_to_download.GetBlockEndPosition(block_size);
            }
        }

        return false;
    }
/*
    void DownloadDriver::OnPlayTimer(uint32_t times)
    {
        if (false == is_running_)
            return;
        // instance_->AsyncGetSubPieceForPlay(playing_position_, proxy_connection_);
        LOG(__EVENT, "proxy", "DownloadDriver::OnPlayTimer PlayingPosition" << proxy_connection_->GetPlayingPosition());
        instance_->AsyncGetSubPieceForPlay(proxy_connection_->GetPlayingPosition(), proxy_connection_);
    }
*/

    bool DownloadDriver::IsHttpDownloaderSupportRange()
    {
        if (!is_running_)
            return false;
        for (std::map<string, HttpDownloader::p>::iterator iter = url_indexer_.begin(); iter != url_indexer_.end(); iter++)
        {
            if (iter->second->IsSupportRange())
            {
                return true;
            }
        }
        return false;
    }

    //////////////////////////////////////////////////////////////////////////
    // IGlobalDataProvider

    uint32_t DownloadDriver::GetBandWidth()
    {
        if (false == is_running_)
            return 0;
        uint32_t total_download_speed = StatisticModule::Inst()->GetBandWidth();
        // SWITCH_DEBUG("band_width=" << total_download_speed);
        return total_download_speed;
    }

    uint32_t DownloadDriver::GetFileLength()
    {
        if (false == is_running_)
            return 0;
        return instance_->GetFileLength();
    }

    uint32_t DownloadDriver::GetDataRate()
    {
        if (false == is_running_)
            return 0;
        uint32_t data_rate = instance_->GetMetaData().VideoDataRate;
        uint32_t default_rate = (is_open_service_ ? 60 * 1024 : 30 * 1024);
        return data_rate == 0 ? default_rate : data_rate;
    }

    uint32_t DownloadDriver::GetPlayElapsedTimeInMilliSec()
    {
        if (false == is_running_)
            return 0;
        return download_time_counter_.elapsed();
    }

    uint32_t DownloadDriver::GetDownloadingPosition()
    {
        if (false == is_running_)
            return 0;
        return proxy_connection_->GetPlayingPosition();
    }

    uint32_t DownloadDriver::GetDownloadedBytes()
    {
        if (false == is_running_)
            return 0;
        return instance_->GetDownloadBytes();
    }

    uint32_t DownloadDriver::GetDataDownloadSpeed()
    {
        if (false == is_running_)
            return 0;
        uint32_t data_download_speed = statistic_->GetHttpDownloadAvgSpeed();
        if (p2p_downloader_)
        {
            data_download_speed += p2p_downloader_->GetSpeedInfo().AvgDownloadSpeed;
        }
        return data_download_speed;
    }

    boost::uint32_t DownloadDriver::GetSecondDownloadSpeed()
    {
        if (false == is_running_)
            return 0;

        u_int speed = 0;

        if (GetHTTPControlTarget())
            speed += GetHTTPControlTarget()->GetSecondDownloadSpeed();
        if (GetP2PControlTarget())
            speed += GetP2PControlTarget()->GetSecondDownloadSpeed();

        return speed;
    }

    bool DownloadDriver::IsStartFromNonZero()
    {
        return openservice_start_position_ != 0;
    }

    bool DownloadDriver::IsDrag()
    {
        LOG(__DEBUG, "switch", "IsDrag = " << is_drag_);
        return is_drag_;
    }

    bool DownloadDriver::IsHeadOnly()
    {
        return is_head_only_;
    }

    bool DownloadDriver::IsPPLiveClient()
    {
        if (GetSessionID() == "")
            return true;

        return false;
    }

    bool DownloadDriver::IsDragLocalPlay()
    {
        LOG(__DEBUG, "switch", "IsDragLocalPlay = " << is_drag_local_play);
        return is_drag_local_play;
    }

    void DownloadDriver::SetSwitchState(boost::int32_t h, boost::int32_t p, boost::int32_t tu, boost::int32_t t)
    {
        statistic_->SetHttpState(h);
        statistic_->SetP2PState(p);
        statistic_->SetTimerusingState(tu);
        statistic_->SetTimerState(t);
    }

    boost::uint32_t DownloadDriver::GetRestPlayableTime()
    {
        if (rest_play_time_set_counter_.elapsed() >= 5000)
        {
            // 超过5s不设置剩余缓冲时间，一定是预下载，将剩余缓冲设置为无穷大，使得状态机用纯p2p下载
            return 0xFFFFFFFF;
        }
        else
        {
            return rest_play_time_;
        }        
    }

    void DownloadDriver::SetDragMachineState(boost::int32_t state)
    {
        drag_machine_state_ = state;
    }

    bool DownloadDriver::HasRID()
    {
        if (false == is_running_)
            return false;
        return instance_->HasRID();
    }

    IHTTPControlTarget::p DownloadDriver::GetHTTPControlTarget()
    {
        if (false == is_running_)
            return IHTTPControlTarget::p();
        return url_indexer_.empty() ? IHTTPControlTarget::p() : url_indexer_.begin()->second;
    }
    IP2PControlTarget::p DownloadDriver::GetP2PControlTarget()
    {
        if (false == is_running_)
            return IP2PControlTarget::p();
        return p2p_downloader_;
    }
    void DownloadDriver::OnStateMachineType(uint32_t state_machine_type)
    {
        if (false == is_running_)
            return;
        if (statistic_)
        {
            statistic_->SetStateMachineType(state_machine_type);
        }
    }
    void DownloadDriver::OnStateMachineState(const string& state_machine_state)
    {
        if (false == is_running_)
            return;
        if (statistic_)
        {
            statistic_->SetStateMachineState(state_machine_state);
        }
    }

    void DownloadDriver::NoticePieceTaskTimeOut(const protocol::PieceInfoEx& piece_info_ex, VodDownloader__p downloader)
    {
        if (false == is_running_)
            return;
        piece_request_manager_->NoticePieceTaskTimeOut(piece_info_ex, downloader);
    }

    void DownloadDriver::OnNoticePragmaInfo(string server_mod, uint32_t head_length)
    {
        if (false == is_running_)
            return;

        if (true == is_pragmainfo_noticed_)
        {
            LOG(__DEBUG, "proxy", __FUNCTION__ << ":" << __LINE__ << " is_pragmainfo_noticed_ = true");
            return;
        }

        LOG(__DEBUG, "proxy", __FUNCTION__ << ":" << __LINE__ << " Notice, head_length = " << head_length << ", mode = " << server_mod);
        is_pragmainfo_noticed_ = true;

        // change piece request manager
        openservice_head_length_ = head_length;

        if (boost::algorithm::istarts_with(server_mod, SYNACAST_FLV_STREAMING))
        {
            // parse head_length
            proxy_connection_->OnNoticeOpenServiceHeadLength(head_length);
        }
        else if (boost::algorithm::istarts_with(server_mod, SYNACAST_MP4_STREAMING))
        {
            LOG(__DEBUG, "proxy", __FUNCTION__ << ":" << __LINE__ << " start_position = " << openservice_start_position_);
            uint32_t task_start_position = openservice_start_position_;
            if (true == is_head_only_)
            {
                LOG(__DEBUG, "proxy", __FUNCTION__ << ":" << __LINE__ << " task_range = [0, " << openservice_head_length_ << "]; head_only = true!");
                piece_request_manager_->ClearTaskRangeMap();
                piece_request_manager_->AddTaskRange(0, openservice_head_length_);
            }
            else if (task_start_position > openservice_head_length_)
            {
                LOG(__DEBUG, "proxy", __FUNCTION__ << ":" << __LINE__ << " task_range = [0, " << openservice_head_length_
                    << "]; [" << openservice_start_position_ << ", EOF]");
                piece_request_manager_->ClearTaskRangeMap();
                piece_request_manager_->AddTaskRange(0, openservice_head_length_);
                piece_request_manager_->AddTaskRange(task_start_position, 0);
            }
            // parse head_length
            proxy_connection_->OnNoticeOpenServiceHeadLength(head_length);
        }
        else
        {
            // ignore
            is_pragmainfo_noticed_ = false;
        }
    }

    void DownloadDriver::SetSpeedLimitInKBps(boost::int32_t speed_limit_in_KBps)
    {
        LOGX(__DEBUG, "http_downlimiter", "speed_limit_in_KBps = " << speed_limit_in_KBps);
        speed_limit_in_KBps_ = speed_limit_in_KBps;
        if (false == is_running_)
        {
            LOGX(__DEBUG, "http_downlimiter", "DownloadDriver Not Running, Just Store!");
            return;
        }
        for (std::set<VodDownloader__p>::iterator it = downloaders_.begin(); it != downloaders_.end(); ++it)
        {
            (*it)->SetSpeedLimitInKBps(speed_limit_in_KBps_);
        }
    }

    boost::int32_t DownloadDriver::GetSpeedLimitInKBps()
    {
        if (false == is_running_)
        {
            return 0;
        }

        return speed_limit_in_KBps_;
    }

    void DownloadDriver::SetOpenServiceStartPosition(uint32_t start_position)
    {
        // no need to check is_running_;
        openservice_start_position_ = start_position;
    }

    void DownloadDriver::SetIsDrag(bool is_drag)
    {
        is_drag_ = is_drag;
    }

    void DownloadDriver::SetOpenServiceHeadLength(uint32_t head_length)
    {
        if (head_length > 0 && head_length < 2 * 1024 * 1024)
        {
            openservice_head_length_ = head_length;
        }
        else
        {
            openservice_head_length_ = 0;
        }
    }

    void DownloadDriver::SetOpenServiceRange(bool is_openservice_range)
    {
        is_openservice_range_ = is_openservice_range;
    }

    void DownloadDriver::OnNoticeFileDownloadComplete()
    {
        if (false == is_running_)
        {
            return;
        }LOG(__DEBUG, "proxy", __FUNCTION__ << ":" << __LINE__ << " OnNoticeFileDownloadComplete dd=" << shared_from_this() << ", proxy=" << proxy_connection_);
        // MainThread::Post(boost::bind(&ProxyConnection::OnNoticeStopDownloadDriver, proxy_connection_));
        proxy_connection_->OnNoticeStopDownloadDriver();
    }

    bool DownloadDriver::IsSaveMode() const
    {
        if (false == is_running_)
            return false;
        if (proxy_connection_)
            return proxy_connection_->IsSaveMode();
        return false;
    }

    bool DownloadDriver::IsDownloading() const
    {
        if (false == is_running_)
            return false;

        // 判断是否有downloader处于非pause状态
        bool is_all_pausing = true;
        for (std::set<VodDownloader__p>::const_iterator iter = downloaders_.begin();
                iter != downloaders_.end(); ++iter)
        {
            if (!(*iter)->IsPausing())
            {
                is_all_pausing = false;
                break;
            }
        }

        if (is_all_pausing)
            return false;

        if (piece_request_manager_)
            return piece_request_manager_->HasPieceTask();
        else
            return false;
    }
    void DownloadDriver::OnPieceRequest(const protocol::PieceInfo & piece)
    {
        if (false == is_running_)
            return;

        if (piece_request_manager_)
            piece_request_manager_->OnPieceRequest(piece);
    }

    void DownloadDriver::SetRestPlayTime(boost::uint32_t rest_play_time)
    {
        DD_DEBUG("SetRestPlayTime " << rest_play_time);
        rest_play_time_ = rest_play_time;
        rest_play_time_set_counter_.reset();

        DetectBufferring();
    }

    void DownloadDriver::SetDownloadMode(boost::int32_t download_mode)
    {
        download_mode_ = download_mode;
    }

    boost::int32_t DownloadDriver::GetDownloadMode()
    {
        return download_mode_;
    }
    void DownloadDriver::SmartLimitSpeed(boost::uint32_t times)
    {
        if (!is_running_)
        {
            return;
        }

        if (disable_smart_speed_limit_)
        {
            return;
        }

        // herain:push任务不智能限速
        if (is_push_)
        {
            return;
        }

        boost::int32_t data_rate = GetDataRate();
        boost::uint32_t bandwidth = GetBandWidth();

        boost::int32_t b;
        double alpha;
        boost::int32_t beta;

        // 智能限速模式是不会对HTTP限速
        // 所以这里需要设置默认限速
        // 因为后可能切换到选节目模式之后，是限总速度的，这需要恢复过来
        if (GetHTTPControlTarget())
        {
            // HTTP默认限速400
            GetHTTPControlTarget()->SetSpeedLimitInKBps(400);
        }

        if (download_mode_ == IGlobalControlTarget::SMART_MODE)
        {
            // 智能限速模式
            // 带宽设置
            if (bandwidth > 512 * 1024)
            {
                // 4M +
                b = 360;
            }
            else if (bandwidth > 256 * 1024)
            {
                // 4M
                b = 250;
            }
            else if (bandwidth > 128 * 1024)
            {
                // 2M
                b = 160;
            }
            else
            {
                // 1M
                b = 90;
            }

            // 时间设置
            if (rest_play_time_ > 120 * 1000)
            {
                // 60 秒以上
                if (GetSessionID() == "")
                {
                    // pplive
                    alpha = 1.2;
                    beta = 20;
                }
                else
                {
                    // ikan
                    alpha = 1.4;
                    beta = 40;
                }
            }
//             else if (rest_play_time_ > 30 * 1000)
//             {
//                 // 30 - 60 秒
//                 if (GetSessionID() == "")
//                 {
//                     // pplive
//                     alpha = 1.4;
//                     beta = 40;
//                 }
//                 else
//                 {
//                     // ikan
//                     alpha = 1.6;
//                     beta = 60;
//                 }
//             }
            else
            {
                alpha = 0;
                data_rate = 0;
                beta = -1;
                b = -1;
            }
            boost::int32_t speed_limit = std::max(std::min((boost::int32_t)(data_rate*alpha/1024), (boost::int32_t)(data_rate/1024 + beta)), b);
            speed_limit_in_KBps_ = speed_limit;

            statistic_->SetSmartPara(rest_play_time_, bandwidth, speed_limit);

            LOG(__DEBUG, "test", "Rest Play Time = " << rest_play_time_ << " speed_limit = " << speed_limit << " alpha = " << alpha << " beta = " << beta);

            if (times % 5 == 0)
            {
                if (p2p_downloader_ && !p2p_downloader_->IsPausing())
                {
                    if (rest_play_time_ < 30 * 1000)
                    {
                        p2p_downloader_->SetSpeedLimitInKBps(P2SPConfigs::P2P_DOWNLOAD_SPEED_LIMIT);
                    }
                    else
                    {
                        p2p_downloader_->SetSpeedLimitInKBps(speed_limit);
                    }
                }
            }
        }
        else if (download_mode_ == IGlobalControlTarget::FAST_MODE)
        {
            // P2P默认限速500
            if (p2p_downloader_)
            {
                p2p_downloader_->SetSpeedLimitInKBps(P2SPConfigs::P2P_DOWNLOAD_SPEED_LIMIT);
            }
            statistic_->SetSmartPara(rest_play_time_, -1, P2SPConfigs::P2P_DOWNLOAD_SPEED_LIMIT);
        }
        else if (download_mode_ == IGlobalControlTarget::SLOW_MODE)
        {
            // 柔和下载模式
            boost::uint32_t data_rate = GetDataRate();

            float rate = 0.8;
            if (p2p_downloader_ && p2p_downloader_->GetStatistic())
            {
                rate = p2p_downloader_->GetStatistic()->GetUDPLostRate();
                rate = (1 - (float)p2p_downloader_->GetStatistic()->GetUDPLostRate() / (float)100) + 0.001;
                LOG(__DEBUG, "test", "RATE = " << rate);
            }

            if (p2p_downloader_)
            {
                if (rate > 0 && rate < 1)
                {
                    p2p_downloader_->SetSpeedLimitInKBps(((std::max)((boost::uint32_t)(data_rate * 1.3/1024), data_rate/1024 + 30))/rate);
                    statistic_->SetSmartPara(rest_play_time_, -2, ((std::max)((boost::uint32_t)(data_rate * 1.3/1024), data_rate/1024 + 30))/rate);
                }
                else
                {
                    p2p_downloader_->SetSpeedLimitInKBps(((std::max)((boost::uint32_t)(data_rate * 1.3/1024), data_rate/1024 + 30)));
                    statistic_->SetSmartPara(rest_play_time_, -2, ((std::max)((boost::uint32_t)(data_rate * 1.3/1024), data_rate/1024 + 30)));
                }
            }
        }
    }

    // 获得拖动时状态机状态
    // 播放器拖动后完成缓冲2s数据后会检测内核带宽状态，如果为Yes则立即播放，
    // 否则继续判断缓冲2s数据消耗的时间，若不足2s，则立即播放，反之增加缓冲时间为4s。
    void DownloadDriver::GetDragMachineState(boost::int32_t & state)
    {
        if (switch_control_mode_ != SwitchController::CONTROL_MODE_VIDEO_OPENSERVICE)
        {
            state = (boost::int32_t)SwitchController::MS_UNDEFINED;
            LOG(__DEBUG, "downloaddriver", "GetDragMachineState: MS_UNDEFINED");


            return;
        }
        if (is_drag_)
        {
            if (is_drag_local_play)
            {
                state = (boost::int32_t)SwitchController::MS_YES;
                LOG(__DEBUG, "downloaddriver", "GetDragMachineState is_drag_local_play: MS_YES");
                return;
            }
            else
            {
                state = drag_machine_state_;
                LOG(__DEBUG, "downloaddriver", "GetDragMachineState: " << state);
                return;
            }
        }
        state = (boost::int32_t)SwitchController::MS_UNDEFINED;
    }

    void DownloadDriver::DetectBufferring()
    {
        if (GetRestPlayableTime() > max_rest_playable_time_ && static_cast<int>(GetRestPlayableTime()) > 0)
        {
            max_rest_playable_time_ = GetRestPlayableTime();
        }

        if (bufferring_monitor_)
        {
            if (is_running_ &&
                download_time_counter_.running() && 
                download_time_counter_.elapsed() > 3*1000 &&
                max_rest_playable_time_ > 2*1000 &&
                GetRestPlayableTime() <= 1000)
            {
                uint32_t data_rate = GetDataRate();
                if (data_rate > 0)
                {
                    uint32_t bufferring_position_in_seconds = GetPlayingPosition()/data_rate;
                    bufferring_monitor_->BufferringOccurs(bufferring_position_in_seconds);
                }
                else
                {
                    LOG(__ERROR, "", "data rate = 0");
                }
            }
            else
            {
                LOG(__DEBUG, "", "RestPlayableTime " << GetRestPlayableTime());
                LOG(__DEBUG, "", "download_time_counter.running " << download_time_counter_.running());
                LOG(__DEBUG, "", "download_time_counter.elapsed " << download_time_counter_.elapsed());
            }
        }
    }
    
    void DownloadDriver::OnTimerElapsed(framework::timer::Timer * pointer)
    {
        if (pointer == &speed_limit_timer_)
        {
            if (is_open_service_)
            {
                LOG(__DEBUG, "test", "DownloadDriver::OnTimerElapsed");
                SmartLimitSpeed(pointer->times());
            }
        }
    }

    void DownloadDriver::SetAcclerateStatus(boost::int32_t status)
    {
        if (!is_got_accelerate_http_speed && (status == 17 || status == 24))
        {
            // P2P下载字节数
            boost::uint32_t total = 0;
            if (p2p_downloader_ && p2p_downloader_->GetStatistic())
            {
                total += p2p_downloader_->GetStatistic()->GetTotalP2PDataBytes();
            }
            // HTTP总下载字节数
            total += statistic_->GetTotalHttpDataBytes();
            accelerate_http_speed = total * 1000.0 / download_time_counter_.elapsed();

            // 设置 is_got_accelerate_http_speed
            is_got_accelerate_http_speed = true;
        }

        accelerate_status_ = status;
    }

    void DownloadDriver::EnableSmartSpeedLimit()
    {
        disable_smart_speed_limit_ = false;
    }

    void DownloadDriver::DisableSmartSpeedLimit()
    {
        disable_smart_speed_limit_ = true;
    }

    bool DownloadDriver::IsHeaderResopnsed()
    {
        return proxy_connection_->IsHeaderResopnsed();
    }

    void DownloadDriver::NoticeP2pSpeedLimited()
    {
        if (avg_download_speed_before_limit_ == -1)
        {
            // 只统计点播的数据，非点播的情况可能在一开始就被限速，数据失去意义
            if (source_type_ == 0 || source_type_ == 1)
            {
                DownloadDriverStatistic__p statistic = GetStatistic();
                assert(statistic && download_time_counter_.elapsed() > 0);

                int32_t p2p_download_bytes = 0, http_download_bytes = 0;

                // P2P下载字节数
                if (p2p_downloader_ && p2p_downloader_->GetStatistic())
                {
                    p2p_download_bytes = (int32_t)(p2p_downloader_->GetStatistic()->GetTotalP2PDataBytes());
                }

                http_download_bytes = (int32_t)(statistic->GetTotalHttpDataBytes());

                // 平均下载速度(AvgDownloadSpeed)
                avg_download_speed_before_limit_ = (int32_t)((p2p_download_bytes + http_download_bytes) * 
                    1000.0  / download_time_counter_.elapsed());
            }
        }
    }

    void DownloadDriver::NoticeLeave2300()
    {
        if (avg_http_download_speed_in2300_ == -1)
        {
            assert(GetStatistic() && download_time_counter_.elapsed() > 0);

            int32_t http_download_bytes = 0;

            http_download_bytes = (int32_t)(GetStatistic()->GetTotalHttpDataBytes());

            // 平均下载速度(AvgDownloadSpeed)
            avg_http_download_speed_in2300_ = (int32_t)(http_download_bytes * 
                1000.0  / download_time_counter_.elapsed());
        }
    }

    void DownloadDriver::SetDragHttpStatus(int32_t status)
    {
        if (drag_http_status_ == 0)
        {
            drag_http_status_ = status;
        }
    }

    void DownloadDriver::SetRidInfo(const RidInfo & ridinfo)
    {
        assert(!rid_info_.HasRID());
        assert(ridinfo.HasRID());

        LOG(__DEBUG, "", "SetRidInfo: this:" << shared_from_this() <<", instance:" << instance_);
        LOG(__DEBUG, "", "SetRidInfo: " << ridinfo.GetRID().to_string());

        rid_info_ = ridinfo;
        if (proxy_connection_)
        {
            proxy_connection_->OnNoticeGetContentLength(rid_info_.file_length_, network::HttpResponse::p());
        }

        if (instance_)
        {
            Storage::Inst()->AttachRidByUrl(origanel_url_info_.url_, rid_info_, MD5(), 0, protocol::RID_BY_PLAY_URL);
        }

        bufferring_monitor_.reset();
        StartBufferringMonitor();
    }

    std::vector<IHTTPControlTarget::p> DownloadDriver::GetAllHttpControlTargets()
    {
        std::vector<IHTTPControlTarget::p> target_vec;
        for (std::map<string, HttpDownloader__p>::iterator iter = url_indexer_.begin();
            iter != url_indexer_.end(); ++iter)
        {
            target_vec.push_back(iter->second);
        }

        return target_vec;
    }

    void DownloadDriver::AddBakHttpDownloaders(protocol::UrlInfo& original_url_info)
    {
        network::Uri uri(original_url_info.url_);
        string uri_path(uri.getpath());
        string uri_parameter(uri.getparameter());
        for (int i = 0; i < bak_hosts_.size(); ++i)
        {
            string bak_url = "http://" + bak_hosts_[i] + uri_path + "?" + uri_parameter;
            AddHttpDownloader(HttpRequest::p(), protocol::UrlInfo(bak_url, ""), true);
        }
    }
}
