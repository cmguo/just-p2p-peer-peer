//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/proxy/ProxyModule.h"
#include "p2sp/proxy/ProxyConnection.h"
#include "p2sp/push/PushModule.h"
#include "p2sp/proxy/MessageBufferManager.h"
#include "p2sp/AppModule.h"
#include "p2sp/stun/StunModule.h"
#include "p2sp/download/DownloadDriver.h"
#include "p2sp/p2p/P2PDownloader.h"
#include "p2sp/stun/StunClient.h"

#include "p2sp/download/LiveDownloadDriver.h"

#include "statistic/StatisticModule.h"

#include "network/UrlCodec.h"
#include "base/util.h"
#include "downloadcenter/DownloadCenterModule.h"
#include "storage/Storage.h"
#include "storage/Instance.h"
#include "PlayInfo.h"

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("proxy");

    ProxyModule::p ProxyModule::inst_;

    ProxyModule::ProxyModule(
        boost::asio::io_service & io_svc)
        : io_svc_(io_svc)
        , proxy_timer_(global_250ms_timer(), 250, boost::bind(&ProxyModule::OnTimerElapsed, this, &proxy_timer_))
        , speed_query_counter_(false)
        , is_running_(false)
        , is_limit_download_connection_(false)
    {
    }

    void ProxyModule::Start(const string& config_path, boost::uint16_t local_http_proxy_port)
    {
        if (is_running_ == true) return;


        is_running_ = true;

        test_http_proxy_port_ = 0;

        if (config_path.length() == 0) {
            string szPath;
            if (base::util::GetAppDataPath(szPath)) {
                ppva_config_path_.assign(szPath);
            }
        }
        else {
            ppva_config_path_ = config_path;
        }

        history_max_download_speed_ = 0;
        LoadHistoricalMaxDownloadSpeed();
        if (local_http_proxy_port > 0)
        {
            acceptor_ = network::HttpAcceptor::create(io_svc_, shared_from_this());
            assert(acceptor_);

            boost::system::error_code error;
            boost::uint16_t port;
            boost::asio::ip::address localhost = boost::asio::ip::address::from_string("127.0.0.1", error);
            boost::uint16_t end_port = (std::min)(65535u, local_http_proxy_port + 1000u);
            for (port = local_http_proxy_port; port < end_port; port ++)
            {

                LOG(__INFO, "proxy", __FUNCTION__ << ":" << __LINE__ << " Try Endpoint " << localhost.to_string() << ":" << port);
                acceptor_->Close();
                // try 127.0.0.1
                boost::asio::ip::tcp::endpoint ep(localhost, port);
                if (acceptor_->Listen(ep))
                {
                    LOG(__DEBUG, "proxy", __FUNCTION__ << ":" << __LINE__ << " OK, Try to Listen 0.0.0.0:" << port);
                    // ok, change to listen all
                    acceptor_->Close();
                    if (acceptor_->Listen(port))
                    {
                        LOG(__DEBUG, "proxy", __FUNCTION__ << ":" << __LINE__ << " OK, Listen port: " << port);
                        acceptor_->TcpAccept();
                        // now try to hold "127.0.0.1:port"
                        acceptor_place_holder_ = network::HttpAcceptor::create(io_svc_, shared_from_this());
                        if (acceptor_place_holder_->Listen(ep)) {
                            LOG(__DEBUG, "proxy", __FUNCTION__ << ":" << __LINE__ << " OK, Hold Address: " << ep);
                            acceptor_place_holder_->TcpAccept();
                        }
                        break;
                    }
                    else
                    {
                        // 尝试 0.0.0.0 失败, 仅仅监听 127.0.0.1
                        LOG(__INFO, "proxy", "Try 0.0.0.0 Failed");
                        if (acceptor_->Listen(ep))
                        {
                            LOG(__INFO, "proxy", "Listen 127.0.0.1 Succeed");
                            acceptor_->TcpAccept();
                            break;
                        }
                        else
                        {
                            // 因为之前127.0.0.1已经监听成功了，所以这里不应该失败
                            assert(false);
                        }
                    }
                }
            }
            if (port >= end_port)
            {
                // 彻底失败
                acceptor_->Close();
                acceptor_.reset();
                is_running_ = false;
                LOG(__INFO, "proxy", "Try Failed: " << port);
                return;
            }
        }
        LOG(__INFO, "proxy", "Succeed: " << GetHttpPort());

        statistic::StatisticModule::Inst()->SetLocalPeerTcpPort(GetHttpPort());

   //     proxy_timer_ = framework::timer::PeriodicTimer::create(250, shared_from_this());
        proxy_timer_.start();
    }

    void ProxyModule::Stop()
    {
        if (is_running_ == false) return;
        LOGX(__EVENT, "proxy", "关闭ProxyModule");

        if (acceptor_)
        {
            acceptor_->Close();
        }
        if (acceptor_place_holder_)
        {
            acceptor_place_holder_->Close();
        }

        for (std::set<ProxyConnection::p>::iterator iter = proxy_connections_.begin();
            iter != proxy_connections_.end();
            iter ++)
        {
            (*iter)->Stop();
        }
        proxy_connections_.clear();

        proxy_timer_.stop();

        is_running_ = false;

        inst_.reset();
    }

    void ProxyModule::StartTestCoreSucced(boost::uint16_t test_core_proxy_port)
    {
        if (is_running_ == false) return;

        test_http_proxy_port_ = test_core_proxy_port;
        LOG(__INFO, "proxy", "StartTestCoreSucced");

        for (std::set<ProxyConnection__p>::iterator iter =  proxy_connections_.begin();
            iter != proxy_connections_.end();
            iter++)
        {
            if ((*iter)->IsTestCore())
            {
                (*iter)->SendHttpRequest();
            }
        }

    }

    void ProxyModule::OnTimerElapsed(framework::timer::Timer * pointer)
    {
        if (is_running_ == false) return;
        uint32_t times = pointer->times();
        if (pointer == &proxy_timer_)
        {
            OnProxyTimer(times);
        }
        else
        {
            assert(0);
        }
    }

    void ProxyModule::OnProxyTimer(uint32_t times)
    {
        if (is_running_ == false) return;

#ifdef TEST_LOCAL_READ
        return;
#endif

        if (times % 4 == 0)
        {
            boost::int32_t curr_download_speed = 0;

            for (std::set<ProxyConnection::p>::iterator i = proxy_connections_.begin();
                i != proxy_connections_.end();)
            {
                // ProxyConnection的OnProxyTimer里面有可能会调用WillStop
                // 会remove proxy_connections_集合里面的对象
                std::set<ProxyConnection::p>::iterator iter = i++;
                if ((*iter) && (*iter)->GetDownloadDriver())
                {
                    curr_download_speed += (*iter)->GetDownloadDriver()->GetSecondDownloadSpeed();
                    LOG(__WARN, "proxy", "second_speed " << (*iter)->GetDownloadDriver() << " : "
                        << (*iter)->GetDownloadDriver()->GetSecondDownloadSpeed() << "B/s");
                }
                (*iter)->OnProxyTimer(times);
            }

            // 每秒更新最大下载速度
            if (curr_download_speed > history_max_download_speed_)
            {
                history_max_download_speed_ = curr_download_speed;
            }
            LOG(__WARN, "proxy", "curr_download_speed:" << curr_download_speed << ", history_max_download_speed_" << history_max_download_speed_);

            // 全局限速管理
            GlobalSpeedLimit();
        }

        if (times % (4*5) == 0)  // 5s
        {
            uint32_t local_ip = p2sp::CStunClient::GetLocalFirstIP();
            if (local_ip_from_ini_ != local_ip)
            {
                history_max_download_speed_ = 0;
                history_max_download_speed_ini_ = 0;
                local_ip_from_ini_ = local_ip;
                LOG(__WARN, "proxy", "local_ip_from_ini_ != local_ip");
                SaveHistoricalMaxDownloadSpeed();
            }
            else if (history_max_download_speed_ > history_max_download_speed_ini_)
            {
                LOG(__WARN, "proxy", "history_max_download_speed_ini_ < history_max_download_speed_:" << history_max_download_speed_ini_ << "<" << history_max_download_speed_);
                history_max_download_speed_ini_ = history_max_download_speed_;
                SaveHistoricalMaxDownloadSpeed();
            }
        }

        if (times % (4*60) == 0)  // 1 min
        {
            // expire
            MessageBufferManager::Inst()->ExpireCache();

            // statistic
            statistic::StatisticModule::Inst()->SetLocalUploadPriority(AppModule::Inst()->GenUploadPriority());
            statistic::StatisticModule::Inst()->SetLocalIdleTime(AppModule::Inst()->GetIdleTimeInMins());
            statistic::StatisticModule::Inst()->SetLocalNatType(StunModule::Inst()->GetPeerNatType() & 0xFFu);

            // 10 min
            if (times % (4 * 60 * 10) == 0)
            {
                ExpireSegno();
            }
        }

    }

    void ProxyModule::RemoveProxyConnection(ProxyConnection::p server_socket)
    {
        if (false == is_running_)
            return;

        if (proxy_connections_.find(server_socket) == proxy_connections_.end())
        {
            LOG(__WARN, "proxy", "RemoveProxyConnection but ServerSocket Not Found");
            return;
        }

        // if (server_socket->GetSourceUrl().length() > 0 &&
        //    server_socket->GetSourceUrl() == PushModule::Inst()->GetCurrentTaskUrl())
        // {
        //    LOG(__DEBUG, "push", __FUNCTION__ << " ProxyConnection Stop, Remove Push Task!");
        //    PushModule::Inst()->StopCurrentTask();
        // }

        LOG(__EVENT, "proxy", "RemoveProxyConnection Succed");
        proxy_connections_.erase(server_socket);

        server_socket->Stop();
    }

    boost::uint16_t ProxyModule::GetHttpPort() const
    {
        if (false == is_running_)
            return 0;

        if (! acceptor_)
            return 0;
        return acceptor_->GetHttpPort();
    }

    void ProxyModule::OnHttpAccept(boost::shared_ptr<network::HttpServer> http_server_for_accept)
    {
        if (is_running_ == false) return;

        LOG(__EVENT, "proxy", "OnHttpAccept Succed " << http_server_for_accept->GetEndPoint());

        ProxyConnection::p pointer = ProxyConnection::create(io_svc_, http_server_for_accept);
        pointer->Start();
        proxy_connections_.insert(pointer);
    }

    void ProxyModule::OnHttpAcceptFailed()
    {
        LOG(__EVENT, "proxy", "OnHttpAccept Failed ");
        LOG(__EVENT, "pplive", "HttpAccept Failed");
    }

    ProxyConnection::p ProxyModule::GetProxyConnection(const string& url)
    {
        if (false == is_running_) {
            return ProxyConnection::p();
        }
        std::set<ProxyConnection::p>::iterator it;
        for (it = proxy_connections_.begin(); it != proxy_connections_.end(); ++it) {
            ProxyConnection::p conn = *it;
            string source_url = conn->GetSourceUrl();
            string original_url = conn->GetOriginalUrlInfo().url_;
            if (source_url == url || original_url == url) {
                return conn;
            }
        }
        return ProxyConnection::p();
    }

    void ProxyModule::StopProxyConnection(const string& url)
    {
        if (false == is_running_) {
            return;
        }
        // stop downloading
        ProxyConnection::p conn = GetProxyConnection(url);
        if (conn) {
            conn->SetPausedByUser(true);
            conn->WillStop();
        }
    }

    void ProxyModule::ForEachProxyConnection(boost::function<void(ProxyConnection__p)> processor)
    {
        if (false == is_running_) {
            return;
        }
        std::set<ProxyConnection::p>::iterator it;
        for (it = proxy_connections_.begin(); it != proxy_connections_.end(); ++it) {
            ProxyConnection::p conn = *it;
            processor(conn);
        }
    }

    void ProxyModule::StartDownloadFile(const string& url, const string& refer_url, const string& web_url, const string& qualified_file_name)
    {
        if (false == is_running_) {
            return;
        }

#ifdef DISK_MODE
        // check url
        if (true == downloadcenter::DownloadCenterModule::Inst()->IsUrlDownloading(url)) {
            LOG(__WARN, "downloadcenter", "");
            return;
        }
        // start download
        ProxyConnection::p download_center_conn = ProxyConnection::create(io_svc_);
        download_center_conn->Start();
        download_center_conn->OnNoticeDownloadMode(url, refer_url, web_url, qualified_file_name);
        proxy_connections_.insert(download_center_conn);
#endif  // #ifdef DISK_MODE

    }

    void ProxyModule::StartDownloadFileEx(const string& request_head, const string& web_url, const string& qualified_file_name, const string& source_url)
    {
        if (false == is_running_) {
            return;
        }
#ifdef DISK_MODE
        // parse
        network::HttpRequest::p http_request = network::HttpRequest::ParseFromBuffer(request_head);
        if (!http_request) {
            LOG(__ERROR, "downloadcenter", __FUNCTION__ << " Invalid Http Request: \n" << request_head);
            return;
        }
        // check url
        string pa = http_request->GetPath();
        pa = RemovePpvakeyFromUrl(pa);
        http_request->SetPath(pa);
        string url = http_request->GetUrl();
        LOG(__DEBUG, "downloadcenter", "Start Download Url: " << url);
        if (true == downloadcenter::DownloadCenterModule::Inst()->IsUrlDownloading(url)) {
            LOG(__WARN, "downloadcenter", "Url already exists: " << url);
            storage::Storage::Inst()->AttachSaveModeFilenameByUrl(url, web_url, qualified_file_name);
            return;
        }
        // start download
        ProxyConnection::p download_center_conn = ProxyConnection::create(io_svc_);
        download_center_conn->SetHttpRequestString(request_head);
        download_center_conn->Start();
        download_center_conn->OnNoticeDownloadModeEx(http_request, web_url, qualified_file_name, source_url);
        proxy_connections_.insert(download_center_conn);
#endif  // #ifdef DISK_MODE

    }

    void ProxyModule::StartDownloadFileByRid(const protocol::RidInfo& rid_info, const protocol::UrlInfo& url_info, protocol::TASK_TYPE task_type, bool is_push)
    {
        if (false == is_running_) {
            return;
        }
#ifdef DISK_MODE
        if (true == downloadcenter::DownloadCenterModule::Inst()->IsUrlDownloading(url_info.url_)) {
            return;
        }
        // start download
        ProxyConnection::p download_conn = ProxyConnection::create(io_svc_);
        download_conn->Start();
        download_conn->OnNoticeDownloadFileByRid(rid_info, url_info, task_type, is_push);
        proxy_connections_.insert(download_conn);
#endif  // #ifdef DISK_MODE
    }

    void ProxyModule::StartDownloadFileByRidWithTimeout(const protocol::RidInfo& rid_info, const protocol::UrlInfo& url_info, boost::uint32_t timeout_sec)
    {
        if (false == is_running_) {
            return;
        }
        if (true == downloadcenter::DownloadCenterModule::Inst()->IsUrlDownloading(url_info.url_)) {
            return;
        }
        // start download
        ProxyConnection::p download_conn = ProxyConnection::create(io_svc_);
        protocol::TASK_TYPE task_type = protocol::TASK_TEST_OPENSERVICE;
        download_conn->Start();
        download_conn->OnNoticeDownloadFileByRid(rid_info, url_info, task_type, false);
        proxy_connections_.insert(download_conn);

//         boost::shared_ptr<boost::asio::deadline_timer> timeout_timer(new boost::asio::deadline_timer(MainThread::IOS()));
//         timeout_timer->expires_from_now(boost::posix_time::seconds(timeout_sec));
//         timeout_timer->async_wait(boost::bind(&ProxyModule::StopProxyConnection, ProxyModule::Inst(), url_info.url_));
    }

    void ProxyModule::LimitDownloadSpeedInKBps(const string& url, boost::int32_t speed_limit_KBps)
    {
        if (false == is_running_) {
            return;
        }
        ProxyConnection::p conn = GetProxyConnection(url);
        if (conn && conn->GetDownloadDriver()) {
            LOG(__DEBUG, "downloadcenter", __FUNCTION__ << " LimitSpeedLimit = " << speed_limit_KBps << "(KBps), Url = " << url);
            conn->GetDownloadDriver()->SetSpeedLimitInKBps(speed_limit_KBps);
        }
        else {
            LOG(__DEBUG, "downloadcenter", __FUNCTION__ << " Url Not Found: " << url);
        }
    }

    string ProxyModule::RemovePpvakeyFromUrl(const string& url)
    {
        const static string ppvakey = "ppvakey=";

        string::size_type start_idx = url.rfind(ppvakey);
        if (string::npos != start_idx) {
            if (start_idx > 0 && ('?' == url[start_idx-1] || '&' == url[start_idx-1])) {
                // find end
                string::size_type end_idx = url.find('&', start_idx + ppvakey.length());
                if (end_idx == string::npos) {  // no '&' after ppvakey
                    return url.substr(0, start_idx - 1);
                }
                else {
                    return url.substr(0, start_idx) + url.substr(end_idx + 1);
                }
            }
        }
        return url;
    }

    void ProxyModule::StopProxyDownload(const ProxyType& proxy_type, ProxyConnection__p proxy_connection)
    {
        if (false == is_running_) return;

        LOG(__DEBUG, "proxy", __FUNCTION__ << " proxy_type: " << proxy_type);

        std::set<ProxyConnection::p>::iterator it = proxy_connections_.begin();
        for (; it != proxy_connections_.end(); ++it) {
            if (*it && (proxy_type == (*it)->GetProxyType()) && *it != proxy_connection) {
                (*it)->StopDownloadDriver();
            }
        }
    }

    void ProxyModule::StopProxyConnection(const ProxyType& proxy_type, ProxyConnection__p proxy_connection)
    {
        if (false == is_running_) return;

        LOGX(__DEBUG, "proxy", " proxy_type: " << proxy_type);

        // last segno
        string segno = "";
        PlayInfo::p play_info = proxy_connection->GetPlayInfo();
        if (play_info && play_info->GetPlayType() == PlayInfo::PLAY_BY_OPEN)
        {
            network::Uri uri(play_info->GetUrlInfo().url_);
            segno = uri.getparameter("segno");
            if (segno.length() == 0) {
                segno = "0";
            }
        }
        int last_segno = -1;
        if (segno.length() > 0)
        {
            //    last_segno = boost::lexical_cast<int>(segno) - 1;
            boost::system::error_code ec = framework::string::parse2(segno, last_segno);
            if (!ec)
            {
                last_segno -= 1;
            }
            else
                last_segno = -1;
        }
        string last_segno_str = framework::string::format(last_segno);
        LOGX(__DEBUG, "proxy", "last_segno = " << last_segno_str << ", play_info = " << play_info);

        // foreach connection
        std::set<ProxyConnection::p>::iterator it = proxy_connections_.begin();
        for (; it != proxy_connections_.end(); )
        {
            ProxyConnection::p proxy_conn = *it;
            if (proxy_conn && (proxy_type == proxy_conn->GetProxyType()) && proxy_conn != proxy_connection)
            {
                PlayInfo::p curr_playinfo = proxy_conn->GetPlayInfo();
                // curr segno
                string curr_segno = "";
                if (curr_playinfo)
                {
                    network::Uri curr_uri(curr_playinfo->GetUrlInfo().url_);
                    curr_segno = curr_uri.getparameter("segno");
                }
                LOGX(__DEBUG, "proxy", "curr_segno = " << curr_segno);
                // check segno
                if (play_info && true == play_info->HasStart())
                {
                    LOGX(__DEBUG, "proxy", " StopConnection [HasStart]: " << proxy_conn << ", SourceUrl = " << proxy_conn->GetSourceUrl());
                    proxy_conn->WillStop();
                    it = proxy_connections_.begin();
                }
                else if (play_info && false == play_info->HasStart())
                {
                    if (curr_segno != last_segno_str)
                    {
                        LOGX(__DEBUG, "proxy", " StopConnection [NoStart,SegnoInvalid]: " << proxy_conn << ", SourceUrl = " << proxy_conn->GetSourceUrl());
                        proxy_conn->WillStop();
                        it = proxy_connections_.begin();

                    }
                    else
                    {
                        LOGX(__DEBUG, "proxy", " StopConnection [NoStart,SegnoValid]: " << proxy_conn << ", SourceUrl = " << proxy_conn->GetSourceUrl());
                        ++it;
                    }
                }
                else
                {
                    LOGX(__DEBUG, "proxy", " StopConnection [NoPlayInfo]: " << proxy_conn << ", SourceUrl = " << proxy_conn->GetSourceUrl());
                    proxy_conn->WillStop();
                    it = proxy_connections_.begin();
                }
            }
            else
            {
                ++it;
            }
        }
    }

    void ProxyModule::QueryDownloadProgress(RID rid, boost::function<void(boost::int32_t , boost::int32_t)> result_handler)
    {
        if (false == is_running_) {
            LOGX(__DEBUG, "downloadcenter", "Not Running!");
            result_handler(0, 0);
            return;
        }
        storage::Instance::p inst = boost::dynamic_pointer_cast<storage::Instance>(storage::Storage::Inst()->GetInstanceByRID(rid));
        if (!inst)
        {
            LOGX(__DEBUG, "downloadcenter", "No Such RID: " << rid);
            result_handler(0, 0);
            return;
        }
        uint32_t filelength = inst->GetFileLength();
        uint32_t downloaded = inst->GetDownloadBytes();
        LOGX(__DEBUG, "downloadcenter", "Found RID: " << rid << ", FileLength: " << filelength << ", DownloadedBytes: " << downloaded);
        result_handler(downloaded, filelength);
    }

    void ProxyModule::QueryDownloadProgressByUrl(string url, boost::function<void(boost::int32_t , boost::int32_t)> result_handler)
    {
        if (false == is_running_)
        {
            LOGX(__DEBUG, "downloadcenter", "Not Running!");
            result_handler(0, 0);
            return;
        }

        storage::Instance::p inst = boost::dynamic_pointer_cast<storage::Instance>(storage::Storage::Inst()->GetInstanceByUrl(url));
        if (!inst)
        {
            inst = boost::dynamic_pointer_cast<storage::Instance>(
                storage::Storage::Inst()->GetInstanceByFileName(ParseOpenServiceFileName(network::Uri(url))));
            if (!inst)
            {
                LOGX(__DEBUG, "downloadcenter", "No Such url: " << url);
                result_handler(0, 0);
                return;
            }  
        }

        uint32_t filelength = inst->GetFileLength();
        uint32_t downloaded = inst->GetDownloadBytes();
        LOGX(__DEBUG, "downloadcenter", "Found url: " << url << ", FileLength: " << filelength << ", DownloadedBytes: " << downloaded);
        result_handler(downloaded, filelength);
    }

    void ProxyModule::QueryDownloadSpeed(RID rid, boost::function<void(boost::int32_t)> result_handler)
    {
        if (false == is_running_) {
            LOGX(__DEBUG, "downloadcenter", "Not Running!");
            result_handler(0);
            return;
        }
        speed_query_counter_.start();
        std::set<ProxyConnection::p>::iterator it = proxy_connections_.begin();
        for (; it != proxy_connections_.end(); ++it)
        {
            ProxyConnection::p proxy_conn = *it;
            if (!proxy_conn) {
                LOGX(__DEBUG, "downloadcenter", "ProxyConnection NULL!!");
                continue;
            }
            DownloadDriver::p dd = proxy_conn->GetDownloadDriver();
            if (dd && dd->GetInstance() && dd->GetStatistic())
            {
                if (dd->GetInstance()->GetRID() == rid)
                {
                    int now_speed = 0;
                    if (dd->GetStatistic()) {
                        now_speed += dd->GetStatistic()->GetSpeedInfo().NowDownloadSpeed;
                    }
                    if (dd->GetP2PDownloader() && dd->GetP2PDownloader()->GetStatistic()) {
                        now_speed += dd->GetP2PDownloader()->GetStatistic()->GetSpeedInfo().NowDownloadSpeed;
                    }
                    LOGX(__DEBUG, "downloadcenter", "Found RID: " << rid << ", DownloadSpeed: " << now_speed);
                    result_handler(now_speed);
                    return;
                }
            }
            else
            {
                LOGX(__DEBUG, "downloadcenter", "DownloadDriver NULL!!");
            }
        }
        LOGX(__DEBUG, "downloadcenter", "Not Downloading, RID: " << rid);
        result_handler(0);
    }

    void ProxyModule::QueryDownloadSpeedByUrl(string url, boost::function<void(boost::int32_t)> result_handler)
    {
        if (false == is_running_) 
        {
            LOGX(__DEBUG, "downloadcenter", "Not Running!");
            result_handler(0);
            return;
        }

        speed_query_counter_.start();
        std::set<ProxyConnection::p>::iterator it = proxy_connections_.begin();
        for (; it != proxy_connections_.end(); ++it)
        {
            ProxyConnection::p proxy_conn = *it;
            if (!proxy_conn) 
            {
                LOGX(__DEBUG, "downloadcenter", "ProxyConnection NULL!!");
                continue;
            }

            DownloadDriver::p dd = proxy_conn->GetDownloadDriver();
            if (proxy_conn->GetSourceUrl() == url || 
                (dd && dd->IsOpenService() && 
                 ParseOpenServiceFileName(network::Uri(url)) == dd->GetOpenServiceFileName()))
            {
                int now_speed = 0;
                if (dd->GetStatistic())
                {
                    now_speed += dd->GetStatistic()->GetSpeedInfo().NowDownloadSpeed;
                }
                if (dd->GetP2PDownloader() && dd->GetP2PDownloader()->GetStatistic())
                {
                    now_speed += dd->GetP2PDownloader()->GetStatistic()->GetSpeedInfo().NowDownloadSpeed;
                }
                LOGX(__DEBUG, "downloadcenter", "Found url: " << url << ", DownloadSpeed: " << now_speed);
                result_handler(now_speed);
                return;
            }
        }
        LOGX(__DEBUG, "downloadcenter", "Not Downloading, URL: " << url);
        result_handler(0);
    }

    void ProxyModule::SetRestPlayTime(RID rid, boost::uint32_t rest_play_time)
    {
        if (!is_running_)
        {
            return;
        }

        for (std::set<ProxyConnection__p>::iterator iter = proxy_connections_.begin();
            iter != proxy_connections_.end(); ++iter)
        {
            ProxyConnection::p proxy_conn = *iter;
            if (!proxy_conn) 
            {
                LOGX(__DEBUG, "downloadcenter", "ProxyConnection NULL!!");
                continue;
            }

            DownloadDriver::p dd = proxy_conn->GetDownloadDriver();
            if (dd && dd->GetInstance() && dd->GetStatistic() && dd->GetStatistic()->GetResourceID() == rid)
            {
                dd->SetRestPlayTime(rest_play_time);
            }
        }
    }

    void ProxyModule::SetRestPlayTimeByUrl(string url, boost::uint32_t rest_play_time)
    {
        if (!is_running_)
        {
            return;
        }

        string filename = ParseOpenServiceFileName(network::Uri(url));

        for (std::set<ProxyConnection__p>::iterator iter = proxy_connections_.begin();
            iter != proxy_connections_.end(); ++iter)
        {
            ProxyConnection::p proxy_conn = *iter;
            if (!proxy_conn) 
            {
                LOGX(__DEBUG, "downloadcenter", "ProxyConnection NULL!!");
                continue;
            }

            DownloadDriver::p dd = proxy_conn->GetDownloadDriver();
            if (dd && dd->GetOpenServiceFileName() == filename)
            {
                dd->SetRestPlayTime(rest_play_time);
            }
        }
    }

    void ProxyModule::SetDownloadMode(RID rid, boost::uint32_t download_mode)
    {
        if (!is_running_)
        {
            return;
        }

        for (std::set<ProxyConnection__p>::iterator iter = proxy_connections_.begin();
            iter != proxy_connections_.end(); ++iter)
        {
            ProxyConnection::p proxy_conn = *iter;
            if (!proxy_conn) {
                LOGX(__DEBUG, "downloadcenter", "ProxyConnection NULL!!");
                continue;
            }

            DownloadDriver::p dd = proxy_conn->GetDownloadDriver();
            if (dd && dd->GetInstance() && dd->GetStatistic() && dd->GetStatistic()->GetResourceID() == rid)
            {
                dd->SetDownloadMode(download_mode);
            }
        }
    }

    void ProxyModule::SetDownloadModeByUrl(string url, boost::uint32_t download_mode)
    {
        if (!is_running_)
        {
            return;
        }

        string filename = ParseOpenServiceFileName(network::Uri(url));
        for (std::set<ProxyConnection__p>::iterator iter = proxy_connections_.begin();
            iter != proxy_connections_.end(); ++iter)
        {
            ProxyConnection::p proxy_conn = *iter;
            if (!proxy_conn) 
            {
                LOGX(__DEBUG, "downloadcenter", "ProxyConnection NULL!!");
                continue;
            }

            DownloadDriver::p dd = proxy_conn->GetDownloadDriver();
            if (dd && dd->GetOpenServiceFileName() == filename)
            {
                dd->SetDownloadMode(download_mode);
            }
        }
    }

    void ProxyModule::QueryPeerStateMachine(RID rid, boost::function<void(boost::int32_t, boost::int32_t, boost::int32_t)> result_handler)
    {
        if (false == is_running_) 
        {
            LOGX(__DEBUG, "downloadcenter", "Not Running!");
            result_handler(-1, -1, -1);
            return;
        }

        bool is_downlonding_byurl = false;

        for (std::set<ProxyConnection::p>::iterator iter = proxy_connections_.begin();
            iter != proxy_connections_.end();
            iter ++)
        {
            if ((*iter) && (*iter)->GetPlayInfo() && (*iter)->GetPlayInfo()->GetPlayType() == PlayInfo::DOWNLOAD_BY_URL)
                is_downlonding_byurl = true;
        }

        speed_query_counter_.reset();
        std::set<ProxyConnection::p>::iterator it = proxy_connections_.begin();
        for (; it != proxy_connections_.end(); ++it)
        {
            ProxyConnection::p proxy_conn = *it;
            if (!proxy_conn) 
            {
                LOGX(__DEBUG, "downloadcenter", "ProxyConnection NULL!!");
                continue;
            }

            DownloadDriver::p dd = proxy_conn->GetDownloadDriver();
            if (dd && dd->GetInstance() && dd->GetStatistic())
            {
                if (dd->GetInstance()->GetRID() == rid)
                {
                    boost::int32_t http_speed = 0;
                    boost::int32_t p2p_speed = 0;
                    boost::int32_t state_machine = 0;

                    if (is_downlonding_byurl)
                        state_machine = 100;            // 正在下载节目单或广告素材
                    else if (proxy_connections_.size() != 1)
                        state_machine = 200;            // 同时在下载其他节目

                    IHTTPControlTarget::p http = dd->GetHTTPControlTarget();
                    IP2PControlTarget::p p2p = dd->GetP2PControlTarget();
                    boost::int32_t http_state = !http ? 0 : (!http->IsPausing() ? 2 : 3);
                    boost::int32_t p2p_state = !p2p ? 0 : (!p2p->IsPausing() ? 2 : 3);
                    state_machine += http_state * 10 + p2p_state;

                    if (dd->GetHTTPControlTarget())
                    {
                        http_speed += dd->GetHTTPControlTarget()->GetSecondDownloadSpeed();
                    }

                    if (dd->GetP2PControlTarget())
                    {
                        p2p_speed += dd->GetP2PControlTarget()->GetSecondDownloadSpeed();
                    }

                    LOGX(__DEBUG, "proxy", "Found RID: " << rid << ", HTTP_Speed: " << http_speed << " , P2P_speed = " << p2p_speed);
                    result_handler(state_machine, http_speed, p2p_speed);
                    return;
                }
            }
        }

        LOGX(__DEBUG, "downloadcenter", "Not Downloading, RID: " << rid);
        result_handler(-1, -1, -1);
    }

    void ProxyModule::QueryPeerStateMachineByUrl(const char * url, boost::function<void(boost::int32_t, boost::int32_t, boost::int32_t)> result_handler)
    {
        if (false == is_running_)
        {
            LOGX(__DEBUG, "downloadcenter", "Not Running!");
            result_handler(-1, -1, -1);
            return;
        }

        bool is_downlonding_byurl = false;

        for (std::set<ProxyConnection::p>::iterator iter = proxy_connections_.begin();
            iter != proxy_connections_.end();
            iter ++)
        {
            if ((*iter) && (*iter)->GetPlayInfo() && (*iter)->GetPlayInfo()->GetPlayType() == PlayInfo::DOWNLOAD_BY_URL)
                is_downlonding_byurl = true;
        }

        speed_query_counter_.reset();

        network::Uri uri(url);
        string filename = ParseOpenServiceFileName(uri);

        std::set<ProxyConnection::p>::iterator it = proxy_connections_.begin();
        for (; it != proxy_connections_.end(); ++it)
        {
            ProxyConnection::p proxy_conn = *it;
            if (!proxy_conn) 
            {
                LOGX(__DEBUG, "downloadcenter", "ProxyConnection NULL!!");
                continue;
            }

            DownloadDriver::p dd = proxy_conn->GetDownloadDriver();
            if (dd && dd->GetInstance() && dd->GetStatistic())
            {
                if (dd->GetOpenServiceFileName() == filename)
                {
                    boost::int32_t http_speed = 0;
                    boost::int32_t p2p_speed = 0;
                    boost::int32_t state_machine = 0;

                    if (is_downlonding_byurl)
                        state_machine = 100;            // 正在下载节目单或广告素材
                    else if (proxy_connections_.size() != 1)
                        state_machine = 200;            // 同时在下载其他节目

                    IHTTPControlTarget::p http = dd->GetHTTPControlTarget();
                    IP2PControlTarget::p p2p = dd->GetP2PControlTarget();
                    boost::int32_t http_state = !http ? 0 : (!http->IsPausing() ? 2 : 3);
                    boost::int32_t p2p_state = !p2p ? 0 : (!p2p->IsPausing() ? 2 : 3);
                    state_machine += http_state * 10 + p2p_state;

                    if (dd->GetHTTPControlTarget())
                    {
                        http_speed += dd->GetHTTPControlTarget()->GetSecondDownloadSpeed();
                    }

                    if (dd->GetP2PControlTarget())
                    {
                        p2p_speed += dd->GetP2PControlTarget()->GetSecondDownloadSpeed();
                    }

                    //LOGX(__DEBUG, "proxy", "Found RID: " << rid << ", HTTP_Speed: " << http_speed << " , P2P_speed = " << p2p_speed);
                    result_handler(state_machine, http_speed, p2p_speed);
                    return;
                }
            }
        }

        //LOGX(__DEBUG, "downloadcenter", "Not Downloading, RID: " << rid);
        result_handler(-1, -1, -1);
    }

    void ProxyModule::QueryDragState(RID rid, boost::int32_t *state, boost::function<void ()> fun)
    {
        if (false == is_running_) 
        {
            LOGX(__DEBUG, "proxy", "Not Running!");
            *state = 0;
            return;
        }

        *state = 0;
        std::set<ProxyConnection::p>::iterator it = proxy_connections_.begin();
        for (; it != proxy_connections_.end(); ++it)
        {
            ProxyConnection::p proxy_conn = *it;
            if (!proxy_conn) 
            {
                LOGX(__DEBUG, "proxy", "ProxyConnection NULL!!");
                continue;
            }

            DownloadDriver::p dd = proxy_conn->GetDownloadDriver();
            if (dd && dd->GetInstance())
            {
                if (dd->GetInstance()->GetRID() == rid)
                {
                    dd->GetDragMachineState(*state);
                    break;
                }
            }
        }

        fun();
    }

    void ProxyModule::QueryDragStateByUrl(const char * url, boost::int32_t *state, boost::function<void ()> fun)
    {
        if (false == is_running_) 
        {
            LOGX(__DEBUG, "proxy", "Not Running!");
            *state = 0;
            return;
        }

        *state = 0;
        string filename = ParseOpenServiceFileName(network::Uri(url));        
        std::set<ProxyConnection::p>::iterator it = proxy_connections_.begin();
        for (; it != proxy_connections_.end(); ++it)
        {
            ProxyConnection::p proxy_conn = *it;
            if (!proxy_conn) 
            {
                LOGX(__DEBUG, "proxy", "ProxyConnection NULL!!");
                continue;
            }

            DownloadDriver::p dd = proxy_conn->GetDownloadDriver();
            if (dd && dd->GetOpenServiceFileName() == filename)
            {
                dd->GetDragMachineState(*state);
                break;
            }
        }

        fun();
    }

    bool ProxyModule::IsHttpDownloading()
    {
        // LOGX(__DEBUG, "proxy", "IsHttpDownloading: proxy_connections_.size() = " << proxy_connections_.size());

        std::set<ProxyConnection::p>::iterator it = proxy_connections_.begin();
        for (; it != proxy_connections_.end(); ++it)
        {
            ProxyConnection::p proxy_conn = *it;
            if (!proxy_conn /*|| !proxy_conn->IsRunning()*/) {
                // LOGX(__DEBUG, "downloadcenter", "ProxyConnection NULL or Not Running!!");
                continue;
            }
            // LOGX(__DEBUG, "proxy", "IsHttpDownloading: proxy_connection " << proxy_conn);

            DownloadDriver::p dd = proxy_conn->GetDownloadDriver();
            if (!dd || !dd->IsRunning()) {
                // LOGX(__DEBUG, "downloadcenter", "DownloadDriver NULL or Not Running or HTTPControlTarget NULL!!");
                continue;
            }
            // LOGX(__DEBUG, "proxy", "IsHttpDownloading: DownloadDriver " << dd);

            IHTTPControlTarget::p http = dd->GetHTTPControlTarget();
            if (http && !http->IsPausing())
                return true;
        }
        return false;
    }

    bool ProxyModule::IsP2PDownloading()
    {
        // LOGX(__DEBUG, "proxy", "IsP2PDownloading: proxy_connections_.size() = " << proxy_connections_.size());

        std::set<ProxyConnection::p>::iterator it = proxy_connections_.begin();
        for (; it != proxy_connections_.end(); ++it)
        {
            ProxyConnection::p proxy_conn = *it;
            if (!proxy_conn /*|| !proxy_conn->IsRunning()*/) {
                // LOGX(__DEBUG, "downloadcenter", "ProxyConnection NULL or Not Running!!");
                continue;
            }
            // LOGX(__DEBUG, "proxy", "IsP2PDownloading: proxy_connection " << proxy_conn);

            DownloadDriver::p dd = proxy_conn->GetDownloadDriver();
            if (!dd || !dd->IsRunning()) {
                // LOGX(__DEBUG, "downloadcenter", "DownloadDriver NULL or Not Running or HTTPControlTarget NULL!!");
                continue;
            }
            // LOGX(__DEBUG, "proxy", "IsP2PDownloading: DownloadDriver " << dd);

            IP2PControlTarget::p p2p = dd->GetP2PControlTarget();
            if (p2p && !p2p->IsPausing())
                return true;
        }
        return false;
    }
    bool ProxyModule::IsDownloadingMovie()
    {
        if (false == is_running_)
    {
            return false;
        }
        //
        std::set<ProxyConnection::p>::iterator it = proxy_connections_.begin();
        for (; it != proxy_connections_.end(); ++it)
        {
            ProxyConnection::p proxy_conn = *it;
            if (!proxy_conn) {
                LOGX(__DEBUG, "downloadcenter", "ProxyConnection NULL!!");
                continue;
            }
            if (proxy_conn->IsMovieUrl()) return true;
        }
        return false;
    }

    bool ProxyModule::IsDownloadWithSlowMode()
    {
        if (!is_running_)
        {
            return false;
        }

        for (std::set<ProxyConnection::p>::iterator it = proxy_connections_.begin();
            it != proxy_connections_.end(); ++it)
        {
            ProxyConnection::p proxy_conn = *it;
            if (!proxy_conn)
            {
                LOGX(__DEBUG, "downloadcenter", "ProxyConnection NULL!!");
                continue;
            }

            if (proxy_conn->GetDownloadDriver() && proxy_conn->GetDownloadDriver()->GetDownloadMode() == IGlobalControlTarget::SLOW_MODE)
            {
                return true;
            }
        }

        return false;
    }

    bool ProxyModule::IsWatchingMovie()
    {
        if (false == is_running_)
        {
            return false;
        }
        // client is watching
        if (speed_query_counter_.running() && speed_query_counter_.elapsed() <= 10 * 1000)
        {
            LOGX(__DEBUG, "downloadcenter", "speed_query_counter_ != 0 && speed_query_counter_.GetElapsed() <= 10 * 1000, "
                << "value = " << speed_query_counter_.elapsed());
            return true;
        }

        return IsDownloadingMovie();
    }

    int ProxyModule::GetLastSegno(string sessionid)
    {
        std::map< string, std::pair<int, time_t> >::iterator iter;
        for (iter = drag_record_.begin(); iter != drag_record_.end(); ++iter)
        {
            if (iter->first == sessionid)
            {
                return iter->second.first;
            }
        }
        return -1;
    }

    void ProxyModule::SetSegno(string sessionid, int segno)
    {
        time_t now = time(NULL);

        drag_record_.insert(std::make_pair(sessionid, std::make_pair(segno, now)));
    }

    void ProxyModule::ExpireSegno()
    {
        LOG(__DEBUG, "switch", "ExpireSegno");
        time_t now = time(NULL);
        std::map< string, std::pair<int, time_t> >::iterator iter;
        for (iter = drag_record_.begin(); iter != drag_record_.end();)
        {
            int elapsed_time = now - iter->second.second;
            LOG(__DEBUG, "switch", "elapsed time = " << elapsed_time);
            if (elapsed_time > 600)
            {
                drag_record_.erase(iter++);
                LOG(__DEBUG, "switch", "ExpireSegno bingo");
            }
            else
            {
                ++iter;
            }
        }
    }

    uint32_t ProxyModule::GetDragPrecent()
    {
        return last_drag_precent;
    }

    void ProxyModule::SetLastDragPrecent(uint32_t drag_precent)
    {
        last_drag_precent = drag_precent;
    }

    void ProxyModule::LoadHistoricalMaxDownloadSpeed()
    {
        if (false == is_running_)
            return;

        if (ppva_config_path_.length() > 0)
        {
            boost::filesystem::path configpath(ppva_config_path_);
            configpath /= "ppvaconfig.ini";
            string filename = configpath.file_string();

            try
            {
                framework::configure::Config conf(filename);
                framework::configure::ConfigModule & ppva_dm_conf =
                    conf.register_module("PPVA_DM");

                ppva_dm_conf(CONFIG_PARAM_NAME_RDONLY("I", local_ip_from_ini_));
                // IP check
                boost::uint32_t ip_local = CStunClient::GetLocalFirstIP();
                if (local_ip_from_ini_ != ip_local)
                    return;

                // download velocity
                boost::uint32_t v;
                ppva_dm_conf(CONFIG_PARAM_NAME_RDONLY("V", v));

                if (v != 0)
                {
                    history_max_download_speed_ini_ = v;
                }
            }
            catch(...)
            {
                history_max_download_speed_ini_ = 65536;
                base::filesystem::remove_nothrow(filename);
            }
        }
    }

    void ProxyModule::SaveHistoricalMaxDownloadSpeed()
    {
        if (false == is_running_)
            return;

        if (ppva_config_path_.length() > 0)
        {
                boost::filesystem::path configpath(ppva_config_path_);
                configpath /= "ppvaconfig.ini";

                string filename = configpath.file_string();
            try
            {
                framework::configure::Config conf(filename);
                framework::configure::ConfigModule & ppva_dm_conf =
                    conf.register_module("PPVA_DM");

                ppva_dm_conf(CONFIG_PARAM_NAME_RDONLY("I", local_ip_from_ini_));
                ppva_dm_conf(CONFIG_PARAM_NAME_RDONLY("V", history_max_download_speed_ini_));

                // store
                conf.sync();
            }
            catch(...)
            {
                base::filesystem::remove_nothrow(filename);
            }
        }
    }

    // 开始限制下载电影的请求速度
    void ProxyModule::StartLimitDownloadConnection()
    {
        // 原来采用类似引用计数的机制，客户端暂时保证是一个start对应一个stop
        // 现在出于质量的考虑，改成true false形式
        is_limit_download_connection_ = true;

        // 禁用上传
        P2PModule::Inst()->SetUploadSwitch(true);

        GlobalSpeedLimit();
    }

    // 停止限制下载电影的请求速度
    void ProxyModule::StopLimitDownloadConnection()
    {
        // 原来采用类似引用计数的机制，客户端暂时保证是一个start对应一个stop
        // 现在出于质量的考虑，改成true false形式
        is_limit_download_connection_ = false;

        // 启用上传
        P2PModule::Inst()->SetUploadSwitch(false);

        GlobalSpeedLimit();
    }

    // 设置内核推送数据的速度
    void ProxyModule::SetSendSpeedLimitByUrl(string url, boost::int32_t send_speed_limit)
    {
        for (std::set<ProxyConnection::p>::iterator iter = proxy_connections_.begin();
            iter != proxy_connections_.end(); ++iter)
        {
            ProxyConnection::p proxy_conn = *iter;
            if (!proxy_conn)
            {
                LOGX(__DEBUG, "downloadcenter", "ProxyConnection NULL");
                continue;
            }

            if (proxy_conn->GetSourceUrl() == url)
            {
                proxy_conn->SetSendSpeedLimit(send_speed_limit);
            }
        }
    }

    // 全局限速管理
    void ProxyModule::GlobalSpeedLimit()
    {
        bool is_watch_live = (AppModule::Inst()->GetPeerState() & 0x0000ffff) == PEERSTATE_LIVE_WORKING;
        std::set<ProxyConnection::p> download_connections;
        std::set<ProxyConnection::p> play_vod_connections;
        ProxyConnection::p vod_connection;
        boost::int32_t bandwidth = statistic::StatisticModule::Inst()->GetBandWidth();
        boost::int32_t download_speed_limit_in_KBps = 0;

        for (std::set<ProxyConnection::p>::iterator i = proxy_connections_.begin();
            i != proxy_connections_.end(); ++i)
        {
            // 观看点播
            if ((*i) &&
                (*i)->GetPlayInfo() &&
                (*i)->GetPlayInfo()->GetPlayType() == PlayInfo::PLAY_BY_OPEN &&
                (*i)->GetDownloadDriver() &&
                (*i)->GetDownloadDriver()->GetSourceType() != PlayInfo::SOURCE_DOWNLOAD_MOVIE)
            {
                play_vod_connections.insert(*i);
            }
            else
            {
                // 下载广告素材或者电影
                if ((*i)->GetDownloadDriver())
                {
                    (*i)->GetDownloadDriver()->DisableSmartSpeedLimit();
                    download_connections.insert(*i);
                }
            }
        }

        if (is_limit_download_connection_)
        {
            // 网页预留带宽模式
            if (!play_vod_connections.empty())
            {
                // 正在看点播
                boost::int32_t total_data_rate_in_bps = 0;
                for (std::set<ProxyConnection::p>::iterator iter = play_vod_connections.begin();
                    iter != play_vod_connections.end(); ++iter)
                {
                    total_data_rate_in_bps += (*iter)->GetDownloadDriver()->GetDataRate();
                }

                if (bandwidth - 25*1024 > total_data_rate_in_bps)
                {
                    if (play_vod_connections.size() == 1)
                    {
                        // 只有一个播放, 按照min(带宽-25KB，码流*1.4，码流 + 40)限速
                        std::set<ProxyConnection::p>::iterator iter = play_vod_connections.begin();
                        (*iter)->GetDownloadDriver()->DisableSmartSpeedLimit();

                        boost::int32_t speed_limit = std::min(bandwidth / 1024 - 25,
                            std::min(
                            (boost::int32_t)((*iter)->GetDownloadDriver()->GetDataRate() / 1024 * 1.4),
                            (boost::int32_t)((*iter)->GetDownloadDriver()->GetDataRate() / 1024 + 40)));

                        (*iter)->GetDownloadDriver()->SetSpeedLimitInKBps(speed_limit);

                        // 写共享内存
                        if ((*iter)->GetDownloadDriver()->GetStatistic())
                        {
                            (*iter)->GetDownloadDriver()->GetStatistic()->SetSmartPara(
                                (*iter)->GetDownloadDriver()->GetRestPlayableTime(), bandwidth,
                                speed_limit);
                        }
                    }
                    else
                    {
                        // 一个以上的播放，按照每个播放的码流限速
                        for (std::set<ProxyConnection::p>::iterator iter = play_vod_connections.begin();
                            iter != play_vod_connections.end(); ++iter)
                        {
                            // 禁止智能限速，防止速度被智能限速刷掉
                            (*iter)->GetDownloadDriver()->DisableSmartSpeedLimit();
                            (*iter)->GetDownloadDriver()->SetSpeedLimitInKBps(
                                (*iter)->GetDownloadDriver()->GetDataRate() / 1024);

                            // 写共享内存，在Peermonitor上显示下载的限速
                            if ((*iter)->GetDownloadDriver()->GetStatistic())
                            {
                                (*iter)->GetDownloadDriver()->GetStatistic()->SetSmartPara(
                                    (*iter)->GetDownloadDriver()->GetRestPlayableTime(), bandwidth,
                                    (*iter)->GetDownloadDriver()->GetDataRate() / 1024);
                            }
                        }
                    }
                }
                else
                {
                    boost::int32_t speed_limit_in_Kbps = (bandwidth - 25*1024) / 1024 / play_vod_connections.size();
                    for (std::set<ProxyConnection::p>::iterator iter = play_vod_connections.begin();
                        iter != play_vod_connections.end(); ++iter)
                    {
                        // 禁止智能限速，防止速度被智能限速刷掉
                        (*iter)->GetDownloadDriver()->DisableSmartSpeedLimit();
                        (*iter)->GetDownloadDriver()->SetSpeedLimitInKBps(speed_limit_in_Kbps);

                        // 写共享内存，在Peermonitor上显示下载的限速
                        if ((*iter)->GetDownloadDriver()->GetStatistic())
                        {
                            (*iter)->GetDownloadDriver()->GetStatistic()->SetSmartPara(
                                (*iter)->GetDownloadDriver()->GetRestPlayableTime(), bandwidth,
                                speed_limit_in_Kbps);

                        }
                    }
                }

                download_speed_limit_in_KBps = 1;
            }
            else if (is_watch_live)
            {
                // 正在看直播
                download_speed_limit_in_KBps = 1;
            }
            else
            {
                // 不在观看，75%带宽平分给下载
                if (!download_connections.empty())
                {
                    download_speed_limit_in_KBps = bandwidth * 3/4 / 1024 / download_connections.size();
                }
            }
        }
        else
        {
            // 无需为网页预留带宽

            // 点播视频不限速
            for (std::set<ProxyConnection::p>::iterator i = play_vod_connections.begin();
                i != play_vod_connections.end(); ++i)
            {
                // 启用智能限速，恢复原来的现场(高速模式也会恢复)
                (*i)->GetDownloadDriver()->EnableSmartSpeedLimit();
            }

            // 下载请求集合不为空，需要对下载请求限速
            if (!download_connections.empty())
            {
                if (!play_vod_connections.empty())
                {
                    // 点播请求集合不为空，表示正在看点播
                    boost::int32_t total_limit_in_KBps = 0;
                    bool is_exist_fast_mode = false;

                    for (std::set<ProxyConnection__p>::iterator iter = play_vod_connections.begin();
                        iter != play_vod_connections.end(); ++iter)
                    {
                        if ((*iter)->GetDownloadDriver()->GetDownloadMode() == IGlobalControlTarget::FAST_MODE ||
                            (*iter)->GetDownloadDriver()->GetDownloadMode() == IGlobalControlTarget::SMART_MODE &&
                            (*iter)->GetDownloadDriver()->GetRestPlayableTime() < 30*1000)
                        {
                            // 存在高速模式
                            is_exist_fast_mode = true;
                            break;
                        }
                        else
                        {
                            total_limit_in_KBps += ((*iter)->GetDownloadDriver()->GetDataRate() / 1024 +
                                (*iter)->GetDownloadDriver()->GetSpeedLimitInKBps()) / 2;
                        }
                    }

                    if (is_exist_fast_mode)
                    {
                        // 高速模式
                        download_speed_limit_in_KBps = 1;
                    }
                    else
                    {
                        // 智能限速模式
                        if (bandwidth / 1024 > total_limit_in_KBps)
                        {
                            download_speed_limit_in_KBps =
                                (bandwidth / 1024 - total_limit_in_KBps) / download_connections.size();
                        }
                        else
                        {
                            LIMIT_MIN(download_speed_limit_in_KBps, 1);
                        }
                    }
                }
                else if (is_watch_live)
                {
                    // 正在看直播
                    if (bandwidth / 1024 > 70)
                    {
                        download_speed_limit_in_KBps = (bandwidth / 1024 - 70) / download_connections.size();
                    }
                    else
                    {
                        LIMIT_MIN(download_speed_limit_in_KBps, 1);
                    }
                }
                else
                {
                    // 不在观看，75%带宽平分给下载
                    download_speed_limit_in_KBps = bandwidth * 3/4 / 1024 / download_connections.size();
                }
            }
        }

        for (std::set<ProxyConnection::p>::iterator i = download_connections.begin();
            i != download_connections.end(); ++i)
        {
#ifdef DISK_MODE
            if ((*i)->GetDownloadDriver()->IsPush())
            {
                // herain:push任务的限速最终由pushmodule决定
                PushModule::Inst()->SetGlobalSpeedLimitInKBps(download_speed_limit_in_KBps);
            }
            else
#endif
            {
                (*i)->GetDownloadDriver()->SetSpeedLimitInKBps(download_speed_limit_in_KBps);
                if ((*i)->GetDownloadDriver()->GetStatistic())
                {
                    (*i)->GetDownloadDriver()->GetStatistic()->SetSmartPara(0, bandwidth, download_speed_limit_in_KBps);
                }
            }            
        }
    }

#ifdef DISK_MODE
    // 查询某个rid的校验失败的次数
    void ProxyModule::GetBlockHashFailed(const RID & rid, 
        boost::int32_t * failed_num, boost::function<void ()> fun)
    {
        storage::Instance::p inst = 
            boost::dynamic_pointer_cast<storage::Instance>(storage::Storage::Inst()->GetInstanceByRID(rid));

        if (!inst)
        {
            LOGX(__DEBUG, "proxy", "GetBlockHashFailed No Such RID: " << rid);
            *failed_num =  0;
        }
        else
        {
            *failed_num = inst->GetBlockHashFailed();
        }
        
        fun();
    }

    // 查询某个url的校验失败的次数
    void ProxyModule::GetBlockHashFailedByUrl(const char * url, 
        boost::int32_t * failed_num, boost::function<void ()> fun)
    {
        storage::Instance::p inst = 
            boost::dynamic_pointer_cast<storage::Instance>(storage::Storage::Inst()->GetInstanceByUrl(url));

        if (!inst)
        {
            string filename = ParseOpenServiceFileName(network::Uri(url));
            inst = boost::dynamic_pointer_cast<storage::Instance>(
                storage::Storage::Inst()->GetInstanceByFileName(filename));
        }

        if (!inst)
        {
            //LOGX(__DEBUG, "proxy", "GetBlockHashFailed No Such RID: " << rid);
            *failed_num =  0;
        }
        else
        {
            *failed_num = inst->GetBlockHashFailed();
        }

        fun();
    }
#endif

    string ProxyModule::ParseOpenServiceFileName(const network::Uri & uri)
    {
        string filename = uri.getfile();
#ifdef BOOST_WINDOWS_API
        // TODO(herain):2011-4-14:why BOOST_WINDOWS_API
        filename = network::UrlCodec::Decode(filename);
#endif
        string segno = base::util::GetSegno(uri);
        LOG(__DEBUG, "proxy", "segno = " << segno);

        string ext = "[" + segno + "]";
        uint32_t pos = filename.rfind('.');
        if (pos != string::npos) 
        {
            filename.insert(pos, ext);
        }
        else 
        {
            filename += ext;
        }

        return filename;
    }

#ifdef PEER_PC_CLIENT
    void ProxyModule::GetCompeletedFilePath(const RID & rid, string & file_path, boost::function<void ()> fun)
    {
        storage::Instance::p inst = boost::dynamic_pointer_cast<storage::Instance>(
            storage::Storage::Inst()->GetInstanceByRID(rid));
        if (inst && inst->IsComplete())
        {
            file_path = inst->GetResourceName();
        }

        fun();
    }

    void ProxyModule::GetCompeletedFilePathByUrl(const char * url, string & file_path, boost::function<void ()> fun)
    {
        storage::Instance::p inst = boost::dynamic_pointer_cast<storage::Instance>(
            storage::Storage::Inst()->GetInstanceByUrl(url));
        if (!inst)
        {
            string filename = ParseOpenServiceFileName(network::Uri(url));
            inst = boost::dynamic_pointer_cast<storage::Instance>(
                storage::Storage::Inst()->GetInstanceByFileName(filename));
        }

        if (inst && inst->IsComplete())
        {
            file_path = inst->GetResourceName();
        }

        fun();
    }
#endif

    bool ProxyModule::IsWatchingLive()
    {
        if (false == is_running_)
        {
            return false;
        }

        for (std::set<ProxyConnection::p>::iterator iter = proxy_connections_.begin();
            iter != proxy_connections_.end(); ++iter)
        {
            ProxyConnection::p proxy_conn = *iter;
            if (!proxy_conn)
            {
                LOGX(__DEBUG, "downloadcenter", "ProxyConnection NULL!!");
                continue;
            }
            if (proxy_conn->IsLiveConnection())
            {
                return true;
            }
        }
        return false;
    }

    void ProxyModule::OnLivePause(const RID & channel_id, bool pause, boost::uint32_t unique_id)
    {
        for (std::set<ProxyConnection::p>::iterator iter = proxy_connections_.begin();
            iter != proxy_connections_.end(); ++iter)
        {
            if ((*iter)->IsLiveConnection() &&
                (*iter)->GetLiveDownloadDriver()->GetChannelId() == channel_id &&
                (*iter)->GetLiveDownloadDriver()->GetUniqueID() == unique_id)
            {
                (*iter)->GetLiveDownloadDriver()->OnPause(pause);
            }
        }
    }
}
