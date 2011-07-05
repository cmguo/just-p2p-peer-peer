//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef P2SP_PUSH_PUSHMODULE_H
#define P2SP_PUSH_PUSHMODULE_H

#include "protocol/PushServerPacket.h"
#include "network/Resolver.h"

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
        void Start();
        void Stop();
        bool IsRunning() const { return is_running_; }
        bool IsPushEnabled() const { return is_push_enabled_; }
        void EnablePush(bool enable_push) { is_push_enabled_ = enable_push; }

        void OnUdpRecv(const protocol::ServerPacket & packet);

        void OnPushTaskResponse(protocol::QueryPushTaskPacket const & packet);
        void OnReportPushTaskCompleteResponse(protocol::ReportPushTaskCompletedPacket const & packet);

        void OnTimerElapsed(framework::timer::Timer * timer_ptr);

        virtual void OnResolverSucced(uint32_t ip, boost::uint16_t port);
        virtual void OnResolverFailed(uint32_t error_code);

        string GetCurrentTaskUrl() const;
        protocol::RidInfo GetCurrentTaskRidInfo() const;

        void ReportPushTaskCompete(const protocol::RidInfo & rid_info);

        void SetGlobalSpeedLimitInKBps(boost::int32_t speed_limit_in_KBps);

    private:
        void CheckDiskSpace();
        void CheckUserState();
        void DoResolvePushServer();
        void DoQueryPushTask();

        void StartCurrentTask();
        void LimitTaskSpeed();

        uint32_t GetUserState() const;

        void SendReportPushTaskCompletePacket(const protocol::RidInfo & ridinfo);
        void CheckUsePush();

    private:
        static PushModule::p inst_;
    private:
        PushModule();
    private:

        struct State {
            uint32_t user_;
            uint32_t task_;
            uint32_t disk_;
            uint32_t domain_;

            State();
        } state_;

        volatile bool is_running_;
        volatile bool is_push_enabled_;

        uint32_t last_transaction_id_;
        framework::timer::TickCounter last_query_timer_;
        int32_t query_error_num_;
        protocol::PushTask push_task_;

        boost::asio::ip::udp::endpoint push_server_endpoint_;

        string push_server_domain_;
        boost::uint16_t push_server_port_;

        framework::timer::PeriodicTimer timer_;
        framework::timer::TickCounter task_complete_timer_;
        int32_t task_complete_error_num_;

        int32_t global_speed_limit_in_kbps_;
        int32_t push_wait_interval_in_sec_;
    };

#endif
}

#endif  // P2SP_PUSH_PUSHMODULE_H
