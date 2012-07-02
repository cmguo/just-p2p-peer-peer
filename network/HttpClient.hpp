//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "network/HttpClient.h"
#include "network/Uri.h"

#include <framework/network/Endpoint.h>

#include <boost/asio/placeholders.hpp>
#include <boost/asio/read.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/write.hpp>

#ifdef COUNT_CPU_TIME
#include "count_cpu_time.h"
#endif

// #define COUT(msg)  // std::cout << msg

namespace network
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_httpclient = log4cplus::Logger::getInstance("[http_client]");
#endif
    template <typename ContentType>
    HttpClient<ContentType>::HttpClient(
        boost::asio::io_service & io_svc,
        string domain,
        boost::uint16_t port,
        string request,
        string refer_url,
        uint32_t range_begin,
        uint32_t range_end,
        bool is_accept_gzip,
        string user_agent)
        : socket_(io_svc)
        , resolver_(io_svc)
        , file_offset_(0)
        , content_offset_(0)
        , resolver_timer_(global_second_timer(), 6 * 1000, boost::bind(&HttpClient::OnTimerElapsed, this, &resolver_timer_))
        , resolver_timeout_(6 * 1000)
        , connect_timer_(global_second_timer(), 4 * 1000, boost::bind(&HttpClient::OnTimerElapsed, this, &connect_timer_))
        , connect_timeout_(4 * 1000)
        , recv_timer_(global_second_timer(), 5 * 1000, boost::bind(&HttpClient::OnTimerElapsed, this, &recv_timer_))
        , recv_timeout_(60 * 1000)
        , is_connecting_(false)
        , is_requesting_(false)
        , is_bogus_accept_range_(false)
        , is_chunked_(false)
        , connect_count_(0)
        , get_count_(0)
        , recv_time_counter_(false)
        , is_accept_gzip_(is_accept_gzip)
        , is_response_gzip_(false)
    {
        request_info_.domain_ = domain;
        if (port == 80)
            request_info_.host_ = domain;
        else
            request_info_.host_ = domain + ":" + framework::string::format(port);
        request_info_.port_ = port;
        request_info_.path_ = request;
        request_info_.refer_url_ = refer_url;
        request_info_.range_begin_ = range_begin;
        request_info_.range_end_ = range_end;
        request_info_.is_accept_gzip_ = is_accept_gzip_;
        if (!user_agent.empty())
        {
            request_info_.user_agent_ = user_agent;
        }
        // target
        target_host_ = domain;
        target_port_ = port;
    }

    template <typename ContentType>
    typename boost::shared_ptr<HttpClient<ContentType> > HttpClient<ContentType>::create(
        boost::asio::io_service & io_svc,
        string url,
        string refer_url,
        uint32_t range_begin,
        uint32_t range_end,
        bool is_accept_gzip,
        string user_agent)
    {
        Uri uri(url);
        string host = uri.getdomain();
        boost::uint16_t port;
        boost::system::error_code ec = framework::string::parse2(uri.getport(), port);
        if (ec)
        {
            LOG4CPLUS_WARN_LOG(logger_httpclient, "get port faildd. use dafault port 80.");
            port = 80;
        }

        string path = uri.getrequest();
        return boost::shared_ptr<HttpClient<ContentType> >(new HttpClient<ContentType>(io_svc, host, port, path, refer_url, range_begin, range_end, is_accept_gzip, user_agent));
    }

    template <typename ContentType>
    typename boost::shared_ptr<HttpClient<ContentType> > HttpClient<ContentType>::create(
        boost::asio::io_service & io_svc,
        network::HttpRequest::p http_request_demo,
        string url,
        string refer_url,
        uint32_t range_begin,
        uint32_t range_end,
        bool is_accept_gzip,
        string user_agent)
    {
        if (!http_request_demo)
        {
            return create(io_svc, url, refer_url, range_begin, range_end, is_accept_gzip, user_agent);
        }
        // check Pragma: Proxy = host:port
        if (false == http_request_demo->HasPragma("Proxy"))
        {
            return create(io_svc, url, refer_url, range_begin, range_end, is_accept_gzip);
        }
        // parse Proxy
        string hostport = http_request_demo->GetPragma("Proxy");
        boost::asio::ip::udp::endpoint ep = framework::network::Endpoint(hostport);
        if (ep.address().to_v4().to_ulong() == 0)
        {
            // invalid proxy string, remove it!
            http_request_demo->RemovePragma("Proxy");
            return create(io_svc, url, refer_url, range_begin, range_end, is_accept_gzip);
        }
        // target host:port
        boost::system::error_code error;
        string target_host = ep.address().to_string(error);
        boost::uint16_t target_port = ep.port();
        if (error)
        {
            // invalid proxy string, remove it!
            http_request_demo->RemovePragma("Proxy");
            return create(io_svc, url, refer_url, range_begin, range_end, is_accept_gzip);
        }
        if (target_port == 0)
        {
            target_port = 80;
        }
        // parse host:port
        Uri uri(url);
        string host = uri.getdomain();
        boost::uint16_t port = 0;
        boost::system::error_code ec = framework::string::parse2(uri.getport(), port);
        if (ec)
        {
            LOG4CPLUS_WARN_LOG(logger_httpclient, "get port faildd. use dafault port 80.");
            port = 80;
        }
        boost::shared_ptr<HttpClient<ContentType> > http_client = 
            create(io_svc, host, port, url, refer_url, range_begin, range_end, is_accept_gzip);
        http_client->target_host_ = target_host;
        http_client->target_port_ = target_port;
        // save local
        return http_client;
    }

    template <typename ContentType>
    typename boost::shared_ptr<HttpClient<ContentType> > HttpClient<ContentType>::create(
        boost::asio::io_service & io_svc,
        string domain,
        boost::uint16_t port,
        string request,
        string refer_url,
        uint32_t range_begin,
        uint32_t range_end,
        bool is_accept_gzip,
        string user_agent)
    {
        return boost::shared_ptr<HttpClient<ContentType> >
            (new HttpClient<ContentType>(io_svc, domain, port, request, refer_url, range_begin, range_end, is_accept_gzip));
    }

    template <typename ContentType>
    void HttpClient<ContentType>::Close()
    {
        LOG4CPLUS_INFO_LOG(logger_httpclient, shared_from_this());
        is_connecting_ = false;
        is_connected_ = false;
        is_requesting_ = false;
        boost::system::error_code error;
        socket_.close(error);
        resolver_.cancel();
        resolver_timer_.stop();
        connect_timer_.stop();
        recv_timer_.stop();
        if (http_response_)
        {
            http_response_.reset();
        }
        if (handler_)
        {
            handler_.reset();
        }
    }

    template <typename ContentType>
    void HttpClient<ContentType>::OnTimerElapsed(framework::timer::Timer * pointer)
    {
        if (pointer == &resolver_timer_)
            HandleResolveTimeout();
        else if (pointer == &connect_timer_)
            HandleConnectTimeout();
        else if (pointer == &recv_timer_)
            HandleRecvTimeout();
    }

    template <typename ContentType>
    void HttpClient<ContentType>::Connect()
    {
        if (true == is_connecting_)
            return;

        is_connecting_ = true;

        assert(connect_count_ == 0);
        connect_count_++;

        string domain = request_info_.domain_;
        if (domain == "")
        {
            if (handler_)
            {
                LOG4CPLUS_INFO_LOG(logger_httpclient, "post IHttpClientListener::OnConnectFailed 1");
                handler_->OnConnectFailed(1);
            }

            Close();

            return;
        }

        boost::system::error_code err;
        boost::asio::ip::address_v4 addr = boost::asio::ip::address_v4::from_string(target_host_, err);
        if (err)
        {
            boost::asio::ip::tcp::resolver::query query(boost::asio::ip::tcp::v4(), target_host_, framework::string::format(target_port_));

            try
            {
                resolver_.async_resolve(query, boost::bind(&HttpClient<ContentType>::HandleResolve, this->shared_from_this(),
                    boost::asio::placeholders::error, boost::asio::placeholders::iterator));
            }
            catch(boost::system::system_error&)
            {
                // to avoid async_resolve throw exception
                LOG4CPLUS_INFO_LOG(logger_httpclient, "HttpClient::Connect() - async_resolve failed 401");

                if (handler_)
                {
                    handler_->OnConnectFailed(401);
                }

                Close();

                return;
            }

            resolver_timer_.start();
            LOG4CPLUS_INFO_LOG(logger_httpclient, "async_resolve " << domain << ", TargetHost: " << target_host_ << 
                ", TargetPort: " << target_port_);
        }
        else
        {
            boost::asio::ip::tcp::resolver::iterator endpoint_iter;
            endpoint_ = boost::asio::ip::tcp::endpoint(addr, target_port_);

            try
            {
                socket_.async_connect(endpoint_, boost::bind(&HttpClient::HandleConnect, this->shared_from_this(),
                    boost::asio::placeholders::error, endpoint_iter));
            }
            catch(boost::system::system_error&)
            {
                // async_conect sometimes throws
                LOG4CPLUS_INFO_LOG(logger_httpclient, "HttpClient::Connect() - async_connect failed 402");

                if (handler_)
                {
                    handler_->OnConnectFailed(402);
                }

                Close();

                return;
            }

            connect_timer_.start();
            LOG4CPLUS_INFO_LOG(logger_httpclient, "async_connect " << domain << ", endpoint = " << endpoint_ << 
                ", TargetHost: " << target_host_ << ", TargetPort: " << target_port_);
        }
    }

    template <typename ContentType>
    void HttpClient<ContentType>::HandleResolveTimeout()
    {
        assert(is_connecting_);
        if (false == is_connecting_)
            return;
        LOG4CPLUS_INFO_LOG(logger_httpclient, "Handler = " << handler_);
        if (handler_)
        {
            handler_->OnConnectFailed(2);
        }
        Close();
    }

    template <typename ContentType>
    void HttpClient<ContentType>::HandleResolve(const boost::system::error_code& err,
        boost::asio::ip::tcp::resolver::iterator endpoint_iterator)
    {
        assert(is_connecting_);
        if (false == is_connecting_)
            return;

        resolver_timer_.stop();

        if (!err)
        {
            endpoint_ = *endpoint_iterator;
            LOG4CPLUS_INFO_LOG(logger_httpclient, "Succed " << endpoint_ << ", TargetHost: " << target_host_ << 
                ", TargetPort: " << target_port_);

            try
            {
                socket_.async_connect(endpoint_, boost::bind(&HttpClient::HandleConnect, this->shared_from_this(),
                    boost::asio::placeholders::error, ++endpoint_iterator));
            }
            catch(boost::system::system_error&)
            {
                // async_connect sometimes throws
                LOG4CPLUS_DEBUG_LOG(logger_httpclient, "HttpClient::HandleResolve() - async_connect failed 403");

                if (handler_)
                {
                    handler_->OnConnectFailed(403);
                }

                Close();

                return;
            }

            connect_timer_.start();
        }
        else if (err == boost::asio::error::operation_aborted)
        {
            LOG4CPLUS_ERROR_LOG(logger_httpclient, "Handler = " << handler_ << ", Error = " << err.message());
            if (handler_)
            {
                handler_->OnConnectFailed(103);
                LOG4CPLUS_INFO_LOG(logger_httpclient, "post IHttpClientListener::OnConnectFailed 103");
            }
            Close();
        }
        else
        {
            LOG4CPLUS_INFO_LOG(logger_httpclient, "Handler = " << handler_ << ", Error = " << err.message());
            if (handler_)
            {
                handler_->OnConnectFailed(3);
                LOG4CPLUS_INFO_LOG(logger_httpclient, "post IHttpClientListener::OnConnectFailed 3");
            }
            Close();
        }
    }

    template <typename ContentType>
    void HttpClient<ContentType>::HandleConnectTimeout()
    {
        assert(is_connecting_);
        if (false == is_connecting_)
            return;
        if (handler_)
        {
            handler_->OnConnectTimeout();
            LOG4CPLUS_INFO_LOG(logger_httpclient, "post IHttpClientListener::OnConnectTimeout");
        }
        Close();
    }

    template <typename ContentType>
    void HttpClient<ContentType>::HandleConnect
        (const boost::system::error_code& err,
        boost::asio::ip::tcp::resolver::iterator endpoint_iterator)
    {
        if (false == is_connecting_)
            return;

        is_connected_ = true;

        connect_timer_.stop();

        if (!err)
        {
            LOG4CPLUS_INFO_LOG(logger_httpclient, "Succed " << err.message() << ", " << shared_from_this());
            if (handler_)
            {
                handler_->OnConnectSucced();
            }
            recv_timer_.start();
            LOG4CPLUS_INFO_LOG(logger_httpclient, "post IHttpClientListener::OnConnectSucced" << err.message());
        }
        else if (endpoint_iterator != boost::asio::ip::tcp::resolver::iterator())
        {
            boost::system::error_code error;
            socket_.close(error);
            endpoint_ = *endpoint_iterator;

            try
            {
                socket_.async_connect(endpoint_, boost::bind(&HttpClient::HandleConnect, this->shared_from_this(),
                    boost::asio::placeholders::error, ++endpoint_iterator));
            }
            catch(boost::system::system_error&)
            {
                // async_connect sometimes throws
                LOG4CPLUS_DEBUG_LOG(logger_httpclient, "HttpClient::HandleConnect() - async_connect failed 404");

                if (handler_)
                {
                    handler_->OnConnectFailed(404);
                }

                Close();

                return;
            }

            connect_timer_.interval(connect_timeout_);
            connect_timer_.start();
            LOG4CPLUS_INFO_LOG(logger_httpclient, "async_connect " << endpoint_);
        }
        else if (err == boost::asio::error::operation_aborted)
        {
            LOG4CPLUS_INFO_LOG(logger_httpclient, "Error because operation_aborted");
            if (handler_)
            {
                handler_->OnConnectFailed(101);
            }
            Close();
        }
        else
        {
            LOG4CPLUS_INFO_LOG(logger_httpclient, "Error because " << err.message());
            if (handler_)
            {
                handler_->OnConnectFailed(1);
            }
            Close();
        }
    }

    template <typename ContentType>
    void HttpClient<ContentType>::HttpGetByString(string request_string)
    {
        if (is_connected_ == false)
            return;

        request_string_ = request_string;

        assert(get_count_ == 0);
        get_count_++;

        LOG4CPLUS_INFO_LOG(logger_httpclient, "HTTP GET " << request_string_);

        assert(is_requesting_ == false);
        if (is_requesting_ == true)
            return;

        is_requesting_ = true;

        boost::asio::async_write(socket_, boost::asio::buffer(request_string_), boost::bind(
            &HttpClient::HandleWriteRequest, this->shared_from_this(), boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
        LOG4CPLUS_INFO_LOG(logger_httpclient, "async_write " << request_string_);
    }

    template <typename ContentType>
    void HttpClient<ContentType>::HttpGet()
    {
        if (is_connected_ == false)
            return;

        assert(get_count_ == 0);
        get_count_++;

        boost::system::error_code error;
        request_string_ = request_info_.ToString();
        std::ostream os(&request_);
        os << request_string_;
        LOG4CPLUS_DEBUG_LOG(logger_httpclient, "RemoteEndpoint: " << socket_.remote_endpoint(error) << 
            " Request:\n" << request_string_);

        assert(is_requesting_ == false);
        if (is_requesting_ == true)
            return;

        is_requesting_ = true;

        boost::asio::async_write(socket_,  request_, boost::bind(
            &HttpClient::HandleWriteRequest, this->shared_from_this(), boost::asio::placeholders::error,
            boost::asio::placeholders::bytes_transferred));
        LOG4CPLUS_INFO_LOG(logger_httpclient, "async_write " << request_string_);
    }

    template <typename ContentType>
    void HttpClient<ContentType>::SetMethod(const string& method)
    {
        request_info_.method_ = method;
    }

    template <typename ContentType>
    void HttpClient<ContentType>::AddPragma(const string& key, const string& value)
    {
        if (key.length() != 0)
        {
            request_info_.pragmas_[key] = value;
        }
    }

    template <typename ContentType>
    void HttpClient<ContentType>::HttpGet(uint32_t range_begin, uint32_t range_end)
    {
        if (!is_accept_gzip_)
        {
            request_info_.range_begin_ = range_begin;
            request_info_.range_end_ = range_end;
        }
        HttpGet();
    }

    template <typename ContentType>
    void HttpClient<ContentType>::HttpGet(string refer_url, uint32_t range_begin, uint32_t range_end)
    {
        request_info_.refer_url_ = refer_url;
        if (!is_accept_gzip_)
        {
            request_info_.range_begin_ = range_begin;
            request_info_.range_end_ = range_end;
        }
        HttpGet();
    }

    template <typename ContentType>
    void HttpClient<ContentType>::HttpGet(network::HttpRequest::p http_request_demo, uint32_t range_begin, uint32_t range_end)
    {
        request_info_.http_request_demo_ = http_request_demo;
        request_info_.range_begin_ = range_begin;
        request_info_.range_end_ = range_end;
        HttpGet();
    }

    template <typename ContentType>
    void HttpClient<ContentType>::HttpGet(network::HttpRequest::p http_request_demo, string refer_url, uint32_t range_begin, uint32_t range_end)
    {
        request_info_.http_request_demo_ = http_request_demo;
        request_info_.refer_url_ = refer_url;
        request_info_.range_begin_ = range_begin;
        request_info_.range_end_ = range_end;
        HttpGet();
    }

    template <typename ContentType>
    void HttpClient<ContentType>::HandleRecvTimeout()
    {
        if (false == is_connected_)
            return;
        if (recv_time_counter_.running() && recv_time_counter_.elapsed() >= recv_timeout_)
        {
            if (handler_)
            {
                handler_->OnRecvTimeout();
                LOG4CPLUS_INFO_LOG(logger_httpclient, "post IHttpClientListener::OnRecvTimeout");
            }

            Close();
        }
    }

    template <typename ContentType>
    void HttpClient<ContentType>::HandleWriteRequest(const boost::system::error_code& err, uint32_t bytes_transferred)
    {
        if (false == is_connected_)
            return;
        if (!err)
        {
            string delim("\r\n\r\n");
            LOG4CPLUS_INFO_LOG(logger_httpclient, "Succed " << bytes_transferred);
            boost::asio::async_read_until(socket_, response_, delim, boost::bind(&HttpClient::HandleReadHttpHeader,
                this->shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
            recv_time_counter_.start();
            LOG4CPLUS_INFO_LOG(logger_httpclient, "HttpClient::HandleWriteRequest async_read_until");
        }
        else if (err == boost::asio::error::operation_aborted)
        {
            LOG4CPLUS_INFO_LOG(logger_httpclient, "Handler = " << handler_ << ", Error = " << err.message());
            if (handler_)
            {
                handler_->OnRecvHttpHeaderFailed(105);
                LOG4CPLUS_INFO_LOG(logger_httpclient, "post IHttpClientListener::OnRecvHttpHeaderFailed " << 105);
            }
            Close();
        }
        else
        {
            LOG4CPLUS_INFO_LOG(logger_httpclient, "Handler = " << handler_ << ", Error = " << err.message());
            if (handler_)
            {
                handler_->OnRecvHttpHeaderFailed(5);
                LOG4CPLUS_INFO_LOG(logger_httpclient, "post IHttpClientListener::OnRecvHttpHeaderFailed " << 5);
            }
            Close();
        }
    }

    template <typename ContentType>
    void HttpClient<ContentType>::HandleReadHttpHeader(const boost::system::error_code& err, uint32_t bytes_transferred)
    {
        if (false == is_connected_)
            return;

        assert(is_requesting_ == true);
        if (is_requesting_ == false)
            return;

        is_requesting_ = false;

        recv_time_counter_.stop();

        if (!err)
        {
            LOG4CPLUS_INFO_LOG(logger_httpclient, "Succed, BytesTransferred = " << bytes_transferred << 
                ", ResponseSize = " << response_.size());
            assert(bytes_transferred <= response_.size());
            string response_string;
            uint32_t header_length;
            std::istream is(&response_);
            response_string.resize(bytes_transferred);
            is.read((char*)&response_string[0], bytes_transferred);
            http_response_ = network::HttpResponse::ParseFromBuffer(response_string, header_length);
            if (!http_response_)
            {
                if (handler_)
                {
                    // MainThread::Post(boost::bind(&IHttpClientListener::OnRecvHttpHeaderFailed, handler_, 1));
                    LOG4CPLUS_INFO_LOG(logger_httpclient, "post IHttpClientListener::OnRecvHttpHeaderFailed " << 1);
                    handler_->OnRecvHttpHeaderFailed(1);
                }
                Close();
                return;
            }

            is_response_gzip_ = http_response_->IsGzip();

            assert(header_length > 0);
            if (header_length == 0)
            {
                if (handler_)
                {
                    // MainThread::Post(boost::bind(&IHttpClientListener::OnRecvHttpHeaderFailed, handler_, 2));
                    LOG4CPLUS_INFO_LOG(logger_httpclient, "post IHttpClientListener::OnRecvHttpHeaderFailed " << 2);
                    handler_->OnRecvHttpHeaderFailed(1);
                }
                Close();
                return;
            }
            assert(header_length == bytes_transferred);
            LOG4CPLUS_DEBUG_LOG(logger_httpclient, "HeaderLength = " << header_length << ", BytesTransferred = " << 
                bytes_transferred);
            if (header_length > bytes_transferred)
            {
                if (handler_)
                {
                    // MainThread::Post(boost::bind(&IHttpClientListener::OnRecvHttpHeaderFailed, handler_, 3));
                    LOG4CPLUS_INFO_LOG(logger_httpclient, "post IHttpClientListener::OnRecvHttpHeaderFailed " << 3);
                    handler_->OnRecvHttpHeaderFailed(3);
                }
                Close();
                return;
            }

            if (false == http_response_->HasContentLength())
            {
                content_length_ = std::numeric_limits<uint32_t>::max();
                is_chunked_ = true;
            }
            else
            {
                content_length_ = http_response_->GetContentLength();
            }
            content_offset_ = 0;

            if (request_info_.range_begin_ != 0)
            {
                uint32_t range_begin = http_response_->GetRangeBegin();
                file_offset_ = range_begin;
                LOG4CPLUS_INFO_LOG(logger_httpclient, "http_response_->GetRangeBegin(): " << range_begin << 
                    " file_offset=" << file_offset_ << " client=" << shared_from_this());
            }

            if (handler_)
            {
                LOG4CPLUS_INFO_LOG(logger_httpclient, "post IHttpClientListener::OnRecvHttpHeaderSucced \n" << 
                    http_response_->ToString());
                handler_->OnRecvHttpHeaderSucced(http_response_);
            }
        }
        else if (err == boost::asio::error::operation_aborted)
        {
            LOG4CPLUS_INFO_LOG(logger_httpclient, "Handler = " << handler_ << ", Error = " << err.message());
            if (handler_)
            {
                // MainThread::Post(boost::bind(&IHttpClientListener::OnRecvHttpHeaderFailed, handler_, 104));
                LOG4CPLUS_INFO_LOG(logger_httpclient, 
                    "HttpClient::HandleReadHttpHeader post IHttpClientListener::OnRecvHttpHeaderFailed " << 104);
                handler_->OnRecvHttpHeaderFailed(104);
            }
            Close();
        }
        else
        {
            LOG4CPLUS_INFO_LOG(logger_httpclient, "Handler = " << handler_ << ", Error = " << err.message());
            if (handler_)
            {
                // MainThread::Post(boost::bind(&IHttpClientListener::OnRecvHttpHeaderFailed, handler_, 4));
                LOG4CPLUS_INFO_LOG(logger_httpclient, "post IHttpClientListener::OnRecvHttpHeaderFailed " << 4);
                handler_->OnRecvHttpHeaderFailed(4);
            }
            Close();
            return;
        }
    }

    template <typename ContentType>
    void HttpClient<ContentType>::HttpRecv(uint32_t length)
    {
        if (false == is_connected_)
            return;

        assert(length <= MaxRecvLength);

        if (content_offset_ >= content_length_)
        {
            if (handler_)
            {
                handler_->OnComplete();
            }

            LOG4CPLUS_INFO_LOG(logger_httpclient, "post IHttpClientListener::OnComplete -> HttpClient.Close()");
            Close();
            return;
        }

        if ((content_offset_ + length) > content_length_)
        {
            length = content_length_ - content_offset_;
        }

        uint32_t network_length = length;
        uint32_t buffer_offset = 0;

        protocol::SubPieceBufferImp<ContentType> buffer(new ContentType(), length);
        if (!buffer.GetSubPieceBuffer())
        {
            assert(false);
            return;
        }

        if (network_length <= response_.size())
        {
            std::istream is(&response_);
            is.read((char*) buffer.Data(), length);
            buffer.Length(length);

            uint32_t file_offset = file_offset_;
            uint32_t content_offset = content_offset_;
            file_offset_ += length;
            content_offset_ += length;
            if (handler_)
            {
                // MainThread::Post(boost::bind(&IHttpClientListener::OnRecvHttpDataSucced, handler_, buffer, file_offset_,
                //    content_offset_));
                LOG4CPLUS_INFO_LOG(logger_httpclient, "post IHttpClientListener::OnRecvHttpDataSucced " << 
                    file_offset_ << " " << content_offset_);
                
                handler_->OnRecvHttpDataSucced(buffer, file_offset, content_offset, is_response_gzip_);
            }
        }
        else
        {
            buffer_offset = response_.size();
            if (buffer_offset > 0)
            {
                std::istream is(&response_);

                is.read((char*) buffer.Data(), buffer_offset);
                network_length -= buffer_offset;
            }

            buffer.Length(buffer_offset);

            // 这个assert是没有意义的，只要限速模块有效，就有可能触发
            // 第一次Recv是PutPieceTask触发的，在数据没回来之前
            // 限速器再触发一次就会有这个assert
            // assert(is_requesting_ == false);
            if (is_requesting_ == true)
                return;

            is_requesting_ = true;

            LOG4CPLUS_INFO_LOG(logger_httpclient, "async_read length= " << length << " network_length=" << 
                network_length << " response.size()=" << buffer_offset);
            boost::asio::async_read(socket_,
                boost::asio::buffer(buffer.Data() + buffer_offset, network_length),  // response_,
                boost::asio::transfer_all(),  // boost::asio::transfer_at_least(network_length),
                boost::bind(&HttpClient::HandleReadHttp, this->shared_from_this(),
                boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred, length, file_offset_,
                content_offset_, buffer, buffer_offset));

            recv_time_counter_.start();
            LOG4CPLUS_INFO_LOG(logger_httpclient, "async_read " << length << " " << file_offset_ << 
                " " << content_offset_);

            file_offset_ += length;
            content_offset_ += length;
        }
    }

    template <typename ContentType>
    void HttpClient<ContentType>::HttpRecvSubPiece()
    {
        LOG4CPLUS_INFO_LOG(logger_httpclient, "HttpClient::HttpRecvSubPiece");
        HttpRecv(ContentType::sub_piece_size);
    }

    template <typename ContentType>
    void HttpClient<ContentType>::HandleReadHttp(const boost::system::error_code& err, uint32_t bytes_transferred, uint32_t buffer_length,
        uint32_t file_offset, uint32_t content_offset, protocol::SubPieceBufferImp<ContentType> buffer, uint32_t buffer_offset)
    {
        LOG4CPLUS_INFO_LOG(logger_httpclient, "BytesTransferred = " << bytes_transferred);
        if (false == is_connected_)
            return;

        static uint32_t id = 0;

        assert(is_requesting_ == true);
        if (is_requesting_ == false)
            return;

        is_requesting_ = false;

        recv_time_counter_.stop();

        buffer.Length(buffer.Length() + bytes_transferred);
        if (!err)
        {
            LOG4CPLUS_INFO_LOG(logger_httpclient, "Succed " << buffer_length << " file_offset=" << file_offset << 
                " content_offset=" << content_offset << " range_begin=" << request_info_.range_begin_ << " client=" 
                << shared_from_this());
            assert(buffer_length>0);
            // assert(response_.size() >= buffer_length);
            // protocol::SubPieceBuffer buffer(buffer_length);
            // std::istream is(&response_);
            // is.read((char*) buffer.Data(), buffer_length);
            if (handler_)
            {
                // MainThread::Post(boost::bind(&IHttpClientListener::OnRecvHttpDataSucced, handler_, buffer, file_offset,
                //    content_offset));
                LOG4CPLUS_INFO_LOG(logger_httpclient, "post IHttpClientListener::OnRecvHttpDataSucced " << 
                    file_offset << " " << content_offset);

                handler_->OnRecvHttpDataSucced(buffer, file_offset, content_offset, is_response_gzip_);
            }
        }
        else if (err == boost::asio::error::operation_aborted)
        {
            LOG4CPLUS_INFO_LOG(logger_httpclient, "Handler = " << handler_ << ", Error = " << err.message());
            if (handler_)
            {
                // MainThread::Post(boost::bind(&IHttpClientListener::OnRecvHttpDataFailed, handler_, 101));
                LOG4CPLUS_INFO_LOG(logger_httpclient, "post IHttpClientListener::OnRecvHttpDataFailed " << 101);
                handler_->OnRecvHttpDataFailed(101);
            }
            Close();
        }
        else if (err == boost::asio::error::eof)
        {
            //
            LOG4CPLUS_INFO_LOG(logger_httpclient, "Handler = " << handler_ << ", Error = " << err.message());
            if (response_.size() > 0)
            {
                assert(!"Don't come here!");

                if (handler_)
                {
                    // MainThread::Post(boost::bind(&IHttpClientListener::OnRecvHttpDataPartial, handler_, buffer,
                    //    file_offset, content_offset));
                    LOG4CPLUS_INFO_LOG(logger_httpclient, "post IHttpClientListener::OnRecvHttpDataPartial " << 
                        file_offset << " " << content_offset);
                    handler_->OnRecvHttpDataPartial(buffer, file_offset, content_offset);
                }
            }
            else
            {
                if (buffer_offset + bytes_transferred <= buffer.Length())
                {
                    buffer.Length(buffer_offset + bytes_transferred);
                }
                else
                {
                    assert(!"Invalid Bytes transferred!!");
                }

                if (handler_)
                {
                    // MainThread::Post(boost::bind(&IHttpClientListener::OnRecvHttpDataPartial, handler_, buffer,
                    //    file_offset, content_offset));
                    LOG4CPLUS_INFO_LOG(logger_httpclient, "post IHttpClientListener::OnRecvHttpDataPartial " << 
                        file_offset << " " << content_offset);

                    handler_->OnRecvHttpDataSucced(buffer, file_offset, content_offset, is_response_gzip_);
                }
            }

            if (is_chunked_ == false)
            {
                if (handler_)
                {
                    // MainThread::Post(boost::bind(&IHttpClientListener::OnRecvHttpDataFailed, handler_, 2));
                    LOG4CPLUS_INFO_LOG(logger_httpclient, "post IHttpClientListener::OnRecvHttpDataFailed " << 2);
                    handler_->OnRecvHttpDataFailed(2);
                }
            }
            else
            {
                if (handler_)
                {
                    // MainThread::Post(boost::bind(&IHttpClientListener::OnComplete, handler_));
                    LOG4CPLUS_INFO_LOG(logger_httpclient, "post IHttpClientListener::OnComplete, because it is chunked");
                    handler_->OnComplete();
                }
            }
            Close();
        }
        else
        {
            LOG4CPLUS_INFO_LOG(logger_httpclient, "Handler = " << handler_ << ", Error = " << err.message());
            if (handler_)
            {
                // MainThread::Post(boost::bind(&IHttpClientListener::OnRecvHttpDataFailed, handler_, 1));
                LOG4CPLUS_INFO_LOG(logger_httpclient, "post IHttpClientListener::OnRecvHttpDataFailed " << 1);
                handler_->OnRecvHttpDataFailed(1);
            }
            Close();
        }
    }

    template <typename ContentType>
    bool HttpClient<ContentType>::IsBogusAcceptRange() const
    {
        if (!http_response_)
        {
            assert(0);
            return false;
        }

        if (request_info_.range_begin_ == 0)
        {
            return false;
        }
        else
        {
            if (http_response_->GetStatusCode() == 403)
            {
                return true;
            }
            if (http_response_->GetStatusCode() == 200)
            {
                if (http_response_->HasContentRange() == false)
                {
                    return true;
                }
            }
        }
        return false;
    }
}
