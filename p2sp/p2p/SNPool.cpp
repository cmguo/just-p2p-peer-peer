#include "Common.h"
#include "SNPool.h"

namespace p2sp
{
    boost::shared_ptr<SNPool> SNPool::inst_;

    void SNPool::AddSN(const std::vector<protocol::SuperNodeInfo> & sn_server_list)
    {
        for (std::vector<protocol::SuperNodeInfo>::const_iterator iter = sn_server_list.begin();
            iter != sn_server_list.end(); ++iter)
        {
            boost::asio::ip::udp::endpoint ep = framework::network::Endpoint(
                (*iter).ip_, (*iter).port_);

            sn_server_list_.push_back(ep);
        }
    }

    const std::list<boost::asio::ip::udp::endpoint> & SNPool::GetAllSNList()
    {
        return sn_server_list_;
    }

    void SNPoolObject::Add(const std::list<boost::asio::ip::udp::endpoint> & sn_list)
    {
        bool is_sn_list_empty = sn_server_list_.empty();

        for (std::list<boost::asio::ip::udp::endpoint>::const_iterator iter = sn_list.begin();
            iter != sn_list.end(); ++iter)
        {
            if (std::find(sn_server_list_.begin(), sn_server_list_.end(), *iter) == sn_server_list_.end())
            {
                sn_server_list_.push_back(*iter);
            }
        }

        if (is_sn_list_empty)
        {
            sn_iter_ = sn_server_list_.begin();
        }
    }

    bool SNPoolObject::IsHaveReserveSn()
    {
        if (sn_server_list_.empty())
        {
            return false;
        }

        return !(sn_iter_ == sn_server_list_.end());
    }

    boost::asio::ip::udp::endpoint SNPoolObject::GetReserveSn()
    {
        assert(sn_iter_ != sn_server_list_.end());
        return *sn_iter_++;
    }

    bool SNPoolObject::IsSn(const boost::asio::ip::udp::endpoint & end_point)
    {
        return std::find(sn_server_list_.begin(), sn_server_list_.end(), end_point) != sn_server_list_.end();
    }
}