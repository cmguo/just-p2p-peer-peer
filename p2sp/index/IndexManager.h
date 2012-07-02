//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// IndexManager.h

#ifndef _P2SP_INDEX_INDEX_MANAGER_H_
#define _P2SP_INDEX_INDEX_MANAGER_H_

#include "network/Resolver.h"
#include "protocol/BootstrapPacket.h"
#include "protocol/IndexPacket.h"
#include "network/Resolver.h"

namespace p2sp
{
    class IndexManager
        : public boost::enable_shared_from_this<IndexManager>
        , public network::IResolverListener
#ifdef DUMP_OBJECT
        , public count_object_allocate<IndexManager>
#endif
    {
        friend class AppModule;
    public:
        typedef boost::shared_ptr<IndexManager> p;

        /**
        * @brief 启动该模块
        */

        void Start(string domain, boost::uint16_t port);

        /**
        * @brief 停止该模块
        */
        void Stop();

        void DoQueryVodListTrackerList();

        void DoQueryVodReportTrackerList();

        void DoQueryLiveListTrackerList();

        void DoQueryLiveReportTrackerList();

        void DoQueryStunServerList();

#ifdef NOTIFY_ON
        void DoQueryNotifyServerList();
#endif

        void DoQueryBootStrapConfig();

        void DoQuerySnList();

        void DoQueryVipSnList();

        void OnUdpRecv(protocol::ServerPacket const & packet_header);

    public:
        // 接口
        virtual void OnResolverSucced(uint32_t ip, boost::uint16_t port);
        virtual void OnResolverFailed(uint32_t error_code);  // 1-Url有问题 2-域名无法解析 3-域名解析出错 4-连接失败

    protected:
        void OnQueryVodListTrackerListPacket(const protocol::QueryTrackerForListingPacket & packet);

        void OnQueryVodReportTrackerListPacket(protocol::QueryTrackerListPacket const & packet);

        void OnQueryLiveListTrackerListPacket(const protocol::QueryTrackerForListingPacket & packet);

        void OnQueryLiveReportTrackerListPacket(protocol::QueryLiveTrackerListPacket const & packet);

        void OnQueryStunServerListPacket(protocol::QueryStunServerListPacket const & packet);

        void OnQueryBootStrapConfigPacket(protocol::QueryConfigStringPacket const & packet);

        void OnQuerySnListPacket(protocol::QuerySnListPacket const & packet);

        void OnQueryVipSnListPacket(protocol::QueryVipSnListPacket const & packet);

    protected:

        void OnTimerElapsed(
            framework::timer::Timer * pointer);

        void OnQueryVodListTrackerListTimerElapsed(uint32_t times);

        void OnQueryVodReportTrackerListTimerElapsed(uint32_t times);

        void OnQueryLiveListTrackerListTimerElapsed(uint32_t times);

        void OnQueryLiveReportTrackerListTimerElapsed(uint32_t times);

        void OnQueryStunServerListTimerElapsed(uint32_t times);

#ifdef NOTIFY_ON
        void OnQueryNotifyServerTimerElapsed(boost::uint32_t times);
#endif
        void OnQueryBootStrapConfigTimerElapsed(boost::uint32_t times);

        void OnQuerySnListTimerElapsed(boost::uint32_t times);
        void OnQueryVipSnListTimerElapsed(boost::uint32_t times);

    private:
        boost::asio::io_service & io_svc_;

        bool is_have_vod_list_tracker_list_;
        bool is_have_vod_report_tracker_list_;

        bool is_have_live_list_tracker_list_;
        bool is_have_live_report_tracker_list_;

        bool is_have_stun_server_list_;
        bool is_have_change_domain_;
        bool is_resolving_;
        bool is_have_notify_server_;
        
        bool is_have_bootstrap_config_;
        bool is_have_sn_list_;
        bool is_have_vip_sn_list_;

        framework::timer::OnceTimer query_vod_list_tracker_list_timer_;
        framework::timer::OnceTimer query_vod_report_tracker_list_timer_;

        framework::timer::OnceTimer query_live_list_tracker_list_timer_;
        framework::timer::OnceTimer query_live_report_tracker_list_timer_;

        framework::timer::OnceTimer change_domain_resolver_timer_;
        framework::timer::OnceTimer query_stun_server_list_timer_;
#ifdef NOTIFY_ON
        framework::timer::OnceTimer query_notify_server_list_timer_;
#endif
        framework::timer::OnceTimer query_bootstrap_config_timer_;
        framework::timer::OnceTimer query_sn_list_timer_;
        framework::timer::OnceTimer query_vip_sn_list_timer_;

        volatile bool is_running_;

        boost::asio::ip::udp::endpoint server_list_endpoint_;

        uint32_t last_query_vod_list_tracker_list_interval_;
        uint32_t last_query_vod_report_tracker_list_interval_;
        uint32_t last_querystunlist_intervaltimes_;
        uint32_t last_queryindexlist_intervaltimes_;
#ifdef NOTIFY_ON
        uint32_t last_querynotifyserverlist_intervaltimes_;
#endif
        uint32_t last_query_live_list_tracker_list_interval_;
        uint32_t last_query_live_report_tracker_list_interval_;
        uint32_t last_query_bootstrap_config_interval_times_;
        uint32_t last_query_sn_list_interval_times_;
        uint32_t last_query_vip_sn_list_interval_times_;

        // Resolver
        network::Resolver::p resolver_;

        string domain_;
        boost::uint16_t port_;
        string boss_ip_;
        string boss_domain_;
        boost::uint16_t boss_port_;
        boost::uint16_t failed_times_;
        boost::uint16_t resolve_times_;

    private:
        static IndexManager::p inst_;
        IndexManager(
            boost::asio::io_service & io_svc);

    private:
        static const uint32_t INITIAL_QUERY_INTERVAL = 15*1000;
        static const uint32_t DEFAULT_QUERY_INTERVAL = 4 * 60 * 60*1000U;
        static const uint32_t DEFAULT_QUERY_BOOTSTRAP_CONFIG_INTERVAL = 60 * 60*1000U;

    public:
        static IndexManager::p CreateInst(
            boost::asio::io_service & io_svc)
        {
            inst_.reset(new IndexManager(io_svc));
            return inst_;
        }

        static IndexManager::p Inst() { return inst_; };
    };
}

#endif  // _P2SP_INDEX_INDEX_MANAGER_H_
