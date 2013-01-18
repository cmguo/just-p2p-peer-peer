//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef NETCHECK
#define NETCHECK

#include "p2sp/AppModule.h"
#include <boost/date_time.hpp>
#include "framework/timer/Timer.h"

namespace p2sp
{
    enum CheckState
    {
        IDLE = 0,
        ISNATCHECKING,
        FULLCORNCHECKING,
        SYMMETRICCHECKING,
        RESTRICTEDCHECKING,
        UPNPCHECKING,
        COMPLETE
    };

    class NatCheckClient
    {
    public:
        NatCheckClient(boost::asio::io_service &ios)
        : timer_(global_second_timer(), 5000, boost::bind(&NatCheckClient::OnTimerElapsed, this, &timer_))
        , check_state_(IDLE)
        , times_(0)
        , upnp_ex_udp_port_(0)
        , nat_timer_(global_second_timer(), 5000, boost::bind(&NatCheckClient::OnTimerElapsed, this, &nat_timer_))
        {}
        
        void Start(const string& config_path);
        void OnHandleTimeOut();
        void OnUdpReceive(protocol::ServerPacket const &packet);
        CheckState GetNatCheckState() const { return check_state_;}
        void CheckForUpnp(boost::uint16_t innerUdpPort,boost::uint16_t exUdpPort);
        void Stop();

    private:
        void StartCheck();
        void OnReceiveNatCheckSameRoutePacket(protocol::NatCheckSameRoutePacket const & packet);
        void OnReceiveNatCheckDiffPortPacket(protocol::NatCheckDiffPortPacket const &packet);
        void OnReceiveNatCheckDiffIpPacket(protocol::NatCheckDiffIpPacket const &packet);
        void OnReceiveNatCheckForUpnpPacket(protocol::NatCheckForUpnpPacket const &packet);
        void DoSendNatCheckpacket();
        bool IsNeedToUpdateNat(protocol::MY_STUN_NAT_TYPE &snt_result, const string& config_path);
        void WriteConfigAfterCheck(protocol::MY_STUN_NAT_TYPE nat_type);
        void OnTimerElapsed(framework::timer::Timer *timer);

    private:
        CheckState check_state_;
        framework::timer::PeriodicTimer nat_timer_;
        framework::timer::PeriodicTimer timer_;
        boost::asio::ip::udp::endpoint endpoint_;
        boost::asio::ip::udp::endpoint endpoint__symncheck_;
        boost::uint32_t times_;
        std::vector<boost::uint32_t> local_ips_;
        boost::uint32_t local_first_ip_;
        boost::uint16_t local_port_;
        boost::uint16_t detect_port_;
        boost::uint32_t detect_ip_;
        boost::uint16_t upnp_ex_udp_port_;
        bool is_nat_;
        string m_strConfig;
        vector<string> nat_check_server_list_;
        vector<string>::iterator server_iter_;
        std::string config_path_;
    };
}

#endif