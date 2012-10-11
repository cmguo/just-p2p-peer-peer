//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef NETCHECK
#define NETCHECK

#include "p2sp/AppModule.h"
#include <boost/date_time.hpp>

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
        : timer_(ios)
        , check_state_(IDLE)
        , times_(0)
        , upnp_ex_udp_port_(0)
        {}
        
        void Start(const string& config_path);
        void OnHandleTimeOut(const boost::system::error_code& er);
        void OnUdpReceive(protocol::ServerPacket const &packet);
        CheckState GetNatCheckState() const { return check_state_;}
        void CheckForUpnp(boost::uint16_t innerUdpPort,boost::uint16_t exUdpPort);

    private:
        void StartCheck(const string& config_path);
        void OnReceiveNatCheckSameRoutePacket(protocol::NatCheckSameRoutePacket const & packet);
        void OnReceiveNatCheckDiffPortPacket(protocol::NatCheckDiffPortPacket const &packet);
        void OnReceiveNatCheckDiffIpPacket(protocol::NatCheckDiffIpPacket const &packet);
        void OnReceiveNatCheckForUpnpPacket(protocol::NatCheckForUpnpPacket const &packet);
        void DoSendNatCheckpacket();
        bool IsNeedToUpdateNat(protocol::MY_STUN_NAT_TYPE &snt_result, const string& config_path);
        void WriteConfigAfterCheck(protocol::MY_STUN_NAT_TYPE nat_type);

    private:
        CheckState check_state_;
        boost::asio::deadline_timer timer_;
        boost::asio::ip::udp::endpoint endpoint_;
        boost::asio::ip::udp::endpoint endpoint__symncheck_;
        boost::uint32_t times_;
        std::vector<uint32_t> local_ips_;
        uint32_t local_first_ip_;
        uint16_t local_port_;
        uint16_t detect_port_;
        uint32_t detect_ip_;
        uint16_t upnp_ex_udp_port_;
        bool is_nat_;
        string m_strConfig;
        vector<string> nat_check_server_list_;
        vector<string>::iterator server_iter_;
    };
}

#endif