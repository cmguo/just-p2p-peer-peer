//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "network/HttpPost.h"
#include "network/IHttpPostListener.h"
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>

namespace network
{
    using namespace util::protocol;

    HttpPost::HttpPost(boost::asio::io_service& service, string server, string path, boost::shared_ptr<IHttpPostListener> listener)
        : service_(service), server_(server), path_(path), post_listener_(listener)
    {
    }

    void HttpPost::OnPostResult(boost::shared_ptr<util::protocol::HttpClient> http_client, const boost::system::error_code& err)
    {
        post_listener_->OnPostResult(err);
    }

    void HttpPost::AsyncPost(std::istream& data_to_post)
    {
        boost::shared_ptr<util::protocol::HttpClient> http_client(new util::protocol::HttpClient(service_));

        std::vector<string> host_and_port;
        boost::algorithm::split(host_and_port, server_, boost::algorithm::is_any_of(":"));

        boost::system::error_code error;
        if (host_and_port.size() == 2)
        {
            http_client->bind_host(host_and_port[0], host_and_port[1], error);
        }
        else
        {
            http_client->bind_host(server_, error);
        }

        if (error)
        {
            post_listener_->OnPostResult(error);
            return;
        }

        HttpRequest request;
        request.head().method = util::protocol::HttpRequestHead::post;
        request.head()["Accept"] = "{*/*}";
        request.head().host = server_;
        request.head().path = path_;
#ifdef PEER_PC_CLIENT
        request.head().connection = util::protocol::http_filed::Connection::close;
#else
        request.head().connection = util::protocol::http_field::Connection::close;
#endif

        std::ostream os(&(request.data())); 

        os << data_to_post.rdbuf();

        http_client->async_fetch(request, boost::bind(&HttpPost::OnPostResult, shared_from_this(), http_client, _1));
    }
}
