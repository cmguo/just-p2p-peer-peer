//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef FRAMEWORK_NETWORK_HTTPACCEPTOR_H
#define FRAMEWORK_NETWORK_HTTPACCEPTOR_H

#include "framework/timer/Timer.h"
#include <boost/asio/ip/tcp.hpp>

#include <util/RefenceFromThis.h>

namespace network
{
    class HttpServer;

    struct IHttpAcceptorListener
    {
        typedef boost::shared_ptr<IHttpAcceptorListener> p;
        virtual void OnHttpAccept(boost::shared_ptr<HttpServer> http_server_for_accept) = 0;
        virtual void OnHttpAcceptFailed() = 0;
        virtual ~IHttpAcceptorListener()
        {
        }
    };

    class HttpAcceptor
        : public boost::noncopyable
        , public util::RefenceFromThis<HttpAcceptor>
#ifdef DUMP_OBJECT
        , public count_object_allocate<HttpAcceptor>
#endif
    {
    public:
        static pointer create(
            boost::asio::io_service & io_svc, IHttpAcceptorListener::p handler)
        {
            return pointer(new HttpAcceptor(io_svc, handler));
        }

    private:
        HttpAcceptor(
            boost::asio::io_service & io_svc,
            IHttpAcceptorListener::p handler);

    public:
        bool Listen(const boost::asio::ip::tcp::endpoint& ep);
        bool Listen(boost::uint16_t port);
        void TcpAccept();
        void Close();
    public:
        boost::uint16_t GetHttpPort() const
        {
            return port_;
        }
    public:
        void OnTimerElapsed(framework::timer::Timer::pointer timer);

    protected:
        void HandleAccept(boost::shared_ptr<HttpServer> http_server_for_accept, const boost::system::error_code& err);

    private:
        IHttpAcceptorListener::p handler_;
        boost::asio::ip::tcp::acceptor acceptor_;
        bool is_open_;
        boost::uint16_t port_;
        framework::timer::OnceTimer delay_once_timer_;
    };
}

#endif  // FRAMEWORK_NETWORK_HTTPACCEPTOR_H
