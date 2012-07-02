#ifndef SN_POOL_H
#define SN_POOL_H

namespace p2sp
{
    class SNPool
    {
    public:
        static boost::shared_ptr<SNPool> Inst()
        {
            if (!inst_)
            {
                inst_.reset(new SNPool());
            }
            
            return inst_;
        }

        void AddSN(const std::vector<protocol::SuperNodeInfo> & sn_server_list);
        void AddVipSN(const std::vector<protocol::SuperNodeInfo> &vip_sn_server_list);
        const std::list<boost::asio::ip::udp::endpoint> & GetAllSNList();
        const std::list<boost::asio::ip::udp::endpoint> & GetVipSnList();

    private:
        SNPool()
        {
        }

        static boost::shared_ptr<SNPool> inst_;

        std::list<boost::asio::ip::udp::endpoint> sn_server_list_;
        std::list<boost::asio::ip::udp::endpoint> vip_sn_server_list_;
    };

    class SNPoolObject
    {
    public:
        void Add(const std::list<boost::asio::ip::udp::endpoint> & sn_list);
        bool IsSn(const boost::asio::ip::udp::endpoint & end_point);
        bool IsHaveReserveSn();
        boost::asio::ip::udp::endpoint GetReserveSn();

    private:
        std::list<boost::asio::ip::udp::endpoint> sn_server_list_;
        std::list<boost::asio::ip::udp::endpoint>::iterator sn_iter_;
    };
}

#endif