//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------
#include "Common.h"
#include "NatCheckClient.h"

#include <protocol/NatCheckPacket.h>
#include <boost/algorithm/string.hpp>
#include <boost/date_time.hpp>
#include <algorithm>
#include "p2sp/stun/StunModule.h"
#include "p2sp/bootstrap/BootStrapGeneralConfig.h"
#include "base/util.h"

namespace p2sp
{
    extern void LoadLocalIPs(std::vector<boost::uint32_t>& ipArray);

    void NatCheckClient::Start(const string& config_path)
    {
        config_path_ = config_path;
        //等待BS返回，所有操作等待5s后，
        //原有stunmodule中is_running_为false导致nat_type无法设置的bug也可以直接得到修复
        nat_timer_.start();

    }

    void NatCheckClient::Stop()
    {
        nat_timer_.stop();
        timer_.stop();
    }

    void NatCheckClient::OnTimerElapsed(framework::timer::Timer *timer)
    {
        if (timer == &nat_timer_)
        {
            StartCheck();
        }
        else if (timer == &timer_)
        {
            OnHandleTimeOut();
        }
        else
        {
            assert(false);
        }
    }

    void NatCheckClient::StartCheck()
    {
        nat_timer_.stop();

        protocol::MY_STUN_NAT_TYPE nat_type = protocol::TYPE_ERROR;

        //这些信息，是netcheck检查相关的，但是由于有些情况不用检查natcheck，但是需要检测upnp，所以这里提取出来。
        {
            string server_list = BootStrapGeneralConfig::Inst()->GetNatCheckServerIP();
            boost::algorithm::split(nat_check_server_list_, server_list,
                boost::algorithm::is_any_of("@"));

            assert(!nat_check_server_list_.empty());

            if (nat_check_server_list_.empty())
            {
                protocol::MY_STUN_NAT_TYPE nat_type = protocol::TYPE_ERROR;
                StunModule::Inst()->OnGetNATType(nat_type);
                return;
            }
            //均衡服务器压力
            std::random_shuffle(nat_check_server_list_.begin(), nat_check_server_list_.end());
            server_iter_ = nat_check_server_list_.begin();
            endpoint_ = framework::network::Endpoint(*(server_iter_++), 13478);

            LoadLocalIPs(local_ips_);
            //这里利用Windows Api取到的是网络序，发到服务器的地址需要转成主机序方便统计
            boost::uint32_t tmp_ip = base::util::GetLocalFirstIP();
#ifdef BOOST_WINDOWS_API
            local_first_ip_ = ntohl(tmp_ip); 
#else
            local_first_ip_ = tmp_ip;
#endif
            local_port_ = AppModule::Inst()->GetLocalUdpPort();
        }

        if (IsNeedToUpdateNat(nat_type, config_path_))
        {
            check_state_ = ISNATCHECKING;
            DoSendNatCheckpacket();
        }
        else
        {
            StunModule::Inst()->OnGetNATType(nat_type);
        }
    }

    void NatCheckClient::OnUdpReceive(protocol::ServerPacket const &packet)
    {
        timer_.stop();
        times_ = 0;

        switch (packet.PacketAction)
        {
        case protocol::NatCheckSameRoutePacket::Action:
            OnReceiveNatCheckSameRoutePacket((protocol::NatCheckSameRoutePacket const &)packet);
            break;
        case protocol::NatCheckDiffIpPacket::Action:
            OnReceiveNatCheckDiffIpPacket((protocol::NatCheckDiffIpPacket const &)packet);
            break;
        case protocol::NatCheckDiffPortPacket::Action:
            OnReceiveNatCheckDiffPortPacket((protocol::NatCheckDiffPortPacket const &)packet);
            break;
        case protocol::NatCheckForUpnpPacket::Action:
            OnReceiveNatCheckForUpnpPacket((protocol::NatCheckForUpnpPacket const &)packet);
            break;
        default:
            assert(false);
            break;
        }
    }

    void NatCheckClient::OnReceiveNatCheckSameRoutePacket(protocol::NatCheckSameRoutePacket const &packet)
    {
        if (check_state_ == ISNATCHECKING)
        {
            detect_port_ = packet.response.detect_udp_port_;
            detect_ip_ = packet.response.detected_ip_;

            //如果服务器检测地址及端口与本地地址一致，则表明并没有进行NAT转换
            if (std::find(local_ips_.begin(), local_ips_.end(), detect_ip_) != local_ips_.end() && detect_port_ == local_port_)
            {
                is_nat_ = false;
            }
            else
            {
                is_nat_ = true;
            }

            check_state_ = FULLCORNCHECKING;
            DoSendNatCheckpacket();
        }
        else if (check_state_ == SYMMETRICCHECKING)
        {
            if (detect_port_ != packet.response.detect_udp_port_ || detect_ip_ != packet.response.detected_ip_)
            {
                // peer endpoint1 和 peer endpoint2 是不同的，返回nat类型为 TYPE_SYMNAT，检测结束
                protocol::MY_STUN_NAT_TYPE nat_type = protocol::TYPE_SYMNAT;
                StunModule::Inst()->OnGetNATType(nat_type);
                
                WriteConfigAfterCheck(nat_type);
                check_state_ = COMPLETE;
            }
            else
            {
                //Restricted类型检测
                check_state_ = RESTRICTEDCHECKING;
                DoSendNatCheckpacket();
            }
        }
        else
        {
            assert(false);
        }
    }

    void NatCheckClient::OnReceiveNatCheckDiffIpPacket(protocol::NatCheckDiffIpPacket const &packet)
    {
        assert(check_state_ == FULLCORNCHECKING);

        protocol::MY_STUN_NAT_TYPE nat_type;

        if (is_nat_)
        {
            //收到回包。返回nat类型为 TYPE_FULLCONENAT
            nat_type = protocol::TYPE_FULLCONENAT;           
        }
        else
        {
            //公网环境
            nat_type = protocol::TYPE_PUBLIC;            
        }

        StunModule::Inst()->OnGetNATType(nat_type);

        WriteConfigAfterCheck(nat_type);
        check_state_ = COMPLETE;

    }

    void NatCheckClient::OnReceiveNatCheckDiffPortPacket(protocol::NatCheckDiffPortPacket const &packet)
    {
        assert(check_state_ == RESTRICTEDCHECKING);

        //收到回复，nat类型为TYPE_IP_RESTRICTEDNAT
        protocol::MY_STUN_NAT_TYPE nat_type = protocol::TYPE_IP_RESTRICTEDNAT;
        StunModule::Inst()->OnGetNATType(nat_type);

        WriteConfigAfterCheck(nat_type);
        check_state_ = COMPLETE;
    }

    void NatCheckClient::OnReceiveNatCheckForUpnpPacket(protocol::NatCheckForUpnpPacket const &packet)
    {
        assert(check_state_ == UPNPCHECKING);

        StunModule::Inst()->OnUpnpCheck(packet.response.send_udp_upnp_port_);

        WriteConfigAfterCheck(protocol::TYPE_FULLCONENAT);
        check_state_ = COMPLETE;
            
    }


    void NatCheckClient::DoSendNatCheckpacket()
    {
        switch (check_state_)
        {
        case ISNATCHECKING:
            {
                //检测客户端是否有能力进行UDP通信
                // 向服务器（ip1，port1）发包，要求返回服务器看到的peer endpoint
                protocol::NatCheckSameRoutePacket packet(
                    times_,
                    local_first_ip_,
                    local_port_,
                    protocol::Packet::NewTransactionID(),
                    protocol::PEER_VERSION,
                    endpoint_
                    );

                AppModule::Inst()->DoSendPacket(packet);
                break;
            }
        case FULLCORNCHECKING:
            {
                //向服务器（ip1，port1）发包，要求返回服务器用其他的endpoint（ip2，port2）返回
                protocol::NatCheckDiffIpPacket packet(
                    times_,
                    local_first_ip_,
                    local_port_,
                    protocol::Packet::NewTransactionID(),
                    protocol::PEER_VERSION,
                    endpoint_
                    );

                AppModule::Inst()->DoSendPacket(packet);
                break;
            }
        case SYMMETRICCHECKING:
            {
                //向服务器的(ip2,port2)发请求，要求返回服务器看到的peer endpoint2
                protocol::NatCheckSameRoutePacket packet2(
                    times_,
                    local_first_ip_,
                    local_port_,
                    protocol::Packet::NewTransactionID(),
                    protocol::PEER_VERSION,
                    endpoint__symncheck_
                    );

                AppModule::Inst()->DoSendPacket(packet2);
                break;
            }
        case RESTRICTEDCHECKING:
            {
                //向服务器(ip1,port1)发请求，要求服务用(ip1,port2)进行回应
                protocol::NatCheckDiffPortPacket packet(
                    times_,
                    local_first_ip_,
                    local_port_,
                    protocol::Packet::NewTransactionID(),
                    protocol::PEER_VERSION,
                    endpoint_
                    );
                AppModule::Inst()->DoSendPacket(packet);
                break;
            }
        case UPNPCHECKING:
            {
                //向服务器(ip1,port1)发请求，要求服务用(ip2,port2)进行回应,且回复到upnp_ex_udp_port_ 端口上
                protocol::NatCheckForUpnpPacket packet(
                    times_,
                    local_first_ip_,
                    local_port_,
                    upnp_ex_udp_port_,
                    protocol::Packet::NewTransactionID(),
                    protocol::PEER_VERSION,
                    endpoint_
                    );
                AppModule::Inst()->DoSendPacket(packet);
                break;
            }
        default:
            break;
        }

        timer_.start();
    }

    void NatCheckClient::OnHandleTimeOut()
    {
        timer_.stop();

        if (times_ < 3)
        {
            times_++;
            DoSendNatCheckpacket();
            return;
        }

        protocol::MY_STUN_NAT_TYPE nat_type;
        switch (check_state_)
        {
        case ISNATCHECKING:
            {
                if (server_iter_ != nat_check_server_list_.end())
                {
                    times_ = 0;
                    endpoint_ = framework::network::Endpoint(*(server_iter_++), 13478);
                    DoSendNatCheckpacket();
                }
                else
                {
                    //不具备udp发包能力
                    nat_type = protocol::TYPE_ERROR;
                    StunModule::Inst()->OnGetNATType(nat_type);
                    WriteConfigAfterCheck(nat_type);
                }

                break;
            }
        case FULLCORNCHECKING:
            {
                if (is_nat_ == true)
                {
                    // SYMMETRIC检测
                    times_ = 0;
                    check_state_ = SYMMETRICCHECKING;
                    if (server_iter_ != nat_check_server_list_.end())
                    {
                        endpoint__symncheck_ = framework::network::Endpoint(*(server_iter_++), 13478);
                        DoSendNatCheckpacket();
                    }
                    else
                    {
                        protocol::MY_STUN_NAT_TYPE nat_type = protocol::TYPE_ERROR;
                        StunModule::Inst()->OnGetNATType(nat_type);
                        WriteConfigAfterCheck(nat_type);
                    }
                }
                else
                {
                    //某些公网的防火墙设置会导致与nat类型中的SYMNAT表现一致
                    protocol::MY_STUN_NAT_TYPE nat_type = protocol::TYPE_SYMNAT;
                    StunModule::Inst()->OnGetNATType(nat_type);

                    WriteConfigAfterCheck(nat_type);
                    check_state_ = COMPLETE;
                }

                break;
            }
        case SYMMETRICCHECKING:
            {
                if (server_iter_ == nat_check_server_list_.end())
                {
                    //不能收到回应，返回nat类型为type_error，检测结束
                    protocol::MY_STUN_NAT_TYPE nat_type = protocol::TYPE_ERROR;
                    StunModule::Inst()->OnGetNATType(nat_type);
                    WriteConfigAfterCheck(nat_type);
                }
                else
                {
                    times_ = 0;
                    endpoint__symncheck_ = framework::network::Endpoint(*(server_iter_++), 13478);
                    DoSendNatCheckpacket();
                }
                break;
            }
        case RESTRICTEDCHECKING:
            {
                //3次无回复，nat类型为 TYPE_IP_PORT_RESTRICTEDNAT
                protocol::MY_STUN_NAT_TYPE nat_type = protocol::TYPE_IP_PORT_RESTRICTEDNAT;
                StunModule::Inst()->OnGetNATType(nat_type);

                WriteConfigAfterCheck(nat_type);
                check_state_ = COMPLETE;
                break;
            }
        case UPNPCHECKING:
            {
                //upnp检测失败，nat类型保持不变
                StunModule::Inst()->OnUpnpCheck(0);
                check_state_ = COMPLETE;
                break;
            }
        default:
            break;
        }
    }

    void NatCheckClient::CheckForUpnp(boost::uint16_t innerUdpPort,boost::uint16_t exUdpPort)
    {
        check_state_ = UPNPCHECKING;
        upnp_ex_udp_port_ = exUdpPort;
        DoSendNatCheckpacket(); 
    }

    /*
    // 判断是否需要使用stun协议检测NAT信息，同时将保存的nat信息输出到snt_result
    // 需要更新NAT信息则返回true；否则返回false
    需要更新NAT的条件为：
    1.保存的nattype为error
    2.获取的本地ip地址与保存的地址不同
    3.保存信息已经过期(超过3天)
    */
    bool NatCheckClient::IsNeedToUpdateNat(protocol::MY_STUN_NAT_TYPE &snt_result, const string& config_path)
    {
        if (config_path.length() == 0)
        {
            return false;
        }

        boost::filesystem::path filepath(config_path);
        filepath /= ("ppvaconfig.ini");
        string filename = filepath.file_string();

        try
        {
            framework::configure::Config conf(filename);
            framework::configure::ConfigModule & ppva_conf = conf.register_module("PPVA");
            m_strConfig = filename;

            int LastTime = 0;
            boost::uint32_t LastLocalIP = 0;

            ppva_conf(CONFIG_PARAM_NAME_RDONLY("NTYPE", snt_result));
            if (snt_result < protocol::TYPE_FULLCONENAT || snt_result > protocol::TYPE_PUBLIC)
            {
                return true;
            }

            ppva_conf(CONFIG_PARAM_NAME_RDONLY("NIP", LastLocalIP));
            if (LastLocalIP != base::util::GetLocalFirstIP())
            {
                return true;
            }

            ppva_conf(CONFIG_PARAM_NAME_RDONLY("NTIME", LastTime));

            boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
            // 如果日期相差三天，则判断已经过期
            if (abs(now.date().day() - LastTime) >= BootStrapGeneralConfig::Inst()->GetNatNeedCheckMinDays())
            {
                return true;
            }

        }
        catch(...)
        {
            base::filesystem::remove_nothrow(filename);
        }

        return false;
    }

    void NatCheckClient::WriteConfigAfterCheck(protocol::MY_STUN_NAT_TYPE nat_type)
    {
        boost::filesystem::path configpath(m_strConfig);
        string filename = configpath.file_string();
        int LastTime = 0;
        boost::uint32_t LastLocalIP = 0;
        protocol::MY_STUN_NAT_TYPE nat_result;
        string nat_string;

        try
        {
            // 保存获取到的NAT信息
            framework::configure::Config conf(filename);
            framework::configure::ConfigModule & ppva_conf = conf.register_module("PPVA");

            ppva_conf(CONFIG_PARAM_NAME_RDONLY("NTYPE", nat_result));
            nat_result = nat_type;

            // 保存当天的时间
            boost::posix_time::ptime now = boost::posix_time::second_clock::local_time();
            ppva_conf(CONFIG_PARAM_NAME_RDONLY("NTIME", LastTime));
            LastTime = now.date().day();

            // 保存获取的本地IP
            ppva_conf(CONFIG_PARAM_NAME_RDONLY("NIP", LastLocalIP));
            LastLocalIP = base::util::GetLocalFirstIP();

            ppva_conf(CONFIG_PARAM_NAME_RDONLY("NATTYPE", nat_string));

            switch (nat_type)
            {
            case protocol::TYPE_ERROR:
                nat_string = "protocol::TYPE_ERROR";
                break;
            case  protocol::TYPE_PUBLIC:
                nat_string = "protocol::TYPE_PUBLIC";
                break;
            case protocol::TYPE_FULLCONENAT:
                nat_string = "protocol::TYPE_FULLCONENAT";
                break;
            case protocol::TYPE_SYMNAT:
                nat_string = "protocol::TYPE_SYMNAT";
                break;
            case protocol::TYPE_IP_RESTRICTEDNAT:
                nat_string = "protocol::TYPE_RESTRICTEDNAT";
                break;
            case protocol::TYPE_IP_PORT_RESTRICTEDNAT:
                nat_string = "protocol::TYPE_IP_PROT_RESTRICTEDNAT";
                break;
            default:
                break;
            }
            conf.sync();
        }
        catch(...)
        {
            base::filesystem::remove_nothrow(filename);
        }
    }
}