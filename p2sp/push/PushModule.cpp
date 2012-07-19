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
#include "p2sp/stun/StunModule.h"

#include <cmath>

#ifdef BOOST_WINDOWS_API
#include <TlHelp32.h>
#endif

static const std::string PUSH_SERVER = "ppvaps.pplive.com";
static const boost::uint16_t PUSH_SERVER_PORT = 6900;
static const std::string PLAY_HISTORY_FILE = "playhistory.dat"; 
static const boost::uint16_t PUSH_SERVER_PROCESS_NUM = 4;

namespace p2sp
{
#ifdef DISK_MODE
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_push = log4cplus::Logger::getInstance("[push_module]");
#endif
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
            TASK_WAIT,               
            TASK_QUERY,             
            TASK_QUERY_RETRY,
            TASK_HAVE,               
            TASK_COMPELETING,
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
    static const int32_t QueryPushTaskWaitIntervalInSec = 3600; //60 minutes
    static const int32_t FirstTimeQueryPushTaskWaitIntervalInSec = 60;//1 minute

    static const int32_t ReportPushTaskCompeteTimeOutInMs = 5000;
    static const int32_t MaxDownloadSpeedInKBS = 40;//40K
    static const int32_t MaxPlayHistoryNum = 16;
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
        , query_sum_num_(0)
        , task_complete_error_num_(0)
        , global_speed_limit_in_kbps_(-1)
        , push_wait_interval_in_sec_(QueryPushTaskWaitIntervalInSec)
    {
    }

    void PushModule::Start(const std::string& history_path)
    {
        if (true == is_running_) {
            return;
        }

        LOG4CPLUS_DEBUG_LOG(logger_push, "Start");

        is_running_ = true;

        push_server_domain_ = PUSH_SERVER;
        srand(time(0));
        push_server_port_ = PUSH_SERVER_PORT + rand()%PUSH_SERVER_PROCESS_NUM;

        state_.domain_ = push::DOMAIN_NONE;

        task_param_.ProtectTimeIntervalInSeconds = P2SPConfigs::PUSH_PROTECTION_INTERVAL_IN_SEC;

        boost::filesystem::path path(history_path);
        path /= PLAY_HISTORY_FILE;

        play_history_mgr_ = PlayHistoryManager::create(path.file_string());
        play_history_mgr_->LoadFromFile();

        timer_.start();
        last_query_timer_.start(); 
    }

    void PushModule::Stop()
    {
        if (false == is_running_) {
            return;
        }

        timer_.stop();
        last_query_timer_.stop();

        if (push_download_task_) {
            push_download_task_->Stop();
            push_download_task_.reset();
        }

        if (play_history_mgr_) {
            play_history_mgr_->SaveToFile();
            play_history_mgr_.reset();
        }

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

        LOG4CPLUS_DEBUG_LOG(logger_push, "ServerEndpoint = " << push_server_endpoint_);

        DoQueryPushTask();
    }

    void PushModule::OnResolverFailed(boost::uint32_t error_code)
    {
        if (false == is_running_) {
            return;
        }

        LOG4CPLUS_DEBUG_LOG(logger_push, "resolve dns failed");
        state_.domain_ = push::DOMAIN_NONE;
    }

    bool PushModule::IsDownloadingPushTask() const
    {
        return push_download_task_;
    }

    bool PushModule::HavePushTask() const
    {
        return push_task_deq_.size() > 0;
    }

    bool PushModule::ShouldQueryPushServer() const
    {
        BOOST_ASSERT(last_query_timer_.running());
        
        if (query_sum_num_ == 0) {//first time
            return last_query_timer_.elapsed() > FirstTimeQueryPushTaskWaitIntervalInSec * 1000;
        }
        else {
            return last_query_timer_.elapsed() > (uint32_t)push_wait_interval_in_sec_ * 1000;
        }
    }

    void PushModule::OnTimerElapsed(framework::timer::Timer * timer_ptr)
    {
        if (false == is_running_) {
            return;
        }

        if (timer_ptr != &timer_) 
            return;

        if (!BootStrapGeneralConfig::Inst()->UsePush()) {
            return;
        }

        if (IsDownloadingPushTask()) {
            BOOST_ASSERT(push_download_task_);
            if (p2sp::ProxyModule::Inst()->IsWatchingMovie()) {
                //if user is watching movie we should pause push download task
                push_task_deq_.push_front(push_download_task_->GetPushTaskParam());
                push_download_task_->Stop();
                push_download_task_.reset();
                return;
            }

            if (push_download_task_->IsTaskTerminated()) {
                //current push download task terminate
                push_download_task_->Stop();
                push_download_task_.reset();
                StartADownloadTask();//try to create a new push task
            }
            else {
                LimitTaskSpeed();
            }
        }

        if (!IsDownloadingPushTask()) {
            if (!HavePushTask() && ShouldQueryPushServer()) {
                //request push task from push server
                last_query_timer_.reset();   //reset
                query_sum_num_++;
                DoQueryPushTask();
            }
            else if (HavePushTask() && !p2sp::ProxyModule::Inst()->IsWatchingMovie()){
                //if there is push task and user is not watching movie, start a push task
                StartADownloadTask();
            }
        }
    }

    void PushModule::DoQueryPushTask()
    {
        if (false == is_running_) {
            return;
        }

        state_.task_ = push::TASK_QUERY;
        if (push::DOMAIN_HAVE == state_.domain_) {
            LOG4CPLUS_DEBUG_LOG(logger_push, "DOMAIN_HAVE, QueryTask; Server = " << push_server_endpoint_);
            SendQueryPushTaskPacket();
        }
        else if (push::DOMAIN_NONE == state_.domain_) {
            // resolve
            LOG4CPLUS_DEBUG_LOG(logger_push, "DOMAIN_NONE, ResolveDomain");
            DoResolvePushServer();
        }
        else if (push::DOMAIN_RESOLVE == state_.domain_) {
            LOG4CPLUS_DEBUG_LOG(logger_push, "Push::DOMAIN_RESOLVE");
        }
        else {
            BOOST_ASSERT(!"Invalid Domain State");
        }
    }

    void PushModule::SendQueryPushTaskPacket()
    {
        last_transaction_id_ = protocol::Packet::NewTransactionID();

        protocol::QueryPushTaskPacketV3 query_push_task_request(last_transaction_id_, AppModule::Inst()->GetUniqueGuid(), 
            push_server_endpoint_, 
            storage::Storage::Inst()->GetUsedDiskSpace(), 
            P2PModule::Inst()->GetUploadBandWidthInKBytes(), 
            statistic::StatisticModule::Inst()->GetRecentMinuteUploadDataSpeedInKBps(), 
            storage::Storage::Inst()->GetStoreSize(), 
            statistic::StatisticModule::Inst()->GetOnlinePercent(), 
            p2sp::StunModule::Inst()->GetPeerNatType());
        
        //get local play history and convert it to the one server can handle
        const std::deque<LocalPlayHistoryItem>& play_history_vec = play_history_mgr_->GetPlayHistory();
        LocalHistoryConverter::Convert(play_history_vec.begin(), play_history_vec.end(), 
            std::back_inserter(query_push_task_request.request.play_history_vec_));
        
        //limit max play history num
        if (query_push_task_request.request.play_history_vec_.size() > MaxPlayHistoryNum) {
            query_push_task_request.request.play_history_vec_.resize(MaxPlayHistoryNum);
        }

        //send it to server
        AppModule::Inst()->DoSendPacket(query_push_task_request);
    }

    void PushModule::DoResolvePushServer()
    {
        if (false == is_running_) {
            return;
        }

        LOG4CPLUS_DEBUG_LOG(logger_push, "PushServer = " << push_server_domain_ << ":" << push_server_port_);
        state_.domain_ = push::DOMAIN_RESOLVE;
        network::Resolver::p resolver =
            network::Resolver::create(global_io_svc(), push_server_domain_, push_server_port_, shared_from_this());
        resolver->DoResolver();
    }

    void PushModule::OnPushTaskResponse(protocol::QueryPushTaskPacketV3 const & response)
    {
        if (false == is_running_) {
            return;
        }

        if (response.transaction_id_ == last_transaction_id_) {
            if (response.error_code_ == 0) {
                //save control parameter
                task_param_ = response.response.response_push_task_param_;

                //save push task in push_task_deq_
                std::copy(response.response.push_task_vec_.begin(), response.response.push_task_vec_.end(), 
                    std::back_inserter(push_task_deq_));
            }
            else if(response.error_code_ == protocol::QueryPushTaskPacket::NO_TASK) {
                push_wait_interval_in_sec_ = response.response.push_wait_interval_in_sec_;
            }
        }
        else {
            LOG4CPLUS_DEBUG_LOG(logger_push, "InvalidTransID From " << response.end_point << ", TransID " << 
                response.transaction_id_);
        }
    }

    void PushModule::StartADownloadTask()
    {
        if (false == is_running_) {
            return;
        }
        
        if (push_task_deq_.size() == 0) {
            return;
        }

        if (push_download_task_) {
            return;
        }

        state_.task_ = push::TASK_HAVE;

        push_download_task_ = PushDownloadTask::create(global_io_svc(), push_task_deq_[0]);
        push_task_deq_.pop_front();
        push_download_task_->Start();

        LimitTaskSpeed();
    }

    void PushModule::LimitTaskSpeed()
    {
        if (false == is_running_) {
            return;
        }

        if (!push_download_task_) {
            return;
        }

        uint32_t bandwidth = std::max(statistic::StatisticModule::Inst()->GetBandWidth(), (uint32_t)64 * 1024);
        int speed_limit;

        BOOST_ASSERT(push::USER_NONE == state_.user_);
        speed_limit = 1.0 * bandwidth * task_param_.BandwidthRatioWhenNormal / (255 * 1024);
        LimitMinMax(speed_limit, task_param_.MinDownloadSpeedInKBps, task_param_.MaxDownloadSpeedInKBpsWhenNormal);
        LOG4CPLUS_DEBUG_LOG(logger_push, " USER_NONE, SpeedLimit = " << speed_limit << ", BandWidth = " << 
            statistic::StatisticModule::Inst()->GetBandWidth());
       
        if (global_speed_limit_in_kbps_ >= 0) {
            LIMIT_MAX(speed_limit, global_speed_limit_in_kbps_);
        }

        LIMIT_MAX(speed_limit, MaxDownloadSpeedInKBS);
        push_download_task_->LimitSpeed(speed_limit);
    }

    void PushModule::OnUdpRecv(const protocol::ServerPacket & packet)
    {
        switch(packet.PacketAction) {
        case protocol::QueryPushTaskPacketV3::Action:
            OnPushTaskResponse((protocol::QueryPushTaskPacketV3 const &)packet);
            break;
        default:
            break;
        }
    }

    void PushModule::SetGlobalSpeedLimitInKBps(boost::int32_t speed_limit_in_KBps)
    {
        global_speed_limit_in_kbps_ = speed_limit_in_KBps;
    }

#endif
}
