//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/AppModule.h"
#include "p2sp/index/IndexManager.h"

#include "p2sp/stun/StunModule.h"

#include "statistic/StatisticModule.h"
#include "statistic/DACStatisticModule.h"
#include "storage/Storage.h"

#include <framework/network/Interface.h>
#include <boost/date_time.hpp>
#include "../bootstrap/BootStrapGeneralConfig.h"
using namespace framework::network;

using namespace protocol;

namespace p2sp
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_stun = log4cplus::Logger::getInstance("[stun_module]");
#endif

    StunModule::p StunModule::inst_;

    StunModule::StunModule(
        boost::asio::io_service & io_svc)
        : io_svc_(io_svc)
        , stun_timer_(global_second_timer(), 10 * 1000, boost::bind(&StunModule::OnTimerElapsed, this, &stun_timer_))
        , handshake_timer_(global_second_timer(), 2 * 1000, boost::bind(&StunModule::OnTimerElapsed, this, &handshake_timer_))
        , is_running_(false)
        , is_select_stunserver_(false)
        , is_needed_stun_(true)
        , nat_type_(protocol::TYPE_ERROR)
        , nat_check_client_(io_svc)
        , nat_check_returned_(false)
        , upnp_ex_udp_port_(0)
        , cur_stun_score_(0)
    {
    }

    void StunModule::Start(const string& config_path)
    {
        if (is_running_) return;

        LOG4CPLUS_INFO_LOG(logger_stun, "StunModule::Start ");

        // config path
        ppva_config_path_ = config_path;

        ClearStunInfo();

        stun_timer_.start();

        nat_check_timer_.reset();
        nat_check_client_.Start(ppva_config_path_);
        is_running_ = true;
    }

    void StunModule::Stop()
    {
        if (!is_running_) return;

        LOG4CPLUS_INFO_LOG(logger_stun, "StunModule::Stop ");
        // 停止 Stun 脉冲定时器

        stun_timer_.stop();
        handshake_timer_.stop();
        nat_check_client_.Stop();

        is_running_ = false;
        inst_.reset();
    }

    void StunModule::OnGetNATType(protocol::MY_STUN_NAT_TYPE nat_type)
    {
        nat_check_returned_ = true;
        //统计正式检测NAT TYPE所耗费的时间
        statistic::DACStatisticModule::Inst()->SubmitNatCheckTimeCost(
            nat_check_client_.GetNatCheckState() == p2sp::IDLE ? 0 : nat_check_timer_.elapsed());
        if (is_running_ == false) return;

        if (nat_type == TYPE_PUBLIC || nat_type == TYPE_FULLCONENAT)
        {
            is_needed_stun_ = false;
            protocol::SocketAddr stun_socket_addr(0, 0);
            //设置stunsocket，表示没有使用stun
            statistic::StatisticModule::Inst()->SetLocalStunSocketAddress(stun_socket_addr);
            //设置detectsocket，是让tracker知道，无需使用stun检测到的ipport，而是用tracker从socket上看到的ipport
            statistic::StatisticModule::Inst()->SetLocalDetectSocketAddress(stun_socket_addr);
            LOG4CPLUS_INFO_LOG(logger_stun, "OnGetNATType nat_type == TYPE_PUBLIC || nat_type == TYPE_FULLCONENAT");
        }
        else
        {
            is_needed_stun_ = true;
            LOG4CPLUS_INFO_LOG(logger_stun, "OnGetNATType nat_type != TYPE_PUBLIC && nat_type != TYPE_FULLCONENAT");
        }
        nat_type_ = nat_type;
    }

    void StunModule::OnUpnpCheck(boost::uint16_t exUdpPort)
    {
        upnp_ex_udp_port_ = exUdpPort;
        if(exUdpPort != 0)
        {            
            //走到这里，说明upnp是成功的
            nat_check_returned_ = true;
            protocol::SocketAddr stun_socket_addr(0, 0);
            //设置stunsocket，表示没有使用stun
            statistic::StatisticModule::Inst()->SetLocalStunSocketAddress(stun_socket_addr);
            statistic::StatisticModule::Inst()->SetLocalDetectSocketAddress(stun_socket_addr);
            nat_type_ = TYPE_FULLCONENAT;
            is_needed_stun_ = false;

                  //成功设置为0，失败设置为1
            statistic::DACStatisticModule::Inst()->SetUpnpCheckResult(0);
        }
        else
        {
                  //成功设置为0，失败设置为1
            statistic::DACStatisticModule::Inst()->SetUpnpCheckResult(1);
        }
    }

    void StunModule::ClearStunInfo()
    {
        if (!is_running_)
            return;
        is_select_stunserver_ = false;
        cur_stun_score_ = 0;
        stun_server_info_.clear();
    }

    void StunModule::GetStunEndpoint(boost::uint32_t &ip, boost::uint16_t &port)
    {
        if (!is_running_)
            return;
        if (!is_select_stunserver_)
        {
            ip = 0; 
            port = 0;
            return;
        }
        ip = stun_endpoint_.address().to_v4().to_ulong();
        port = stun_endpoint_.port();

        return;
    }

    void StunModule::PickStunAnotherIp()
    {
        //尽量选择没有失败过的ip，如果找不到，那只好随便选一个了。
        cur_stun_score_ = 0;

        boost::uint32_t i=0;

        for (i = 0; i < stun_server_info_.size(); ++i)
        {
            if (failed_ips_.find(stun_server_info_[i].GetIP()) == failed_ips_.end())
            {
                stun_endpoint_ =framework::network::Endpoint(stun_server_info_[i].IP, stun_server_info_[i].Port);  
                return;
            }
        }

        if(i == stun_server_info_.size())
        {
            //既然所有的ip都失败过，那么就清空失败列表，大家再重来。
            failed_ips_.clear();
            int picknum = rand() % stun_server_info_.size();
            //有小概率还会选到原来用的，但是没有关系
            stun_endpoint_ = framework::network::Endpoint(stun_server_info_[picknum].IP, stun_server_info_[picknum].Port);
        }         
    }

    void StunModule::OnHandShakeTimeOut()
    {
        //分数减少到原来的1/2
        if(cur_stun_score_ > 4)
        {
            cur_stun_score_ = 2;
        }
        else
        {
            cur_stun_score_ /=2;
        }

        if(cur_stun_score_ <1)
        {
            //当前的stun不好了，要换一个。
            is_select_stunserver_ = false;
            failed_ips_.insert(stun_endpoint_.address().to_v4().to_ulong());
        }
        else
        {
            //当前的stun，虽然刚丢包了，但是以前几次还不错，所以再试试
        }
    }

    void StunModule::OnTimerElapsed(framework::timer::Timer * pointer)
    {
        if (!is_running_) return;

        if (pointer == &stun_timer_)
        {
            if (is_needed_stun_)
            {
                if (stun_server_info_.size() == 0)
                {
                    // IndexManager::Inst()->DoQueryStunServerList();
                }
                else if (!is_select_stunserver_)
                {
                    assert(stun_server_info_.size() != 0);
                    PickStunAnotherIp();
                    DoHandShake();
                }
                else
                {
                    const int KPLTIMES = 4;
                    if (pointer->times() % (KPLTIMES +1) == KPLTIMES)
                        DoHandShake();
                    else
                        DoKPL();
                }
            }
        } 
        else if(pointer == &handshake_timer_)
        {
            //丢包了
            OnHandShakeTimeOut();
        }
        else 
        {
            assert(0);
        }
    }

    void StunModule::SetStunServerList(const std::vector<STUN_SERVER_INFO>& stun_servers)
    {
        if (!is_running_) return;
        LOG4CPLUS_INFO_LOG(logger_stun, "StunModule::SetStunServerList size=" << stun_servers.size());
        STL_FOR_EACH_CONST (std::vector<STUN_SERVER_INFO>, stun_servers, server)
        {
            LOG4CPLUS_INFO_LOG(logger_stun, "IP:" << server->IP << ", port:" << server->Port << ", type:" 
                << (int)server->Type);
        }

        if (stun_servers.size() == 0)
        {
            LOG4CPLUS_ERROR_LOG(logger_stun, "StunModule::SetStunServerList stun_servers.size() == 0");
            assert(0);
            return;
        }
   
        if (is_select_stunserver_)
        {
            for (boost::uint32_t i = 0; i < stun_servers.size(); ++i)
            {
                if (stun_servers[i].GetIP() == stun_endpoint_.address().to_v4().to_ulong() && 
                    stun_servers[i].Port == stun_endpoint_.port())
                {
                    // 正在使用的stun_server在新的stun_server_list中,更换一下stun列表
                    stun_server_info_ = stun_servers;
                    return;
                }
            }
        }

        // 当前没有使用stun_server或者使用的stun_server不在新的stun_server_list中
        ClearStunInfo();    
        stun_server_info_ = stun_servers;
        

        //需要尽可能选一个和当前stun_endpoint_相同ip的，因为当前的ip，很可能是经过淘汰后优胜的。
        for (boost::uint32_t i = 0; i < stun_server_info_.size(); ++i)
        {
            if (stun_server_info_[i].GetIP() == stun_endpoint_.address().to_v4().to_ulong())
            {
                stun_endpoint_.port(stun_server_info_[i].Port);
                is_select_stunserver_ = true;
                break;
            }
        }

        if(!is_select_stunserver_)
        {
            //如果没有找到，也没有关系，因为在 OnTimerElapse的 PickStunAnotherIp 里，会进行选择的。
            LOG4CPLUS_INFO_LOG(logger_stun,"not find same ip stun");
        }

        statistic::StatisticModule::Inst()->SetStunInfo(stun_server_info_);
    }

    void StunModule::OnUdpRecv(protocol::Packet const & packet_header)
    {
        if (!is_running_) return;

        switch (packet_header.PacketAction)
        {
        case protocol::StunHandShakePacket::Action:
            {
                protocol::StunHandShakePacket const & stun_handshake_response_packet =
                    (protocol::StunHandShakePacket const &)packet_header;

                OnStunHandShakePacket(stun_handshake_response_packet);
            }
            break;
        case protocol::StunInvokePacket::Action:
            {
                protocol::StunInvokePacket const & stun_invoke_packet =
                    (protocol::StunInvokePacket const &)packet_header;

                OnStunInvokePacket(stun_invoke_packet);
            }
            break;
        case protocol::NatCheckSameRoutePacket::Action:
        case protocol::NatCheckDiffPortPacket::Action:
        case protocol::NatCheckDiffIpPacket::Action:
        case protocol::NatCheckForUpnpPacket::Action:
            {
                nat_check_client_.OnUdpReceive((ServerPacket const &)packet_header);
            }
            break;
        default:
            {
                assert(0);
            }
            break;
        }
    }

    boost::uint32_t LoadIPs(boost::uint32_t maxCount, boost::uint32_t ipArray[], const hostent& host)
    {
        boost::uint32_t count = 0;
        for (int index = 0; host.h_addr_list[index] != NULL; index++)
        {
            if (count >= maxCount)
                break;
            const in_addr* addr = reinterpret_cast<const in_addr*> (host.h_addr_list[index]);
            boost::uint32_t v =
#ifdef PEER_PC_CLIENT
                addr->S_un.S_addr;
#else
                addr->s_addr;
#endif
            ipArray[count++] = ntohl(v);
        }
        std::sort(ipArray, ipArray + count, std::greater<boost::uint32_t>());
        return count;
    }

    boost::uint32_t LoadLocalIPs(boost::uint32_t maxCount, boost::uint32_t ipArray[])
    {
        char hostName[256] = { 0 };
        if (gethostname(hostName, 255) != 0)
        {
            boost::uint32_t error =
#ifdef PEER_PC_CLIENT
                ::WSAGetLastError();
#else
                0;
#endif
            (void)error;
            LOG4CPLUS_ERROR_LOG(logger_stun, "CIPTable::LoadLocal: gethostname failed, ErrorCode=" << error);
            return 0;
        }

        LOG4CPLUS_ERROR_LOG(logger_stun, "CIPTable::LoadLocal: gethostname=" << hostName);
        struct hostent* host = gethostbyname(hostName);
        if (host == NULL)
        {
            boost::uint32_t error =
#ifdef PEER_PC_CLIENT
                ::WSAGetLastError();
#else
                0;
#endif
            (void)error;
            LOG4CPLUS_ERROR_LOG(logger_stun, "CIPTable::LoadLocal: gethostbyname(" << hostName 
                << ") failed, ErrorCode=" << error);
            return 0;
        }
        boost::uint32_t count = LoadIPs(maxCount, ipArray, *host);
        if (count == 0)
        {
            LOG4CPLUS_ERROR_LOG(logger_stun, "CIPTable::LoadLocal: No hostent found.");
        }
        return count;
    }

    void LoadLocalIPs(std::vector<boost::uint32_t>& ipArray)
    {
#ifdef PEER_PC_CLIENT
        const boost::uint32_t max_count = 32;
        ipArray.clear();
        ipArray.resize(max_count);
        boost::uint32_t count = LoadLocalIPs(max_count, &ipArray[0]);
        assert(count <= max_count);
        if (count < max_count)
        {
            ipArray.resize(count);
        }
#else

        // PPBox获取本地IP的实现
        std::vector<framework::network::Interface> infs;
        enum_interface(infs);
        for (size_t i = 0; i < infs.size(); ++i) {
            framework::network::Interface const & inf = infs[i];

            if (inf.flags & framework::network::Interface::loopback)
                continue;
            if (!(inf.flags & framework::network::Interface::up))
                continue;

            if (inf.addr.is_v4())
            {
                ipArray.push_back(inf.addr.to_v4().to_ulong());
            }
        }
#endif
    }

    void StunModule::OnStunHandShakePacket(protocol::StunHandShakePacket const & packet)
    {
        if (!is_running_)
            return;

        if(packet.end_point != stun_endpoint_)
        {
            LOG4CPLUS_INFO_LOG(logger_stun,"recv handshake packet and the packet is not mine");
            return;
        }

        //收到了handshake的回包，消除handshake的超时定时器
        handshake_timer_.stop();

        statistic::DACStatisticModule::Inst()->SubmitStunHandShakeResponseCount(packet.end_point.address().to_v4().to_ulong());

        LOG4CPLUS_INFO_LOG(logger_stun, "StunModule::OnStunHandShakePacket ");

        if (nat_type_ != TYPE_FULLCONENAT  && nat_type_ != TYPE_PUBLIC)
        {
            // 在未检测出类型时, 检查是否公网
            if (nat_type_ == TYPE_ERROR)
            {
                boost::uint32_t detected_ip = packet.response.detected_ip_;
                std::vector<boost::uint32_t> local_ips;
                LoadLocalIPs(local_ips);
                if (find(local_ips.begin(), local_ips.end(), detected_ip) != local_ips.end())
                {
                    nat_type_ = TYPE_PUBLIC;
                    protocol::SocketAddr stun_socket_addr(0, 0);
                    statistic::StatisticModule::Inst()->SetLocalStunSocketAddress(stun_socket_addr);
                    is_needed_stun_ = false;
                    stun_timer_.stop();
                    return;
                }
            }

            // stun address
            protocol::SocketAddr stun_socket_addr(stun_endpoint_);
            statistic::StatisticModule::Inst()->SetLocalStunSocketAddress(stun_socket_addr);

            // timer
            protocol::SocketAddr detect_socket_addr(packet.response.detected_ip_, packet.response.detect_udp_port_);
            statistic::StatisticModule::Inst()->SetLocalDetectSocketAddress(detect_socket_addr);
            is_select_stunserver_ = true;

            //这个stun的得分加1
            ++cur_stun_score_;
            stun_timer_.interval(packet.response.keep_alive_interval_ * 1000);
            stun_timer_.start();
        }

    }

    void StunModule::OnStunInvokePacket(protocol::StunInvokePacket const & packet)
    {
        if (!is_running_)
            return;

        protocol::ConnectPacket connect_packet(
            packet.transaction_id_,
            packet.resource_id_,
            packet.peer_guid_,
            packet.candidate_peer_info_mine_.PeerVersion,
            0x00,
            packet.send_off_time_,
            packet.candidate_peer_info_mine_.PeerVersion,
            packet.candidate_peer_info_mine_,
            packet.connect_type_,
            packet.peer_download_info_mine_,
            framework::network::Endpoint(packet.candidate_peer_info_mine_.DetectIP, packet.candidate_peer_info_mine_.DetectUdpPort));
        
        connect_packet.PacketAction = connect_packet.Action;

        // need to be fixed
        /*
        protocol::SubPieceBuffer buf = connect_packet->GetBuffer();
        if (protocol::Cryptography::Encrypt(buf) == false)
        {
        assert(0);
        return;
        }

        AppModule::Inst()->OnUdpRecv(e_p, buf);
        */
        AppModule::Inst()->OnPacketRecv(connect_packet);
    }

    void StunModule::DoHandShake()
    {
        if (!is_running_)
            return;
        LOG4CPLUS_INFO_LOG(logger_stun, "StunModule::DoHandShake " << stun_endpoint_);

        protocol::StunHandShakePacket stun_handshake_packet;
        stun_handshake_packet.end_point = stun_endpoint_;

        LOG4CPLUS_INFO_LOG(logger_stun, "DoHandShake " << stun_endpoint_);
        AppModule::Inst()->DoSendPacket(stun_handshake_packet);

        statistic::DACStatisticModule::Inst()->SubmitStunHandShakeRequestCount(stun_endpoint_.address().to_v4().to_ulong());

        //添加一个定时器，检查有没有丢包,收到回包之后将会清除
        handshake_timer_.start();

    }

    void StunModule::DoKPL()
    {
        if (!is_running_)
            return;
        LOG4CPLUS_INFO_LOG(logger_stun, "StunModule::DoKPL " << stun_endpoint_);

        protocol::StunKPLPacket stun_kpl_packet;
        stun_kpl_packet.end_point = stun_endpoint_;
        LOG4CPLUS_INFO_LOG(logger_stun, "DoKPL " << stun_endpoint_);
        AppModule::Inst()->DoSendPacket(stun_kpl_packet);

    }

    void StunModule::CheckForUpnp(boost::uint16_t innerUdpPort,boost::uint16_t exUdpPort)
    {
        nat_check_client_.CheckForUpnp(innerUdpPort,exUdpPort);
    }

}
