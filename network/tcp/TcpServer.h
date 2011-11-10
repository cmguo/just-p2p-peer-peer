#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include "TcpConnection.h"

namespace network
{
    class TcpServer
    {
    public:
        TcpServer(boost::asio::io_service & io_service)
            : acceptor_(io_service)
        {
        }

        bool Start(boost::uint32_t port)
        {
            tcp_port_ = port;

            boost::system::error_code error;

            boost::asio::ip::tcp::endpoint tcp_endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port);

            acceptor_.open(tcp_endpoint.protocol(), error);

            if (error)
            {
                assert(false);
                return false;
            }

            boost::asio::socket_base::reuse_address option(true);
            acceptor_.set_option(option, error);

            if (error)
            {
                assert(false);
                return false;
            }

            acceptor_.bind(tcp_endpoint, error);
            if (error)
            {
                assert(false);
                return false;
            }

            acceptor_.listen(0, error);

            if (error)
            {
                assert(false);
                return false;
            }

            if (packet_handlers_.empty())
            {
                assert(false);
                return false;
            }

            StartAccept();
            return true;
        }

        template <typename PacketType>
        void RegisterPacket()
        {
            boost::uint8_t action = PacketType::Action;
            packet_handlers_[action] = &TcpConnection::HandlePacket<PacketType>;
        }

        boost::uint16_t GetTcpPort() const
        {
            return tcp_port_;
        }

    private:
        void StartAccept()
        {
            TcpConnection::pointer new_connection = TcpConnection::create(acceptor_.get_io_service(), packet_handlers_);

            acceptor_.async_accept(new_connection->socket(),
                boost::bind(&TcpServer::HandleAccept, this, new_connection,
                boost::asio::placeholders::error));
        }

        void HandleAccept(TcpConnection::pointer new_connection,
            const boost::system::error_code& error)
        {
            if (!error)
            {
                new_connection->DoRecv();
            }

            StartAccept();
        }
    private:
        boost::uint16_t tcp_port_;

        boost::asio::ip::tcp::acceptor acceptor_;

        typedef void (TcpConnection::* packet_handler_type)(std::istream &);
        std::map<boost::uint8_t, packet_handler_type> packet_handlers_;
    };
}

#endif
