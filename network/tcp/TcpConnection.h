#ifndef TCP_CONNECTION_H
#define TCP_CONNECTION_H

#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>

#include <util/archive/LittleEndianBinaryIArchive.h>
#include <util/archive/LittleEndianBinaryOArchive.h>

namespace network
{
    typedef util::archive::LittleEndianBinaryIArchive<char> ITcpArchive;
    typedef util::archive::LittleEndianBinaryOArchive<char> OTcpArchive;

    class TcpConnection
        : public boost::enable_shared_from_this<TcpConnection>
    {
    public:
        typedef boost::shared_ptr<TcpConnection> pointer;
        typedef void (TcpConnection::* packet_handler_type)(std::istream &);

        static pointer create(
            boost::asio::io_service & io_service,
            const std::map<boost::uint8_t, packet_handler_type> & packet_handlers
            )
        {
            return pointer(new TcpConnection(io_service, packet_handlers));
        }

        boost::asio::ip::tcp::socket& socket()
        {
            return socket_;
        }

        void DoRecv();

        template <typename PacketType>
        void DoSend(const PacketType & packet)
        {
            boost::shared_ptr<boost::asio::streambuf> response 
                = boost::shared_ptr<boost::asio::streambuf> (new boost::asio::streambuf());

            OTcpArchive oa(*response);
            boost::uint8_t action = PacketType::Action;

            oa << action << packet;

            boost::uint8_t eof_r = 13;
            boost::uint8_t eof_n = 10;

            oa << eof_r << eof_n << eof_r << eof_n;

            bool is_send_list_empty = send_list_.empty();
            send_list_.push_back(response);

            if (is_send_list_empty)
            {
                TcpSend(response, false);
            }
        }

        template <typename PacketType>
        void HandlePacket(std::istream & is)
        {
            ITcpArchive ia(is);
            PacketType packet;
            packet.PacketAction = PacketType::Action;
            ia >> packet;

            packet.tcp_connection_ = shared_from_this();

            p2sp::AppModule::Inst()->OnPacketRecv(packet);
        }

    private:
        TcpConnection(boost::asio::io_service & io_service,
            const std::map<boost::uint8_t, packet_handler_type> & packet_handles)
            : socket_(io_service)
            , packet_handlers_(packet_handles)
        {
        }

        void HandleRecv(const boost::system::error_code& ec, size_t bytes_transferred);

        // 异步发送buffer，need_close 发送完成之后是否断开连接
        void TcpSend(boost::shared_ptr<boost::asio::streambuf> buffer, bool need_close);

        void HandleTcpSend(bool need_close, const boost::system::error_code& ec,
            size_t bytes_transferred);

    private:
        boost::asio::ip::tcp::socket socket_;
        boost::asio::streambuf request_;
        std::deque<boost::shared_ptr<boost::asio::streambuf> > send_list_;
        std::map<boost::uint8_t, packet_handler_type> packet_handlers_;
    };
}

#endif
