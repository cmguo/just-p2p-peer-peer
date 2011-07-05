//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef FRAMEWORK_NETWORK_HTTPRESPONSE_H
#define FRAMEWORK_NETWORK_HTTPRESPONSE_H

namespace network
{
    class HttpResponse: boost::noncopyable
#ifdef DUMP_OBJECT
        , public count_object_allocate<HttpResponse>
#endif
    {
        friend std::ostream& operator << (std::ostream& out, const network::HttpResponse& http_response);
    public:
        typedef boost::shared_ptr<network::HttpResponse> p;
        static p ParseFromBuffer(string response, uint32_t& http_header_length);
        static p ParseFromBufferByFlvStart(string response, uint32_t start, uint32_t& http_header_length);
        static p ParseFromStream(std::istream& iss, uint32_t http_header_length);
    protected:
        static p ParseFromBufferMini(string response, uint32_t& http_header_length);
    public:
        uint32_t GetStatusCode() const
        {
            return status_code_;
        }
        string GetStatusString() const
        {
            return status_string_;
        }
        string GetProperty(const string& key);
        void SetProperty(const string& key, const string& value);
        bool RemoveProperty(const string& key);
        string GetPragma(const string& key);
        void SetPragma(const string& key, const string& value);
        // Content-Type
        string GetContentType();
        // Content-Length
        bool HasContentLength();
        uint32_t GetContentLength();
        void SetContentLength(uint32_t content_length);
        uint32_t GetFileLength()
        {
            return file_length_;
        }
        // Content-Range
        bool HasContentRange();
        uint32_t GetRangeBegin() const
        {
            return range_begin_;
        }
        uint32_t GetRangeEnd() const
        {
            return range_end_;
        }
        //
        bool IsChunked();
        string ToString();
    private:
        string response_header_string_;
        volatile bool response_modified_;

        string http_version_;
        uint32_t status_code_;
        string status_string_;
        std::map<string, string> properties_;
        std::map<string, string> pragmas_;
        uint32_t range_begin_;
        uint32_t range_end_;
        uint32_t file_length_;
    };

    std::ostream& operator << (std::ostream& out, const network::HttpResponse& http_response);
}

#endif  // FRAMEWORK_NETWORK_HTTPRESPONSE_H
