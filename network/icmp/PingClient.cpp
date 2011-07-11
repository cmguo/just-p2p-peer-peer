//PingClient.cpp

#include "Common.h"
#include "PingClient.h"
#include "ipv4_header.h"

namespace network
{
    uint16_t PingClient::sequence_num_ = 0;

    PingClient::p PingClient::Create(boost::asio::io_service & io_svc)
    {
        try
        {
            return PingClient::p(new PingClient(io_svc));
        }
        catch(boost::system::system_error & e)
        {
            DebugLog("upload create ping client failed ec:%d, %s", e.code().value(), e.what());
            return PingClient::p();
        }
    }

    PingClient::PingClient(boost::asio::io_service & io_svc)
        : resolver_(io_svc), socket_(io_svc, boost::asio::ip::icmp::v4()), is_receiving_(false)
        , is_ttl_support_tested_(false), is_ttl_supported_(false)
    {
    }

    void PingClient::Bind(const string & destination_ip)
    {
        destination_endpoint_ = 
            boost::asio::ip::icmp::endpoint(boost::asio::ip::address_v4::from_string(destination_ip), 0);
    }

    void PingClient::Cancel(uint16_t sequence_num)
    {
        handler_map_.erase(sequence_num);
    }

    void PingClient::CancelAll()
    {
        handler_map_.clear();
    }

    void PingClient::Receive()
    {
        if (!is_receiving_)
        {
            is_receiving_ = true;
            socket_.async_receive(recv_buffer_.prepare(65535),
                boost::bind(&PingClient::HandleReceive, shared_from_this(),
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
        }
    }

    void PingClient::HandleReceive(const boost::system::error_code & error_code, uint32_t bytes_transfered)
    {
        if (!error_code)
        {
            recv_buffer_.commit(bytes_transfered);

            std::istream is(&recv_buffer_);
            ipv4_header ipv4_hdr;
            icmp_header icmp_hdr;
            is >> ipv4_hdr >> icmp_hdr;

            if (is)
            {
                switch(icmp_hdr.type())
                {
                case icmp_header::echo_reply:
                    if (icmp_hdr.identifier() == GetIdentifier() &&
                        handler_map_.find(icmp_hdr.sequence_number()) != handler_map_.end())
                    {
                        handler_map_[icmp_hdr.sequence_number()](icmp_hdr.type(), ipv4_hdr.source_address().to_string());
                        handler_map_.erase(icmp_hdr.sequence_number());
                    }
                    break;
                case icmp_header::destination_unreachable:
                case icmp_header::time_exceeded:
                    {
                        ipv4_header ipv4_hdr_src;
                        icmp_header icmp_hdr_src;
                        is >> ipv4_hdr_src >> icmp_hdr_src;
                        if (is && icmp_hdr_src.type() == icmp_header::echo_request
                            && icmp_hdr_src.identifier() == GetIdentifier())
                        {
                            if (handler_map_.find(icmp_hdr_src.sequence_number()) != handler_map_.end())
                            {
                                handler_map_[icmp_hdr_src.sequence_number()](icmp_hdr.type(), ipv4_hdr.source_address().to_string());
                                handler_map_.erase(icmp_hdr_src.sequence_number());
                            }
                        }
                    }
                    break;
                default:
                    assert(false);
                }
            }
        }  
        else
        {
            DebugLog("PingClient::HandleReceive error:%s", error_code.message().c_str());
        }
        
        recv_buffer_.consume(bytes_transfered);
        socket_.async_receive(recv_buffer_.prepare(65535),
            boost::bind(&PingClient::HandleReceive, shared_from_this(),
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
    }

    unsigned short PingClient::GetIdentifier()
    {
#ifdef BOOST_WINDOWS_API
        return static_cast<unsigned short>(::GetCurrentProcessId());
#else
        return static_cast<unsigned short>(::getpid());
#endif
    }

    bool PingClient::SetTtl(int32_t ttl)
    {
        if (!is_ttl_support_tested_)
        {
            int current_ttl = 0;
            is_ttl_supported_ = TryGetCurrentTtl(current_ttl);
            is_ttl_support_tested_ = true;
            DebugLog("SetTtl test %d", is_ttl_supported_);
        }

        if (is_ttl_supported_)
        {
            // windows and linux both have the setsockopt api with the same parameter definition.
            if(setsockopt(socket_.native(), IPPROTO_IP, IP_TTL, (char*)&ttl, sizeof(int32_t)) != 0)
            {
                return false;
            }
            
            int current_ttl = 0;
            bool can_get_ttl = TryGetCurrentTtl(current_ttl);
            assert(can_get_ttl);
            DebugLog("SetTtl tryget:%d, current_ttl:%d, ttl %d", can_get_ttl, current_ttl, ttl);
            return current_ttl == ttl;
        }
        else
        {
            return false;
        }
    }

    bool PingClient::TryGetCurrentTtl(int & ttl)
    {
        // windows and linux both have the getsockopt api with the similar parameter definition.
#ifdef BOOST_WINDOWS_API
        int opt_len = sizeof(int);
#else
        uint32_t opt_len = sizeof(int);
#endif
        return (getsockopt(socket_.native(), IPPROTO_IP, IP_TTL, (char*)&ttl, &opt_len) == 0);
    }
}
