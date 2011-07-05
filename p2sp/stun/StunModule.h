//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------


#ifndef _P2SP_STUN_STUN_MODULE_H_
#define _P2SP_STUN_STUN_MODULE_H_

#include <protocol/StunServerPacket.h>

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

        void GetStunEndpoint(uint32_t &ip, boost::uint16_t &port);
        // 消息
        void OnTimerElapsed(framework::timer::Timer * pointer);

        void OnGetNATType(protocol::MY_STUN_NAT_TYPE nat_type);

        void SetStunServerList(std::vector<protocol::STUN_SERVER_INFO> stun_servers);

        void OnUdpRecv(protocol::Packet const & packet_header);

        bool GetIsNeededStun(){ return is_needed_stun_;}
        void SetIsNeededStun(bool is_needed_stun){ is_needed_stun_ = is_needed_stun;}

        protocol::MY_STUN_NAT_TYPE GetPeerNatType() const { return nat_type_; }

    private:
        // 清理StunServer信息
        void ClearStunInfo();

        void DoHandShake();
        void DoKPL();

        void OnStunHandShakePacket(protocol::StunHandShakePacket const & packet);
        void OnStunInvokePacket(protocol::StunInvokePacket const & packet);


    private:
        boost::asio::io_service & io_svc_;
        boost::asio::ip::udp::endpoint stun_endpoint_;
        framework::timer::PeriodicTimer stun_timer_;
        std::vector<protocol::STUN_SERVER_INFO> stun_server_info_;
        uint32_t stun_server_index_;
        // 状态
        volatile bool is_running_;
        volatile bool is_select_stunserver_;
        volatile bool is_needed_stun_;
        protocol::MY_STUN_NAT_TYPE nat_type_;
        string ppva_config_path_;
    private:
        static StunModule::p inst_;
        StunModule(
            boost::asio::io_service & io_svc);

    public:
        static StunModule::p Inst()
        {
            return inst_;
        }

        static StunModule::p CreateInst(
            boost::asio::io_service & io_svc)
        {
            inst_.reset(new StunModule(io_svc));
            return inst_;
        }
    };
}

#endif  // _P2SP_STUN_STUN_MODULE_H_
