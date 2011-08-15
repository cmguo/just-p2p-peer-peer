#ifndef _NETWORK_PING_CLIENT_H_
#define _NETWORK_PING_CLIENT_H_

#include "icmp_header.h"
#include "PingClientBase.h"

namespace network
{
    class PingClient
        : public PingClientBase
        , public boost::enable_shared_from_this<PingClient>
    {
    public:
        typedef boost::shared_ptr<PingClient> p;
        static p Create(boost::asio::io_service & io_svc);

        virtual bool Bind(const string & destination_ip);

        virtual uint16_t AsyncRequest(boost::function<void(unsigned char, string)> handler);

        virtual bool SetTtl(int32_t ttl);

    private:
        PingClient(boost::asio::io_service & io_svc);

        static unsigned short GetIdentifier();
        void Receive();
        void HandleReceive(const boost::system::error_code & error_code, uint32_t bytes_transfered);
        bool TryGetCurrentTtl(int & ttl);

    private:
        boost::asio::ip::icmp::socket socket_;
        boost::asio::ip::icmp::resolver resolver_;
        bool is_receiving_;
        boost::asio::streambuf recv_buffer_;
        boost::asio::ip::icmp::endpoint destination_endpoint_;

        static uint16_t sequence_num_;

        bool is_ttl_supported_;
        bool is_ttl_support_tested_;
    };
}

#endif