#ifndef _NETWORK_PING_CLIENT_H_
#define _NETWORK_PING_CLIENT_H_

#include "icmp_header.h"

namespace network
{
    class PingClient
        : public boost::enable_shared_from_this<PingClient>
    {
    public:
        typedef boost::shared_ptr<PingClient> p;
        static p Create(boost::asio::io_service & io_svc);

        void Bind(const string & destination_ip);
        template <typename ResponseHandler>
        uint16_t AsyncRequest(ResponseHandler handler)
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
                assert(handler_map_.find(sequence_num_) == handler_map_.end());
                handler_map_.insert(std::make_pair(sequence_num_, handler));

                Receive();
            }
            
            return sequence_num_;
        }

        void Cancel(uint16_t sequence_num);
        void CancelAll();
        bool SetTtl(int32_t ttl);

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
        std::map<uint16_t, boost::function<void (unsigned char, string)> > handler_map_;
        boost::asio::ip::icmp::endpoint destination_endpoint_;

        static uint16_t sequence_num_;

        bool is_ttl_supported_;
        bool is_ttl_support_tested_;
    };
}

#endif