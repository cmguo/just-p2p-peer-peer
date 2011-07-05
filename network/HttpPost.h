//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _NETWORK_HTTP_POST_H_
#define _NETWORK_HTTP_POST_H_

#include "util/protocol/http/HttpClient.h"

namespace network
{
    class IHttpPostListener;

    class HttpPost
        : public boost::enable_shared_from_this<HttpPost>
    {
    private:
        boost::asio::io_service& service_;
        string server_;
        string path_;
        boost::shared_ptr<IHttpPostListener> post_listener_;

    public:
        HttpPost(boost::asio::io_service& service, string server, string path, boost::shared_ptr<IHttpPostListener> listener);

        void AsyncPost(std::istream& data_to_post);

    private:
        void OnPostResult(boost::shared_ptr<util::protocol::HttpClient> http_client, const boost::system::error_code& err);
    };
}

#endif  // _NETWORK_HTTP_POST_H_
