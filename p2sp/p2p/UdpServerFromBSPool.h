#ifndef UDPSERVER_FROM_BS_POOL_H
#define UDPSERVER_FROM_BS_POOL_H

namespace p2sp
{
    class UdpServerFromBSPool
    {
    public:
        static boost::shared_ptr<UdpServerFromBSPool> Inst()
        {
            if (!inst_)
            {
                inst_.reset(new UdpServerFromBSPool());
            }

            return inst_;
        }

        void AddUdpServer(const std::vector<protocol::UdpServerInfo> & udpserver_list);
        const std::vector<protocol::CandidatePeerInfo> & GetUdpServerList();
        boost::uint32_t GetUdpServerCount() const;

    private:
        UdpServerFromBSPool()
        {
        }

        void AddCandidatePeerInfo(const protocol::UdpServerInfo & udpserver_info);

    private:
        std::vector<protocol::CandidatePeerInfo> udpserver_list_;
        std::set<boost::asio::ip::udp::endpoint> udpserver_endpoint_;
        static boost::shared_ptr<UdpServerFromBSPool> inst_;
        static const boost::uint8_t DefaultUdpServerUploadPriority;
        static const boost::uint8_t DefaultUdpServerIdleTime;
        static const boost::uint8_t DefaultUdpServerTrackerPriority;
    };
}

#endif  // UDPSERVER_FROM_BS_POOL_H
