//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _P2SP_CONFIGURATION_DOWNLOADER_H_
#define _P2SP_CONFIGURATION_DOWNLOADER_H_

#include "util/protocol/http/HttpClient.h"

namespace p2sp
{
    class IConfigurationDownloadListener;

    class ConfigurationDownloader
        : public boost::enable_shared_from_this<ConfigurationDownloader>
    {
        boost::shared_ptr<IConfigurationDownloadListener> download_listener_;
        boost::asio::io_service& io_service_;
        string server_;
        string path_;

    public:
        ConfigurationDownloader(
            boost::asio::io_service& io_service, 
            const string& server,
            const string& path,
            boost::shared_ptr<IConfigurationDownloadListener> download_listener);

        //下载结果(完成或出错)会经由所登记的download_listener返回
        void AsyncDownload();

    private:
        void HandleFetchResult(boost::shared_ptr<util::protocol::HttpClient> http_client, const boost::system::error_code& err);
    };
}

#endif  // _P2SP_CONFIGURATION_DOWNLOADER_H_
