//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/index/IndexManager.h"
#include "p2sp/AppModule.h"
#include "p2sp/tracker/TrackerModule.h"
#include "p2sp/stun/StunModule.h"
#include "p2sp/bootstrap/BootStrapGeneralConfig.h"
#include "network/Uri.h"
#include "p2sp/p2p/SNPool.h"

#ifdef NOTIFY_ON
#include "p2sp/notify/NotifyModule.h"
#endif

#include "statistic/DACStatisticModule.h"
#include "statistic/StatisticModule.h"
#include "message.h"

using namespace statistic;
using namespace network;
using namespace protocol;

namespace p2sp
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_index = log4cplus::Logger::getInstance("[index]");
#endif

    IndexManager::p IndexManager::inst_;

    IndexManager::IndexManager(
        boost::asio::io_service & io_svc)
        : io_svc_(io_svc)
        , is_have_vod_list_tracker_list_(false)
        , is_have_vod_report_tracker_list_(false)
        , is_have_live_list_tracker_list_(false)
        , is_have_live_report_tracker_list_(false)

        , is_have_stun_server_list_(false)
#ifdef NOTIFY_ON
        , is_have_notify_server_(false)
#endif
        , is_have_bootstrap_config_(false)
        , is_have_change_domain_(false)
        , is_resolving_(false)
        , is_have_sn_list_(false)

        , query_vod_list_tracker_list_timer_(global_second_timer(), INITIAL_QUERY_INTERVAL, boost::bind(&IndexManager::OnTimerElapsed, this, &query_vod_list_tracker_list_timer_))
        , query_vod_report_tracker_list_timer_(global_second_timer(), INITIAL_QUERY_INTERVAL, boost::bind(&IndexManager::OnTimerElapsed, this, &query_vod_report_tracker_list_timer_))
        , query_live_list_tracker_list_timer_(global_second_timer(), INITIAL_QUERY_INTERVAL, boost::bind(&IndexManager::OnTimerElapsed, this, &query_live_list_tracker_list_timer_))
        , query_live_report_tracker_list_timer_(global_second_timer(), INITIAL_QUERY_INTERVAL, boost::bind(&IndexManager::OnTimerElapsed, this, &query_live_report_tracker_list_timer_))
        , change_domain_resolver_timer_(global_second_timer(), 3000, boost::bind(&IndexManager::OnTimerElapsed, this, &change_domain_resolver_timer_))
        , query_stun_server_list_timer_(global_second_timer(), INITIAL_QUERY_INTERVAL, boost::bind(&IndexManager::OnTimerElapsed, this, &query_stun_server_list_timer_))
#ifdef NOTIFY_ON
        , query_notify_server_list_timer_(global_second_timer(), INITIAL_QUERY_INTERVAL, boost::bind(&IndexManager::OnTimerElapsed, this, &query_notify_server_list_timer_))
#endif
        , query_bootstrap_config_timer_(global_second_timer(), INITIAL_QUERY_INTERVAL, 
            boost::bind(&IndexManager::OnTimerElapsed, this, &query_bootstrap_config_timer_))
        , query_sn_list_timer_(global_second_timer(), INITIAL_QUERY_INTERVAL, 
            boost::bind(&IndexManager::OnTimerElapsed, this, &query_sn_list_timer_))
        , is_running_(false)
        , failed_times_(0)
        , resolve_times_(0)
    {

    }

    void IndexManager::OnResolverSucced(uint32_t ip, boost::uint16_t port)
    {
        if (is_running_ == false)
            return;

        LOG4CPLUS_INFO_LOG(logger_index, "Start OnResolverSucced ,index_end_point" << ip << " " << port);

        // IndexServer 的 IP 和 端口设置
        server_list_endpoint_ = boost::asio::ip::udp::endpoint(boost::asio::ip::address_v4(ip), port);
        protocol::SocketAddr index_socket(server_list_endpoint_);

        LOG4CPLUS_DEBUG_LOG(logger_index, "Resolve Succeed: ");

        statistic::StatisticModule::Inst()->SetBsInfo(server_list_endpoint_);
        statistic::StatisticModule::Inst()->SetIndexServerInfo(index_socket);
        is_resolving_ = false;
        if (false == is_have_vod_list_tracker_list_) DoQueryVodListTrackerList();
        if (false == is_have_vod_report_tracker_list_) DoQueryVodReportTrackerList();
        if (false == is_have_stun_server_list_) DoQueryStunServerList();
#ifdef NOTIFY_ON
        if (false == is_have_notify_server_) DoQueryNotifyServerList();
#endif
        if (false == is_have_live_list_tracker_list_)
        {
            DoQueryLiveListTrackerList();
        }

        if (false == is_have_live_report_tracker_list_)
        {
            DoQueryLiveReportTrackerList();
        }

        if (false == is_have_bootstrap_config_)
        {
            DoQueryBootStrapConfig();
        }

        if (false == is_have_sn_list_)
        {
            DoQuerySnList();
        }
    }

    void IndexManager::OnResolverFailed(uint32_t error_code)  // 1-Url锟斤拷锟斤拷锟斤拷 2-锟斤拷锟斤拷锟睫凤拷锟斤拷锟斤拷 3-锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷锟斤拷 4-锟斤拷锟绞э拷锟?
    {
        if (is_running_ == false) return;

        LOG4CPLUS_DEBUG_LOG(logger_index, "Resolve Failed: ");
        change_domain_resolver_timer_.stop();
        is_resolving_ = false;
        // 域名尚未切换至世纪互联
        if (false == is_have_change_domain_)
        {
            ++resolve_times_;
            bool need_change_domain = !(is_have_vod_report_tracker_list_
                || is_have_stun_server_list_);
            // 解析失败三次并且三个回包一个都没收到，将域名切换至世纪互联，再次解析，不开启定时器
            if (resolve_times_ >= 3 && need_change_domain)
            {
                if (resolver_) { resolver_->Close(); resolver_.reset(); }
                resolver_ = Resolver::create(io_svc_, boss_domain_, boss_port_, shared_from_this());
                is_have_change_domain_ = true;
                resolve_times_ = 0;
                resolver_->DoResolver();
                is_resolving_ = true;
            }
            else  // 否则，不改变域名再次解析，开启定时器
            {
                resolver_ = Resolver::create(io_svc_, domain_, port_, shared_from_this());
                resolver_->DoResolver();
                is_resolving_ = true;
                change_domain_resolver_timer_ .start();
            }
            failed_times_ = 0;
        }
        else  // 域名已经切换至世纪互联，此时解析失败，直接设置server_list的ip-port
        {
            boost::system::error_code error;
            boost::asio::ip::address_v4 addr = boost::asio::ip::address_v4::from_string(boss_ip_, error);
            server_list_endpoint_ = boost::asio::ip::udp::endpoint(addr, boss_port_);
            protocol::SocketAddr index_socket(server_list_endpoint_);
            statistic::StatisticModule::Inst()->SetIndexServerInfo(index_socket);
            DoQueryVodListTrackerList();
            DoQueryVodReportTrackerList();
            DoQueryLiveListTrackerList();
            DoQueryLiveReportTrackerList();
            DoQueryStunServerList();
#ifdef NOTIFY_ON
            DoQueryNotifyServerList();
#endif
            DoQuerySnList();
        }

        LOG4CPLUS_WARN_LOG(logger_index, "IndexManager::OnResolverFailed " << error_code);
    }

    void IndexManager::Start(string domain, boost::uint16_t port)
    {
        if (is_running_ == true) return;

        is_running_ = true;

        is_have_vod_list_tracker_list_ = false;
        is_have_vod_report_tracker_list_ = false;
        is_have_live_list_tracker_list_ = false;
        is_have_live_report_tracker_list_ = false;
        is_have_stun_server_list_ = false;
        is_have_change_domain_ = false;
#ifdef NOTIFY_ON
        is_have_notify_server_ = false;
#endif
        is_have_bootstrap_config_ = false;

        boss_domain_ = "ppvaindex.pplive.com";  // 世纪互联
        boss_ip_ = "60.28.216.149";
        boss_port_ = 6400;

        domain_ = domain;
        port_ = port;

        last_query_vod_list_tracker_list_interval_= INITIAL_QUERY_INTERVAL;
        last_query_vod_report_tracker_list_interval_ = INITIAL_QUERY_INTERVAL;
        last_querystunlist_intervaltimes_ = INITIAL_QUERY_INTERVAL;
        last_queryindexlist_intervaltimes_ = INITIAL_QUERY_INTERVAL;
#ifdef NOTIFY_ON
        last_querynotifyserverlist_intervaltimes_ = INITIAL_QUERY_INTERVAL;
#endif
        last_query_bootstrap_config_interval_times_ = INITIAL_QUERY_INTERVAL;
        last_query_live_list_tracker_list_interval_ = INITIAL_QUERY_INTERVAL;
        last_query_live_report_tracker_list_interval_ = INITIAL_QUERY_INTERVAL;

        last_query_sn_list_interval_times_ = INITIAL_QUERY_INTERVAL;

        resolver_ = Resolver::create(io_svc_, domain_, port_, shared_from_this());
        resolver_->DoResolver();
        is_resolving_ = true;
        failed_times_ = 0;

        change_domain_resolver_timer_.start();

    }

    void IndexManager::Stop()
    {
        LOG4CPLUS_INFO_LOG(logger_index, "Stop");

        if (is_running_ == false) return;

        // 停止定时器
        query_vod_list_tracker_list_timer_.stop();
        query_vod_report_tracker_list_timer_.stop();
        query_stun_server_list_timer_.stop();
#ifdef NOTIFY_ON
        query_notify_server_list_timer_.stop();
#endif

        change_domain_resolver_timer_.stop();

        query_sn_list_timer_.stop();

        query_live_list_tracker_list_timer_.stop();
        query_live_report_tracker_list_timer_.stop();

        if (resolver_) { resolver_->Close(); resolver_.reset(); }

        is_running_ = false;

        inst_.reset();
    }

    void IndexManager::DoQueryVodListTrackerList()
    {
        if (is_running_ == false)
        {
            return;
        }

        query_vod_list_tracker_list_timer_.interval(last_query_vod_list_tracker_list_interval_);
        query_vod_list_tracker_list_timer_.start();

        protocol::QueryTrackerForListingPacket packet(
            protocol::Packet::NewTransactionID(), protocol::PEER_VERSION,
            protocol::QueryTrackerForListingPacket::VOD_TRACKER_FOR_LISTING,
            server_list_endpoint_);

        AppModule::Inst()->DoSendPacket(packet);
    }

    void IndexManager::DoQueryLiveListTrackerList()
    {
        if (is_running_ == false)
        {
            return;
        }

        query_live_list_tracker_list_timer_.interval(last_query_live_list_tracker_list_interval_);
        query_live_list_tracker_list_timer_.start();

        protocol::QueryTrackerForListingPacket packet(
            protocol::Packet::NewTransactionID(), protocol::PEER_VERSION,
            protocol::QueryTrackerForListingPacket::LIVE_TRACKER_FOR_LISTING,
            server_list_endpoint_);

        AppModule::Inst()->DoSendPacket(packet);
    }

    void IndexManager::DoQueryVodReportTrackerList()
    {
        LOG4CPLUS_INFO_LOG(logger_index, "DoQueryTrackerList");

        if (is_running_ == false) return;

        query_vod_report_tracker_list_timer_.interval(last_query_vod_report_tracker_list_interval_);
        query_vod_report_tracker_list_timer_.start();


        // 直接发送 QueryTrackerListRequestPacket 包

        uint32_t transaction_id_ = protocol::Packet::NewTransactionID();

        Guid unique_guid = AppModule::Inst()->GetUniqueGuid();

        protocol::QueryTrackerListPacket query_tracker_list_request_packet(
            transaction_id_, protocol::PEER_VERSION, AppModule::Inst()->GetUniqueGuid(), server_list_endpoint_);

        AppModule::Inst()->DoSendPacket(query_tracker_list_request_packet);

        statistic::StatisticModule::Inst()->SubmitQueryTrackerListRequest();

    }

    void IndexManager::DoQueryStunServerList()
    {
        LOG4CPLUS_INFO_LOG(logger_index, "DoQueryStunServerList");

        if (is_running_ == false) return;

        query_stun_server_list_timer_.interval(last_querystunlist_intervaltimes_);
        query_stun_server_list_timer_.start();

        // 直接发送 QueryTrackerListRequestPacket 包

        uint32_t transaction_id_ = protocol::Packet::NewTransactionID();

        protocol::QueryStunServerListPacket query_stun_server_list_request_packet(transaction_id_, protocol::PEER_VERSION, server_list_endpoint_);
        AppModule::Inst()->DoSendPacket(query_stun_server_list_request_packet);
        // StatisticModule::Inst()->SubmitQueryTrackerListRequest();
    }

    void IndexManager::OnUdpRecv(protocol::ServerPacket const & packet)
    {
        if (is_running_ == false)
            return;

        switch (packet.PacketAction)
        {
        case protocol::QueryTrackerListPacket::Action:
            OnQueryVodReportTrackerListPacket((protocol::QueryTrackerListPacket const &)packet);
            break;
        case protocol::QueryStunServerListPacket::Action:
            OnQueryStunServerListPacket((protocol::QueryStunServerListPacket const &)packet);
            break;
        case protocol::QueryTrackerForListingPacket::Action:
            {
                const protocol::QueryTrackerForListingPacket & query_tracket_packet = 
                    (const protocol::QueryTrackerForListingPacket  &)packet;

                if (query_tracket_packet.tracker_type_ == protocol::QueryTrackerForListingPacket::VOD_TRACKER_FOR_LISTING)
                {
                    OnQueryVodListTrackerListPacket(query_tracket_packet);
                }
                else
                {
                    assert(query_tracket_packet.tracker_type_ == protocol::QueryTrackerForListingPacket::LIVE_TRACKER_FOR_LISTING);
                    OnQueryLiveListTrackerListPacket(query_tracket_packet);
                }
                break;
            }
#ifdef NOTIFY_ON
        case protocol::QueryNotifyListPacket::Action:
            {
                LOG4CPLUS_DEBUG_LOG(logger_index, "收到QueryNotifyListResponsePacket");

                is_have_notify_server_ = true;
                LOG4CPLUS_DEBUG_LOG(logger_index, "post OnGetNotifyServerList");
                global_io_svc().post(boost::bind(&p2sp::NotifyModule::OnGetNotifyServerList, NotifyModule::Inst(), (protocol::QueryNotifyListPacket const &)packet));
            }
            break;
#endif
        case protocol::QueryConfigStringPacket::Action:
            OnQueryBootStrapConfigPacket((protocol::QueryConfigStringPacket const &)packet);
            break;
        case protocol::QueryLiveTrackerListPacket::Action:
            OnQueryLiveReportTrackerListPacket((protocol::QueryLiveTrackerListPacket const &)packet);
            break;
        case protocol::QuerySnListPacket::Action:
            OnQuerySnListPacket((protocol::QuerySnListPacket const &)packet);
            break;
        default:
            assert(0);
        }
    }

    void IndexManager::OnQueryVodReportTrackerListPacket(protocol::QueryTrackerListPacket const & packet)
    {
        LOG4CPLUS_INFO_LOG(logger_index, "IndexManager::OnQueryTrackerListPacket");

        if (is_running_ == false) return;

        statistic::StatisticModule::Inst()->SubmitQueryTrackerListResponse();


        // 判断时都出错
        if (packet.error_code_ == 0)
        {
            // 成功收到服务器回复，定时器时间设置为比较长的时间
            query_vod_report_tracker_list_timer_.interval(DEFAULT_QUERY_INTERVAL);
            query_vod_report_tracker_list_timer_.start();
            is_have_vod_report_tracker_list_ = true;
            failed_times_ = 0;
            resolve_times_ = 0;
            last_query_vod_report_tracker_list_interval_ = INITIAL_QUERY_INTERVAL;

            // 根据返回情况调用
            // 从packet中, 解出每个 TrackerInfo 到 tracker_vector
            // TrackerModule::Inst()->SetTrackerList(group_count, tracker_vector);
            for (uint32_t i = 0; i < packet.response.tracker_info_.size(); i ++)
            {
                LOG4CPLUS_INFO_LOG(logger_index, "Tracker List :[" << i << "] ModNo:" << 
                    packet.response.tracker_info_[i].ModNo << " IP: " << packet.response.tracker_info_[i].IP);
            }
            TrackerModule::Inst()->SetTrackerList(packet.response.tracker_group_count_, packet.response.tracker_info_, true,  p2sp::REPORT);
        }
        else
        {
            LOG4CPLUS_INFO_LOG(logger_index, "IndexManager::OnQueryTrackerListPacketERROR");
            LOG4CPLUS_ERROR_LOG(logger_index, "QueryTList Failed.");
        }
    }

    void IndexManager::OnQueryLiveReportTrackerListPacket(protocol::QueryLiveTrackerListPacket const & packet)
    {
        LOG4CPLUS_INFO_LOG(logger_index, "IndexManager::OnQueryLiveTrackerListPacket");

        if (is_running_ == false)
        {
            return;
        }

        // 判断时都出错
        if (packet.error_code_ == 0)
        {
            // 成功收到服务器回复，定时器时间设置为比较长的时间
            query_live_report_tracker_list_timer_.interval(DEFAULT_QUERY_INTERVAL);
            query_live_report_tracker_list_timer_.start();
            is_have_live_report_tracker_list_ = true;
            failed_times_ = 0;
            resolve_times_ = 0;
            last_query_live_report_tracker_list_interval_ = INITIAL_QUERY_INTERVAL;

            // 根据返回情况调用
            // 从packet中, 解出每个 TrackerInfo 到 tracker_vector

            for (uint32_t i = 0; i < packet.response.tracker_info_.size(); i ++)
            {
                LOG4CPLUS_INFO_LOG(logger_index, "Tracker List :[" << i << "] ModNo:" << 
                    packet.response.tracker_info_[i].ModNo << " IP: " << packet.response.tracker_info_[i].IP);
            }

            TrackerModule::Inst()->SetTrackerList(packet.response.tracker_group_count_, packet.response.tracker_info_, false, p2sp::REPORT);
        }
        else
        {
            LOG4CPLUS_INFO_LOG(logger_index, "IndexManager::OnQueryTrackerListPacketERROR");
            LOG4CPLUS_ERROR_LOG(logger_index, "QueryTList Failed.");
        }
    }

    void IndexManager::OnQueryBootStrapConfigPacket(protocol::QueryConfigStringPacket const & packet)
    {
        LOG4CPLUS_INFO_LOG(logger_index, "IndexManager::OnQueryBootStrapConfigPacket");

        if (is_running_ == false) return;

        // 判断时都出错
        if (packet.error_code_ == 0)
        {
            // 成功收到服务器回复，定时器时间设置为比较长的时间:1小时
            query_bootstrap_config_timer_.interval(DEFAULT_QUERY_BOOTSTRAP_CONFIG_INTERVAL);
            query_bootstrap_config_timer_.start();
            is_have_bootstrap_config_ = true;
            failed_times_ = 0;
            resolve_times_ = 0;
            last_query_bootstrap_config_interval_times_ = INITIAL_QUERY_INTERVAL;

            BootStrapGeneralConfig::Inst()->SetConfigString(packet.response.config_string_, true);
        }
        else
        {
            assert(false);
        }
    }

    void IndexManager::OnTimerElapsed(framework::timer::Timer * pointer)
    {
        if (is_running_ == false) return;
        uint32_t times = pointer->times();
        LOG4CPLUS_WARN_LOG(logger_index, "IndexManager::OnTimerElapsed");

        if (pointer == &query_vod_list_tracker_list_timer_)
        {
            OnQueryVodListTrackerListTimerElapsed(times);
        }
        if (pointer == &query_vod_report_tracker_list_timer_)
        {   // QueryTrackerList 定时器出发
            OnQueryVodReportTrackerListTimerElapsed(times);
        }
        else if (pointer == &query_stun_server_list_timer_)
        {   // QueryTrackerList 定时器出发
            OnQueryStunServerListTimerElapsed(times);
        }
#ifdef NOTIFY_ON
        else if (pointer == &query_notify_server_list_timer_)
        {
            OnQueryNotifyServerTimerElapsed(times);
        }
#endif
        else if (pointer == &query_live_list_tracker_list_timer_)
        {
            OnQueryLiveListTrackerListTimerElapsed(times);
        }
        else if (pointer == &query_live_report_tracker_list_timer_)
        {
            OnQueryLiveReportTrackerListTimerElapsed(times);
        }
        else if (pointer == &query_bootstrap_config_timer_)
        {
            OnQueryBootStrapConfigTimerElapsed(times);
        }
        else if (pointer == &change_domain_resolver_timer_)
        {
            LOG4CPLUS_WARN_LOG(logger_index, "IndexManager::OnTimerElapsed DoResolver");

            assert(resolver_);
            // 收包失败，需要再次解析或者切换域名(如果已经切换域名，则不该有此定时器)
            if (failed_times_ >= 3 && false == is_have_change_domain_)
            {
                bool need_change_domain = !(is_have_vod_report_tracker_list_
                    || is_have_stun_server_list_);
                ++resolve_times_;

                // 解析三次都收包失败，并且三个回包一个都没收到，切换域名，关闭定时器
                if (resolve_times_ >= 3 && need_change_domain)
                {
                    if (resolver_) { resolver_->Close(); resolver_.reset(); }
                    resolver_ = Resolver::create(io_svc_, boss_domain_, port_, shared_from_this());
                    is_have_change_domain_ = true;
                    resolve_times_ = 0;
                    change_domain_resolver_timer_.stop();
                }
                else    // 不改变域名，再次尝试解析，开启一次性定时器
                {
                    change_domain_resolver_timer_.start();
                    failed_times_ = 0;
                }
                // 锟劫次斤拷锟斤拷
                resolver_->DoResolver();
                is_resolving_ = true;
                LOG4CPLUS_INFO_LOG(logger_index, "Resolve again.");
            }
            else    // 不需要切换域名，也不需要解析，只是开启一次性定时器
            {
                change_domain_resolver_timer_.start();
            }
        }
        else if (pointer == &query_sn_list_timer_)
        {
            OnQuerySnListTimerElapsed(times);
        }
        else
        {    // 本类中不存在这个定时器
            // assert(!"No Such framework::timer::Timer");
            LOG4CPLUS_WARN_LOG(logger_index, "IndexManager::OnTimerElapsed No Such framework::timer::Timer, Ignored");
        }
    }

    void IndexManager::OnQueryStunServerListPacket(protocol::QueryStunServerListPacket const & packet)
    {
        LOG4CPLUS_INFO_LOG(logger_index, "IndexManager::OnQueryStunServerListPacket");

        if (is_running_ == false) return;

        // StatisticModule::Inst()->SubmitQueryTrackerListResponse();

        // 判断时都出错
        if (packet.error_code_ == 0)
        {
            // 成功收到服务器回复，定时器时间设置为比较长的时间
            query_stun_server_list_timer_.interval(DEFAULT_QUERY_INTERVAL);  // 定义成为4个小时
            query_stun_server_list_timer_.start();
            is_have_stun_server_list_ = true;
            failed_times_ = 0;
            resolve_times_ = 0;
            last_querystunlist_intervaltimes_ = INITIAL_QUERY_INTERVAL;

            // 根据返回情况调用
            // 从packet中, 解出每个 TrackerInfo 到 tracker_vector
            // TrackerModule::Inst()->SetTrackerList(group_count, tracker_vector);
            StunModule::Inst()->SetStunServerList(packet.response.stun_infos_);
        }
        else
        {
            // 不管
            LOG4CPLUS_INFO_LOG(logger_index, "IndexManager::OnQueryTrackerListPacketERROR");
            LOG4CPLUS_ERROR_LOG(logger_index, "QuerySList Failed.");
        }

    }

    void IndexManager::OnQueryVodListTrackerListTimerElapsed(uint32_t times)
    {
        if (false == is_running_)
        {
            return;
        }

        if (is_resolving_)
        {
            return;
        }

        ++failed_times_;

        DoQueryVodListTrackerList();

        last_query_vod_list_tracker_list_interval_ *= 2;
        if (last_query_vod_list_tracker_list_interval_ > DEFAULT_QUERY_INTERVAL)
            last_query_vod_list_tracker_list_interval_ = DEFAULT_QUERY_INTERVAL;
    }

    void IndexManager::OnQueryVodReportTrackerListTimerElapsed(uint32_t times)
    {
        if (false == is_running_)
            return;

        // 如果是成功状态
        //     定时时间4个小时

        if (is_resolving_) return;
        ++failed_times_;

        DoQueryVodReportTrackerList();

        // 指数增长
        last_query_vod_report_tracker_list_interval_ *= 2;
        if (last_query_vod_report_tracker_list_interval_ > DEFAULT_QUERY_INTERVAL)
            last_query_vod_report_tracker_list_interval_ = DEFAULT_QUERY_INTERVAL;
    }

    void IndexManager::OnQueryLiveListTrackerListTimerElapsed(uint32_t times)
    {
        if (false == is_running_)
        {
            return;
        }

        if (is_resolving_)
        {
            return;
        }

        ++failed_times_;

        DoQueryLiveListTrackerList();

        last_query_live_list_tracker_list_interval_ *= 2;
        if (last_query_live_list_tracker_list_interval_ > DEFAULT_QUERY_INTERVAL)
        {
            last_query_live_list_tracker_list_interval_ = DEFAULT_QUERY_INTERVAL;
        }
    }

    void IndexManager::OnQueryLiveReportTrackerListTimerElapsed(uint32_t times)
    {
        if (false == is_running_)
        {
            return;
        }

        // 如果是成功状态
        // 定时时间4个小时

        if (is_resolving_)
        {
            return;
        }

        ++failed_times_;

        DoQueryLiveReportTrackerList();

        // 指数增长
        last_query_live_report_tracker_list_interval_ *= 2;
        if (last_query_live_report_tracker_list_interval_ > DEFAULT_QUERY_INTERVAL)
        {
            last_query_live_report_tracker_list_interval_ = DEFAULT_QUERY_INTERVAL;
        }
    }

    void IndexManager::OnQueryStunServerListTimerElapsed(uint32_t times)
    {
        if (false == is_running_)
            return;

        // 如果是成功状态
        //     定时时间4个小时

        if (is_resolving_) return;
        ++failed_times_;
        DoQueryStunServerList();

        // 指数增长
        last_querystunlist_intervaltimes_ *= 2;
        if (last_querystunlist_intervaltimes_ > DEFAULT_QUERY_INTERVAL)
            last_querystunlist_intervaltimes_ = DEFAULT_QUERY_INTERVAL;

        //         if (true == is_have_stun_server_list_/*锟角凤拷锟秸碉拷锟斤拷trackerlist*/)
        //         {
        //             DoQueryStunServerList();
        //         }
        //         else
        //         {
        //             DoQueryStunServerList();
        //
        //             last_querystunlist_intervaltimes_ *= 2;
        //             if (last_querystunlist_intervaltimes_ > DEFAULT_QUERY_INTERVAL)
        //                 last_querystunlist_intervaltimes_ = DEFAULT_QUERY_INTERVAL;
        //         }

        // 如果是失败状态
        //   指数退避，
    }

#ifdef NOTIFY_ON
    void IndexManager::OnQueryNotifyServerTimerElapsed(boost::uint32_t times)
    {
        if (false == is_running_)
        {
            return;
        }

        if (is_have_notify_server_)
        {
            return;
        }

        DoQueryNotifyServerList();

        last_querynotifyserverlist_intervaltimes_ *= 2;
        if (last_querynotifyserverlist_intervaltimes_ > DEFAULT_QUERY_INTERVAL)
            last_querynotifyserverlist_intervaltimes_ = DEFAULT_QUERY_INTERVAL;
    }
#endif

    void IndexManager::OnQueryBootStrapConfigTimerElapsed(boost::uint32_t times)
    {
        if (false == is_running_)
        {
            return;
        }

        if (is_resolving_) return;
        ++failed_times_;

        DoQueryBootStrapConfig();

        last_query_bootstrap_config_interval_times_ *= 2;
        if (last_query_bootstrap_config_interval_times_ > DEFAULT_QUERY_INTERVAL)
            last_query_bootstrap_config_interval_times_ = DEFAULT_QUERY_INTERVAL;
    }

#ifdef NOTIFY_ON
    void IndexManager::DoQueryNotifyServerList()
    {
        LOG4CPLUS_INFO_LOG(logger_index, "DoQueryNotifyServerList");

        if (!is_running_)
        {
            return;
        }

        query_notify_server_list_timer_.interval(last_querynotifyserverlist_intervaltimes_);
        query_notify_server_list_timer_.start();

        boost::uint32_t transaction_id = protocol::Packet::NewTransactionID();

        protocol::QueryNotifyListPacket query_notify_server_list_request_packet(transaction_id, protocol::PEER_VERSION, AppModule::Inst()->GetUniqueGuid(), server_list_endpoint_);

        AppModule::Inst()->DoSendPacket(query_notify_server_list_request_packet);
    }
#endif

    void IndexManager::DoQueryLiveReportTrackerList()
    {
        LOG4CPLUS_INFO_LOG(logger_index, "DoQueryLiveTrackerList");

        if (is_running_ == false)
        {
            return;
        }

        query_live_report_tracker_list_timer_.interval(last_query_live_report_tracker_list_interval_);
        query_live_report_tracker_list_timer_.start();

        // 直接发送 QueryIndexServerListRequestPacket 包

        uint32_t transaction_id = protocol::Packet::NewTransactionID();

        protocol::QueryLiveTrackerListPacket query_live_tracker_list_request_packet(transaction_id,
            protocol::PEER_VERSION, AppModule::Inst()->GetUniqueGuid(), server_list_endpoint_);
        AppModule::Inst()->DoSendPacket(query_live_tracker_list_request_packet);
    }

    void IndexManager::DoQueryBootStrapConfig()
    {
        LOG4CPLUS_INFO_LOG(logger_index, "DoQueryNotifyServerList");

        if (!is_running_)
        {
            return;
        }

        query_bootstrap_config_timer_.interval(last_query_bootstrap_config_interval_times_);
        query_bootstrap_config_timer_.start();

        boost::uint32_t transaction_id = protocol::Packet::NewTransactionID();

        protocol::QueryConfigStringPacket pakt(transaction_id, server_list_endpoint_);

        AppModule::Inst()->DoSendPacket(pakt);
    }

    void IndexManager::DoQuerySnList()
    {
        if (is_running_ == false)
        {
            return;
        }

        query_sn_list_timer_.interval(last_query_sn_list_interval_times_);
        query_sn_list_timer_.start();

        protocol::QuerySnListPacket query_sn_list_packet(protocol::Packet::NewTransactionID(),
            protocol::PEER_VERSION, server_list_endpoint_);
        AppModule::Inst()->DoSendPacket(query_sn_list_packet);
    }

    void IndexManager::OnQuerySnListPacket(protocol::QuerySnListPacket const & packet)
    {
        if (is_running_ == false)
        {
            return;
        }

        // 判断时都出错
        if (packet.error_code_ == 0)
        {
            // 成功收到服务器回复，定时器时间设置为比较长的时间
            query_sn_list_timer_.interval(DEFAULT_QUERY_INTERVAL);
            query_sn_list_timer_.start();
            is_have_sn_list_ = true;
            failed_times_ = 0;
            resolve_times_ = 0;
            last_query_sn_list_interval_times_ = INITIAL_QUERY_INTERVAL;

            SNPool::Inst()->AddSN(packet.response.super_node_infos_);
        }
    }

    void IndexManager::OnQuerySnListTimerElapsed(boost::uint32_t times)
    {
        if (false == is_running_)
        {
            return;
        }

        // 如果是成功状态
        // 定时时间4个小时
        if (is_resolving_)
        {
            return;
        }

        ++failed_times_;

        DoQuerySnList();

        // 指数增长
        last_query_sn_list_interval_times_ *= 2;
        if (last_query_sn_list_interval_times_ > DEFAULT_QUERY_INTERVAL)
        {
            last_query_sn_list_interval_times_ = DEFAULT_QUERY_INTERVAL;
        }
    }

    void IndexManager::OnQueryVodListTrackerListPacket(const protocol::QueryTrackerForListingPacket & packet)
    {
        if (is_running_ == false)
        {
            return;
        }

        if (packet.error_code_ == 0)
        {
            query_vod_list_tracker_list_timer_.interval(DEFAULT_QUERY_INTERVAL);
            query_vod_list_tracker_list_timer_.start();
            is_have_vod_list_tracker_list_ = true;
            failed_times_ = 0;
            resolve_times_ = 0;
            last_query_vod_list_tracker_list_interval_ = INITIAL_QUERY_INTERVAL;

            TrackerModule::Inst()->SetTrackerList(packet.response.tracker_group_count_, packet.response.tracker_info_, true,  p2sp::LIST);
        }
    }

    void IndexManager::OnQueryLiveListTrackerListPacket(const protocol::QueryTrackerForListingPacket & packet)
    {
        if (is_running_ == false)
        {
            return;
        }

        if (packet.error_code_ == 0)
        {
            query_live_list_tracker_list_timer_.interval(DEFAULT_QUERY_INTERVAL);
            query_live_list_tracker_list_timer_.start();
            is_have_live_list_tracker_list_ = true;
            failed_times_ = 0;
            resolve_times_ = 0;
            last_query_live_list_tracker_list_interval_ = INITIAL_QUERY_INTERVAL;

            TrackerModule::Inst()->SetTrackerList(packet.response.tracker_group_count_, packet.response.tracker_info_, false, p2sp::LIST);
        }
    }
}
