//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// HttpDragDownloader.cpp

#include "Common.h"
#include "HttpDragDownloader.h"
#include <boost/program_options.hpp>
#include <boost/algorithm/string.hpp>
#include <cstdlib>

#include "base/util.h"
#include "network/Uri.h"
#include "p2sp/download/DownloadDriver.h"
#include "p2sp/bootstrap/BootStrapGeneralConfig.h"

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("tinydrag");
    static const int max_error_num_per_domain = 2;

    HttpDragDownloader::p HttpDragDownloader::Create(boost::asio::io_service & io_svc, DownloadDriver__p download_driver,
        string url)
    {
        network::Uri uri(url);
        string filename(uri.getfile());
        string segno = base::util::GetSegno(uri);
        return p(new HttpDragDownloader(io_svc, download_driver, filename, std::atoi(segno.c_str())));
    }

    HttpDragDownloader::HttpDragDownloader(
        boost::asio::io_service & io_svc, DownloadDriver__p download_driver, 
        string filename, int segno)
        : io_svc_(io_svc), download_driver_(download_driver)
        , filename_(filename), segno_(segno), using_udp_proxy_(false)
        , udp_proxy_port_(80), error_times_(0), tried_times_(0), using_backup_domain_(false)
    {
        string hou_list = BootStrapGeneralConfig::Inst()->GetHouServerList();
        boost::algorithm::split(udp_proxy_domain_vec_, hou_list,
            boost::algorithm::is_any_of("@"));
    }

    void HttpDragDownloader::Start()
    {
        Connect();
    }

    void HttpDragDownloader::Stop()
    {
        if (client_)
        {
            client_->Close();
            client_.reset();
        }

        if (proxy_client_)
        {
            proxy_client_->Close();
            proxy_client_.reset();
        }

        download_driver_.reset();
    }

    void HttpDragDownloader::Connect()
    {
        string request_url = ConstructUrl();
        ++tried_times_;

        if (using_udp_proxy_)
        {
            if (proxy_client_)
            {
                proxy_client_->Close();
                proxy_client_.reset();
            }
            
            proxy_client_ = network::HttpClientOverUdpProxy::create(io_svc_, 
                using_proxy_domain_, udp_proxy_port_, request_url);
            proxy_client_->SetHandler(shared_from_this());
            if (error_times_ < (boost::int32_t)udp_proxy_domain_vec_.size())
            {
                // 第一次尝试所有udp服务器超时时间设置为2s
                proxy_client_->SetRecvTimeoutInSec(2);
            }

            proxy_client_->Connect();
            DebugLog("Udp Proxy Connect %s", request_url.c_str());
            LOG(__DEBUG, "", "Udp Proxy Connect " << request_url);
        }
        else
        {
            if (client_)
            {
                client_->Close();
                client_.reset();
            }

            client_ = network::HttpClient<protocol::SubPieceContent>::create(io_svc_, request_url, "", 0, 0, false);
            client_->SetHandler(shared_from_this());
            client_->SetRecvTimeout(5 * 1000);
            client_->Connect();
            DebugLog("Connect %s", request_url.c_str());
            LOG(__DEBUG, "", "Connect " << request_url);
        }
    }

    string HttpDragDownloader::ConstructUrl()
    {
        std::ostringstream request_path_stream;
        request_path_stream << "/" << segno_ << "/" << filename_ << "0drag";
        if (!using_backup_domain_)
        {
            return string("tinydrag.synacast.com") + request_path_stream.str();
        }
        else
        {
            return string("tinydrag.pptv.com") + request_path_stream.str();
        }
    }

    void HttpDragDownloader::OnConnectSucced()
    {
        DebugLog("HttpDragDownloader::OnConnectSucced");
        LOG(__DEBUG, "", "HttpDragDownloader::OnConnectSucced");
        if (using_udp_proxy_)
        {
            proxy_client_->HttpGet();
        }
        else
        {
            client_->HttpGet();
        }
    }

    void HttpDragDownloader::OnConnectFailed(uint32_t error_code)
    {
        DebugLog("HttpDragDownloader::OnConnectFailed error_code:%d, error_times:%d", (int)error_code, error_times_);
        LOG(__DEBUG, "", "HttpDragDownloader::OnConnectFailed error_code:" << error_code << ", error_times:" << error_times_);
        DealError(error_code == 2 || error_code ==3);
    }

    void HttpDragDownloader::OnConnectTimeout()
    {
        DebugLog("HttpDragDownloader::OnConnectTimeout error_times:%d", error_times_);
        LOG(__DEBUG, "", "HttpDragDownloader::OnConnectTimeout error_times:" << error_times_);
        DealError();
    }

    void HttpDragDownloader::OnRecvHttpHeaderSucced(network::HttpResponse::p http_response)
    {
        download_driver_->ReportDragHttpStatus(http_response->GetStatusCode());

        switch (http_response->GetStatusCode())
        {
        case 200:
            DebugLog("HttpDragDownloader::OnRecvHttpHeaderSucced drag_length:%d", http_response->GetContentLength());
            LOG(__DEBUG, "", "HttpDragDownloader::OnRecvHttpHeaderSucced drag_length:" << http_response->GetContentLength());

            drag_length_ = http_response->GetContentLength();
            drag_string_.clear();
            Recv(drag_length_);
        	break;
        default:
            DebugLog("HttpDragDownloader::OnRecvHttpHeaderSucced error_code:%d", http_response->GetStatusCode());
            DealError();
        }
    }

    void HttpDragDownloader::OnRecvHttpHeaderFailed(uint32_t error_code)
    {
        DebugLog("HttpDragDownloader::OnRecvHttpHeaderFailed error_code:%d, error_times:%d", 
            (int)error_code, error_times_);
        LOG(__DEBUG, "", "HttpDragDownloader::OnRecvHttpHeaderFailed error_code:" << error_code
            << ", error_times:" << error_times_);
        DealError();
    }

    void HttpDragDownloader::OnRecvHttpDataPartial(
        protocol::SubPieceBuffer const & buffer, uint32_t file_offset, uint32_t content_offset)
    {
        assert(false);
        DealError();
    }

    void HttpDragDownloader::OnRecvHttpDataFailed(uint32_t error_code)
    {
        DebugLog("HttpDragDownloader::OnRecvHttpDataFailed error_code:%d, error_times:%d", 
            (int)error_code, error_times_);
        LOG(__DEBUG, "", "HttpDragDownloader::OnRecvHttpDataFailed error_code:" << error_code
            << ", error_times:" << error_times_);
        DealError();
    }

    void HttpDragDownloader::OnRecvTimeout()
    {
        DebugLog("OnRecvTimeout error_times:%d", error_times_);
        LOG(__DEBUG, "", "OnRecvTimeout error_times:" << error_times_);
        DealError();
    }

    void HttpDragDownloader::OnComplete()
    {
        DebugLog("OnComplete");
    }

    void HttpDragDownloader::DealError(bool dns_error)
    {
        error_times_++;
        int max_error_num = using_udp_proxy_ ? 
            max_error_num_per_domain * udp_proxy_domain_vec_.size() : max_error_num_per_domain;

        if (error_times_ < max_error_num)
        {
            DebugLog("HttpDragDownloader::DealError error_times:%d", error_times_);
            LOG(__DEBUG, "", "HttpDragDownloader::DealError error_times:" << error_times_);
            // TODO(herain):2011-3-28:是否考虑延时重试？
            if (using_udp_proxy_)
            {
                using_proxy_domain_ = udp_proxy_domain_vec_[error_times_ % udp_proxy_domain_vec_.size()];
            }

            Connect();
        }
        else
        {
            DebugLog("HttpDragDownloader::DealError error_times:%d, using_udp_proxy:%d", error_times_, using_udp_proxy_);
            LOG(__DEBUG, "", "HttpDragDownloader::DealError error_times:" << error_times_
                << ", using_udp_proxy:" << using_udp_proxy_);
            if (using_udp_proxy_)
            {
                proxy_client_->Close();
                proxy_client_.reset();
                
                // 获取Drag失败要汇报，drag_fetch_result定义见DownloadDriver.h
                uint32_t drag_fetch_result = 0;
                drag_fetch_result |= tried_times_ << 24;
                download_driver_->ReportDragFetchResult(drag_fetch_result, 0, 0, 0, tried_times_, 0);
            }
            else
            {
                if (dns_error && !using_backup_domain_)
                {
                    error_times_ = 0;
                    using_backup_domain_ = true;
                    Connect();
                }
                else
                {
                    client_->Close();
                    client_.reset();

                    if (!udp_proxy_domain_vec_.empty())
                    {
                        error_times_ = 0;
                        using_udp_proxy_ = true;
                        using_proxy_domain_ = udp_proxy_domain_vec_[0];
                        Connect();
                    }
                }
            }
        }
    }

    void HttpDragDownloader::Recv(uint32_t recv_length)
    {
        uint32_t real_recv_length = std::min(network::HttpClient<protocol::SubPieceContent>::MaxRecvLength, recv_length);
        if (using_udp_proxy_)
        {
            proxy_client_->HttpRecv(real_recv_length);
        }
        else
        {
            client_->HttpRecv(real_recv_length);
        }
    }

    void HttpDragDownloader::OnRecvHttpDataSucced(
        protocol::SubPieceBuffer const & buffer, uint32_t file_offset, uint32_t content_offset, bool is_gzip)
    {
        DebugLog("HttpDragDownloader::OnRecvHttpDataSucced fetch_time:%d", fetch_timer_.elapsed());
        LOG(__DEBUG, "", "HttpDragDownloader::OnRecvHttpDataSucced fetch_time:" << fetch_timer_.elapsed());
        
        assert(content_offset == drag_string_.size());
        uint32_t old_drag_string_size = drag_string_.size();
        drag_string_.resize(old_drag_string_size + buffer.Length());
        base::util::memcpy2((void*)(drag_string_.c_str() + old_drag_string_size), buffer.Length(),
            buffer.Data(), buffer.Length());

        if (drag_string_.size() < drag_length_)
        {
            Recv(drag_length_ - drag_string_.size());
            return;
        }
        else
        {
            // TODO(herain):2011-6-29:udp proxy会在content最后多加一个\r\n造成下面的assert不成立，暂时去掉
            //assert(drag_string_.size() == drag_length_);

            // drag_fetch_result的定义见downloaddriver.h
            uint32_t drag_fetch_result = 0x80000000;
            uint32_t is_parse_tinydrag_success = 0;

            if (using_udp_proxy_)
            {
                drag_fetch_result |= 0x40000000;
            }

            drag_fetch_result |= tried_times_<< 24;
            drag_fetch_result |= fetch_timer_.elapsed(); 

            if(ParseTinyDrag())
            {
                // Drag获取成功并解析成功
                drag_fetch_result |= 0x20000000;
                is_parse_tinydrag_success = 1;
            }
            
            download_driver_->ReportDragFetchResult(drag_fetch_result, 1, using_udp_proxy_, 
                is_parse_tinydrag_success, tried_times_, fetch_timer_.elapsed());

            if (using_udp_proxy_)
            {
                proxy_client_->Close();
                proxy_client_.reset();
            }
            else
            {
                client_->Close();
                client_.reset();
            }
        }        
    }

    bool HttpDragDownloader::ParseTinyDrag()
    {
        namespace po = boost::program_options;
        
        LOG(__DEBUG, "", "HttpDragDownloader::ParseTinyDrag");
        LOG(__DEBUG, "", drag_string_);

        std::istringstream drag_stream(drag_string_);

        try
        {
            po::options_description config("drag");
            config.add_options()
                ("tinydrag.n", po::value<int>())
                ("tinydrag.h", po::value<int>())
                ("tinydrag.r", po::value<string>())
                ("tinydrag.f", po::value<int>())
                ("tinydrag.s", po::value<int>())
                ("tinydrag.m", po::value<string>());

            po::variables_map vm;
            po::store(po::parse_config_file(drag_stream, config), vm);
            po::notify(vm);

            if (vm.count("tinydrag.n") == 0 ||
                vm.count("tinydrag.r") == 0 ||
                vm.count("tinydrag.f") == 0 ||
                vm.count("tinydrag.s") == 0 ||
                vm.count("tinydrag.m") == 0)
            {
                assert(false);
                return false;
            }

            protocol::RidInfo ridinfo;
            boost::system::error_code ec;

            ec = ridinfo.rid_.from_string(vm["tinydrag.r"].as<string>());
            if (ec)
            {
                return false;
            }

            LOG(__DEBUG, "", "RID: " << ridinfo.GetRID().to_string());
            DebugLog("RID:%s", ridinfo.rid_.to_string().c_str());

            ridinfo.file_length_ = vm["tinydrag.f"].as<int>();
            ridinfo.block_size_ = vm["tinydrag.s"].as<int>();
            LOG(__DEBUG, "", "file_length: " << ridinfo.GetFileLength());
            LOG(__DEBUG, "", "block_size: " << ridinfo.GetBlockSize());
            DebugLog("file_length:%d", ridinfo.GetFileLength());
            DebugLog("block_size:%d", ridinfo.GetBlockSize());

            string block_md5s = vm["tinydrag.m"].as<string>();
            vector<string> md5_vec;
            boost::algorithm::split(md5_vec, block_md5s, boost::bind(std::equal_to<char>(), '@', _1));
            if (block_md5s.size() == 0)
            {
                return false;
            }

            ridinfo.block_count_ = md5_vec.size();
            for (int i = 0; i < (int)md5_vec.size(); ++i)
            {
                MD5 md5;
                ec = md5.from_string(md5_vec[i]);
                if (ec)
                {
                    return false;
                }

                DebugLog("block_md5s[%d]:%s", i, md5_vec[i].c_str());
                LOG(__DEBUG, "", "block_md5s[" << i << "]: " << md5_vec[i]);

                ridinfo.block_md5_s_.push_back(md5);
            }

            LOG(__DEBUG, "", "SetRidInfo");
            download_driver_->SetRidInfo(ridinfo);
            return true;
        }
        catch (boost::program_options::error & e)
        {
            // TODO(herain):2011-3-29:统计Drag出错的数量
            DebugLog("Exception catched: ", e.what());
            LOG(__DEBUG, "", "Exception catched: " << e.what());
            return false;
        }
    }
}
