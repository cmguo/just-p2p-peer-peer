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

#include "p2sp/download/LiveDownloadDriver.h"

#include "statistic/StatisticModule.h"

#include "network/UrlCodec.h"
#include "base/util.h"
#include "storage/Storage.h"
#include "storage/Instance.h"
#include "PlayInfo.h"

#include "storage/LiveInstance.h"
#include "protocol/TcpPeerPacket.h"

namespace p2sp
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_proxy = log4cplus::Logger::getInstance("[proxy_module]");
#endif

    ProxyModule::p ProxyModule::inst_;

    ProxyModule::ProxyModule(
        boost::asio::io_service & io_svc)
        : io_svc_(io_svc)
        , proxy_timer_(global_250ms_timer(), 250, boost::bind(&ProxyModule::OnTimerElapsed, this, &proxy_timer_))
        , speed_query_counter_(false)
        , is_running_(false)
        , is_download_speed_limited_(false)
    {
    }

    void ProxyModule::Start(const string& config_path, boost::uint16_t local_http_proxy_port)
    {
        if (is_running_ == true) return;


        is_running_ = true;

        ppva_config_path_ = config_path;

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

                LOG4CPLUS_INFO_LOG(logger_proxy, __FUNCTION__ << ":" << __LINE__ << " Try Endpoint " << 
                    localhost.to_string() << ":" << port);
                acceptor_->Close();
                // try 127.0.0.1
                boost::asio::ip::tcp::endpoint ep(localhost, port);
                if (acceptor_->Listen(ep))
                {
                    LOG4CPLUS_DEBUG_LOG(logger_proxy, __FUNCTION__ << ":" << __LINE__ << 
                        " OK, Try to Listen 0.0.0.0:" << port);
                    // ok, change to listen all
                    acceptor_->Close();
                    if (acceptor_->Listen(port))
                    {
                        LOG4CPLUS_DEBUG_LOG(logger_proxy, __FUNCTION__ << ":" << __LINE__ << 
                            " OK, Listen port: " << port);
                        acceptor_->TcpAccept();
                        // now try to hold "127.0.0.1:port"
                        acceptor_place_holder_ = network::HttpAcceptor::create(io_svc_, shared_from_this());
                        if (acceptor_place_holder_->Listen(ep)) {
                            LOG4CPLUS_DEBUG_LOG(logger_proxy, __FUNCTION__ << ":" << __LINE__ << 
                                " OK, Hold Address: " << ep);
                            acceptor_place_holder_->TcpAccept();
                        }
                        break;
                    }
                    else
                    {
                        // 尝试 0.0.0.0 失败, 仅仅监听 127.0.0.1
                        LOG4CPLUS_INFO_LOG(logger_proxy, "Try 0.0.0.0 Failed");
                        if (acceptor_->Listen(ep))
                        {
                            LOG4CPLUS_INFO_LOG(logger_proxy, "Listen 127.0.0.1 Succeed");
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
                LOG4CPLUS_INFO_LOG(logger_proxy, "Try Failed: " << port);
                return;
            }
        }
        LOG4CPLUS_INFO_LOG(logger_proxy, "Succeed: " << GetHttpPort());

        statistic::StatisticModule::Inst()->SetLocalPeerTcpPort(GetHttpPort());

   //     proxy_timer_ = framework::timer::PeriodicTimer::create(250, shared_from_this());
        proxy_timer_.start();

        tick_counter_.start();
    }

    void ProxyModule::Stop()
    {
        if (is_running_ == false) return;
        LOG4CPLUS_INFO_LOG(logger_proxy, "关闭ProxyModule");

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

        tick_counter_.stop();
    }

    void ProxyModule::OnTimerElapsed(framework::timer::Timer * pointer)
    {
        if (is_running_ == false) return;
        boost::uint32_t times = pointer->times();
        if (pointer == &proxy_timer_)
        {
            OnProxyTimer(times);
        }
        else
        {
            assert(0);
        }
    }

    void ProxyModule::OnProxyTimer(boost::uint32_t times)
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
                    LOG4CPLUS_WARN_LOG(logger_proxy, "second_speed " << (*iter)->GetDownloadDriver() << " : "
                        << (*iter)->GetDownloadDriver()->GetSecondDownloadSpeed() << "B/s");
                }
                (*iter)->OnProxyTimer(times);
            }

            // 每秒更新最大下载速度
            if (curr_download_speed > history_max_download_speed_)
            {
                history_max_download_speed_ = curr_download_speed;
            }
            LOG4CPLUS_WARN_LOG(logger_proxy, "curr_download_speed:" << curr_download_speed << 
                ", history_max_download_speed_" << history_max_download_speed_);

            // 全局限速管理
            GlobalSpeedLimit();
        }

        if (times % (4*5) == 0)  // 5s
        {
            boost::uint32_t local_ip = base::util::GetLocalFirstIP();
            if (local_ip_from_ini_ != local_ip)
            {
                history_max_download_speed_ = 0;
                history_max_download_speed_ini_ = 0;
                local_ip_from_ini_ = local_ip;
                LOG4CPLUS_WARN_LOG(logger_proxy, "local_ip_from_ini_ != local_ip");
                SaveHistoricalMaxDownloadSpeed();
            }
            else if (history_max_download_speed_ > history_max_download_speed_ini_)
            {
                LOG4CPLUS_WARN_LOG(logger_proxy, "history_max_download_speed_ini_ < history_max_download_speed_:" 
                    << history_max_download_speed_ini_ << "<" << history_max_download_speed_);
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
        }

    }

    void ProxyModule::RemoveProxyConnection(ProxyConnection::p server_socket)
    {
        if (false == is_running_)
            return;

        if (proxy_connections_.find(server_socket) == proxy_connections_.end())
        {
            LOG4CPLUS_WARN_LOG(logger_proxy, "RemoveProxyConnection but ServerSocket Not Found");
            return;
        }

        // if (server_socket->GetSourceUrl().length() > 0 &&
        //    server_socket->GetSourceUrl() == PushModule::Inst()->GetCurrentTaskUrl())
        // {
        //    PushModule::Inst()->StopCurrentTask();
        // }

        LOG4CPLUS_INFO_LOG(logger_proxy, "RemoveProxyConnection Succed");
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

        LOG4CPLUS_INFO_LOG(logger_proxy, "OnHttpAccept Succed " << http_server_for_accept->GetEndPoint());

        ProxyConnection::p pointer = ProxyConnection::create(io_svc_, http_server_for_accept);
        pointer->Start();
        proxy_connections_.insert(pointer);
    }

    void ProxyModule::OnHttpAcceptFailed()
    {
        LOG4CPLUS_INFO_LOG(logger_proxy, "OnHttpAccept Failed ");
        LOG4CPLUS_INFO_LOG(logger_proxy, "HttpAccept Failed");
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

    void ProxyModule::StartDownloadFile(const string& url, const string& refer_url, const string& user_agent, const string& qualified_file_name)
    {
        if (false == is_running_) {
            return;
        }

#ifdef DISK_MODE
        // start download
        ProxyConnection::p download_center_conn = ProxyConnection::create(io_svc_);
        download_center_conn->Start();
        download_center_conn->OnNoticeDownloadMode(url, refer_url, user_agent, qualified_file_name);
        proxy_connections_.insert(download_center_conn);
#endif  // #ifdef DISK_MODE

    }

    void ProxyModule::StartDownloadFileByRid(const protocol::RidInfo& rid_info, const protocol::UrlInfo& url_info, protocol::TASK_TYPE task_type, bool is_push)
    {
        if (false == is_running_) {
            return;
        }
#ifdef DISK_MODE
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
            LOG4CPLUS_DEBUG_LOG(logger_proxy, __FUNCTION__ << " LimitSpeedLimit = " << speed_limit_KBps 
                << "(KBps), Url = " << url);
            conn->GetDownloadDriver()->SetSpeedLimitInKBps(speed_limit_KBps);
        }
        else {
            LOG4CPLUS_DEBUG_LOG(logger_proxy, __FUNCTION__ << " Url Not Found: " << url);
        }
    }

    void ProxyModule::StopProxyDownload(const ProxyType& proxy_type, ProxyConnection__p proxy_connection)
    {
        if (false == is_running_) return;

        LOG4CPLUS_DEBUG_LOG(logger_proxy, __FUNCTION__ << " proxy_type: " << proxy_type);

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

        LOG4CPLUS_DEBUG_LOG(logger_proxy, " proxy_type: " << proxy_type);

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
        LOG4CPLUS_DEBUG_LOG(logger_proxy, "last_segno = " << last_segno_str << ", play_info = " << play_info);

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
                LOG4CPLUS_DEBUG_LOG(logger_proxy, "curr_segno = " << curr_segno);
                // check segno
                if (play_info && true == play_info->HasStart())
                {
                    LOG4CPLUS_DEBUG_LOG(logger_proxy, " StopConnection [HasStart]: " << proxy_conn << ", SourceUrl = " 
                        << proxy_conn->GetSourceUrl());
                    proxy_conn->WillStop();
                    it = proxy_connections_.begin();
                }
                else if (play_info && false == play_info->HasStart())
                {
                    if (curr_segno != last_segno_str)
                    {
                        LOG4CPLUS_DEBUG_LOG(logger_proxy, " StopConnection [NoStart,SegnoInvalid]: " << proxy_conn 
                            << ", SourceUrl = " << proxy_conn->GetSourceUrl());
                        proxy_conn->WillStop();
                        it = proxy_connections_.begin();

                    }
                    else
                    {
                        LOG4CPLUS_DEBUG_LOG(logger_proxy, " StopConnection [NoStart,SegnoValid]: " << proxy_conn 
                            << ", SourceUrl = " << proxy_conn->GetSourceUrl());
                        ++it;
                    }
                }
                else
                {
                    LOG4CPLUS_DEBUG_LOG(logger_proxy, " StopConnection [NoPlayInfo]: " << proxy_conn << 
                        ", SourceUrl = " << proxy_conn->GetSourceUrl());
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

    void ProxyModule::QueryDownloadProgress(RID rid, boost::function<void()> result_handler, boost::int32_t *file_length, boost::int32_t *download_bytes)
    {
        if (false == is_running_) {
            LOG4CPLUS_DEBUG_LOG(logger_proxy, "Not Running!");
            if(result_handler)
            {
                 *file_length = 0;
                 *download_bytes = 0;
                 result_handler();
            }
            return;
        }
        storage::Instance::p inst = boost::static_pointer_cast<storage::Instance>(storage::Storage::Inst()->GetInstanceByRID(rid));
        if (!inst)
        {
            LOG4CPLUS_DEBUG_LOG(logger_proxy, "No Such RID: " << rid);
            if(result_handler)
            {
                *file_length = 0;
                *download_bytes = 0;
                result_handler();
            }
            return;
        }

        *file_length = inst->GetFileLength();
        *download_bytes = inst->GetDownloadBytes();
        LOG4CPLUS_DEBUG_LOG(logger_proxy, "Found RID: " << rid << ", FileLength: " << *file_length << 
            ", DownloadedBytes: " << *download_bytes);
        if (result_handler)       //Push任务会传进一个空的function句柄
        {
            result_handler();
        }
    }

    void ProxyModule::QueryDownloadProgressByUrl(string url, boost::int32_t * file_length, boost::int32_t * downloaded_bytes,
        boost::int32_t * position, boost::function<void ()> result_handler)
    {
        if (false == is_running_)
        {
            LOG4CPLUS_DEBUG_LOG(logger_proxy, "Not Running!");
            result_handler();
            return;
        }

        string filename = ParseOpenServiceFileName(network::Uri(url));

        storage::Instance::p inst = boost::static_pointer_cast<storage::Instance>(storage::Storage::Inst()->GetInstanceByUrl(url));
        if (!inst)
        {
            inst = boost::static_pointer_cast<storage::Instance>(
                storage::Storage::Inst()->GetInstanceByFileName(filename));
            if (!inst)
            {
                LOG4CPLUS_DEBUG_LOG(logger_proxy, "No Such url: " << url);
                result_handler();
                return;
            }  
        }

        *file_length = inst->GetFileLength();
        *downloaded_bytes = inst->GetDownloadBytes();

        protocol::PieceInfoEx piece_info;
        if (inst->GetNextPieceForDownload(0, piece_info))
        {
            boost::uint32_t block_size = inst->GetBlockSize();
            *position = piece_info.GetPosition(block_size);
        }
        else
        {
            *position = *file_length;
        }

        result_handler();
    }

    void ProxyModule::QueryDownloadProgress2(string url, boost::uint32_t start_pos, boost::uint32_t * last_pos, 
        boost::function<void ()> result_handler)
    {
        if (!is_running_)
        {
            result_handler();
            return;
        }

        string filename = ParseOpenServiceFileName(network::Uri(url));

        storage::Instance::p inst = boost::static_pointer_cast<storage::Instance>(storage::Storage::Inst()->GetInstanceByUrl(url));

        if (!inst)
        {
            inst = boost::static_pointer_cast<storage::Instance>(
                storage::Storage::Inst()->GetInstanceByFileName(filename));

            if (!inst)
            {
                result_handler();
                return;
            }  
        }

        protocol::PieceInfoEx piece_info;
        if (inst->GetNextPieceForDownload(start_pos, piece_info))
        {
            boost::uint32_t block_size = inst->GetBlockSize();
            *last_pos = piece_info.GetPosition(block_size);
        }
        else
        {
            *last_pos = inst->GetFileLength();
        }

        result_handler();
    }

    void ProxyModule::QueryDownloadSpeed(RID rid, boost::function<void()> result_handler, boost::int32_t *download_speed)
    {
        if (false == is_running_) {
            LOG4CPLUS_DEBUG_LOG(logger_proxy, "Not Running!");
            *download_speed = 0;
            result_handler();
            return;
        }
        speed_query_counter_.start();
        std::set<ProxyConnection::p>::iterator it = proxy_connections_.begin();
        for (; it != proxy_connections_.end(); ++it)
        {
            ProxyConnection::p proxy_conn = *it;
            if (!proxy_conn) {
                LOG4CPLUS_DEBUG_LOG(logger_proxy, "ProxyConnection NULL!!");
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
                    LOG4CPLUS_DEBUG_LOG(logger_proxy, "Found RID: " << rid << ", DownloadSpeed: " << now_speed);
                    *download_speed = now_speed;
                    result_handler();
                    return;
                }
            }
            else
            {
                LOG4CPLUS_DEBUG_LOG(logger_proxy, "DownloadDriver NULL!!");
            }
        }
        LOG4CPLUS_DEBUG_LOG(logger_proxy, "Not Downloading, RID: " << rid);
        *download_speed = 0;
        result_handler();
    }

    void ProxyModule::QueryDownloadSpeedByUrl(string url, boost::function<void()> result_handler, boost::int32_t *download_speed)
    {
        if (false == is_running_) 
        {
            LOG4CPLUS_DEBUG_LOG(logger_proxy, "Not Running!");
            *download_speed = 0;
            result_handler();
            return;
        }

        speed_query_counter_.start();
        std::set<ProxyConnection::p>::iterator it = proxy_connections_.begin();
        for (; it != proxy_connections_.end(); ++it)
        {
            ProxyConnection::p proxy_conn = *it;
            if (!proxy_conn) 
            {
                LOG4CPLUS_DEBUG_LOG(logger_proxy, "ProxyConnection NULL!!");
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
                LOG4CPLUS_DEBUG_LOG(logger_proxy, "Found url: " << url << ", DownloadSpeed: " << now_speed);
                *download_speed = now_speed;
                result_handler();
                return;
            }
        }
        LOG4CPLUS_DEBUG_LOG(logger_proxy, "Not Downloading, URL: " << url);
        *download_speed = 0;
        result_handler();
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
                LOG4CPLUS_DEBUG_LOG(logger_proxy, "ProxyConnection NULL!!");
                continue;
            }

            DownloadDriver::p dd = proxy_conn->GetDownloadDriver();
            if (dd && dd->GetInstance() && dd->GetStatistic() && dd->GetStatistic()->GetResourceID() == rid)
            {
                dd->SetRestPlayTime(rest_play_time);
                proxy_conn->StopEstimateIkanRestPlayTime();
            }
        }
    }

    void ProxyModule::SetRestPlayTimeByUrl(string url, boost::uint32_t rest_play_time_in_millisecond)
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
                LOG4CPLUS_DEBUG_LOG(logger_proxy, "ProxyConnection NULL!!");
                continue;
            }

            DownloadDriver::p dd = proxy_conn->GetDownloadDriver();
            if (dd)
            {
                if (dd->GetOpenServiceFileName() == filename)
                {
                    dd->SetRestPlayTime(rest_play_time_in_millisecond);
                }
            }
            else
            {
                LiveDownloadDriver::p live_download_driver = proxy_conn->GetLiveDownloadDriver();
                if (live_download_driver)
                {
                    PlayInfo::p play_info = PlayInfo::Parse(url);

                    assert(play_info);
                                        
                    if (play_info && live_download_driver->GetUniqueID() == play_info->GetUniqueID())
                    {
                        live_download_driver->SetRestTimeInSecond(rest_play_time_in_millisecond / 1000);
                    }
                }
            }
        }
    }

    void ProxyModule::SetVipLevelByUrl(string url_str, boost::uint32_t vip_level)
    {
        if (!is_running_)
        {
            return;
        }
        string filename = ParseOpenServiceFileName(network::Uri(url_str));

        for (std::set<ProxyConnection__p>::iterator iter = proxy_connections_.begin();
            iter != proxy_connections_.end(); ++iter)
        {
            ProxyConnection::p proxy_conn = *iter;
            if (!proxy_conn) 
            {
                LOG4CPLUS_DEBUG_LOG(logger_proxy, "ProxyConnection NULL!!");
                continue;
            }

            DownloadDriver::p dd = proxy_conn->GetDownloadDriver();
            if (dd)
            {
                if (dd->GetOpenServiceFileName() == filename)
                {
                    dd->SetVipLevel((VIP_LEVEL)vip_level);
                }
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
                LOG4CPLUS_DEBUG_LOG(logger_proxy, "ProxyConnection NULL!!");
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
                LOG4CPLUS_DEBUG_LOG(logger_proxy, "ProxyConnection NULL!!");
                continue;
            }

            DownloadDriver::p dd = proxy_conn->GetDownloadDriver();
            if (dd && dd->GetOpenServiceFileName() == filename)
            {
                dd->SetDownloadMode(download_mode);
            }
        }
    }

    void ProxyModule::QueryPeerStateMachine(RID rid, boost::function<void()> result_handler, PEERSTATEMACHINE *peer_state)
    {
        if (false == is_running_) 
        {
            LOG4CPLUS_DEBUG_LOG(logger_proxy, "Not Running!");
            peer_state->http_speed_ = -1;
            peer_state->p2p_speed_ = -1;
            peer_state->state_machine_ = -1;
            result_handler();
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
                LOG4CPLUS_DEBUG_LOG(logger_proxy, "ProxyConnection NULL!!");
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

                    LOG4CPLUS_DEBUG_LOG(logger_proxy, "Found RID: " << rid << ", HTTP_Speed: " << http_speed << 
                        " , P2P_speed = " << p2p_speed);
                    peer_state->http_speed_ = http_speed;
                    peer_state->p2p_speed_ = p2p_speed;
                    peer_state->state_machine_ = state_machine;
                    result_handler();
                    return;
                }
            }
        }

        LOG4CPLUS_DEBUG_LOG(logger_proxy, "Not Downloading, RID: " << rid);
        peer_state->http_speed_ = -1;
        peer_state->p2p_speed_ = -1;
        peer_state->state_machine_ = -1;
        result_handler();
    }

    void ProxyModule::QueryPeerStateMachineByUrl(const char * url, boost::function<void()> result_handler, PEERSTATEMACHINE *peer_state)
    {
        if (false == is_running_)
        {
            LOG4CPLUS_DEBUG_LOG(logger_proxy, "Not Running!");
            peer_state->http_speed_ = -1;
            peer_state->p2p_speed_ = -1;
            peer_state->state_machine_ = -1;
            result_handler();
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
                LOG4CPLUS_DEBUG_LOG(logger_proxy, "ProxyConnection NULL!!");
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

                    peer_state->http_speed_ = http_speed;
                    peer_state->p2p_speed_ = p2p_speed;
                    peer_state->state_machine_ = state_machine;
                    result_handler();
                    return;
                }
            }
        }

        peer_state->http_speed_ = -1;
        peer_state->p2p_speed_ = -1;
        peer_state->state_machine_ = -1;
        result_handler();
    }

    void ProxyModule::QueryDragState(RID rid, boost::int32_t *state, boost::function<void ()> fun)
    {
        if (false == is_running_) 
        {
            LOG4CPLUS_DEBUG_LOG(logger_proxy, "Not Running!");
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
                LOG4CPLUS_DEBUG_LOG(logger_proxy, "ProxyConnection NULL!!");
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
            LOG4CPLUS_DEBUG_LOG(logger_proxy, "Not Running!");
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
                LOG4CPLUS_DEBUG_LOG(logger_proxy, "ProxyConnection NULL!!");
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
        std::set<ProxyConnection::p>::iterator it = proxy_connections_.begin();
        for (; it != proxy_connections_.end(); ++it)
        {
            ProxyConnection::p proxy_conn = *it;
            if (!proxy_conn /*|| !proxy_conn->IsRunning()*/) {
                continue;
            }

            if (proxy_conn->IsLiveConnection())
            {
                LiveDownloadDriver__p live_download_driver = proxy_conn->GetLiveDownloadDriver();
                if (!live_download_driver)
                {
                    continue;
                }           

                LiveHttpDownloader__p http;
                if (http && !http->IsPausing())
                {
                    return true;
                }
            }
            else
            {
                DownloadDriver::p vod_download_driver = proxy_conn->GetDownloadDriver();
                if (!vod_download_driver || !vod_download_driver->IsRunning()) 
                {
                    continue;
                }

                IHTTPControlTarget::p http = vod_download_driver->GetHTTPControlTarget();
                if (http && !http->IsPausing())
                {
                    return true;
                }
            }
        }
        return false;
    }

    bool ProxyModule::IsP2PDownloading()
    {

        std::set<ProxyConnection::p>::iterator it = proxy_connections_.begin();
        for (; it != proxy_connections_.end(); ++it)
        {
            ProxyConnection::p proxy_conn = *it;
            if (!proxy_conn /*|| !proxy_conn->IsRunning()*/) {
                continue;
            }

            DownloadDriver::p dd = proxy_conn->GetDownloadDriver();
            if (!dd || !dd->IsRunning()) {
                continue;
            }

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
                LOG4CPLUS_DEBUG_LOG(logger_proxy, "ProxyConnection NULL!!");
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
                LOG4CPLUS_DEBUG_LOG(logger_proxy, "ProxyConnection NULL!!");
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
            LOG4CPLUS_DEBUG_LOG(logger_proxy, 
                "speed_query_counter_ != 0 && speed_query_counter_.GetElapsed() <= 10 * 1000, "
                << "value = " << speed_query_counter_.elapsed());
            return true;
        }

        return IsDownloadingMovie();
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
                boost::uint32_t ip_local = base::util::GetLocalFirstIP();
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

    // 设置内核推送数据的速度
    void ProxyModule::SetSendSpeedLimitByUrl(string url, boost::int32_t send_speed_limit)
    {
        for (std::set<ProxyConnection::p>::iterator iter = proxy_connections_.begin();
            iter != proxy_connections_.end(); ++iter)
        {
            ProxyConnection::p proxy_conn = *iter;
            if (!proxy_conn)
            {
                LOG4CPLUS_DEBUG_LOG(logger_proxy, "ProxyConnection NULL");
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
        std::set<ProxyConnection::p> download_connections;
        std::set<ProxyConnection::p> play_vod_connections;
        ProxyConnection::p vod_connection;
        boost::int32_t bandwidth = statistic::StatisticModule::Inst()->GetBandWidth();
        boost::int32_t download_speed_limit_in_KBps = 0;
        boost::int32_t material_download_speed_limit_inKBps = 0;                //素材下载限速
        boost::int32_t material_download_count = 0;

        for (std::set<ProxyConnection::p>::iterator i = proxy_connections_.begin();
            i != proxy_connections_.end(); ++i)
        {
            if ((*i) &&
                (*i)->GetPlayInfo() &&
                (*i)->GetPlayInfo()->GetSpeedLimit() != -1 &&
                (*i)->GetDownloadDriver())
            {
                // 设定固定限速值
                (*i)->GetDownloadDriver()->SetSpeedLimitInKBps((*i)->GetPlayInfo()->GetSpeedLimit());

                // 屏蔽全局限速
                continue;
            }

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
                    if ((*i)->GetPlayInfo() && (*i)->GetPlayInfo()->GetSourceType() != PlayInfo::SOURCE_DOWNLOAD_MOVIE
                        && (*i)->GetPlayInfo() && (*i)->GetPlayInfo()->GetLevel() != PlayInfo::POSITIVELY_DOWNLOAD_LEVEL)      //素材下载
                    {
                        ++material_download_count;
                    }
                }
            }
        }

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
                        (*iter)->GetDownloadDriver()->GetRestPlayableTime() <= 
                        (*iter)->GetDownloadDriver()->GetRestTimeNeedLimitSpeed())
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
                    download_speed_limit_in_KBps = 5;
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
                        LIMIT_MIN(download_speed_limit_in_KBps, 5);
                    }
                }
                material_download_speed_limit_inKBps = download_speed_limit_in_KBps;
                is_download_speed_limited_ = true;
            }
            else
            {
                // 不在观看，不限速
                download_speed_limit_in_KBps = -1;
                if (material_download_count != 0)
                {
                    material_download_speed_limit_inKBps = bandwidth * 3 / 4 / 1024 / material_download_count;
                    LimitMinMax(material_download_speed_limit_inKBps, 10, 30);
                }
                is_download_speed_limited_ = false;
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
                boost::int32_t speed_limit;
                if((*i)->GetPlayInfo() && ((*i)->GetPlayInfo()->GetSourceType() != PlayInfo::SOURCE_DOWNLOAD_MOVIE
                    && (*i)->GetPlayInfo() && (*i)->GetPlayInfo()->GetLevel() != PlayInfo::POSITIVELY_DOWNLOAD_LEVEL))
                {
                    speed_limit = material_download_speed_limit_inKBps;
                }
                else
                {
                    speed_limit = download_speed_limit_in_KBps;
                }
                (*i)->GetDownloadDriver()->SetSpeedLimitInKBps(speed_limit);
                if ((*i)->GetDownloadDriver()->GetStatistic())
                {
                    (*i)->GetDownloadDriver()->GetStatistic()->SetSmartPara(0, bandwidth, speed_limit);
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
            boost::static_pointer_cast<storage::Instance>(storage::Storage::Inst()->GetInstanceByRID(rid));

        if (!inst)
        {
            LOG4CPLUS_DEBUG_LOG(logger_proxy, "GetBlockHashFailed No Such RID: " << rid);
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
            boost::static_pointer_cast<storage::Instance>(storage::Storage::Inst()->GetInstanceByUrl(url));

        if (!inst)
        {
            string filename = ParseOpenServiceFileName(network::Uri(url));
            inst = boost::static_pointer_cast<storage::Instance>(
                storage::Storage::Inst()->GetInstanceByFileName(filename));
        }

        if (!inst)
        {
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
        LOG4CPLUS_DEBUG_LOG(logger_proxy, "segno = " << segno);

        string ext = "[" + segno + "]";
        boost::uint32_t pos = filename.rfind('.');
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
        storage::Instance::p inst = boost::static_pointer_cast<storage::Instance>(
            storage::Storage::Inst()->GetInstanceByRID(rid));
        if (inst && inst->IsComplete())
        {
            file_path = inst->GetResourceName();
        }

        fun();
    }

    void ProxyModule::GetCompeletedFilePathByUrl(const char * url, string & file_path, boost::function<void ()> fun)
    {
        storage::Instance::p inst = boost::static_pointer_cast<storage::Instance>(
            storage::Storage::Inst()->GetInstanceByUrl(url));
        if (!inst)
        {
            string filename = ParseOpenServiceFileName(network::Uri(url));
            inst = boost::static_pointer_cast<storage::Instance>(
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
                LOG4CPLUS_DEBUG_LOG(logger_proxy, "ProxyConnection NULL!!");
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

    boost::uint32_t ProxyModule::GetLiveRestPlayableTime() const
    {
        boost::uint32_t rest_playable_time = 0xffffffff;

        for (std::set<ProxyConnection__p>::const_iterator iter = proxy_connections_.begin();
            iter != proxy_connections_.end(); ++iter)
        {
            if (rest_playable_time > (*iter)->GetLiveRestPlayableTime() && (*iter)->GetLiveRestPlayableTime() != 0)
            {
                rest_playable_time = (*iter)->GetLiveRestPlayableTime();
            }
        }

        // 如果所有的剩余时间都是0，那么把rest_playable_time置为0
        if (rest_playable_time == 0xffffffff)
        {
            rest_playable_time = 0;
        }

        return rest_playable_time;
    }

    boost::uint8_t ProxyModule::GetLostRate() const
    {
        boost::uint32_t total_lost_rate = 0;
        boost::uint32_t total_request_subpiece_count = 0;

        for (std::set<ProxyConnection__p>::const_iterator iter = proxy_connections_.begin();
            iter != proxy_connections_.end(); ++iter)
        {
            total_request_subpiece_count += (*iter)->GetLiveTotalRequestSubPieceCount();
            total_lost_rate += (*iter)->GetLostRate() * (*iter)->GetLiveTotalRequestSubPieceCount();
        }

        if (total_request_subpiece_count == 0)
        {
            return 0;
        }

        return total_lost_rate / total_request_subpiece_count;
    }

    boost::uint8_t ProxyModule::GetRedundancyRate() const
    {
        boost::uint32_t total_redundancy_rate = 0;
        boost::uint32_t total_received_subpiece_count = 0;

        for (std::set<ProxyConnection__p>::const_iterator iter = proxy_connections_.begin();
            iter != proxy_connections_.end(); ++iter)
        {
            total_received_subpiece_count += (*iter)->GetLiveTotalRecievedSubPieceCount();
            total_redundancy_rate += (*iter)->GetRedundancyRate() * (*iter)->GetLiveTotalRecievedSubPieceCount();
        }

        if (total_received_subpiece_count == 0)
        {
            return 0;
        }

        return total_redundancy_rate / total_received_subpiece_count;
    }

    void ProxyModule::SetReceiveConnectPacket(boost::shared_ptr<storage::LiveInstance> instance)
    {
        for (std::set<ProxyConnection::p>::iterator iter = proxy_connections_.begin();
            iter != proxy_connections_.end(); ++iter)
        {
            if ((*iter)->IsLiveConnection() &&
                (*iter)->GetLiveDownloadDriver()->GetInstance() == instance)
            {
                (*iter)->GetLiveDownloadDriver()->SetReceiveConnectPacket();
            }
        }
    }

    void ProxyModule::SetSendSubPiecePacket(boost::shared_ptr<storage::LiveInstance> instance)
    {
        for (std::set<ProxyConnection::p>::iterator iter = proxy_connections_.begin();
            iter != proxy_connections_.end(); ++iter)
        {
            if ((*iter)->IsLiveConnection() &&
                (*iter)->GetLiveDownloadDriver()->GetInstance() == instance)
            {
                (*iter)->GetLiveDownloadDriver()->SetSendSubPiecePacket();
            }
        }
    }

    void ProxyModule::OnUdpRecv(protocol::Packet const & packet)
    {
        switch (packet.PacketAction)
        {
        case protocol::TcpReportStatusPacket::Action:
            {
                protocol::TcpReportStatusPacket const & stauts_pacekt = (protocol::TcpReportStatusPacket const &)packet;
                SetRestPlayTime(stauts_pacekt.resource_id_, stauts_pacekt.rest_play_time_in_seconds_ * 1000);
            }
            break;
        case protocol::TcpStartDownloadPacket::Action:
            {
                protocol::TcpStartDownloadPacket const & start_download_packet = (protocol::TcpStartDownloadPacket const &)packet;
                StartDownloadFile(start_download_packet.download_url_, start_download_packet.refer_url_, start_download_packet.user_agent_, start_download_packet.filename_);
            }
            break;
        case  protocol::TcpStopDownLoadPacket::Action:
            {
                protocol::TcpStopDownLoadPacket const & stop_download_packet = (protocol::TcpStopDownLoadPacket const &)packet;
                StopProxyConnection(stop_download_packet.stop_url_);
            }
            break;
        case protocol::TcpQuerySpeedPacket::Action:
            {
                protocol::TcpQuerySpeedPacket const & query_speed_packet = (protocol::TcpQuerySpeedPacket const &)packet;
                QuerySpeedInfoByTcpPacket(query_speed_packet);
            }
            break;
        default:
            break;
        }
    }

    void ProxyModule::QuerySpeedInfoByTcpPacket(protocol::TcpQuerySpeedPacket const &packet)
    {
        boost::uint32_t download_speed = 0;
        boost::uint32_t sn_speed = 0;
        for (std::set<ProxyConnection__p>::iterator iter = proxy_connections_.begin();
            iter != proxy_connections_.end(); ++iter)
        {
            ProxyConnection::p proxy_conn = *iter;
            if (!proxy_conn) 
            {
                LOG4CPLUS_DEBUG_LOG(logger_proxy, "ProxyConnection NULL!!");
                continue;
            }

            DownloadDriver::p dd = proxy_conn->GetDownloadDriver();
            if (dd && dd->GetStatistic() && dd->GetStatistic()->GetResourceID() == packet.request.rid_)
            {                        
                download_speed += dd->GetStatistic()->GetSpeedInfo().NowDownloadSpeed;
                if (dd->GetP2PDownloader() && dd->GetP2PDownloader()->GetStatistic())
                {
                    download_speed += dd->GetP2PDownloader()->GetStatistic()->GetSpeedInfo().NowDownloadSpeed;
                    sn_speed += dd->GetP2PDownloader()->GetStatistic()->GetSnSpeedInfo().NowDownloadSpeed;
                }
                
                break;
            }
        }

        protocol::TcpQuerySpeedPacket response_packet(download_speed, sn_speed);
        packet.tcp_connection_->DoSend(response_packet);
    }

    void ProxyModule::UpdateStopTime(const RID & channel_id)
    {
        time_elapsed_since_stop_[channel_id] = tick_counter_.elapsed();
    }

    bool ProxyModule::TryGetTimeElapsedSinceStop(const RID & channel_id, boost::uint32_t & time_elapsed) const
    {
        std::map<RID, boost::uint32_t>::const_iterator iter = time_elapsed_since_stop_.find(channel_id);

        if (iter != time_elapsed_since_stop_.end())
        {
            assert(tick_counter_.elapsed() >= iter->second);
            time_elapsed = tick_counter_.elapsed() - iter->second;

            return true;
        }

        return false;
    }

    void ProxyModule::ResumeOrPauseAllDownload(bool need_pause)
    {
        for(std::set<ProxyConnection__p>::const_iterator iter = proxy_connections_.begin();
            iter != proxy_connections_.end(); ++iter)
        {
            (*iter)->ResumeOrPauseDownload(need_pause);
        }
    }
}
