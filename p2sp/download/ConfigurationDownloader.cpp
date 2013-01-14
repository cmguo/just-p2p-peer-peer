//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "p2sp/download/ConfigurationDownloader.h"
#include "p2sp/download/IConfigurationDownloadListener.h"
#include <boost/algorithm/string/split.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/bind.hpp>
#include <boost/algorithm/string/classification.hpp>

namespace p2sp
{
    ConfigurationDownloader::ConfigurationDownloader(
        boost::asio::io_service& io_service, 
        const string& server,
        const string& path,
        boost::shared_ptr<IConfigurationDownloadListener> download_listener)
        : server_(server), path_(path), download_listener_(download_listener), io_service_(io_service)
    {
    }

    void ConfigurationDownloader::AsyncDownload()
    {
        boost::shared_ptr<util::protocol::HttpClient> http_client(new util::protocol::HttpClient(io_service_));

        boost::system::error_code error;
        
        std::vector<string> host_and_port;
        boost::algorithm::split(host_and_port, server_, boost::algorithm::is_any_of(":"));

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
            download_listener_->OnDownloadFailed(error);
            return;
        }
        
        util::protocol::HttpRequestHead head;
        head.method = util::protocol::HttpRequestHead::get;
        head.path = path_;
        head["Accept"] = "{*/*}";
        head.host = server_;
#ifdef PEER_PC_CLIENT
        head.connection = util::protocol::http_filed::Connection::close;
#else
        head.connection = util::protocol::http_field::Connection::close;
#endif

        http_client->async_fetch(head, boost::bind(&ConfigurationDownloader::HandleFetchResult, shared_from_this(), http_client, _1));
    }

    void ConfigurationDownloader::HandleFetchResult(
        boost::shared_ptr<util::protocol::HttpClient> http_client, 
        const boost::system::error_code& err)
    {
        if (err)
        {
            download_listener_->OnDownloadFailed(err);
        }
        else
        {
            boost::asio::streambuf& response = http_client->response().data();
            std::string response_content((std::istreambuf_iterator<char>(&response)), std::istreambuf_iterator<char>() );
            download_listener_->OnDownloadSucceeded(response_content);
        }

        http_client->close();
    }
}
