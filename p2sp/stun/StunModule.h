//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------


#ifndef _P2SP_STUN_STUN_MODULE_H_
#define _P2SP_STUN_STUN_MODULE_H_

#include <protocol/StunServerPacket.h>
#include <p2sp/AppModule.h>
#include "NatCheckClient.h"

namespace p2sp
{
    class StunModule
        : public boost::noncopyable
        , public boost::enable_shared_from_this<StunModule>
    {
        friend class AppModule;
    public:
        typedef boost::shared_ptr<StunModule> p;
    public:
        // 启动 停止
        void Start(const string& config_path);
        void Stop();
        // 操作

        void GetStunEndpoint(boost::uint32_t &ip, boost::uint16_t &port);
        // 消息
        void OnTimerElapsed(framework::timer::Timer * pointer);

        void OnGetNATType(protocol::MY_STUN_NAT_TYPE nat_type);

        //upnp检测接收之后的回调，如果成功，exUdpPort非0，否则为0，不为0的时候，nattype要改成fullcone
        void OnUpnpCheck(boost::uint16_t exUdpPort);


        //获取upnp映射的外网端口，如果upnp映射失败或者映射的端口不可用，这个函数返回0
        boost::uint16_t GetUpnpExUdpport(){return upnp_ex_udp_port_;}

        void SetStunServerList(const std::vector<protocol::STUN_SERVER_INFO>& stun_servers);

        void OnUdpRecv(protocol::Packet const & packet_header);

        bool GetIsNeededStun(){ return is_needed_stun_;}
        void SetIsNeededStun(bool is_needed_stun){ is_needed_stun_ = is_needed_stun;}

        protocol::MY_STUN_NAT_TYPE GetPeerNatType() const { return nat_type_; }

        int GetNatCheckState() const {  return nat_check_returned_ ? nat_check_client_.GetNatCheckState() : -1;}

        //upnp映射成功之后，检测upnp映射成功的端口能否收到数据。
        void CheckForUpnp(boost::uint16_t innerUdpPort,boost::uint16_t exUdpPort);

    private:
        // 清理StunServer信息
        void ClearStunInfo();

        void PickStun();
        void DoHandShake();
        void DoKPL();

        void OnStunHandShakePacket(protocol::StunHandShakePacket const & packet);
        void OnStunInvokePacket(protocol::StunInvokePacket const & packet);
        void OnHandShakeTimeOut();
        void PickStunAnotherIp();


    private:
        boost::asio::io_service & io_svc_;
        boost::asio::ip::udp::endpoint stun_endpoint_;
        framework::timer::PeriodicTimer stun_timer_;

        //判断握手包是否超时的定时器
        framework::timer::OnceTimer handshake_timer_;
        //因为丢包，被淘汰的ip
        std::set<unsigned> failed_ips_;
        //统计当前stun的评分，换stun的时候，需要清0，收到handshake回复的时候+1，收不到回复的时候减少
        boost::uint32_t cur_stun_score_;


        std::vector<protocol::STUN_SERVER_INFO> stun_server_info_;

        //这个变量本来是用来记录选择时候的指针的，由于改变了选择方法，加了PickStun的函数，所以这个变量也不要了。
        //boost::uint32_t stun_server_index_;

        // 状态
        volatile bool is_running_;

        //这个值的含义是这个ip的stun，是否收到过handshake的回包
        volatile bool is_select_stunserver_;

        volatile bool is_needed_stun_;
        protocol::MY_STUN_NAT_TYPE nat_type_;
        string ppva_config_path_;
        NatCheckClient nat_check_client_;
        framework::timer::TickCounter nat_check_timer_;
        bool nat_check_returned_;
        boost::uint16_t upnp_ex_udp_port_;


    private:
        static StunModule::p inst_;
        StunModule(
            boost::asio::io_service & io_svc);

    public:
        static StunModule::p Inst()
        {
            if (!inst_)
            {
                inst_.reset(new StunModule(global_io_svc()));
            }
            return inst_;
        }
    };
}

#endif  // _P2SP_STUN_STUN_MODULE_H_
