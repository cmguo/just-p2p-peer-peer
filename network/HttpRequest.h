//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef FRAMEWORK_NETWORK_HTTPREQUEST_H
#define FRAMEWORK_NETWORK_HTTPREQUEST_H

#include <boost/asio/streambuf.hpp>

namespace network
{
    struct HttpRequestInfo;
    class HttpRequest: boost::noncopyable
#ifdef DUMP_OBJECT
        , public count_object_allocate<HttpRequest>
#endif
    {
        friend struct HttpRequestInfo;
        friend std::ostream& operator << (std::ostream& out, const network::HttpRequest& http_request);
    public:
        typedef boost::shared_ptr<network::HttpRequest> p;
        static p ParseFromBuffer(string request);
        static p ParseFromBuffer(boost::asio::streambuf buf);
    public:
        string GetMethod() const
        {
            return method_;
        }
        string GetPath() const
        {
            return path_;
        }
        string GetVersion() const
        {
            return version_;
        }
        string GetProperty(const string& key);
        void SetPath(const string& path)
        {
            path_ = path;
        }
        bool HasProperty(const string& key) const;
        string GetPragma(const string& key);
        string RemovePragma(const string& key);
        bool HasPragma(const string& key) const;
        string GetHost();
        string GetUrl();
        string GetRefererUrl();
        string GetRequestString();
        bool ReSetHttpUrl();
    private:
        string method_;
        string path_;
        string version_;
        std::map<string, string> properties_;
        std::map<string, string> pragmas_;
        std::list<std::pair<string, string> > header_lines_;
        string request_string_;
    };

    std::ostream& operator << (std::ostream& out, const HttpRequest& http_request);

    struct HttpRequestInfo
    {
        // data
        string method_;
        string domain_;
        string version_;
        string host_;
        string path_;
        string refer_url_;
        uint32_t range_begin_;
        uint32_t range_end_;
        string ip_;
        string user_agent_;
        boost::uint16_t port_;
        std::map<string, string> pragmas_;
        // template
        HttpRequest::p http_request_demo_;
        bool is_accept_gzip_;

        HttpRequestInfo()
            : method_("GET")
            , domain_("")
            , version_("HTTP/1.0")
            , host_("")
            , path_("/")
            , refer_url_("")
            , range_begin_(0)
            , range_end_(0)
            , ip_("")
            , port_(80)
            , is_accept_gzip_(false)
            , user_agent_("Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1;)")
        {
        }
        string ToString();
        string ToFlvStartRangeString();
        string GetHost();

        bool operator !() const
        {
            return (host_ == "" || path_ == "" || port_ == 0);
        }
    };

    string ProxyRequestToDirectRequest(string proxy_request_string);
}

#endif  // FRAMEWORK_NETWORK_HTTPREQUEST_H
