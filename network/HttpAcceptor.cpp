//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "network/HttpAcceptor.h"
#include "network/HttpServer.h"

#include <boost/asio/placeholders.hpp>

namespace network
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("http_acceptor");

    HttpAcceptor::HttpAcceptor(boost::asio::io_service & io_svc, IHttpAcceptorListener::p handler)
        : handler_(handler)
        , acceptor_(io_svc)
        , is_open_(false)
        , port_(0)
        , delay_once_timer_(global_second_timer(), 1000, boost::bind(&HttpAcceptor::OnTimerElapsed, this, &delay_once_timer_))
    {
    }

    bool HttpAcceptor::Listen(const boost::asio::ip::tcp::endpoint& ep)
    {
        if (is_open_ == true)
            return false;

        boost::system::error_code error;

        acceptor_.open(ep.protocol(), error);
        boost::asio::socket_base::reuse_address option(true);
        acceptor_.set_option(option, error);

        if (error)
        {
            LOG(__ERROR, "http_acceptor", __FUNCTION__ << " Open Error: " << error.message());
            Close();
            return false;
        }

        acceptor_.bind(ep, error);

        if (!error)  // !!
        {
            boost::system::error_code err;
            acceptor_.listen(0, err);
            if (!err)  // !!
            {
                port_ = ep.port();
                is_open_ = true;
                return true;
            }
            else
            {
                LOG(__ERROR, "http_acceptor", __FUNCTION__ << " Listen Error: " << err.message());
                Close();
                return false;
            }
        }
        else
        {
            LOG(__ERROR, "http_acceptor", __FUNCTION__ << " Bind Error: " << error.message());
            Close();
            return false;
        }
    }

    bool HttpAcceptor::Listen(boost::uint16_t port)
    {
        boost::asio::ip::tcp::endpoint ep(boost::asio::ip::tcp::v4(), port);
        return Listen(ep);
    }

    void HttpAcceptor::TcpAccept()
    {
        if (is_open_ == false)
            return;

        network::HttpServer::pointer http_server_for_accept = network::HttpServer::create(acceptor_.get_io_service());

        LOG(__ERROR, "http_acceptor", "acceptor_.async_accept http_server = " << http_server_for_accept
            << ", acceptor = " << ref_this());
        acceptor_.async_accept(http_server_for_accept->GetSocket(), boost::bind(&HttpAcceptor::HandleAccept,
            ref_this(), http_server_for_accept, boost::asio::placeholders::error));

    }

    void HttpAcceptor::Close()
    {
        boost::system::error_code err;
        acceptor_.cancel(err);
        acceptor_.close(err);
        is_open_ = false;
        if (err)
        {
            LOGX(__ERROR, "http_acceptor", "Failed, acceptor = " << ref_this() << ", Close Error: " << err.message());
        }
        else
        {
            LOGX(__ERROR, "http_acceptor", "Succeed, acceptor = " << ref_this());
        }
    }

    void HttpAcceptor::HandleAccept(
        network::HttpServer::pointer http_server_for_accept, const boost::system::error_code& err)
    {
        if (is_open_ == false)
            return;

        if (!err)
        {
            LOGX(__ERROR, "http_acceptor", "http_server = " << http_server_for_accept << ", acceptor = " << ref_this()
                << ", succeed!");
            try
            {
                http_server_for_accept->is_open_ = true;
                if (handler_)
                {
                    handler_->OnHttpAccept(http_server_for_accept);
                }

                TcpAccept();
            }
            catch(...)
            {
                LOGX(__ERROR, "http_acceptor", "Exception!!");
                if (handler_)
                {
                    handler_->OnHttpAcceptFailed();
                }

                delay_once_timer_.start();
            }
        }
        else
        {
            LOGX(__ERROR, "http_acceptor", "http_server = " << http_server_for_accept << ", acceptor = " << ref_this()
                << ", error = " << err.message());
            if (handler_)
            {
                handler_->OnHttpAcceptFailed();
            }

            delay_once_timer_.start();
        }
    }

    void HttpAcceptor::OnTimerElapsed(framework::timer::Timer * pointer)
    {
        if (pointer == &delay_once_timer_)
        {
            delay_once_timer_.stop();
            //
            TcpAccept();
        }
    }
}
