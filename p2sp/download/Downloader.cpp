//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "p2sp/download/Downloader.h"
#include "p2sp/p2s/HttpDownloader.h"
#include "p2sp/p2p/P2PDownloader.h"

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>

namespace p2sp
{
    HttpDownloader::p Downloader::CreateByUrl(
        boost::asio::io_service & io_svc,
        const protocol::UrlInfo& url_info,
        DownloadDriver__p download_driver,
        bool is_open_service)
    {
        if (url_info.type_ == protocol::UrlInfo::HTTP)
        {
            assert(! url_info.url_.empty());
            string url = boost::algorithm::to_lower_copy(url_info.url_);
            if (boost::algorithm::starts_with(url, "http://"))
            {
                return HttpDownloader::Create(io_svc, url_info, download_driver, is_open_service);
            }
            else
            {
                assert(0);
                return HttpDownloader::p();
            }
        }
        else
        {
            assert(0);
            return HttpDownloader::p();
        }
    }

    HttpDownloader::p Downloader::CreateByUrl(
        boost::asio::io_service & io_svc,
        const network::HttpRequest::p http_request_demo,
        const protocol::UrlInfo& url_info,
        DownloadDriver__p download_driver,
        bool is_to_get_header,
        bool is_open_service)
    {
        if (url_info.type_ == protocol::UrlInfo::HTTP)
        {
            assert(! url_info.url_.empty());
            string url = boost::algorithm::to_lower_copy(url_info.url_);
            if (boost::algorithm::starts_with(url, "http://"))
            {
                return HttpDownloader::Create(io_svc, http_request_demo, url_info, download_driver, is_to_get_header, is_open_service);
            }
            else
            {
                assert(!"Invalid Url");
                return HttpDownloader::p();
            }
        }
        else
        {
            assert(0);
            return HttpDownloader::p();
        }

    }
}
