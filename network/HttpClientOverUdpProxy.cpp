//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// HttpClientOverUdp.cpp

#include "Common.h"
#include "HttpClientOverUdpProxy.h"

#include "HttpClient.h"
#include "HttpRequest.h"
#include "HttpResponse.h"
#include "Uri.h"

#include <boost/algorithm/string.hpp>

namespace network 
{
    HttpClientOverUdpProxy::p HttpClientOverUdpProxy::create(
        boost::asio::io_service & io_svc,
        const string proxy_domain,
        boost::uint16_t proxy_port,
        const string & request_url)
    {
        Uri uri(request_url);
        string target_domain = uri.getdomain();
        boost::uint16_t target_port;
        boost::system::error_code ec = framework::string::parse2(uri.getport(), target_port);
        if (ec)
        {
            target_port = 80;
        }

        string request = uri.getrequest();
        return create(io_svc, proxy_domain, proxy_port, target_domain, target_port, request);
    }

    HttpClientOverUdpProxy::p HttpClientOverUdpProxy::create(
        boost::asio::io_service & io_svc,
        const string proxy_domain,
        boost::uint16_t proxy_port,
        const string & target_domain,
        boost::uint16_t target_port,
        const string & request)
    {
        return p(new HttpClientOverUdpProxy(io_svc, proxy_domain, proxy_port, target_domain, target_port, request));
    }

    HttpClientOverUdpProxy::HttpClientOverUdpProxy(
        boost::asio::io_service & io_svc,
        const string proxy_domain,
        boost::uint16_t proxy_port,
        const string & target_domain,
        boost::uint16_t target_port,
        const string & request)
        : proxy_domain_(proxy_domain)
        , proxy_port_(proxy_port)
        , content_buffer_(new protocol::SubPieceContent())
        , socket_(io_svc)
        , get_count_(0), recv_timeout_(5)
        , recv_timer_(global_second_timer(), recv_timeout_ * 1000, 
            boost::bind(&HttpClientOverUdpProxy::HandleRecvTimeOut, this, &recv_timer_))
    {
        request_info_.version_ = "HTTP/1.1";
        request_info_.domain_ = target_domain;
        request_info_.port_ = target_port;
        request_info_.path_ = request;
    }

    void HttpClientOverUdpProxy::Connect()
    {
        assert(handler_);

        boost::system::error_code ec;
        socket_.open(boost::asio::ip::udp::v4(), ec);
        if (ec)
        {
            socket_.io_service().post(boost::bind(&IHttpClientListener<protocol::SubPieceBuffer>::OnConnectFailed,
                handler_, ec.value()));
        }

        int32_t local_udp_port = 15041;
        int32_t try_count = 0;
        while (local_udp_port < 65534 && try_count < 1000)
        {
            boost::asio::ip::udp::endpoint ep(boost::asio::ip::udp::v4(), local_udp_port);
            socket_.bind(ep, ec);
            if (!ec)
            {
                socket_.io_service().post(boost::bind(&IHttpClientListener<protocol::SubPieceBuffer>::OnConnectSucced,
                    handler_));
                return;
            }

            local_udp_port ++;
            try_count++;
        }

        socket_.io_service().post(boost::bind(&IHttpClientListener<protocol::SubPieceBuffer>::OnConnectFailed,
            handler_, ec.value()));
    }

    void HttpClientOverUdpProxy::HttpGet()
    {
        assert(get_count_ == 0);
        get_count_++;

        std::stringstream sstr;
        sstr << "GET http://" << request_info_.domain_ << request_info_.path_ << " HTTP/1.1\r\n";
        sstr << "SID:1234567890FEDCBA\r\n";

        std::ostream os(&request_buffer_);
        os << sstr.str();

        // TODO(herain):2011-4-1:目前仅支持domain为ip地址，如果要支持域名，这里需要增加域名解析的逻辑
        boost::system::error_code ec;
        boost::asio::ip::address_v4 addr = boost::asio::ip::address_v4::from_string(proxy_domain_, ec);
        assert(!ec);

        boost::asio::ip::udp::endpoint end_point(addr, proxy_port_);
        socket_.send_to(request_buffer_.data(), end_point, 0, ec);
        if (!ec)
        {
            recv_timer_.start();
            socket_.async_receive_from(recv_buffer_.prepare(1400), sender_endpoint_,
                boost::bind(&HttpClientOverUdpProxy::HandleRecv, shared_from_this(), 
                    boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
        }
        else
        {
            if (handler_)
            {
                socket_.io_service().post(boost::bind(&IHttpClientListener<protocol::SubPieceBuffer>::OnRecvHttpHeaderFailed,
                    handler_, ec.value()));
            }
        }
    }

    void HttpClientOverUdpProxy::HandleRecv(
        boost::system::error_code ec, boost::uint32_t bytes_transferred)
    {
        recv_timer_.stop();
        DebugLog("HttpClientOverUdpProxy::HandleRecv from:%s, %d", sender_endpoint_.address().to_string().c_str(), sender_endpoint_.port());
        DebugLog("HttpClientOverUdpProxy::HandleRecv ec:%d, bytes_transferred:%d", ec.value(), bytes_transferred);

        if (!ec)
        {
            assert(bytes_transferred > 0);

            recv_buffer_.commit(bytes_transferred);
            DebugLog("HttpClientOverUdpProxy::HandleRecv recv_buffer_.size()=%d", recv_buffer_.size());
            
            std::istream is(&recv_buffer_);
            string response_string;
            response_string.resize(bytes_transferred);
            is.read((char*)&response_string[0], bytes_transferred);

            boost::iterator_range<string::iterator> delim_range = 
                boost::algorithm::find_first(response_string, "\r\n\r\n");

            string response_header(response_string.begin(), delim_range.end());

            uint32_t content_length = bytes_transferred - response_header.length();
            if (content_length > 0 && content_length <= 1024)
            {
                memcpy(content_buffer_.Data(), &response_string[response_header.length()], content_length);
                content_buffer_.Length(content_length);
            }
            else
            {
                // content_length不合法
                if (handler_)
                {
                    handler_->OnRecvHttpHeaderFailed(2);
                }

                Close();
                return;
            }

            uint32_t header_length;
            network::HttpResponse::p http_response = network::HttpResponse::ParseFromBuffer(
                response_header, header_length);
            if (!http_response)
            {
                if (handler_)
                {
                    handler_->OnRecvHttpHeaderFailed(1);
                }
                
                Close();
                return;
            }

            if (!http_response->GetProperty("Content-length").empty())
            {
                // fix the stupid udp proxy bug
                http_response->SetProperty("Content-Length", http_response->GetProperty("Content-length"));
                http_response->RemoveProperty("Content-length");
            }

            assert(http_response->HasContentLength());
            if (handler_)
            {
                handler_->OnRecvHttpHeaderSucced(http_response);
            }
        }
        else
        {
            if (handler_)
            {
                handler_->OnRecvHttpHeaderFailed(ec.value());
            }
            
            Close();
        }
    }

    void HttpClientOverUdpProxy::HttpRecv(boost::uint32_t length)
    {
        assert(handler_);
        socket_.io_service().post(boost::bind(
            &IHttpClientListener<protocol::SubPieceBuffer>::OnRecvHttpDataSucced, handler_, content_buffer_, 0, 0));
    }

    void HttpClientOverUdpProxy::HandleRecvTimeOut(framework::timer::Timer * pointer)
    {
        assert(pointer == &recv_timer_);
        recv_timer_.stop();
        if (handler_)
        {
            handler_->OnRecvTimeout();
        }
        
        Close();
    }

    void HttpClientOverUdpProxy::Close()
    {
        DebugLog("HttpClientOverUdpProxy::Close");
        handler_ = IHttpClientListener<protocol::SubPieceBuffer>::p();
        socket_.close();
    }

    void HttpClientOverUdpProxy::SetRecvTimeoutInSec(boost::int32_t recv_timeout)
    {
        recv_timeout_ = recv_timeout;
        recv_timer_.interval(recv_timeout_ * 1000);
    }
}
