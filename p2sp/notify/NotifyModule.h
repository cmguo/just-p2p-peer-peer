//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#pragma once

#ifndef NOTIFY_MODULE_
#define NOTIFY_MODULE_

#include "protocol/NotifyPacket.h"
#include "protocol/IndexPacket.h"

const boost::uint32_t MAX_PEER_NODE_NUM = 16;
const boost::uint32_t KEEPALIVE_INTERVAL = 30;
const boost::uint32_t NOTIFY_TIMEOUT = 10;
const boost::uint32_t MAX_CONTENT_LENGTH = 1024;

namespace p2sp
{
    enum NOTIFY_TASK_TYPE
    {
        TEXT = 0,// 文本
        LIVE = 1,// 直播
        VIDEO = 2,//视频
        PIC = 3,// 图文
        VIP_MESSAGE = 4,// 小喇叭
        EXE = 5, // 快速下发EXE
        SUBSCRIPTION_MESSAGE = 6, // 订阅通知
        INVALID_TYPE
    };

    struct PEER_TASK_STATUS
    {
        bool is_notify_;
        bool is_notify_response_;
        boost::uint32_t finish_num_;

        PEER_TASK_STATUS()
        {
            memset(this, 0, sizeof(PEER_TASK_STATUS));
        }
    };

    struct PEER_NODE_STATUS
    {
        boost::asio::ip::udp::endpoint end_point;
        boost::uint32_t intern_ip_;
        boost::uint16_t intern_port_;
        boost::uint32_t detect_ip_;
        boost::uint16_t detect_port_;
        boost::uint32_t stun_ip_;
        boost::uint16_t stun_port_;
        boost::uint32_t peer_online_;
        boost::uint32_t dead_time_;
        std::map<boost::uint32_t, PEER_TASK_STATUS> peer_task_map_;
    };

    struct TASK_RECORD
    {
        boost::uint32_t rest_time_;
        boost::uint32_t finish_num_;
        bool   is_my_finish_;
        boost::uint16_t duration_;
        NOTIFY_TASK_TYPE task_type_;
        boost::uint16_t buffer_len_;
        boost::uint32_t task_delay_time_;
        char buf[MAX_CONTENT_LENGTH];
    };

    class NotifyModule
        : public boost::noncopyable
        , public boost::enable_shared_from_this<NotifyModule>
#ifdef DUMP_OBJECT
        , public count_object_allocate<NotifyModule>
#endif
    {
        friend class AppModule;
    public:
        typedef boost::shared_ptr<NotifyModule> p;
    public:
        void Start();
        void Stop();

#if defined(NEED_LOG)
        void LoadConfig();
#endif

        void JoinNotifyNetwork();
        void NotifyPeer(Guid peer_guid, boost::uint32_t task_id);

        void OnNotifyTaskStatusChange(boost::uint32_t task_id, boost::uint32_t task_status);
        void OnNotifyJoinLeave(boost::uint32_t join_or_leave);
        void DoSendJoinRequestPacket(boost::asio::ip::udp::endpoint end_point);

        void OnGetNotifyServerList(protocol::QueryNotifyListPacket const & query_notify_response_packet);

        // 通知重发
        void NotifyReSend();

        // 统计在线人数
        void CalPeerOnline();

        // 统计任务完成数
        void CalTaskComplete();

        // 发送KeepAlive包
        void DoSendKeepAlive();

        void OnUdpRecv(protocol::Packet const & packet);
        void OnTimerElapsed(framework::timer::Timer * pointer);

    private:
        bool is_running_;

        // 统计任务完成
        std::map<boost::uint32_t, TASK_RECORD> task_map_;

        // 待加入节点列表
        std::deque<protocol::CandidatePeerInfo> peer_to_connect_;

        // 服务器
        boost::asio::ip::udp::endpoint server_endpoint_;

        // 父节点
        Guid god_guid_;
        boost::asio::ip::udp::endpoint god_endpoint_;
        boost::uint32_t god_node_time;

        // 是否加入网络
        bool is_join_success_;

        // 子节点
        std::map<Guid, PEER_NODE_STATUS> peer_node_map_;
        boost::uint32_t peer_node_retun_number_;

        // 在线人数
        boost::uint32_t total_peer_online_;

        // 本机IP
        std::vector<boost::uint32_t> local_ips;

        // 连接节点跳数
        boost::uint32_t hops_;

        // 服务器连接节点跳数
        boost::uint32_t server_connect_hops_;
        // Join or Leave
        bool is_need_join_;

        framework::timer::PeriodicTimer notify_timer_;
        framework::timer::OnceTimer join_timer_;

        framework::timer::TickCounter time_count_;

        bool is_have_server_endpoint;

        // NotifyServer 列表
        std::vector<protocol::NOTIFY_SERVER_INFO> notify_server_s_;
        // 临时测试变量
        boost::uint32_t max_peer_node;
        boost::uint32_t max_peer_return;
        boost::uint32_t start_time;

    private:
        static NotifyModule::p inst_;
        NotifyModule() : is_running_(false), is_join_success_(false), hops_(1), is_need_join_(false),
        notify_timer_(global_second_timer(), 1000, boost::bind(&NotifyModule::OnTimerElapsed, this, &notify_timer_)),
        join_timer_(global_second_timer(), 30*1000, boost::bind(&NotifyModule::OnTimerElapsed, this, &join_timer_))
        {};
        double mylog(double a, double b);
    public:
        static NotifyModule::p Inst() { return inst_; }
    };
}
#endif

