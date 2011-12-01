//PingClient.cpp

#include "Common.h"
#include "PingClient.h"
#include "ipv4_header.h"

namespace network
{
    uint16_t PingClient::sequence_num_ = 0;

    PingClient::p PingClient::Create(boost::asio::io_service & io_svc)
    {
        return PingClient::p(new PingClient(io_svc));
    }

    PingClient::PingClient(boost::asio::io_service & io_svc)
        : resolver_(io_svc)
        , socket_(io_svc, boost::asio::ip::icmp::v4())
        , is_receiving_(false)
        , is_ttl_support_tested_(false)
        , is_ttl_supported_(false)
    {
    }

    uint16_t PingClient::AsyncRequest(boost::function<void(unsigned char, string, boost::uint32_t)> handler)
    {
        static string ping_request_body("Hello");

        ++sequence_num_;

        icmp_header request;
        request.type(icmp_header::echo_request);
        request.code(0);
        request.identifier(GetIdentifier());
        request.sequence_number(sequence_num_);
        compute_checksum(request, ping_request_body.begin(), ping_request_body.end());

        boost::asio::streambuf request_buf;
        std::ostream os(&request_buf);
        os << request << ping_request_body;

        boost::system::error_code ec;
        socket_.send_to(request_buf.data(), destination_endpoint_, 0, ec);
        if (!ec)
        {
            AddHandler(sequence_num_, handler);
            Receive();
        }

        return sequence_num_;
    }

    bool PingClient::Bind(const string & destination_ip)
    {
        boost::system::error_code ec;
        boost::asio::ip::address_v4 ip = boost::asio::ip::address_v4::from_string(destination_ip, ec);

        if (ec)
        {
            // LSP可能会导致错误
            // 目前已知的错误码包括10104,10022等
            // 迅雷的LSP和WPS的LSP可能会有这个问题
            return false;
        }

        destination_endpoint_ = boost::asio::ip::icmp::endpoint(ip, 0);

        return true;
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
                    if (icmp_hdr.identifier() == GetIdentifier())
                    {
                        NotifyHandler(icmp_hdr.sequence_number(), icmp_hdr.type(),
                            ipv4_hdr.source_address().to_string());
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
                            NotifyHandler(icmp_hdr_src.sequence_number(), icmp_hdr.type(),
                                ipv4_hdr.source_address().to_string());
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
            DebugLog("PingClient::HandleReceive error:%s\n", error_code.message().c_str());
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
            DebugLog("SetTtl test %d\n", is_ttl_supported_);
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
            DebugLog("SetTtl tryget:%d, current_ttl:%d, ttl %d\n", can_get_ttl, current_ttl, ttl);
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
