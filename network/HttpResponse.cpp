//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "network/HttpResponse.h"

#include <framework/string/Algorithm.h>

#include <boost/cast.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/lexical_cast.hpp>

namespace network
{
    HttpResponse::p HttpResponse::ParseFromStream(std::istream& iss, boost::uint32_t http_header_length)
    {
        return HttpResponse::p();
    }
    HttpResponse::p HttpResponse::ParseFromBufferMini(string response, boost::uint32_t& http_header_length)
    {
        http_header_length = response.find("\r\n\r\n");
        if (string::npos == http_header_length)
        {
            return HttpResponse::p();
        }
        http_header_length += 4;
        HttpResponse::p http_response(new HttpResponse);

        http_response->response_header_string_ = response;
        http_response->response_modified_ = false;

        std::vector<string> lines;
        string first_line = "";
        // lines = framework::util::splite(response, "\r\n");
        framework::string::slice<string>(response, std::inserter(lines, lines.end()), "\r\n");

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

            boost::uint32_t i = line.find_first_of(':');
            if (i == string::npos)
                continue;
            string key = line.substr(0, i);
            string val = line.substr(i + 1);
            boost::algorithm::trim(key);
            boost::algorithm::trim(val);

            if (boost::algorithm::to_lower_copy(key) != "pragma")
            {
                if (http_response->properties_.find(key) != http_response->properties_.end())
                {
                    continue;
                }
                http_response->properties_[key] = val;
            }
            else
            {
                std::vector<string> pragma_key_value;
                boost::algorithm::split(pragma_key_value, val, boost::algorithm::is_any_of("="));
                if (pragma_key_value.size() != 2)
                    continue;
                string pragma_key = pragma_key_value[0];
                boost::algorithm::trim(pragma_key);
                string pragma_val = pragma_key_value[1];
                boost::algorithm::trim(pragma_val);
                if (http_response->pragmas_.find(pragma_key) != http_response->pragmas_.end())
                {
                    continue;
                }
                http_response->pragmas_[pragma_key] = pragma_val;
            }
        }

        std::vector<string> str_s;
        boost::algorithm::split(str_s, first_line, boost::algorithm::is_any_of(" "));
        if (str_s.size() < 3)
        {
            return HttpResponse::p();
        }
        boost::algorithm::trim(str_s[0]);
        boost::algorithm::to_upper(str_s[0]);
        if (!boost::algorithm::starts_with(str_s[0], "HTTP"))
        {
            return HttpResponse::p();
        }
        http_response->http_version_ = str_s[0];

        // http_response->status_code_ = boost::lexical_cast<boost::uint32_t>(str_s[1]);
        boost::system::error_code ec = framework::string::parse2(str_s[1], http_response->status_code_);
        if (ec)
        {
            return HttpResponse::p();
        }

        for (boost::uint32_t i = 2; i < str_s.size(); i++)
            http_response->status_string_.append(str_s[i] + " ");
        boost::algorithm::trim(http_response->status_string_);

        return http_response;
    }

    HttpResponse::p HttpResponse::ParseFromBuffer(string response, boost::uint32_t& http_header_length)
    {
        HttpResponse::p http_response = ParseFromBufferMini(response, http_header_length);

        if (!http_response)
            return http_response;

        http_response->range_begin_ = 0;
        http_response->range_end_ = http_response->GetContentLength() - 1;
        http_response->file_length_ = http_response->GetContentLength();

        if (http_response->properties_.find("Content-Range") != http_response->properties_.end())
        {
            do
            {
                string content_range_string = http_response->properties_["Content-Range"];
                boost::algorithm::trim(content_range_string);
                if (false == boost::algorithm::starts_with(content_range_string, "bytes "))
                {
                    return HttpResponse::p();
                }
                content_range_string = content_range_string.substr(6);
                boost::algorithm::trim(content_range_string);
                boost::uint32_t range_end_position = content_range_string.find('/');
                if (range_end_position == string::npos)
                {
                    return HttpResponse::p();
                }
                string range_string = content_range_string.substr(0, range_end_position);
                string file_length_string = content_range_string.substr(range_end_position + 1);
                std::vector<string> range_st_s;
                boost::algorithm::split(range_st_s, range_string, boost::algorithm::is_any_of("-"));
                if (range_st_s.size() != 2)
                    break;

                // http_response->range_begin_ = boost::lexical_cast<boost::uint32_t>(range_st_s[0]);
                boost::system::error_code ec = framework::string::parse2(range_st_s[0], http_response->range_begin_);
                if (ec)
                    return HttpResponse::p();

                // http_response->range_end_ = boost::lexical_cast<boost::uint32_t>(range_st_s[1]);
                ec = framework::string::parse2(range_st_s[1], http_response->range_end_);
                if (ec)
                    return HttpResponse::p();

                // http_response->file_length_ = boost::lexical_cast<boost::uint32_t>(file_length_string);
                ec = framework::string::parse2(file_length_string, http_response->file_length_);
                if (ec)
                    return HttpResponse::p();

            } while (false);
        }

        return http_response;
    }

    HttpResponse::p HttpResponse::ParseFromBufferByFlvStart(string response, boost::uint32_t start, boost::uint32_t& http_header_length)
    {
        HttpResponse::p http_response = ParseFromBufferMini(response, http_header_length);

        if (start > 0 && http_response->GetStatusCode() == 200)
        {
            assert(http_response->HasContentLength());
            http_response->range_begin_ = start;
            http_response->file_length_ = start + http_response->GetContentLength() - 13;
        }
        else
        {
            http_response->range_begin_ = start;
            http_response->file_length_ = start + http_response->GetContentLength();
        }
        http_response->range_end_ = http_response->file_length_ - 1;

        return http_response;
    }

    string HttpResponse::GetProperty(const string& key)
    {
        if (properties_.find(key) == properties_.end())
            return "";
        return properties_[key];
    }

    void HttpResponse::SetProperty(const string& key, const string& value)
    {
        std::map<string, string>::iterator it = properties_.find(key);
        if (it == properties_.end() || it->second != value)
        {
            properties_[key] = value;
            response_modified_ = true;
        }
    }

    bool HttpResponse::RemoveProperty(const string& key)
    {
        std::map<string, string>::iterator it = properties_.find(key);
        if (it != properties_.end())
        {
            properties_.erase(it);
            response_modified_ = true;
            return true;
        }
        return false;
    }

    string HttpResponse::GetPragma(const string& key)
    {
        if (pragmas_.find(key) == pragmas_.end())
            return "";
        return pragmas_[key];
    }

    void HttpResponse::SetPragma(const string& key, const string& value)
    {
        pragmas_[key] = value;
    }

    bool HttpResponse::HasContentLength()
    {
        return properties_.find("Content-Length") != properties_.end();
    }

    boost::uint32_t HttpResponse::GetContentLength()
    {
        string content_length_string = GetProperty("Content-Length");

        // boost::uint32_t content_length = boost::lexical_cast<boost::uint32_t>(content_length_string);
        boost::uint32_t content_length = std::numeric_limits<boost::uint32_t>::max();
        boost::system::error_code ec = framework::string::parse2(content_length_string, content_length);
        if (ec)
            return 0;
        return content_length;
    }

    void HttpResponse::SetContentLength(boost::uint32_t content_length)
    {
        string content_length_text = framework::string::format(content_length);
        SetProperty("Content-Length", content_length_text);
    }

    string HttpResponse::GetContentType()
    {
        return GetProperty("Content-Type");
    }

    bool HttpResponse::HasContentRange()
    {
        return properties_.find("Content-Range") != properties_.end();
    }

    bool HttpResponse::IsChunked()
    {
        if (properties_.find("Transfer-Encoding") == properties_.end())
            return false;
        else if (properties_["Transfer-Encoding"] != "chunked")
            return false;

        return true;
    }

    string HttpResponse::ToString()
    {
        if (true == response_modified_)
        {
            response_header_string_ = boost::lexical_cast<string>(*this);
            response_modified_ = false;
        }
        return response_header_string_;
    }

    std::ostream& operator << (std::ostream& out, const HttpResponse& http_response)
    {
        out << http_response.http_version_ << " " << http_response.status_code_ << " " << http_response.GetStatusString()
            << "\r\n";
        // properties
        for (std::map<string, string>::const_iterator iter = http_response.properties_.begin(); iter
            != http_response.properties_.end(); ++iter)
        {
            out << iter->first << ": " << iter->second << "\r\n";
        }
        // pragma
        for (std::map<string, string>::const_iterator iter = http_response.pragmas_.begin(); iter
            != http_response.pragmas_.end(); ++iter)
        {
            if (iter->second.empty())
                out << "Pragma: " << iter->first << "\r\n";
            else
                out << "Pragma: " << iter->first << "=" << iter->second << "\r\n";
        }
        out << "\r\n";
        return out;
    }

    bool HttpResponse::IsGzip()
    {
        return GetProperty("Content-Encoding") == "gzip";
    }
}
