#ifndef CROSS_DOMAIN_CONFIG
#define CROSS_DOMAIN_CONFIG

#include "network/HttpClient.h"

namespace network
{
    class CrossDomainConfig
        : public boost::noncopyable
        , public network::IHttpClientListener<protocol::SubPieceBuffer>
        , public boost::enable_shared_from_this<CrossDomainConfig>
    {
    public:
        typedef boost::shared_ptr<CrossDomainConfig> p;      
        string GetCrossDomainString() const;
        void LoadConfig();
        void Stop();
    public:
        static CrossDomainConfig::p GetInstance();

    private:
        void SetCrossDomainString(const string& cross_domain_string);
        void NeedReload();
        CrossDomainConfig();

        //inherit from IHttpClientListener
    public:
        virtual void OnConnectSucced();
        virtual void OnConnectFailed(boost::uint32_t error_code);
        virtual void OnConnectTimeout();

        virtual void OnRecvHttpHeaderSucced(network::HttpResponse::p http_response);
        virtual void OnRecvHttpHeaderFailed(boost::uint32_t error_code);
        virtual void OnRecvHttpDataSucced(protocol::SubPieceBuffer const & buffer, boost::uint32_t file_offset, boost::uint32_t content_offset, bool is_gzip);
        virtual void OnRecvHttpDataPartial(protocol::SubPieceBuffer const & buffer, boost::uint32_t file_offset, boost::uint32_t content_offset);
        virtual void OnRecvHttpDataFailed(boost::uint32_t error_code);
        virtual void OnRecvTimeout();
        virtual void OnComplete();

    private:
        static CrossDomainConfig::p inst_;
        static const string cross_domain_url_;

    private:
        string cross_domain_string_;
        boost::uint32_t failed_count_;
        network::HttpClient<protocol::SubPieceContent>::p client_;
    };
}
#endif