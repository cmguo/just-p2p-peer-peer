//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// HttpDragDownloader.h

#include "network/HttpClient.h"
#include "network/HttpClientOverUdpProxy.h"

namespace p2sp
{
    class DownloadDriver;
    typedef boost::shared_ptr<DownloadDriver> DownloadDriver__p;

    class HttpDragDownloader
        : public boost::noncopyable
        , public boost::enable_shared_from_this<HttpDragDownloader>
        , public network::IHttpClientListener<protocol::SubPieceBuffer>
#ifdef DUMP_OBJECT
        , public count_object_allocate<HttpDragDownloader>
#endif
    {
    public:
        typedef boost::shared_ptr<HttpDragDownloader> p;

        static p Create(boost::asio::io_service & io_svc, DownloadDriver__p download_driver,
            string url);

        void Start();
        void Stop();
        
    private:
        HttpDragDownloader(
            boost::asio::io_service & io_svc, DownloadDriver__p download_driver,
            string filename, int segno);

    private:
        void Connect();
        string ConstructUrl();
        void DealError(bool dns_error = false);
        void Recv(boost::uint32_t recv_length);
        bool ParseTinyDrag();

    private:
        //IHttpClientListener
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
        bool is_running_;

        boost::asio::io_service & io_svc_;
        DownloadDriver__p download_driver_;
        string filename_;
        int segno_;
        framework::timer::TickCounter fetch_timer_;

        bool using_udp_proxy_;
        string using_proxy_domain_;
        std::vector<string> udp_proxy_domain_vec_;
        boost::uint16_t udp_proxy_port_;

        network::HttpClient<protocol::SubPieceContent>::p client_;
        network::HttpClientOverUdpProxy::p proxy_client_;
        boost::int32_t error_times_;
        boost::int32_t tried_times_;

        boost::uint32_t drag_length_;
        string drag_string_;

        bool using_backup_domain_;
    };
}
