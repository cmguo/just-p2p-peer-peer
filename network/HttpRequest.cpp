//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "network/HttpRequest.h"
#include "network/Uri.h"

#include <framework/string/Algorithm.h>

#include <boost/format.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>

namespace network
{
    HttpRequest::p HttpRequest::ParseFromBuffer(boost::asio::streambuf buf)
    {
        string request_string;
        std::copy(std::istreambuf_iterator<char> (&buf), std::istreambuf_iterator<char> (), std::back_inserter(request_string));
        request_string += '\0';
        return ParseFromBuffer(request_string);
    }

    HttpRequest::p HttpRequest::ParseFromBuffer(string request_string)
    {
        if (string::npos == request_string.find("\r\n\r\n"))
        {  // HttpRequest ?в??????
            HttpRequest::p non_http_request;
            return non_http_request;
        }

        HttpRequest::p http_request(new HttpRequest);
        http_request->request_string_ = request_string;
        std::vector<string> lines;
        string first_line = "";
        // lines = framework::util::splite(request_string, "\r\n");
        framework::string::slice<string>(request_string, std::inserter(lines, lines.end()), "\r\n");

        for (std::vector<string>::iterator iter = lines.begin(); iter != lines.end(); iter++)
        {
            string line = *iter;
            if (boost::algorithm::trim_copy(line).empty())
                continue;
            if (first_line == "")
            {
                first_line = line;
                continue;
            }
            std::vector<string> key_value;
            uint32_t splite_pos = line.find_first_of(":");
            if (splite_pos == string::npos)
                continue;
            string key = line.substr(0, splite_pos);
            boost::algorithm::trim(key);
            string val = line.substr(splite_pos + 1, line.length() - splite_pos - 1);
            boost::algorithm::trim(val);
            http_request->header_lines_.push_back(std::make_pair(key, val));
            if (boost::algorithm::to_lower_copy(key) != "pragma")
            {
                if (http_request->properties_.find(key) != http_request->properties_.end())
                {
                    continue;
                }
                http_request->properties_[key] = val;
            }
            else
            {
                std::vector<string> pragma_key_value;
                boost::algorithm::split(pragma_key_value, val, boost::algorithm::is_any_of("="));
                if (pragma_key_value.size() == 1)
                {
                    string pragma_key = pragma_key_value[0];
                    boost::algorithm::trim(pragma_key);
                    string pragma_val = "";
                    if (http_request->pragmas_.find(pragma_key) != http_request->pragmas_.end())
                    {
                        continue;
                    }
                    http_request->pragmas_[pragma_key] = pragma_val;
                }
                else if (pragma_key_value.size() == 2)
                {
                    string pragma_key = pragma_key_value[0];
                    boost::algorithm::trim(pragma_key);
                    string pragma_val = pragma_key_value[1];
                    boost::algorithm::trim(pragma_val);
                    if (http_request->pragmas_.find(pragma_key) != http_request->pragmas_.end())
                    {
                        continue;
                    }
                    http_request->pragmas_[pragma_key] = pragma_val;
                }
            }
        }

        std::vector<string> str_s;
        boost::algorithm::split(str_s, first_line, boost::algorithm::is_any_of(" "));
        if (str_s.size() != 3)
        {
            HttpRequest::p non_http_request;
            return non_http_request;
        }

        http_request->method_ = str_s[0];
        boost::algorithm::trim(http_request->method_);

        http_request->path_ = str_s[1];
        boost::algorithm::trim(http_request->path_);

        http_request->version_ = str_s[2];
        boost::algorithm::trim(http_request->version_);

        return http_request;
    }

    bool HttpRequest::ReSetHttpUrl()
    {
        if (path_.substr(0, 4) != "http")
        {
            path_ = "http://" + GetHost() + path_;
            return true;
        }
        return false;
    }

    bool HttpRequest::HasProperty(const string& key) const
    {
        return properties_.find(key) != properties_.end();
    }

    string HttpRequest::GetProperty(const string& key)
    {
        if (properties_.find(key) == properties_.end())
            return "";
        return properties_[key];
    }

    bool HttpRequest::HasPragma(const string& key) const
    {
        return pragmas_.find(key) != pragmas_.end();
    }

    string HttpRequest::GetPragma(const string& key)
    {
        if (pragmas_.find(key) == pragmas_.end())
            return "";
        return pragmas_[key];
    }

    string HttpRequest::RemovePragma(const string& key)
    {
        std::map<string, string>::iterator it = pragmas_.find(key);
        if (it == pragmas_.end())
        {
            return "";
        }
        string pragma = it->second;
        pragmas_.erase(it);
        return pragma;
    }

    string HttpRequest::GetHost()
    {
        return GetProperty("Host");
    }

    string HttpRequest::GetUrl()
    {
        string lowel_path = boost::algorithm::to_lower_copy(path_);
        if (boost::algorithm::starts_with(lowel_path, "http://") == true)
        {
            return path_;
        }
        else if (boost::algorithm::starts_with(lowel_path, "/") == true)
        {
            string host = GetHost();
            if (host == "")
                host = "localhost";
            string url = "http://";
            url += host;
            url += path_;
            return url;
        }
        else
        {
            // assert(0);
            return "";
        }
    }

    string HttpRequest::GetRefererUrl()
    {
        return GetProperty("Referer");
    }

    string HttpRequest::GetRequestString()
    {
        return request_string_;
    }

    std::ostream& operator << (std::ostream& out, const HttpRequest& http_request)
    {
        out << http_request.GetMethod() << " " << http_request.GetPath() << " " << http_request.GetVersion() << "\r\n";
        for (std::map<string, string>::const_iterator iter = http_request.properties_.begin(); iter
            != http_request.properties_.end(); iter++)
        {
            out << iter->first << ": " << iter->second << std::endl;
        }
        out << std::endl;
        return out;
    }

    string HttpRequestInfo::ToString()
    {
        if (http_request_demo_)
        {
            method_ = http_request_demo_->GetMethod();
            version_ = http_request_demo_->GetVersion();
        }

        std::stringstream sstr;
        sstr << method_ << " " << path_ << " " << version_ << "\r\n";
        if (!http_request_demo_)
        {
            sstr << "Accept: */*\r\n";
            if (refer_url_ != "")
                sstr << "Referer: " << refer_url_ << "\r\n";
            sstr << "x-flash-version: 9,0,28,0\r\n";
            sstr
                << "User-Agent: Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1;)\r\n";
            if (host_ != "")
                sstr << "Host: " << host_ << "\r\n";
            sstr << "Connection: close\r\n";
            // pragams
            if (pragmas_.size() != 0)
            {
                for (std::map<string, string>::const_iterator iter = pragmas_.begin(); iter != pragmas_.end(); ++iter)
                {
                    if (iter->second.empty())
                        sstr << "Pragma: " << iter->first << "\r\n";
                    else
                        sstr << "Pragma: " << iter->first << "=" << iter->second << "\r\n";
                }
            }
        }
        else
        {
            for (std::list<std::pair<string, string> >::iterator iter = http_request_demo_->header_lines_.begin(); iter
                != http_request_demo_->header_lines_.end(); iter++)
            {
                string key = (*iter).first;
                string value = (*iter).second;
                if (key == "Referer")
                {
                    if (refer_url_ != "")
                        value = refer_url_;
                    if (refer_url_.substr(0, 4) != "http")
                        continue;
                }
                else if (key == "Host")
                {
                    if (host_ != "")
                        value = host_;
                }
                else if (key == "Range")
                    continue;
                else if (key == "Content-Length")
                    continue;
                else if (key == "Proxy-Connection")
                {
                    // if contains Pragma: Proxy, do not continue
                    if (false == http_request_demo_->HasPragma("Proxy"))
                        continue;
                }
                else if (key == "Pragma")
                {
                    uint32_t idx = value.find_first_of('=');
                    // remove Proxy
                    if (idx != string::npos && boost::algorithm::trim_copy(value.substr(0, idx)) == "Proxy")
                    {
                        continue;
                    }
                }
                sstr << key << ": " << value << "\r\n";
            }
        }
        if (range_begin_ != 0)
        {
            if (range_end_ == 0)
                sstr << "Range: bytes=" << range_begin_ << "-\r\n";
            else
            {
                assert(range_begin_ <= range_end_);
                sstr << "Range: bytes=" << range_begin_ << "-" << range_end_ << "\r\n";
            }
        }
        else
        {
            if (range_end_ != 0)
            {
                sstr << "Range: bytes=0-" << range_end_ << "\r\n";
            }
        }
        sstr << "\r\n";
        return sstr.str();
    }

    string HttpRequestInfo::ToFlvStartRangeString()
    {
        if (http_request_demo_)
        {
            method_ = http_request_demo_->GetMethod();
            version_ = http_request_demo_->GetVersion();
        }
        string path = path_;
        if (range_begin_ != 0)
        {
            assert(range_end_ == 0);
            assert(path.length() > 0);
            if (path.find("?") == string::npos)
                path.append(boost::str(boost::format("?start=%1%") % range_begin_));
            else
            {
                assert(0);
                if (path.find("start") == string::npos)
                    path.append(boost::str(boost::format("&start=%1%") % range_begin_));
            }
        }
        std::stringstream sstr;
        sstr << method_ << " " << path << " " << version_ << "\r\n";

        if (!http_request_demo_)
        {
            sstr << "Accept: */*\r\n";
            if (refer_url_ != "")
                sstr << "Referer: " << refer_url_ << "\r\n";
            sstr << "x-flash-version: 9,0,28,0\r\n";
            sstr
                << "User-Agent: Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1;)\r\n";
            if (host_ != "")
                sstr << "Host: " << host_ << "\r\n";
            sstr << "Connection: close\r\n";
            // pragmas
            if (pragmas_.size() != 0)
            {
                for (std::map<string, string>::const_iterator iter = pragmas_.begin(); iter != pragmas_.end(); ++iter)
                {
                    if (iter->second.empty())
                        sstr << "Pragma: " << iter->first << "\r\n";
                    else
                        sstr << "Pragma: " << iter->first << "=" << iter->second << "\r\n";
                }
            }
        }
        else
        {
            for (std::list<std::pair<string, string> >::iterator iter = http_request_demo_->header_lines_.begin(); iter
                != http_request_demo_->header_lines_.end(); iter++)
            {
                string key = (*iter).first;
                string value = (*iter).second;
                if (key == "Referer")
                {
                    if (refer_url_ != "")
                        value = refer_url_;
                }
                else if (key == "Host")
                {
                    if (host_ != "")
                        value = host_;
                }
                else if (key == "Range")
                    continue;
                else if (key == "Proxy-Connection")
                    continue;
                sstr << key << ": " << value << "\r\n";
            }
        }
        sstr << "\r\n";
        return sstr.str();
    }

    string HttpRequestInfo::GetHost()
    {
        assert(!(host_ == "" && ip_ == ""));
        if (ip_ != "")
            return ip_;
        if (host_ != "")
            return host_;
        return "";
    }

    string ProxyRequestToDirectRequest(string proxy_request_string)
    {
        string direct_request_string = "";
        std::vector<string> lines;
        bool is_first_line = true;
        // lines = framework::util::splite(proxy_request_string, "\r\n");
        framework::string::slice<string>(proxy_request_string, std::inserter(lines, lines.end()), "\r\n");

        for (std::vector<string>::iterator iter = lines.begin(); iter != lines.end(); iter++)
        {
            string line = *iter;
            boost::algorithm::trim(line);
            if (line.empty())
                continue;
            if (is_first_line == true)
            {
                std::vector<string> str_s;
                boost::algorithm::split(str_s, line, boost::algorithm::is_any_of(" "));
                if (str_s.size() < 3)
                {
                    return proxy_request_string;
                }
                if (false == boost::algorithm::starts_with(boost::algorithm::to_lower_copy(str_s[1]), "http://"))
                {
                    direct_request_string += line + "\r\n";
                }
                else
                {
                    Uri uri(str_s[1]);
                    direct_request_string += str_s[0] + " " + uri.getrequest() + " " + str_s[2] + "\r\n";
                }
                is_first_line = false;
            }

            std::vector<string> key_value;
            // key_value = framework::util::splite(line, ": ");
            framework::string::slice<string>(line, std::inserter(key_value, key_value.end()), ": ");
            if (key_value.size() != 2)
                continue;
            string key = key_value[0];
            boost::algorithm::trim(key);
            string val = key_value[1];
            boost::algorithm::trim(val);
            if (key == "Proxy-Connection")
                continue;
            direct_request_string += line;
            direct_request_string += "\r\n";
        }
        direct_request_string += "\r\n";
        return direct_request_string;
    }
}
