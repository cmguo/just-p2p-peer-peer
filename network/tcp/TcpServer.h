#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include "TcpConnection.h"

namespace network
{
    class TcpServer
        : public boost::enable_shared_from_this<TcpServer>
    {
    public:
        TcpServer(boost::asio::io_service & io_service)
            : acceptor_(io_service)
        {
        }

        bool Start(boost::uint32_t port)
        {
            if (is_running_)
            {
                return false;
            }

            is_running_ = true;

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

            StartAccept();
            return true;
        }

        void Stop()
        {
            if (!is_running_)
            {
                return;
            }

            is_running_ = false;

            boost::system::error_code ec;
            acceptor_.cancel(ec);
            acceptor_.close(ec);
        }

        template <typename PacketType>
        void RegisterPacket()
        {
            if (!is_running_)
            {
                return;
            }

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
            if (!is_running_)
            {
                return;
            }

            TcpConnection::pointer new_connection = TcpConnection::create(acceptor_.get_io_service(), packet_handlers_);

            acceptor_.async_accept(new_connection->socket(),
                boost::bind(&TcpServer::HandleAccept, shared_from_this(), new_connection,
                boost::asio::placeholders::error));
        }

        void HandleAccept(TcpConnection::pointer new_connection,
            const boost::system::error_code& error)
        {
            if (!is_running_)
            {
                return;
            }

            if (error)
            {
                return;
                
            }

            new_connection->DoRecv();
            StartAccept();
        }
    private:
        boost::uint16_t tcp_port_;

        boost::asio::ip::tcp::acceptor acceptor_;

        typedef void (TcpConnection::* packet_handler_type)(std::istream &);
        std::map<boost::uint8_t, packet_handler_type> packet_handlers_;

        bool is_running_;
    };
}

#endif
