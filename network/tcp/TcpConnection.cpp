#include "Common.h"
#include "p2sp/AppModule.h"
#include "network/HttpRequest.h"
#include "network/tcp/CrossDomainConfig.h"
#include <boost/algorithm/string.hpp>

namespace network
{
    void TcpConnection::OnPacketRecv(protocol::Packet const & packet)
    {
        p2sp::AppModule::Inst()->OnPacketRecv(packet);
    }

    void TcpConnection::DoRecv()
    {
        string delim = "<policy-file-request/>|\r\n\r\n";
        boost::regex expression(delim);
        
        boost::asio::async_read_until(socket_, request_, expression,
            boost::bind(&TcpConnection::HandleRecv, shared_from_this(),
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
    }

    void TcpConnection::HandleRecv(const boost::system::error_code& ec, size_t bytes_transferred)
    {
        if (!ec)
        {
            boost::asio::const_buffers_1 bufs = request_.data();
            std::string request_string(boost::asio::buffers_begin(bufs),
                boost::asio::buffers_begin(bufs) + bytes_transferred);
            
            if (boost::algorithm::istarts_with(request_string, "<policy-file-request/>") == true)
            {
                string cross_domain_xml = CrossDomainConfig::GetInstance()->GetCrossDomainString();

                boost::shared_ptr<boost::asio::streambuf> response 
                    = boost::shared_ptr<boost::asio::streambuf> (new boost::asio::streambuf());

                std::ostream oa(response.get());
                oa << cross_domain_xml;

                boost::uint8_t eof = 0;
                oa << eof;

                assert(send_list_.empty());

                send_list_.push_back(response);

                // 发送完成之后断开连接
                TcpSend(response, true);

                request_.consume(bytes_transferred);

                DoRecv();

                return;
            }

            std::istream is(&request_);
            boost::uint8_t action = is.get();

            std::map<boost::uint8_t, packet_handler_type>::const_iterator iter =
                packet_handlers_.find(action);

            if (iter != packet_handlers_.end())
            {
                (this->*iter->second)(is);
            }
            else
            {
                assert(false);
            }

            // 将未解析的\r\n\r\n读完，否会还会产生一次回调
            request_.consume(4);

            DoRecv();
        }
        else
        {
            // Handle the Connection close
            protocol::TcpClosePacket tcp_close_packet;
            tcp_close_packet.tcp_connection_ = shared_from_this();
            p2sp::AppModule::Inst()->OnPacketRecv(tcp_close_packet);
        }
    }

    void TcpConnection::TcpSend(boost::shared_ptr<boost::asio::streambuf> buffer,
        bool need_close)
    {
        boost::asio::async_write(socket_, *buffer,
            boost::bind(&TcpConnection::HandleTcpSend, shared_from_this(),
            need_close,
            boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
    }

    void TcpConnection::HandleTcpSend(bool need_close, const boost::system::error_code& ec,
        size_t bytes_transferred)
    {
        if (ec)
        {
            return;
        }

        if (need_close)
        {
            boost::system::error_code ec;
            socket_.close(ec);
            return;
        }

        send_list_.pop_front();

        if (!send_list_.empty())
        {
            TcpSend(send_list_.front(), false);
        }
    }
}