//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef P2SP_PUSH_PUSHMODULE_H
#define P2SP_PUSH_PUSHMODULE_H

#include "protocol/PushServerPacket.h"
#include "protocol/PushServerPacketV2.h"
#include "network/Resolver.h"
#include "PushDownloadTask.h"
#include "PlayHistoryManager.h"

#include <deque>

namespace p2sp
{
#ifdef DISK_MODE

    class PushModule
        : public boost::noncopyable
        , public boost::enable_shared_from_this<PushModule>
        , public network::IResolverListener
    {
    public:
        typedef boost::shared_ptr<PushModule> p;
        static PushModule::p Inst() { return inst_; }

    public:
        void Start(const std::string& history_path);
        void Stop();
        bool IsRunning() const { return is_running_; }
        bool IsPushEnabled() const { return is_push_enabled_; }
        void EnablePush(bool enable_push) { is_push_enabled_ = enable_push; }
        
        boost::shared_ptr<PlayHistoryManager> GetPlayHistoryManager() { return play_history_mgr_; }

        void OnUdpRecv(const protocol::ServerPacket & packet);
        void OnPushTaskResponse(protocol::QueryPushTaskPacketV3 const & packet);
        void OnTimerElapsed(framework::timer::Timer * timer_ptr);

        virtual void OnResolverSucced(boost::uint32_t ip, boost::uint16_t port);
        virtual void OnResolverFailed(boost::uint32_t error_code);

        void SetGlobalSpeedLimitInKBps(boost::int32_t speed_limit_in_KBps);

    private:
        void DoResolvePushServer();
        void DoQueryPushTask();
        void SendQueryPushTaskPacket();

        bool IsDownloadingPushTask() const;
        bool HavePushTask() const;
        
        bool ShouldQueryPushServer() const;

        void StartADownloadTask();
        void LimitTaskSpeed();
        
    private:
        PushModule();

    private:

        struct State {
            boost::uint32_t user_;
            boost::uint32_t task_;
            boost::uint32_t disk_;
            boost::uint32_t domain_;

            State();
        } state_;

        volatile bool is_running_;
        volatile bool is_push_enabled_;

        boost::uint32_t last_transaction_id_;
        framework::timer::TickCounter last_query_timer_;
        boost::int32_t query_sum_num_;

        boost::asio::ip::udp::endpoint push_server_endpoint_;

        string push_server_domain_;
        boost::uint16_t push_server_port_;

        framework::timer::PeriodicTimer timer_;
        
        boost::int32_t task_complete_error_num_;

        boost::int32_t global_speed_limit_in_kbps_;
        boost::int32_t push_wait_interval_in_sec_;

        boost::shared_ptr<PushDownloadTask> push_download_task_;

        protocol::PUSH_TASK_PARAM task_param_;
        std::deque<protocol::PushTaskItem> push_task_deq_;

        boost::shared_ptr<PlayHistoryManager> play_history_mgr_;

        static PushModule::p inst_;
    };

#endif
}

#endif  // P2SP_PUSH_PUSHMODULE_H
