//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef FRAMEWORK_NETWORK_HTTPSERVER_H
#define FRAMEWORK_NETWORK_HTTPSERVER_H

#include <framework/timer/Timer.h>
#include <framework/timer/TickCounter.h>

#include "network/HttpAcceptor.h"
#include "network/HttpRequest.h"

#include <util/RefenceFromThis.h>

namespace network
{
    struct IHttpServerListener
    {
        typedef boost::shared_ptr<IHttpServerListener> pointer;
        virtual void OnHttpRecvSucced(HttpRequest::p http_request) = 0;
        virtual void OnHttpRecvFailed(uint32_t error_code) = 0;
        virtual void OnHttpRecvTimeout() = 0;
        virtual void OnTcpSendFailed() = 0;
        virtual void OnClose() = 0;

        virtual ~IHttpServerListener()
        {}
    };

    class HttpServer
        : public boost::noncopyable
        , public boost::enable_shared_from_this<HttpServer>
#ifdef DUMP_OBJECT
        , public count_object_allocate<HttpServer>
#endif
    {
        friend class HttpAcceptor;
    public:
        typedef boost::shared_ptr<HttpServer> pointer;

        typedef boost::shared_ptr<HttpServer const> const_pointer;

    public:
        static pointer create(
            boost::asio::io_service & io_svc)
        {
            return pointer(new HttpServer(io_svc));
        }

    private:
        HttpServer(
            boost::asio::io_service & io_svc);
    public:
        void HttpRecv();
        void HttpRecvTillClose();
        void HttpSendHeader(uint32_t content_length, string content_type = "html/text");
        void HttpSendKeepAliveHeader(uint32_t content_length, string content_type = "html/text");
        void HttpSendHeader(string header_string);
        void HttpSendBuffer(const boost::uint8_t* data, uint32_t length);
        void HttpSendBuffer(const base::AppBuffer& buffer);
        void HttpSendBuffer(const protocol::SubPieceBuffer& buffer);
        void HttpSendContent(const boost::uint8_t* data, uint32_t length, string content_type = "html/text");
        void HttpSendContent(const string& text, string content_type = "html/text");
        void TcpSend(const base::AppBuffer& buffer);
        void WillClose();
        void Close();
        void DelayClose();
        boost::asio::ip::tcp::endpoint GetEndPoint() const;
        boost::asio::ip::tcp::socket& GetSocket()
        {
            return socket_;
        }
    public:
        void SetRecvTimeout(uint32_t recv_timeout)
        {
            recv_timeout_ = recv_timeout;
        }
        uint32_t GetRecvTimeout() const
        {
            return recv_timeout_;
        }
        void SetListener(IHttpServerListener::pointer handler)
        {
            handler_ = handler;
        }
        uint32_t GetSendPendingCount() const
        {
            return send_list_.size();
        }
        bool IsOpen() const
        {
            return is_open_;
        }
    protected:
        void HandleHttpRecv(const boost::system::error_code& err, uint32_t bytes_transferred);
        void HandleHttpRecvTillClose(const boost::system::error_code& err, uint32_t bytes_transferred, protocol::SubPieceBuffer buffer);
        void HandleHttpRecvTimeout();
        void HandleCloseTimerElapsed();
        void HandleTcpSend(const boost::system::error_code& error, uint32_t bytes_transferred);
        void OnTimerElapsed(framework::timer::Timer * pointer);
    private:
        boost::asio::ip::tcp::socket socket_;
        boost::asio::streambuf request_;
        uint32_t sended_bytes_;
        std::deque<base::AppBuffer> send_list_;
        framework::timer::OnceTimer recv_timer_;
        framework::timer::OnceTimer close_timer_;
        uint32_t recv_timeout_;
        bool is_open_;
        bool will_close_;
        uint8_t object_state_;
        IHttpServerListener::pointer handler_;
    };
}

#endif  // FRAMEWORK_NETWORK_HTTPSERVER_H
