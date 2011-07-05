//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/AppModule.h"
#include "p2sp/push/PushModule.h"
#include "p2sp/p2p/P2SPConfigs.h"
#include "p2sp/proxy/ProxyModule.h"
#include "p2sp/p2p/P2PModule.h"
#include "statistic/StatisticModule.h"
#include "boost/algorithm/string.hpp"
#include "storage/Storage.h"
#include "storage/Performance.h"
#include "p2sp/bootstrap/BootStrapGeneralConfig.h"

#include <cmath>

#ifdef BOOST_WINDOWS_API
#include <TlHelp32.h>
#endif

#define PUSH_DEBUG(msg) LOG(__DEBUG, "push", __FUNCTION__ << " " << msg)
#define PUSH_WARN(msg) LOG(__WARN, "push", __FUNCTION__ << " " << msg)

namespace p2sp
{
#ifdef DISK_MODE
    FRAMEWORK_LOGGER_DECLARE_MODULE("push");
    //////////////////////////////////////////////////////////////////////////
    // States
    namespace push
    {
        enum USER {
            USER_NONE,
            USER_IDLE,
            USER_AWAY,
        };

        enum TASK {
            TASK_NONE,
            TASK_WAIT,               // 当前没有push任务，间隔固定时间重复查询
            TASK_QUERY,              // 正在查询push任务
            TASK_QUERY_RETRY,        // 查询push任务失败，等一段时间重试
            TASK_HAVE,               // 正在下载push任务
            TASK_COMPELETING,        // 正在汇报push任务完成
            TASK_DISABLE,
        };

        enum DISK {
            DISK_NO,
            DISK_YES,
        };

        enum PUSH_DOMAIN {
            DOMAIN_NONE,
            DOMAIN_RESOLVE,
            DOMAIN_HAVE,
        };
    }

    //////////////////////////////////////////////////////////////////////////
    // Static

    PushModule::p PushModule::inst_(new PushModule());
    static const int32_t QueryPushTaskTimeOutInMs = 5000;
    static const int32_t QueryPushTaskRetryMaxIntervalInSec = 3600;
    static const int32_t PushWaitIntervalInSec = 3600;
    static const int32_t ReportPushTaskCompeteTimeOutInMs = 5000;

    //////////////////////////////////////////////////////////////////////////
    // Methods

    PushModule::State::State()
    {
        user_ = push::USER_NONE;
        task_ = push::TASK_NONE;
        disk_ = push::DISK_NO;
        domain_ = push::DOMAIN_NONE;
    }

    PushModule::PushModule()
        : is_running_(false)
        , is_push_enabled_(true)
        , timer_(global_second_timer(), 1000, boost::bind(&PushModule::OnTimerElapsed, this, &timer_))
        , last_query_timer_(false)
        , query_error_num_(0)
        , task_complete_error_num_(0)
        , global_speed_limit_in_kbps_(-1)
        , push_wait_interval_in_sec_(PushWaitIntervalInSec)
    {
    }

    void PushModule::Start()
    {
        if (true == is_running_) {
            return;
        }
        PUSH_DEBUG("Start");

        is_running_ = true;

        push_server_domain_ = "ppvaps.pplive.com";
        push_server_port_ = 6900;

        state_.domain_ = push::DOMAIN_NONE;

        CheckDiskSpace();
        CheckUserState();

        // init
        push_task_.param_.ProtectTimeIntervalInSeconds = P2SPConfigs::PUSH_PROTECTION_INTERVAL_IN_SEC;

        if (push::DISK_YES == state_.disk_) {
            // timer_ = framework::timer::PeriodicTimer::create(1000, shared_from_this());
            timer_.start();
        }
        else {
            PUSH_DEBUG("Disk Space Not Enough.");
        }
    }

    void PushModule::Stop()
    {
        if (false == is_running_) {
            return;
        }

        timer_.stop();

        is_running_ = false;
    }

    void PushModule::OnResolverSucced(uint32_t ip, uint16_t port)
    {
        if (false == is_running_) {
            return;
        }

        state_.domain_ = push::DOMAIN_HAVE;

        framework::network::Endpoint ep(ip, port);
        push_server_endpoint_ = ep;

        PUSH_DEBUG("ServerEndpoint = " << push_server_endpoint_);

        if (push::TASK_QUERY == state_.task_) {
            DoQueryPushTask();
        }
    }

    void PushModule::OnResolverFailed(boost::uint32_t error_code)
    {
        if (false == is_running_) {
            return;
        }

        PUSH_DEBUG("");
        state_.domain_ = push::DOMAIN_NONE;
    }

    void PushModule::OnTimerElapsed(framework::timer::Timer * timer_ptr)
    {
        if (false == is_running_) {
            return;
        }

        if (timer_ptr == &timer_) 
        {
            // 每次任务完成时, 检查磁盘大小
            // downloading
            PUSH_DEBUG("state_.user_=" << state_.user_ << ", state_.task_" << state_.task_ << ", state_.disk_" << state_.disk_ << ", state_.domain_" << state_.domain_);

            state_.user_ = GetUserState();
            CheckUsePush();

            switch(state_.task_)
            {
            case push::TASK_NONE:
                if (!last_query_timer_.running() || 
                    last_query_timer_.elapsed() > (uint32_t)push_task_.param_.ProtectTimeIntervalInSeconds * 1000)
                {            
                    DoQueryPushTask();
                }
                break;

            case push::TASK_WAIT:
                assert(last_query_timer_.running());
                if (last_query_timer_.elapsed() > (uint32_t)push_wait_interval_in_sec_ * 1000)
                {
                    DoQueryPushTask();
                }
                break;

            case push::TASK_QUERY:
                if (last_query_timer_.running() && last_query_timer_.elapsed() > QueryPushTaskTimeOutInMs)
                {
                    query_error_num_++;
                    state_.task_ = push::TASK_QUERY_RETRY;
                }
                break;

            case push::TASK_QUERY_RETRY:
                {
                    boost::uint32_t retry_interval_in_ms = query_error_num_ >= 2 ?
                        QueryPushTaskRetryMaxIntervalInSec * 1000 : QueryPushTaskTimeOutInMs;
                    
                    if (last_query_timer_.elapsed() > retry_interval_in_ms)
                    {
                        DoQueryPushTask();
                    }
                }                
                break;

            case push::TASK_HAVE:
                LimitTaskSpeed();
                break;

            case push::TASK_COMPELETING:
                if (task_complete_timer_.elapsed() > ReportPushTaskCompeteTimeOutInMs)
                {
                    ++task_complete_error_num_;
                    if (task_complete_error_num_ < 3)
                    {
                        SendReportPushTaskCompletePacket(push_task_.rid_info_);
                    }
                    else
                    {
                        // 汇报失败三次以上放弃汇报
                        // TODO(herain):2011-5-30:是不是应该有另一种途径精确统计push的资源总数
                        state_.task_ = push::TASK_NONE;
                    }
                }
                break;            

            default:
                break;
            }
        }
    }

    void PushModule::DoQueryPushTask()
    {
        if (false == is_running_) 
        {
            return;
        }

        state_.task_ = push::TASK_QUERY;
        if (push::DOMAIN_HAVE == state_.domain_) 
        {
            PUSH_DEBUG("DOMAIN_HAVE, QueryTask; Server = " << push_server_endpoint_);
            //
            last_transaction_id_ = protocol::Packet::NewTransactionID();

            last_query_timer_.start();   // start and reset
            protocol::QueryPushTaskPacket query_push_task_request(last_transaction_id_, AppModule::Inst()->GetUniqueGuid(), 
                push_server_endpoint_, 
                storage::Storage::Inst()->GetUsedDiskSpace(), 
                P2PModule::Inst()->GetUploadBandWidthInKBytes(), 
                statistic::StatisticModule::Inst()->GetRecentMinuteUploadDataSpeedInKBps());
            AppModule::Inst()->DoSendPacket(query_push_task_request);
        }
        else if (push::DOMAIN_NONE == state_.domain_) 
        {
            // resolve
            PUSH_DEBUG("DOMAIN_NONE, ResolveDomain");
            DoResolvePushServer();
        }
        else if (push::DOMAIN_RESOLVE == state_.domain_) 
        {
            PUSH_DEBUG("Push::DOMAIN_RESOLVE");
        }
        else
        {
            assert(!"Invalid Domain State");
        }
    }

    void PushModule::DoResolvePushServer()
    {
        if (false == is_running_) {
            return;
        }

        PUSH_DEBUG("PushServer = " << push_server_domain_ << ":" << push_server_port_);
        state_.domain_ = push::DOMAIN_RESOLVE;
        network::Resolver::p resolver =
            network::Resolver::create(global_io_svc(), push_server_domain_, push_server_port_, shared_from_this());
        resolver->DoResolver();
    }

    void PushModule::OnPushTaskResponse(protocol::QueryPushTaskPacket const & response)
    {
        if (false == is_running_) 
        {
            return;
        }

        query_error_num_ = 0;

        if (response.transaction_id_ == last_transaction_id_) 
        {
            query_error_num_ = 0;
            last_transaction_id_ = 0;
            assert(state_.task_ == push::TASK_QUERY);

            if (response.error_code_ == 0)
            {
                push_task_.param_ = response.response.response_push_task_param_;
                push_task_.refer_url_ = response.response.response_refer_url_;
                push_task_.rid_info_ = response.response.rid_info_;
                push_task_.url_ = response.response.response_url_;
                PUSH_DEBUG("PushTask = " << push_task_);

                StartCurrentTask();
            }
            else if(response.error_code_ == protocol::QueryPushTaskPacket::NO_TASK)
            {
                state_.task_ = push::TASK_WAIT;
                push_wait_interval_in_sec_ = response.response.push_wait_interval_in_sec_;
            }
        }
        else
        {
            PUSH_DEBUG("InvalidTransID From " << response.end_point << ", TransID " << response.transaction_id_);
        }
    }

    void PushModule::CheckDiskSpace()
    {
        if (false == is_running_) {
            return;
        }
#if DISK_MODE
        boost::uint64_t free_size = storage::Storage::Inst()->GetFreeSize();
        boost::uint64_t store_size = storage::Storage::Inst()->GetStoreSize();
        if (free_size * 2 > store_size) {
            state_.disk_ = push::DISK_YES;
        }
        else {
            state_.disk_ = push::DISK_NO;
        }
#endif
    }

    void PushModule::CheckUserState()
    {
        if (false == is_running_) {
            return;
        }

        state_.user_ = GetUserState();
        PUSH_DEBUG("UserState = " << state_.user_ << " [0=NONE, 1=IDLE, 2=AWAY]");
    }

    boost::uint32_t PushModule::GetUserState() const
    {
        storage::DTType desk_type = storage::Performance::Inst()->GetCurrDesktopType();
        PUSH_DEBUG("GetCurrDesktopType:" << desk_type);
        if (storage::DT_SCREEN_SAVER == desk_type || storage::DT_WINLOGON == desk_type) {
            if (storage::Performance::Inst()->IsIdleInSeconds(P2SPConfigs::PUSH_AWAY_TIME_IN_SEC)) {
                return push::USER_AWAY;
            }
        }
        else if (storage::Performance::Inst()->IsIdleInSeconds(P2SPConfigs::PUSH_IDLE_TIME_IN_SEC)) {
            return push::USER_IDLE;
        }

        return push::USER_NONE;
    }

    void PushModule::StartCurrentTask()
    {
        if (false == is_running_) {
            return;
        }

        PUSH_DEBUG("Start Task: " << push_task_.url_);

        state_.task_ = push::TASK_HAVE;

        ProxyModule::Inst()->StartDownloadFileByRid(push_task_.rid_info_,
            protocol::UrlInfo(push_task_.url_, push_task_.refer_url_),
            (protocol::TASK_TYPE)push_task_.param_.TaskType, true);

        LimitTaskSpeed();
    }

    void PushModule::LimitTaskSpeed()
    {
        if (false == is_running_) 
        {
            return;
        }

        uint32_t bandwidth = std::max(statistic::StatisticModule::Inst()->GetBandWidth(), (uint32_t)64 * 1024);
        int speed_limit;
        if (push::USER_AWAY == state_.user_) 
        {
            speed_limit = -1;
            PUSH_DEBUG(" USER_AWAY, SpeedLimit = " << speed_limit);
        }
        else if (push::USER_IDLE == state_.user_) 
        {
            
            // check download drivers
            speed_limit = 1.0 * bandwidth * push_task_.param_.BandwidthRatioWhenIdle / (255 * 1024);
            LimitMinMax(speed_limit, push_task_.param_.MinDownloadSpeedInKBps, push_task_.param_.MaxDownloadSpeedInKBpsWhenIdle);
            PUSH_DEBUG(" USER_IDLE, SpeedLimit = " << speed_limit << ", BandWidth = " << statistic::StatisticModule::Inst()->GetBandWidth());
        }
        else if (push::USER_NONE == state_.user_) 
        {
            boost::uint32_t bandwidth = (std::max)(statistic::StatisticModule::Inst()->GetBandWidth(), (boost::uint32_t)64 * 1024);
            speed_limit = 1.0 * bandwidth * push_task_.param_.BandwidthRatioWhenNormal / (255 * 1024);
            LimitMinMax(speed_limit, push_task_.param_.MinDownloadSpeedInKBps, push_task_.param_.MaxDownloadSpeedInKBpsWhenNormal);
            PUSH_DEBUG(" USER_NONE, SpeedLimit = " << speed_limit << ", BandWidth = " << statistic::StatisticModule::Inst()->GetBandWidth());
        }

        // herain:有全局限速的时候受全局限速的限制
        if (global_speed_limit_in_kbps_ >= 0)
        {
            LIMIT_MAX(speed_limit, global_speed_limit_in_kbps_);
        }

        ProxyModule::Inst()->LimitDownloadSpeedInKBps(push_task_.url_, speed_limit);
    }

    string PushModule::GetCurrentTaskUrl() const
    {
        if (false == is_running_) {
            return string();
        }
        string url;
        if (push::TASK_HAVE == state_.task_) {
            url = push_task_.url_;
        }
        return url;
    }

    protocol::RidInfo PushModule::GetCurrentTaskRidInfo() const
    {
        protocol::RidInfo rid_info;
        if (false == is_running_) {
            return rid_info;
        }
        if (push::TASK_HAVE == state_.task_) {
            rid_info = push_task_.rid_info_;
        }
        return rid_info;
    }

    void PushModule::ReportPushTaskCompete(const protocol::RidInfo & rid_info)
    {
        if (state_.task_ == push::TASK_HAVE)
        {
            // herian:2011-6-22:下面的assert在push开关被频繁关闭打开的情况下是可能被触发的
            assert(rid_info == push_task_.rid_info_);
            if (rid_info == push_task_.rid_info_)
            {
                state_.task_ = push::TASK_COMPELETING;
                task_complete_error_num_ = 0;
            }
            
            SendReportPushTaskCompletePacket(rid_info);
        }
        else
        {
            assert(state_.task_ == push::TASK_DISABLE);
        }
    }

    void PushModule::SendReportPushTaskCompletePacket(const protocol::RidInfo & ridinfo)
    {
        protocol::ReportPushTaskCompletedPacket report_packet(
            protocol::Packet::NewTransactionID(), AppModule::Inst()->GetUniqueGuid(), 
            ridinfo, push_server_endpoint_);
        AppModule::Inst()->DoSendPacket(report_packet);
        task_complete_timer_.reset();
    }

    void PushModule::OnUdpRecv(const protocol::ServerPacket & packet)
    {
        switch(packet.PacketAction)
        {
        case protocol::QueryPushTaskPacket::Action:
            OnPushTaskResponse((protocol::QueryPushTaskPacket const &)packet);
            break;
        case protocol::ReportPushTaskCompletedPacket::Action:
            OnReportPushTaskCompleteResponse((protocol::ReportPushTaskCompletedPacket const &)packet);
            break;
        default:
            break;
        }
    }

    void PushModule::OnReportPushTaskCompleteResponse(protocol::ReportPushTaskCompletedPacket const & packet)
    {
        // herian:2011-6-22:下面两处assert在push开关被频繁关闭打开的情况下是可能被触发的
        assert(state_.task_ == push::TASK_COMPELETING);
        assert(packet.response.rid_info_ == push_task_.rid_info_);
        if (packet.response.rid_info_ == push_task_.rid_info_)
        {
            state_.task_ = push::TASK_NONE;
        }
    }

    void PushModule::SetGlobalSpeedLimitInKBps(boost::int32_t speed_limit_in_KBps)
    {
        global_speed_limit_in_kbps_ = speed_limit_in_KBps;
    }

    void PushModule::CheckUsePush()
    {
        bool use_push = BootStrapGeneralConfig::Inst()->UsePush();
        if (state_.task_ != push::TASK_DISABLE && !use_push)
        {
            state_.task_ = push::TASK_DISABLE;
            DebugLog("CheckUsePush use push no");
        }
        else if (state_.task_ == push::TASK_DISABLE && use_push)
        {
            state_.task_ = push::TASK_NONE;
            DebugLog("CheckUsePush use push yes");
        }
    }
#endif
}
