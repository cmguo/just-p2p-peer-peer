//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// HttpClientOverUdpProxy.h

#include "HttpRequest.h"

namespace network
{
    template<typename BufferType>
    struct IHttpClientListener;

    class HttpClientOverUdpProxy
        : public boost::noncopyable
        , public boost::enable_shared_from_this<HttpClientOverUdpProxy>
    {
    public:
        typedef boost::shared_ptr<HttpClientOverUdpProxy> p;

        static p create(
            boost::asio::io_service & io_svc,
            const string proxy_domain,
            boost::uint16_t proxy_port,
            const string & request_url);

        static p create(
            boost::asio::io_service & io_svc,
            const string proxy_domain,
            boost::uint16_t proxy_port,
            const string & target_domain,
            boost::uint16_t target_port,
            const string & request);

    public:
        HttpClientOverUdpProxy(
            boost::asio::io_service & io_svc,
            const string proxy_domain,
            boost::uint16_t proxy_port,
            const string & target_domain,
            boost::uint16_t target_port,
            const string & request);

        void SetHandler(boost::shared_ptr<IHttpClientListener<protocol::SubPieceBuffer> > handler){handler_ = handler;}

        void Connect();
        void HttpGet();
        void HttpRecv(boost::uint32_t length);
        void Close();

        void SetRecvTimeoutInSec(boost::int32_t recv_timeout);

    private:
        void HandleRecvTimeOut(framework::timer::Timer * pointer);
        void HandleHttpHeader(boost::system::error_code ec, boost::uint32_t bytes_transferred);
        void HandleRecv(boost::system::error_code ec, boost::uint32_t bytes_transferred);

    private:
        string proxy_domain_;
        boost::uint16_t proxy_port_;
        HttpRequestInfo request_info_;

        boost::shared_ptr<IHttpClientListener<protocol::SubPieceBuffer> > handler_;
        boost::asio::streambuf recv_buffer_;
        boost::asio::streambuf request_buffer_;
        protocol::SubPieceBuffer content_buffer_;
        boost::asio::ip::udp::socket socket_;
        boost::uint32_t get_count_;

        boost::asio::ip::udp::endpoint sender_endpoint_;

        boost::int32_t recv_timeout_;
        framework::timer::PeriodicTimer recv_timer_;
    };
}
