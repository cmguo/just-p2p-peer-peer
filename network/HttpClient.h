//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// HttpClient.h

#ifndef _NETWORK_HTTP_CLIENT_H_
#define _NETWORK_HTTP_CLIENT_H_

#include "network/HttpRequest.h"
#include "network/HttpResponse.h"

#include <framework/timer/Timer.h>
#include <framework/timer/TickCounter.h>

#include <boost/asio/ip/tcp.hpp>

namespace network
{
    template<typename BufferType>
    struct IHttpClientListener
    {
        typedef boost::shared_ptr<IHttpClientListener<BufferType> > p;

        virtual void OnConnectSucced() = 0;
        virtual void OnConnectFailed(uint32_t error_code) = 0;
        virtual void OnConnectTimeout() = 0;

        virtual void OnRecvHttpHeaderSucced(network::HttpResponse::p http_response) = 0;
        virtual void OnRecvHttpHeaderFailed(uint32_t error_code) = 0;
        virtual void OnRecvHttpDataSucced(BufferType const & buffer, uint32_t file_offset, uint32_t content_offset, bool is_gzip) = 0;
        virtual void OnRecvHttpDataPartial(BufferType const & buffer, uint32_t file_offset, uint32_t content_offset) = 0;
        virtual void OnRecvHttpDataFailed(uint32_t error_code) = 0;
        virtual void OnRecvTimeout() = 0;

        virtual void OnComplete() = 0;

        virtual ~IHttpClientListener() {}
    };

    template <typename ContentType>
    class HttpClient
        : public boost::noncopyable
        , public boost::enable_shared_from_this<HttpClient<ContentType> >
#ifdef DUMP_OBJECT
        , public count_object_allocate<HttpClient<ContentType> >
#endif
    {
    public:
        typedef boost::shared_ptr<HttpClient<ContentType> > p;
        static p create(
            boost::asio::io_service & io_svc,
            network::HttpRequest::p http_request_demo,
            string url,
            string refer_url,
            uint32_t range_begin,
            uint32_t range_end,
            bool is_accept_gzip);

        static p create(
            boost::asio::io_service & io_svc,
            string url,
            string refer_url,
            uint32_t range_begin,
            uint32_t range_end,
            bool is_accept_gzip);

        static p create(
            boost::asio::io_service & io_svc,
            string domain,
            boost::uint16_t port,
            string request,
            string refer_url,
            uint32_t range_begin,
            uint32_t range_end,
            bool is_accept_gzip);

    public:
        HttpClient(
            boost::asio::io_service & io_svc,
            string domain,
            boost::uint16_t port,
            string request,
            string refer_url,
            uint32_t range_begin,
            uint32_t range_end,
            bool is_accept_gzip);

    public:
        void Close();
        void Connect();
        void SetMethod(const string& method);
        // void SetHeader(const string& key, const string& value);
        void AddPragma(const string& key, const string& value);
        void HttpGet();
        void HttpGet(uint32_t range_begin, uint32_t range_end = 0);
        void HttpGet(string refer_url, uint32_t range_begin = 0, uint32_t range_end = 0);
        void HttpGet(network::HttpRequest::p http_request_demo, uint32_t range_begin, uint32_t range_end = 0);
        void HttpGet(network::HttpRequest::p http_request_demo, string refer_url, uint32_t range_begin = 0, uint32_t range_end = 0);
        void HttpGetByString(string request_string);
        void HttpRecvSubPiece();
        void HttpRecv(uint32_t length);

        static const uint32_t MaxRecvLength = ContentType::sub_piece_size;
        
        bool IsRequesting() const
        {
            return is_requesting_;
        }
    public:
        void SetHandler(typename IHttpClientListener<protocol::SubPieceBufferImp<ContentType> >::p handler)
        {
            handler_ = handler;
        }
        void SetResolverTimeout(uint32_t resolver_timeout)
        {
            resolver_timeout_ = resolver_timeout;
        }
        void SetConnectTimeout(uint32_t connect_timeout)
        {
            connect_timeout_ = connect_timeout;
        }
        void SetRecvTimeout(uint32_t recv_timeout)
        {
            recv_timeout_ = recv_timeout;
        }
        bool IsBogusAcceptRange() const;
    protected:
        void HandleResolve(const boost::system::error_code& err,
            boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
        void HandleConnect(const boost::system::error_code& err,
            boost::asio::ip::tcp::resolver::iterator endpoint_iterator);
        void HandleWriteRequest(const boost::system::error_code& err, uint32_t bytes_transferred);
        void HandleReadHttpHeader(const boost::system::error_code& err, uint32_t bytes_transferred);
        void HandleReadHttp(const boost::system::error_code& err, uint32_t bytes_transferred, uint32_t buffer_length,
            uint32_t file_offset, uint32_t content_offset, protocol::SubPieceBufferImp<ContentType> buffer, uint32_t buffer_offset);
        void OnTimerElapsed(framework::timer::Timer * pointer);
        void HandleResolveTimeout();
        void HandleConnectTimeout();
        void HandleRecvTimeout();
    protected:
        boost::asio::ip::tcp::endpoint endpoint_;
        boost::asio::ip::tcp::socket socket_;
        boost::asio::ip::tcp::resolver resolver_;
        HttpRequestInfo request_info_;
        network::HttpResponse::p http_response_;
        boost::asio::streambuf response_;
        boost::asio::streambuf request_;
        string request_string_;
        uint32_t content_length_;
        uint32_t file_offset_;
        uint32_t content_offset_;
        framework::timer::OnceTimer resolver_timer_;
        uint32_t resolver_timeout_;
        framework::timer::OnceTimer connect_timer_;
        uint32_t connect_timeout_;
        framework::timer::PeriodicTimer recv_timer_;
        uint32_t recv_timeout_;
        typename IHttpClientListener<protocol::SubPieceBufferImp<ContentType> >::p handler_;
        bool is_connecting_;
        bool is_connected_;
        bool is_requesting_;
        bool is_bogus_accept_range_;
        bool is_chunked_;

        uint32_t connect_count_;
        uint32_t get_count_;

        // target host
        string target_host_;
        boost::uint16_t target_port_;

        framework::timer::TickCounter recv_time_counter_;

        bool is_accept_gzip_;
        bool is_response_gzip_;
    };
}

#include "HttpClient.hpp"
#endif  // _NETWORK_HTTP_CLIENT_H_
