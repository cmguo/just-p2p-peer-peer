#include "Common.h"
#include "p2sp/AppModule.h"
#include "UdpServerFromBSPool.h"

namespace p2sp
{
    boost::shared_ptr<UdpServerFromBSPool> UdpServerFromBSPool::inst_;
    const boost::uint8_t UdpServerFromBSPool::DefaultUdpServerUploadPriority = 255;
    const boost::uint8_t UdpServerFromBSPool::DefaultUdpServerIdleTime = 0;
    const boost::uint8_t UdpServerFromBSPool::DefaultUdpServerTrackerPriority = 50;

    void UdpServerFromBSPool::AddUdpServer(const std::vector<protocol::UdpServerInfo> & udpserver_list)
    {
        for (std::vector<protocol::UdpServerInfo>::const_iterator iter = udpserver_list.begin();
            iter != udpserver_list.end();
            ++iter)
        {
            boost::asio::ip::udp::endpoint ep(boost::asio::ip::address_v4(iter->ip_), iter->port_);
            if (udpserver_endpoint_.find(ep) == udpserver_endpoint_.end())
            {
                udpserver_endpoint_.insert(ep);
                AddCandidatePeerInfo(*iter);
            }
        }
    }

    const std::vector<protocol::CandidatePeerInfo> & UdpServerFromBSPool::GetUdpServerList()
    {
        return udpserver_list_;
    }

    void UdpServerFromBSPool::AddCandidatePeerInfo(const protocol::UdpServerInfo & udpserver_info)
    {
        protocol::CandidatePeerInfo candidate_udpserver_info(
            udpserver_info.ip_,
            udpserver_info.port_,
            (boost::uint16_t)AppModule::Inst()->GetPeerVersion(),
            udpserver_info.ip_,
            udpserver_info.port_,
            0,
            0,
            protocol::TYPE_FULLCONENAT,
            DefaultUdpServerUploadPriority,
            DefaultUdpServerIdleTime,
            DefaultUdpServerTrackerPriority);

        udpserver_list_.push_back(candidate_udpserver_info);
    }

    boost::uint32_t UdpServerFromBSPool::GetUdpServerCount() const
    {
        return udpserver_list_.size();
    }
}
