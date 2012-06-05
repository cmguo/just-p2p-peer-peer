//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "base/util.h"
#include "message.h"
#include "storage/storage_base.h"
#include "storage/Storage.h"
#include "storage/format/Mp4Spliter.h"

#include "p2sp/AppModule.h"
#include "p2sp/download/DownloadDriver.h"

#include "p2sp/proxy/ProxyConnection.h"
#include "p2sp/proxy/ProxyModule.h"
#include "p2sp/proxy/NullProxySender.h"
#include "p2sp/proxy/CommonProxySender.h"
#include "p2sp/proxy/FlvDragProxySender.h"
#include "p2sp/proxy/DirectProxySender.h"
#include "p2sp/proxy/OpenServiceProxySender.h"
#include "p2sp/proxy/PlayInfo.h"
#include "p2sp/proxy/Mp4DragProxySender.h"
#include "p2sp/proxy/RangeInfo.h"
#include "p2sp/proxy/RangeProxySender.h"
#include "p2sp/proxy/MessageBufferManager.h"
#include "network/UrlCodec.h"
#include "p2sp/proxy/LiveProxySender.h"
#include "p2sp/download/LiveDownloadDriver.h"
#include "p2sp/push/PushModule.h"

#ifdef AUTO_SVN_VERSION
#include "autopeerversion.hpp"
#else
#include "PeerVersion.h"
#endif
#ifdef BOOST_WINDOWS_API
#include "WindowsMessage.h"
#endif

#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>

#ifndef ALLOW_REMOTE_ACCESS
#ifdef NEED_LOG
# define ALLOW_REMOTE_ACCESS 1
#else
# define ALLOW_REMOTE_ACCESS 0
#endif  // NEED_LOG
#endif

// #define P() std::cout << __FUNCTION__ << ":" << __LINE__ << std::endl
#define P()

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("proxy");

    ProxyConnection::ProxyConnection(
        boost::asio::io_service & io_svc,
        network::HttpServer::pointer http_server_socket)
        : io_svc_(io_svc)
        , http_server_socket_(http_server_socket)
        , is_running_(false)
        , file_length_(0)
        , metadata_parsed_(false)
        , paused_by_user_(false)
        , is_notified_stop_(false)
        , save_mode_(false)
        , is_movie_url_(false)
        , will_stop_download_(false)
        , send_count_(0)
        , send_speed_limit_(DEFAULT_SEND_SPEED_LIMIT)
        , is_live_connection_(false)
#ifdef DISK_MODE
        , play_history_item_handle_(PlayHistoryManager::InvalidHandle())
#endif
        , need_estimate_ikan_rest_play_time_(true)
    {
    }

    ProxyConnection::ProxyConnection(
        boost::asio::io_service & io_svc)
        : io_svc_(io_svc)
        , is_running_(false)
        , file_length_(0)
        , metadata_parsed_(false)
        , paused_by_user_(false)
        , is_notified_stop_(false)
        , save_mode_(true)
        , is_movie_url_(false)
        , will_stop_download_(false)
        , send_count_(0)
        , send_speed_limit_(DEFAULT_SEND_SPEED_LIMIT)
        , is_live_connection_(false)
#ifdef DISK_MODE
        , play_history_item_handle_(PlayHistoryManager::InvalidHandle())
#endif
        , need_estimate_ikan_rest_play_time_(true)
    {
    }

    ProxyConnection::~ProxyConnection()
    {
    }

    void ProxyConnection::initialize()
    {
        if (false == is_running_) {
            return;
        }

        metadata_parsed_ = false;

        file_length_ = 0;
        openservice_head_length_ = 0;

        header_buffer_.Malloc(HEADER_LENGTH);
        header_buffer_length_ = 0;

        content_buffer_.Malloc(CONTENT_LENGTH);
        content_buffer_length_ = 0;

        silent_time_counter_.reset();
        will_stop_ = false;
        will_stop_download_ = false;
        queried_content_ = false;
        is_notified_stop_ = false;

        time_interval_in_ms_ = 250;
        send_subpieces_per_interval_ = 0;
        expected_subpieces_per_interval_ = 512 * time_interval_in_ms_ / 1000;
        // play_timer_ = framework::timer::PeriodicTimer::create(time_interval_in_ms_, shared_from_this());
        // play_timer_->Start();
    }

    void ProxyConnection::clear()
    {
        if (false == is_running_)
        {
            return;
        }

        if (download_driver_)
        {
            download_driver_->Stop();
            download_driver_.reset();
        }

        if (proxy_sender_)
        {
            proxy_sender_->Stop();
            proxy_sender_.reset();
        }

        // header_buffer
        header_buffer_ = base::AppBuffer();
        header_buffer_length_ = 0;
        // content_buffer
        content_buffer_ = base::AppBuffer();
        content_buffer_length_ = 0;
    }

    void ProxyConnection::Start()
    {
        if (is_running_ == true) return;

        is_running_ = true;

        LOG(__EVENT, "proxy", "ProxyConnection::Start " << shared_from_this());
        if (http_server_socket_)
        {
            http_server_socket_->SetListener(shared_from_this());
            http_server_socket_->SetRecvTimeout(5*1000);
            http_server_socket_->HttpRecv();
        }
        rest_time = 0;
    }

    void ProxyConnection::Stop()
    {
        if (is_running_ == false) return;

        LOG(__EVENT, "proxy", "ProxyConnection::Stop " << shared_from_this());
        // assert(http_server_socket_);

        clear();

        if (http_server_socket_)
        {
            LOG(__EVENT, "httpserver", __FUNCTION__ << " close http_server_socket_ = " << http_server_socket_);
            // std::set handler to null, do not need notice any more
            http_server_socket_->SetListener(IHttpServerListener::pointer());
            http_server_socket_->WillClose();
            http_server_socket_.reset();
        }

        // running
        is_running_ = false;
    }

    void ProxyConnection::SendHttpRequest()
    {
        if (false == is_running_)
            return;

        if (proxy_sender_)
        {
            proxy_sender_->SendHttpRequest();
        }
        else
        {
            assert(0);
        }
    }

    void ProxyConnection::WillStop()
    {
        if (is_running_ == false) return;

        LOG(__EVENT, "proxy", "ProxyConnection::WillStop " << shared_from_this());
#ifdef DISK_MODE
        if (play_history_item_handle_ != PlayHistoryManager::InvalidHandle()) {
            PushModule::Inst()->GetPlayHistoryManager()->StopVideoPlay(play_history_item_handle_);
            play_history_item_handle_ = PlayHistoryManager::InvalidHandle();
        }
#endif

        will_stop_ = true;

        if (IsLiveConnection())
        {
            ProxyModule::Inst()->UpdateStopTime(live_download_driver_->GetChannelId());
        }

        StopDownloadDriver();

//         MainThread::Post(
//             boost::bind(&ProxyModule::RemoveProxyConnection, ProxyModule::Inst(), shared_from_this())
//            );
        ProxyModule::Inst()->RemoveProxyConnection(shared_from_this());
    }

    uint32_t ProxyConnection::GetPlayingPosition() const
    {
        if (false == is_running_)
        {
            return 0;
        }

        if (!proxy_sender_)
        {
            return 0;
        }
        else if (buf_deque_.empty())
        {
            return proxy_sender_->GetPlayingPosition();
        }
        else
        {
            return buf_deque_.GetPlayingPostion(proxy_sender_->GetPlayingPosition());
        }
    }

    void ProxyConnection::ResetPlayingPostion()
    {
        if (false == is_running_)
            return;
        if (proxy_sender_)
        {
            proxy_sender_->ResetPlayingPosition();
        }
    }

    void ProxyConnection::OnRecvSubPiece(
        uint32_t start_position,
        std::vector<base::AppBuffer> const & buffers)
    {
        LOG(__DEBUG, "proxy", "OnRecvSubPiece start_position = " << start_position);
        if (is_running_ == false) return;

        silent_time_counter_.reset();

        // IKAN智能限速 - 剩余时间估计
        if (need_estimate_ikan_rest_play_time_ && download_driver_ && !download_driver_->IsPPLiveClient())
        {
            rest_time += 1000 * (buffers.size() * 1024) / download_driver_->GetDataRate();
        }

        assert(proxy_sender_);
        if (GetPlayingPosition() != start_position)
        {
            LOG(__WARN, "proxy", "ProxyConnection::OnAsyncGetSubPieceSucced playing position "
                << GetPlayingPosition() << " start_possition " << start_position);
            return;
        }
#ifdef DISK_MODE        
        if (play_history_item_handle_ != PlayHistoryManager::InvalidHandle()) {
            if (download_driver_) {
                PushModule::Inst()->GetPlayHistoryManager()->SetVideoSize(play_history_item_handle_, download_driver_->GetFileLength());
                PushModule::Inst()->GetPlayHistoryManager()->SetVideoBitrate(play_history_item_handle_, download_driver_->GetDataRate());
            }
            PushModule::Inst()->GetPlayHistoryManager()->SetVideoPlayPosition(play_history_item_handle_, GetPlayingPosition());
        }
#endif
        LOG(__WARN, "proxy", "proxy_sender_->OnRecvSubPiece, position = " << start_position
            << ", buffer.size = " << buffers.size());

        int can_send_cout = send_speed_limit_ - send_count_;
        if (can_send_cout >= buffers.size())
        {
            // herain:2010-12-30:发送速度不到限速值，全部可以发送
            proxy_sender_->OnRecvSubPiece(start_position, buffers);
            send_count_ += buffers.size();
        }
        else
        {
            // herain:2010-12-30:发送速度超过限速，只有部分报文可以发送

            // herain:2011-1-4:能发送的报文数不为0，将能发送的报文发送出去
            if (can_send_cout > 0)
            {
                assert(buf_deque_.empty());
                std::vector<base::AppBuffer> send_buffs;
                for (int i = 0; i < can_send_cout; ++i)
                {
                    send_buffs.push_back(buffers[i]);
                }

            //    DebugLog("send_buffs: %d",send_buffs.size());

                proxy_sender_->OnRecvSubPiece(start_position, send_buffs);
                send_count_ += send_buffs.size();
            }

            // herain:2011-1-4:不能发送的报文存入缓存队列
            for (int i = can_send_cout; i < buffers.size(); ++i)
            {
                buf_deque_.push_back(start_position+i*SUB_PIECE_SIZE, buffers[i]);
            }
        }

        if (!download_driver_)
        {
            LOG(__WARN, "proxy", __FUNCTION__ << ":" << __LINE__ << " download_driver_ = " << download_driver_);
            return;
        }

        if (download_driver_->GetStatistic())
        {
            download_driver_->GetStatistic()->SetPlayingPosition(GetPlayingPosition());
        }

        if (!download_driver_->GetInstance()->HasRID() && !queried_content_ && content_buffer_)
        {
            uint32_t i = 0;
            uint32_t start_pos = start_position;

            while (start_pos < CONTENT_LENGTH && i < buffers.size())
            {
                base::AppBuffer const & buffer = buffers[i++];
                uint32_t remain = CONTENT_LENGTH - start_pos;
                uint32_t len = (remain < buffer.Length() ? remain : buffer.Length());
                base::util::memcpy2(content_buffer_.Data() + start_pos, content_buffer_.Length() - start_pos, buffer.Data(), len);
                start_pos += len;
            }

            if (start_pos >= CONTENT_LENGTH)
            {
                if (download_driver_->GetInstance()) {
                    download_driver_->GetInstance()->DoMakeContentMd5AndQuery(content_buffer_);
                }
                else {
                    LOG(__DEBUG, "proxy", __FUNCTION__ << ":" << __LINE__ << " download_driver_->GetInstance() == null !!!");
                }
                queried_content_ = true;
            }
        }

        if (!metadata_parsed_ && header_buffer_)
        {
            uint32_t i = 0;
            uint32_t start_pos = start_position;

            while (start_pos < HEADER_LENGTH && i < buffers.size())
            {
                base::AppBuffer const & buffer = buffers[i++];
                uint32_t remain = HEADER_LENGTH - start_pos;
                uint32_t len = (remain < buffer.Length() ? remain : buffer.Length());
                base::util::memcpy2(header_buffer_.Data() + start_pos, header_buffer_.Length() - start_pos, buffer.Data(), len);
                start_pos += len;
            }

            if (start_pos >= HEADER_LENGTH)
            {
                // 解析码流率
                // IInstance::p inst = download_driver_->GetInstance();
                if (download_driver_->GetInstance())
                {
                    if (download_driver_->GetInstance()->ParseMetaData(header_buffer_))
                    {
#ifdef NEED_TO_POST_MESSAGE
                        LPRESOURCE_DATA_RATE_INFO data_rate_info =
                            MessageBufferManager::Inst()->NewStruct<RESOURCE_DATA_RATE_INFO>();
                        if (data_rate_info != NULL)
                        {
                            memset(data_rate_info, 0, sizeof(RESOURCE_DATA_RATE_INFO));
                            data_rate_info->uSize = sizeof(RESOURCE_DATA_RATE_INFO);
                            data_rate_info->fDataRate = static_cast<float>(download_driver_->GetInstance()->GetMetaData().VideoDataRate);
                            string orig_url = source_url_;
                            LOGX(__DEBUG, "msg", "source_url dest_length = " << _countof(data_rate_info->szOriginalUrl) <<
                                ", length = " << orig_url.length() << ", url = " << orig_url);
                            strncpy(data_rate_info->szOriginalUrl, orig_url.c_str(), sizeof(data_rate_info->szOriginalUrl)-1);
                            // log
                            LOGX(__DEBUG, "msg", "Post UM_GOT_RESOURCE_DATARATE:" <<
                                "  uSize = " << data_rate_info->uSize <<
                                ", fDataRate = " << data_rate_info->fDataRate <<
                                ", szOriginalUrl = " << data_rate_info->szOriginalUrl);
                            // post message
                            WindowsMessage::Inst().PostWindowsMessage(UM_GOT_RESOURCE_DATARATE, (WPARAM)download_driver_->GetDownloadDriverID(), (LPARAM)data_rate_info);
                        }
#endif
                    }
                }
                metadata_parsed_ = true;
            }
        }


        //DebugLog("proxy_sender_->GetPlayingPosition() = %d file length = %d", proxy_sender_->GetPlayingPosition(), file_length_);
        LOGX(__WARN, "proxy", "proxy_sender_->GetPlayingPosition() = " << proxy_sender_->GetPlayingPosition() << " file length = " << file_length_);
        if (proxy_sender_->GetPlayingPosition() >= file_length_)
        {
            LOGX(__WARN, "proxy", "PlayingPosition >= file_length_, IsComplete = " << download_driver_->GetInstance()->IsComplete());
            if (download_driver_ && download_driver_->GetInstance() && download_driver_->GetInstance()->IsComplete())
            {
                DebugLog("line 409 : proxy StopDownloadDriver();");
                LOG(__WARN, "proxy", __FUNCTION__ << ":" << __LINE__ << " StopDownloadDriver();");
                StopDownloadDriver();
            }
            return;
        }
    }

    void ProxyConnection::OnDownloadByUrlRequest(PlayInfo::p play_info)
    {
        if (false == is_running_) {
            return;
        }
        if (!play_info) {
            return;
        }
        if (play_info->GetPlayType() != PlayInfo::DOWNLOAD_BY_URL) {
            return;
        }

        // parameters
        protocol::UrlInfo url_info = play_info->GetUrlInfo();
        protocol::RidInfo rid_info = play_info->GetRidInfo();
        LOGX(__DEBUG, "proxy", "protocol::UrlInfo = " << url_info << "\n\t protocol::RidInfo = " << rid_info);
        network::HttpRequest::p http_request;
        int speed_limit_in_kBps = play_info->GetSpeedLimit();

        protocol::UrlInfo url_info_local = url_info;
        string url = url_info_local.url_;
        if (!boost::algorithm::istarts_with(url, "http://"))
        {
            LOG(__WARN, "proxy", __FUNCTION__ << "Invalid url! " << url_info.url_);
            url_info_local.url_ = "http://" + url_info_local.url_;
        }

        if (false == save_mode_)
        {
            proxy_sender_ = CommonProxySender::create(io_svc_, http_server_socket_);
            proxy_sender_->Start();

            http_server_socket_->SetAutoClose(false);
        }
        else
        {
            proxy_sender_ = NullProxySender::create(shared_from_this());
            proxy_sender_->Start();
        }

        download_driver_ = DownloadDriver::create(io_svc_, shared_from_this());
        download_driver_->SetSourceType(static_cast<uint32_t>(play_info->GetSourceType()));
        // speed limit
        download_driver_->SetSpeedLimitInKBps(speed_limit_in_kBps);

        // start
        if (play_info->HasRidInfo())
        {
            download_driver_->SetRidInfo(rid_info);
            download_driver_->Start(url_info_local, false, false, SwitchController::CONTROL_MODE_DOWNLOAD);
        }
        else
        {
            download_driver_->Start(http_request, url_info_local, false, SwitchController::CONTROL_MODE_DOWNLOAD);
        }
    }

    void ProxyConnection::OnUrlInfoRequest(const protocol::UrlInfo& url_info, const protocol::RidInfo& rid_info, network::HttpRequest::p http_request)
    {
        if (false == is_running_)
            return;

        LOG(__EVENT, "proxy", __FUNCTION__ << "\n\tUrl: " << url_info.url_ << "\n\tRefer: " << url_info.refer_url_);
        if (http_request) {
            LOG(__EVENT, "proxy" , __FUNCTION__ << "Request:\n" << http_request->GetRequestString());
        }
        // check url
        if (url_info.url_.length() == 0 || url_info.url_.find("'") != string::npos|| url_info.url_.find("\"") != string::npos)
        {
            LOG(__ERROR, "proxy", __FUNCTION__ << " Url ERROR !! Contain ',\"");
            WillStop();
            return;
        }

        // DirectProxySender
        network::Uri uri(url_info.url_);
        string uri_file = uri.getfile();
        string uri_domain = uri.getdomain();
        if (  // !TEST
            // true ||
            (
                uri_domain.find("lisbon") != string::npos
            || ((http_request && (http_request->GetMethod() != "GET"))
            && uri_file.find(".flv") == string::npos
            && uri_file.find(".mp4") == string::npos
            && uri_file.find(".wmf") == string::npos
            && uri_file.find(".wma") == string::npos
            && uri_file.find(".wmv") == string::npos
            && uri_file.find(".asf") == string::npos
            && uri_file.find(".rmvb") == string::npos
            && uri_file.find(".exe") == string::npos
            && uri_file.find(".mp3") == string::npos
            && uri_file.find(".swf") == string::npos
            && uri_domain.find("cache.googlevideo.com") == string::npos
            && uri_file.find("get_video") == string::npos
            && uri_file.find("videoplayback") == string::npos)
           )
            // || boost::algorithm::icontains(uri.getdomain(), "cctv.com")
            || boost::algorithm::icontains(uri.getdomain(), "dog.xnimg.cn")
            || boost::algorithm::icontains(uri.getdomain(), "dog.rrimg.com")
            || boost::algorithm::icontains(uri.getdomain(), "video.kaixin001.com")
            || boost::algorithm::icontains(uri.geturl(), "ppvakey=")
            || (http_request && http_request->HasProperty("Range"))
           )
        {
            if (http_request)
            {
                LOG(__INFO, "proxy", "OnHttpRecvSucced DirectProxySender" << url_info.url_);
                if (false == save_mode_)
                {
                    if (http_request->HasProperty("Range") && RangeInfo::IsSupportedRangeFormat(http_request->GetProperty("Range")))
                    {
                        proxy_sender_ = RangeProxySender::create(http_server_socket_);
                        proxy_sender_->Start(http_request, shared_from_this());

                        download_driver_ = DownloadDriver::create(io_svc_, shared_from_this());
                        if (http_request->GetPath().substr(0, 4) != "http")
                        {
                            download_driver_->SetSourceType(PlayInfo::SOURCE_HOOK);
                        }
                        else
                        {
                            download_driver_->SetSourceType(PlayInfo::SOURCE_PROXY);
                        }
                        download_driver_->Start(http_request, url_info, false);
                    }
                    else
                    {
                        proxy_sender_ = DirectProxySender::create(io_svc_, http_server_socket_, false);
                        proxy_sender_->Start(http_request, shared_from_this());
                        proxy_sender_->SendHttpRequest();
                    }
                }
                else
                {
                    proxy_sender_ = NullProxySender::create(shared_from_this());
                    proxy_sender_->Start();

                    download_driver_ = DownloadDriver::create(io_svc_, shared_from_this());
                    if (http_request->GetPath().substr(0, 4) != "http")
                    {
                        download_driver_->SetSourceType(PlayInfo::SOURCE_HOOK);
                    }
                    else
                    {
                        download_driver_->SetSourceType(PlayInfo::SOURCE_PROXY);
                    }
                    download_driver_->Start(http_request, url_info, false);
                }
            }
            else
            {
                WillStop();
                return;
            }
        }
        else
        {
            protocol::UrlInfo url_info_local = url_info;
            string url = url_info_local.url_;
            boost::algorithm::to_lower(url);
            if (!boost::algorithm::starts_with(url, "http://"))
            {
                LOG(__ERROR, "proxy", __FUNCTION__ << "Invalid url! " << url_info.url_);
                url_info_local.url_ = "http://" + url_info_local.url_;
            }

            if (false == save_mode_)
            {
                proxy_sender_ = CommonProxySender::create(io_svc_, http_server_socket_);
                proxy_sender_->Start();
            }
            else
            {
                proxy_sender_ = NullProxySender::create(shared_from_this());
                proxy_sender_->Start();
            }

            download_driver_ = DownloadDriver::create(io_svc_, shared_from_this());
            if (rid_info.HasRID())
            {
                if (http_request->GetPath().substr(0, 4) != "http")
                {
                    download_driver_->SetSourceType(PlayInfo::SOURCE_HOOK);
                }
                else
                {
                    download_driver_->SetSourceType(PlayInfo::SOURCE_PROXY);
                }

                download_driver_->SetRidInfo(rid_info);
                download_driver_->Start(url_info_local, false);
            }
            else
            {
                if (http_request->GetPath().substr(0, 4) != "http")
                {
                    download_driver_->SetSourceType(PlayInfo::SOURCE_HOOK);
                }
                else
                {
                    download_driver_->SetSourceType(PlayInfo::SOURCE_PROXY);
                }
                download_driver_->Start(http_request, url_info_local, false);
            }

            // limit send speed
            if (boost::algorithm::icontains(uri.gethost(), "qianqian.com") ||
                boost::algorithm::icontains(uri.getfile(), ".wma") ||
                boost::algorithm::icontains(uri.getfile(), ".mp3"))
            {
                LOGX(__DEBUG, "proxy", "QianQian: " << url_info.url_);
                expected_subpieces_per_interval_ = 128 * time_interval_in_ms_ / 1000;
            }
        }
    }

    void ProxyConnection::OnOpenServiceRequest(PlayInfo::p play_info)
    {
        if (false == is_running_)
            return;
        if (!play_info)
            return;
        if (play_info->GetPlayType() != PlayInfo::PLAY_BY_OPEN)
            return;


        LOG(__DEBUG, "bug", __FUNCTION__ << ":" << __LINE__ << " RID = " << play_info->GetRidInfo() << " Url = " << play_info->GetUrlInfo());
        if (play_info->HasUrlInfo())
        {
            // speed limit
            int speed_limit_in_kBps = play_info->GetSpeedLimit();

            protocol::UrlInfo url_info = play_info->GetUrlInfo();
            uint32_t start_position = play_info->GetStartPosition();

#undef TEST_DIRECT
// #define TEST_DIRECT
#ifdef TEST_DIRECT
            if (false == save_mode_)
            {
                network::Uri uri(url_info.url_);
                string request =
                    "GET " + uri.getrequest() + " HTTP/1.1\r\n"
                    "Host: " + uri.gethost() + "\r\n"
                    "Referer: " + url_info.refer_url_ + "\r\n"
                    "User-Agent: Mozilla/5.0 (Windows; U; Windows NT 5.1; zh-CN; rv:1.9.0.3) Gecko/2008092417 Firefox/3.0.3 QQDownload/1.7\r\n"
                    "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
                    "Connection: close\r\n"
                    "\r\n";
                network::HttpRequest::p http_request = network::HttpRequest::ParseFromBuffer(request);

                proxy_sender_ = DirectProxySender::create(http_server_socket_, false);
                proxy_sender_->Start(http_request, shared_from_this());
                proxy_sender_->SendHttpRequest();
            }
            else
            {
                WillStop();
            }
            return;
#endif

            bool auto_close = play_info->GetAutoClose();
            if (false == save_mode_)
            {
                LOGX(__DEBUG, "proxy", "NonSaveMode, http_server_socket_ = " << http_server_socket_);
                http_server_socket_->SetAutoClose(auto_close);
            }
            LOGX(__DEBUG, "proxy", "auto_close = " << auto_close);

            // fix url
            if (!boost::algorithm::istarts_with(url_info.url_, "http://"))
            {
                url_info.url_.insert(0, "http://");
            }
            // fix refer url
            if (url_info.refer_url_.length() == 0)
            {
                url_info.refer_url_ = url_info_.refer_url_;
            }

            network::Uri uri(url_info.url_);
            if (false == save_mode_)
            {
                LOG(__DEBUG, "downloadcenter", __FUNCTION__ << ":" << __LINE__ << " SaveMode = false");
                if (http_request_demo_->HasProperty("Range") && RangeInfo::IsSupportedRangeFormat(http_request_demo_->GetProperty("Range")))
                {
                    P();
                    LOGX(__DEBUG, "proxy", "RangeProxySender::create");
                    proxy_sender_ = RangeProxySender::create(http_server_socket_);
                    proxy_sender_->Start(http_request_demo_, shared_from_this());
                }
                else if(play_info->GetRangeInfo())
                {
                    RangeInfo__p range_info = play_info->GetRangeInfo();
                    LOGX(__DEBUG, "proxy", "RangeProxySender range:" 
                        << range_info->GetRangeBegin() << "-" << range_info->GetRangeEnd());
                    RangeProxySender::p sender = RangeProxySender::create(http_server_socket_);
                    proxy_sender_ = sender;
                    sender->Start(range_info, shared_from_this());
                }
                else if (start_position == 0)
                {
                    P();
                    LOGX(__DEBUG, "proxy", "CommonProxySender start_position = " << start_position);
                    proxy_sender_ = CommonProxySender::create(io_svc_, http_server_socket_);
                    proxy_sender_->Start();
                }
                else
                {
                    P();
                    LOGX(__DEBUG, "proxy", "OpenServiceProxySender start_position = " << start_position);
                    proxy_sender_ = OpenServiceProxySender::create(http_server_socket_, shared_from_this());
                    proxy_sender_->Start(start_position);
                }

                if (play_info->HasRidInfo())
                {
                    assert(start_position < play_info->GetRidInfo().GetFileLength());
                    proxy_sender_->OnNoticeGetContentLength(play_info->GetRidInfo().GetFileLength(), 
                        network::HttpResponse::p());
                }
            }
            else
            {
                LOGX(__DEBUG, "downloadcenter", __FUNCTION__ << ":" << __LINE__ << " SaveMode = true");
                LOGX(__DEBUG, "ppdebug", "is_openservice_range - true, save_mode_ = " << save_mode_);
                proxy_sender_ = NullProxySender::create(shared_from_this());
                proxy_sender_->Start();
            }

            string str_user_agent = http_request_demo_->GetProperty("User-Agent");
            url_info.user_agent_ = str_user_agent;
            LOGX(__DEBUG, "ppdebug", "user agent - " << str_user_agent);
            if (false == save_mode_ && str_user_agent.find("PPLive-Media-Player") != string::npos)
            {
                LOGX(__DEBUG, "ppdebug", "user agent - true");
            }

            P();
            // is_drag
            LOG(__DEBUG, "ppdebug", "play_info->GetIsDrag = " << play_info->GetIsDrag());

            // 客户端的默认发送限速为2MB
            if (play_info->GetPlayerId() == "")
            {
                send_speed_limit_ = DEFAULT_CLIENT_SEND_SPEED_LIMIT;
            }

            // download driver
            download_driver_ = DownloadDriver::create(io_svc_, shared_from_this());
            if (true == save_mode_ || play_info->HasPpvakey())
            {
                LOGX(__DEBUG, "downloadcenter", __FUNCTION__ << ":" << __LINE__ << " SaveMode, SetNeedBubble = false");
                url_info.url_ = ProxyModule::RemovePpvakeyFromUrl(url_info.url_);
            }

            download_driver_->SetRestPlayTime(play_info->GetRestTimeInMillisecond());
            download_driver_->SetIsHeadOnly(play_info->GetHeadOnly());
            download_driver_->SetSessionID(play_info->GetPlayerId());
            download_driver_->SetOpenServiceStartPosition(start_position);
            download_driver_->SetOpenServiceHeadLength(play_info->GetHeadLength());
            download_driver_->SetSourceType(static_cast<uint32_t>(play_info->GetSourceType()));
            download_driver_->SetSpeedLimitInKBps(speed_limit_in_kBps);
            download_driver_->SetBWType((JumpBWType)play_info->GetBWType());
            download_driver_->SetBakHosts(play_info->GetBakHosts());
            download_driver_->SetVipLevel((VIP_LEVEL)play_info->GetVip());
            download_driver_->SetPreroll(play_info->GetPreroll());
            P();
            P();

            string filename = ProxyModule::ParseOpenServiceFileName(uri);

            string index = filename.substr(filename.find_last_of('['),
                filename.find_last_of(']') - filename.find_last_of('[') + 1);
            string file_ext = filename.substr(filename.find_last_of('.'),
                filename.length() - filename.find_last_of('.'));
            download_driver_->SetOpenServiceFileName(filename);
            string segno = base::util::GetSegno(uri);
#ifdef DISK_MODE            
            BOOST_ASSERT(play_history_item_handle_ == PlayHistoryManager::InvalidHandle());
            string filename_provide_for_push = filename;
            if (!play_info->GetChannelName().empty())                          //url请求中含有channelname字段,文件名格式为channel_name + [X]+”.mp4” + ft+Y
            {
                filename_provide_for_push = play_info->GetChannelName() + index + file_ext + ".ft" + play_info->GetFileRateType();
            }
            
            play_history_item_handle_ = PushModule::Inst()->GetPlayHistoryManager()->StartVideoPlay(filename_provide_for_push);
#endif
            if (false == save_mode_)
            {
                int lastsegno = ProxyModule::Inst()->GetLastSegno(play_info->GetPlayerId());
                int nowsegno = atoi(segno.c_str());
                LOGX(__DEBUG, "switch", "lastsegno = " << lastsegno << " nowsegno = " << nowsegno);

                if (download_driver_->IsPPLiveClient())
                {
                    LOG(__DEBUG, "switch", "IsPPLiveClient");
                    download_driver_->SetIsDrag(play_info->GetIsDrag() == 1 ? true : false);
                }
                else
                {
                    LOG(__DEBUG, "switch", "Is NOT PPLiveClient");
                    if (lastsegno + 1 == nowsegno)
                    {
                        if (start_position == 0)
                        {
                            P();
                            LOGX(__DEBUG, "switch", "GetDragPrecent = " << ProxyModule::Inst()->GetDragPrecent() << ", download_driver = " << download_driver_);
                            if (ProxyModule::Inst()->GetDragPrecent() > 96)
                            {
                                download_driver_->SetIsDrag(true);
                            }
                            else
                            {
                                download_driver_->SetIsDrag(false);
                            }
                        }
                        else
                        {
                            P();
                            download_driver_->SetIsDrag(true);
                        }
                    }
                    else
                    {
                        download_driver_->SetIsDrag(play_info->GetIsDrag() == 1 ? true : false);
                    }
                    ProxyModule::Inst()->SetSegno(play_info->GetPlayerId(), nowsegno);
                }
            }

            if (play_info->HasRidInfo())
            {
                download_driver_->SetRidInfo(play_info->GetRidInfo());
            }

            if (play_info->GetSourceType() == PlayInfo::SOURCE_DOWNLOAD_MOVIE)
            {
                download_driver_->Start(url_info, false, true, SwitchController::CONTROL_MODE_DOWNLOAD);
            }
            else
            {
                download_driver_->Start(url_info, false, true);
            }

            if (false == save_mode_)
            {
                if (file_length_ != 0)
                {
                    LOGX(__DEBUG, "switch", "Start = " << start_position << "filelength = " << file_length_ << " SetLastDragPrecent = " << start_position * 100 / file_length_);
                    ProxyModule::Inst()->SetLastDragPrecent(start_position * 100 / file_length_);
                }
            }
            else
            {
                LOGX(__DEBUG, "switch", "save_mode_ = " << save_mode_);
                download_driver_->SetIsDrag(false);
            }
            P();

            // rename
            if (false == save_mode_)
            {
                LOG(__DEBUG, "downloadcenter", __FUNCTION__ << ":" << __LINE__ << " AttachFilenameByUrl, url = " << url_info.url_
                    << ", filename = " << filename);
                // framework::MainThread::Post(boost::bind(&ppva_peer::storage::Storage::AttachFilenameByUrl, ppva_peer::storage::Storage::Inst_Storage(), url_info.url_, filename));
                storage::Storage::Inst_Storage()->AttachFilenameByUrl(url_info.url_, filename);
            }
            else
            {
                // override qualified_filename
                qualified_file_name_ = (filename);
            }
        }
        else
        {
            assert(!"Should have url and rid!");
            LOG(__DEBUG, "downloadcenter", __FUNCTION__ << ":" << __LINE__ << " Should have url and rid!");
        }
    }

    void ProxyConnection::OnPlayByRidRequest(PlayInfo::p play_info)
    {
        if (false == is_running_) {
            return;
        }
        if (!play_info) {
            return;
        }
        if (play_info->GetPlayType() != PlayInfo::PLAY_BY_RID) {
            return;
        }

        uint32_t start_position = play_info->GetStartPosition();

        if (start_position > 3500)
        {
            proxy_sender_ = Mp4DragProxySender::create(io_svc_, http_server_socket_, shared_from_this());
            proxy_sender_->Start(start_position);
        }
        else
        {
            proxy_sender_ = CommonProxySender::create(io_svc_, http_server_socket_);
            proxy_sender_->Start();
        }

        download_driver_ = DownloadDriver::create(io_svc_, shared_from_this());
        download_driver_->SetSourceType(static_cast<uint32_t>(play_info->GetSourceType()));

        // start
        download_driver_->Start(network::HttpRequest::p(), play_info->GetRidInfo());  // no request demo
    }

    void ProxyConnection::OnHttpRecvSucced(network::HttpRequest::p http_request)
    {
        if (is_running_ == false) {
            return;
        }

        P();

        if (false == save_mode_)
        {
#if ALLOW_REMOTE_ACCESS
            // nothing
#else
            boost::system::error_code ec;
            string remote_address = http_server_socket_->GetEndPoint().address().to_string(ec);

            if (!ec)
            {
                // 成功了
                if (false == PlayInfo::IsLocalHost(remote_address, 0) && false == PlayInfo::IsLocalIP(remote_address, 0))
                {
                    LOGX(__EVENT, "proxy", "NOT_FROM_LOCALHOST " << http_server_socket_->GetEndPoint() << " \r\n" << *http_request);
                    WillStop();
                    return;
                }
            }
#endif  // ALLOW_REMOTE_ACCESS
        }

        clear();

        initialize();

        if (false == save_mode_)
        {
            LOG(__EVENT, "proxy", "OnHttpRecvSucced " << http_server_socket_->GetEndPoint() << " \r\n" << *http_request);

        }
        else
        {
            LOG(__EVENT, "downloadcenter", __FUNCTION__ << ":" << __LINE__ << " SaveMode Mock network::HttpRequest\n" << *http_request);
        }

        // save
        http_request_demo_ = http_request;

        if (false == save_mode_)
        {
            source_url_ = http_request->GetUrl();
            LOG(__DEBUG, "downloadcenter", __FUNCTION__ << ":" << __LINE__ << " save_mode = false, source_url = " << source_url_);
        }
        else
        {
            LOG(__DEBUG, "downloadcenter", __FUNCTION__ << ":" << __LINE__ << " save_mode = true, source_url = " << source_url_);
        }

        // 过滤掉含有非法字符的请求
        if (
            http_request->GetUrl().length() == 0 ||
            http_request->GetUrl().find("'") != string::npos||
            http_request->GetUrl().find("\"") != string::npos
           )
        {
            LOG(__ERROR, "proxy", "OnHttpRecvSucced Url ERROR !! Contain '  \"");
            WillStop();
            return;
        }

        P();
        // 启动 AppModule 的Start工程，开启定时器，定期Get数据s

        string request_path = http_request->GetPath();
        string request_url = http_request->GetUrl();

        if (
            (
            PlayInfo::IsLocalHost(http_request->GetHost(), ProxyModule::Inst()->GetHttpPort()) ||
            PlayInfo::IsLocalIP(http_request->GetHost(), ProxyModule::Inst()->GetHttpPort())
           ) &&
            !PlayInfo::IsGreenWayUrl(request_url)
         )
        {
            if (false == save_mode_)
            {
                if (boost::algorithm::istarts_with(request_path, "/crossdomain.xml") == true)
                {
                    LOG(__EVENT, "proxy", "OnHttpRecvSucced Request CrossDomain.xml");
                    string cross_domain_xml =
                        "<!-- PPVA -->\n"
                        "<?xml version=\"1.0\" ?>\n"
                        "<!DOCTYPE cross-domain-policy SYSTEM \"http://www.adobe.com/xml/dtds/cross-domain-policy.dtd\">\n"
                        "<cross-domain-policy>\n"
                        "  <!-- PPVA " PEER_KERNEL_VERSION_STR " -->\n"
                        "  <site-control permitted-cross-domain-policies=\"all\" />\n"
                        "  <allow-access-from domain=\"*\" />\n"
                        "  <allow-http-request-headers-from domain=\"*\" headers=\"*\"/>\n"
                        "</cross-domain-policy>\n"
                       ;
                    http_server_socket_->HttpSendContent(cross_domain_xml, "text/x-cross-domain-policy");
                }
                else if (boost::algorithm::istarts_with(request_path, "/synacast.xml") == true)
                {
                    std::ostringstream oss;
                    oss << "<?xml version=\"1.0\" ?>\n"
                        "<root>\n";
                    oss << "  <PPVA v=\"" PEER_KERNEL_VERSION_STR "\" p=\"" << AppModule::Inst()->GetLocalTcpPort() << "\"/>\n"
                        "</root>\n";
                    http_server_socket_->HttpSendContent(oss.str(), "text/xml");
                }
                else
                {
                    LOG(__EVENT, "proxy", "OnHttpRecvSucced Reject: \n" << *http_request);
                    http_server_socket_->HttpSend403Header();
                }
            }
            WillStop();
        }
        else
        {
            P();
            if (http_request->GetUrl().substr(0, 4) != "http")
            {
                http_request->ReSetHttpUrl();
                LOG(__EVENT, "proxy", "OnHttpRecvSucced ReSetHttpUrl" << http_request->GetUrl());
            }

            url_info_.type_ = protocol::UrlInfo::HTTP;
            url_info_.url_ = http_request->GetUrl();
            url_info_.refer_url_ = http_request->GetRefererUrl();

            LOG(__INFO, "proxy", "OnHttpRecvSucced Url  = " << url_info_.url_);
            LOG(__INFO, "proxy", "OnHttpRecvSucced Refer= " << url_info_.refer_url_);

            if (url_info_.url_ == "")
            {
                WillStop();
            }
            else
            {
                network::Uri uri_(url_info_.url_);

                is_movie_url_ = PlayInfo::IsMovieUrl(url_info_.url_);

                // 判断是不是直播的请求
                if (PlayInfo::IsLiveUrl(url_info_.url_))
                {
                    // 直播的请求
                    PlayInfo::p play_info = PlayInfo::Parse(url_info_.url_);
                    if (play_info)
                    {
                        play_info_ = play_info;
                        OnLiveRequest(play_info);
                    }
                }
                else if (PlayInfo::IsSetLiveUrl(url_info_.url_))
                {
                    // 直播参数修改请求(目前仅仅用于暂停)
                    // 直播的请求
                    PlayInfo::p play_info = PlayInfo::Parse(url_info_.url_);
                    if (play_info)
                    {
                        OnLivePause(play_info->GetChannelID(), play_info->GetLivePause(), play_info->GetUniqueID());
                    }
                }
                else if (PlayInfo::IsGreenWayUri(uri_))
                {
                    PlayInfo::p play_info = PlayInfo::Parse(url_info_.url_);
                    play_info_ = play_info;
                    // Modified by jeffrey 2010/2/21, play_info有可能为空
                    // to fix Crash(0x5b785) @ Peer 2.0.2.1051
                    if (play_info)
                    {
                        SetSendSpeedLimit(play_info->GetSendSpeedLimit());
                    }

                    if (play_info && (play_info->HasRidInfo() || play_info->HasUrlInfo()))
                    {
                        if (play_info->GetPlayType() == PlayInfo::PLAY_BY_OPEN)
                        {
                            LOG(__DEBUG, "bug", __FUNCTION__ << ":" << __LINE__ << " PlayInfo::PLAY_BY_OPEN");
                            if (play_info->HasUrlInfo())
                            {
                                if (play_info->HasPlayerId() /* && play_info->HasStart() */)
                                {
                                    ProxyType tmp_proxy_type(play_info->GetPlayType(), play_info->GetPlayerId());
                                    LOG(__DEBUG, "proxy", __FUNCTION__  << "(" << __LINE__ \
                                        << ") Check ProxyType: " << tmp_proxy_type);
                                    // ProxyModule::Inst()->StopProxyDownload(tmp_proxy_type, shared_from_this());
                                    ProxyModule::Inst()->StopProxyConnection(tmp_proxy_type, shared_from_this());
                                    proxy_type_ = tmp_proxy_type;
                                }
                                P();
                                // !OpenService
                                OnOpenServiceRequest(play_info);
                            }
                            else
                            {
                                WillStop();
                                return;
                            }
                        }
                        else if (play_info->GetPlayType() == PlayInfo::DOWNLOAD_BY_URL)
                        {
                            LOG(__DEBUG, "bug", __FUNCTION__ << ":" << __LINE__ << " PlayInfo::DOWNLOAD_BY_URL");
                            if (play_info->HasUrlInfo() || play_info->HasRidInfo())
                            {
                                if (play_info->GetRidInfo().rid_.is_empty() && play_info->HasRidInfo())
                                {
                                    LOG(__DEBUG, "proxy", "rid empty, will stop");
                                    WillStop();
                                    return;
                                }
                                else
                                {
                                    LOG(__DEBUG, "proxy", "rid not empty, OnDownloadByUrlRequest");
                                // OnDownloadByUrlRequest(play_info->GetUrlInfo(), play_info->GetRidInfo(), network::HttpRequest::p(), play_info->GetSpeedLimit());
                                    OnDownloadByUrlRequest(play_info);
                                }
                            }
                            else {
                                WillStop();
                                return;
                            }
                        }
                        else if (play_info->HasUrlInfo())
                        {
                            LOG(__DEBUG, "bug", __FUNCTION__ << ":" << __LINE__ << " OnUrlInfoRequest");
                            OnUrlInfoRequest(play_info->GetUrlInfo(), play_info->GetRidInfo(), network::HttpRequest::p());
                        }
                        else if (play_info->HasRidInfo())
                        {
                            LOG(__DEBUG, "bug", __FUNCTION__ << ":" << __LINE__ << " RID_PLAY");
                            OnPlayByRidRequest(play_info);
                        }
                    }
                    else
                    {
                        LOG(__ERROR, "proxy", __FUNCTION__ << "Invalid Url: " << url_info_.url_);
                        WillStop();
                        return;
                    }
                }
                else if (PlayInfo::IsLocalHost(http_request->GetHost(), ProxyModule::Inst()->GetHttpPort()) ||
                    PlayInfo::IsLocalIP(http_request->GetHost(), ProxyModule::Inst()->GetHttpPort()))
                {
                    LOG(__ERROR, "proxy", __FUNCTION__ << ":" << __LINE__ << "Invalid Request");
                    WillStop();
                    return;
                }
                else
                {
                    OnUrlInfoRequest(url_info_, protocol::RidInfo(), http_request);
                }
            }

            if (!will_stop_)
            {
                if (false == save_mode_)
                {
                    LOG(__EVENT, "proxy", "=========================> HttpRecv()");
                    /*
                    http_server_socket_->HttpRecvTillClose();
                    /*/
                    http_server_socket_->SetRecvTimeout(0);
                    http_server_socket_->HttpRecv();
                    // */
                }
                else
                {
                    LOG(__DEBUG, "downloadcenter", __FUNCTION__ << ":" << __LINE__ << " SaveMode, Do not RecvHttp");
                }
            }
        }
    }

    void ProxyConnection::OnHttpRecvFailed(uint32_t error_code)
    {
        if (is_running_ == false) return;
        if (false == save_mode_)
        {
            LOG(__ERROR, "proxy", "OnHttpRecvFailed " << " " << error_code << " " << shared_from_this());
            WillStop();
        }
        else
        {
            LOG(__DEBUG, "proxy", __FUNCTION__ << ":" << __LINE__ << " error_code = " << error_code << " " << shared_from_this());
            WillStop();
        }
    }

    void ProxyConnection::OnHttpRecvTimeout()
    {
        if (is_running_ == false) return;

        if (false == save_mode_)
        {
            LOG(__ERROR, "proxy", "OnHttpRecvTimeout " << http_server_socket_->GetEndPoint() << " " << shared_from_this());
            LOG(__ERROR, "pplive", "OnHttpRecvTimeout " << http_server_socket_->GetEndPoint() << " " << shared_from_this());
        }
        else
        {
            LOG(__DEBUG, "proxy", __FUNCTION__ << ":" << __LINE__ << " " << shared_from_this());
        }

        WillStop();
    }

    void ProxyConnection::OnTcpSendFailed()
    {
        if (is_running_ == false) return;

        if (false == save_mode_)
        {
            LOG(__WARN, "proxy", "OnTcpSendFailed " << http_server_socket_->GetEndPoint()  << " " << shared_from_this());
        }
        else
        {
            LOG(__DEBUG, "proxy", __FUNCTION__ << ":" << __LINE__ << " " << shared_from_this());
        }

        WillStop();
    }

    void ProxyConnection::StopDownloadDriver()
    {
        if (false == is_running_) {
            LOG(__WARN, "proxy", __FUNCTION__ << ":" << __LINE__ << " is_running_ = false");
            return;
        }

        LOG(__WARN, "proxy", __FUNCTION__ << ":" << __LINE__ << " will_stop_download_ = true");

        will_stop_download_ = true;

        // download driver
        if (download_driver_)
        {
            LOG(__WARN, "proxy", __FUNCTION__ << "(" << shared_from_this() << ") Stop!");
            global_io_svc().post(boost::bind(&DownloadDriver::Stop, download_driver_));
        }

        if (live_download_driver_)
        {
            live_download_driver_->Stop();
            live_download_driver_.reset();
        }
    }
/*
    void ProxyConnection::OnTimerElapsed(framework::timer::Timer * pointer, uint32_t times)
    {

        // LOG(__EVENT, "proxy", "ProxyConnection::OnTimerElapsed ");
        assert(0);

        if (is_running_ == false) return;
        if (pointer == subpiece_fail_timer_)
        {
            OnPlayTimer(times);
        }
        else if (pointer == play_timer_)
        {
            rest_time -= 250;

            if (rest_time < 0)
            {
                rest_time = 0;
            }

            if (download_driver_ && download_driver_->GetSessionID() != "")
            {
                download_driver_->SetRestPlayTime(rest_time);

                LOG(__DEBUG, "test", "rest_time = " << rest_time << " sessionid =  " << download_driver_->GetSessionID());
            }
            if (send_subpieces_per_interval_ >= expected_subpieces_per_interval_) {
                send_subpieces_per_interval_ = 0;
                OnPlayTimer(times);
            }
            else {
                send_subpieces_per_interval_ = 0;
            }
        }
        else
        {
            LOG(__EVENT, "proxy", "ProxyConnection::OnTimerElapsed assert(0)");
            // assert(0);
        }
    }
*/
    void ProxyConnection::OnProxyTimer(uint32_t times)
    {
        if (is_running_ == false)
            return;

        // 直播没有主动断连接策略
        // 只有播放器关闭才断开连接
        if (live_download_driver_)
        {
            return;
        }

        if (!proxy_sender_)
        {
            LOGX(__EVENT, "proxy", "ProxySender is Null. " << shared_from_this());
            return;
        }

        // IKAN预估剩余时间
        if (need_estimate_ikan_rest_play_time_ && download_driver_ && !download_driver_->IsPPLiveClient())
        {
            rest_time -= 1000;
            if (rest_time < 0)
            {
                rest_time = 0;
            }

            download_driver_->SetRestPlayTime(rest_time);
        }

        // herain:2010-12-30:处理发送限速的逻辑，每1s执行一次
        if (times % 4 == 0)
        {
            send_count_ = 0;
            std::vector<base::AppBuffer> buffs;

            boost::uint32_t playing_postion = proxy_sender_->GetPlayingPosition();

            while (send_count_ < send_speed_limit_ && false == buf_deque_.empty())
            {
                if (playing_postion == buf_deque_.front().first)
                {
                    buffs.push_back(buf_deque_.front().second);
                    playing_postion += buf_deque_.front().second.Length();
                    buf_deque_.pop_front();
                    send_count_++;
                }
                else
                {
                    buf_deque_.pop_front();
                }
            }

            if (false == buffs.empty())
            {
                proxy_sender_->OnRecvSubPiece(proxy_sender_->GetPlayingPosition(), buffs);
            }
        }

        if (times % 4 == 0)
        {
            LOG(__EVENT, "proxy", __FUNCTION__ << ":" << __LINE__ << " " << shared_from_this() << " IsWillStopDownload = " << IsWillStopDownload() << ", IsNotifiedStop = " << IsNotifiedStop());
            // check file length & download driver
            if (IsWillStopDownload() && IsNotifiedStop())
            {
                LOG(__EVENT, "proxy", __FUNCTION__ << ":" << __LINE__ << " " << shared_from_this() << " will_stop_download & download_finished");
                // stop
                WillStop();
            }
            // has play info
            else if (download_driver_ && play_info_ && file_length_ > 0 && proxy_sender_->GetPlayingPosition() >= file_length_)
            {
                if (!download_driver_->GetInstance())
                {
                    LOGX(__WARN, "proxy", "Instance Null");
                    WillStop();
                }
                else if (download_driver_->GetInstance()->IsComplete())
                {
                    LOGX(__WARN, "proxy", "Instance IsComplete = " << download_driver_->GetInstance()->IsComplete());
                    WillStop();
                }
            }
            else if (false == IsWillStopDownload())
            {
                // check
                CheckDeath();
            }
            // accelerate
            else
            {
                LOG(__EVENT, "proxy", "accelerate");
                WillStop();
            }
        }
    }

    void ProxyConnection::OnClose()
    {
        if (is_running_ == false) return;

        LOG(__EVENT, "proxy", "ProxyConnection::OnClose");
        WillStop();
    }

    void ProxyConnection::OnNoticeGetContentLength(uint32_t content_length, network::HttpResponse::p http_response)
    {
        if (is_running_ == false) return;
        // assert(is_response_header_ == false);
        assert(proxy_sender_);

        if (content_length == file_length_)
            return;

        LOG(__WARN, "proxy", "ProxyConnection::OnNoticeGetContentLength content_length" << content_length);

        proxy_sender_->OnNoticeGetContentLength(content_length, http_response);

        file_length_ = content_length;

        if (content_length != 0) {
            LOG(__DEBUG, "proxy", __FUNCTION__ << ":" << __LINE__ << " content_length != 0, OnPlayTimer(0)");
            // OnPlayTimer(0);
        }
    }

    void ProxyConnection::OnNoticeOpenServiceHeadLength(uint32_t head_length)
    {
        if (false == is_running_)
            return;
        // !
        openservice_head_length_ = head_length;
        proxy_sender_->OnNoticeOpenServiceHeadLength(head_length);
    }

    void ProxyConnection::OnNoticeDirectMode(DownloadDriver__p download_driver)
    {
        if (false == is_running_)
        {
            LOG(__WARN, "proxy", __FUNCTION__ << " Proxy connection is not running.");
            return;
        }
        if (!download_driver_)  // already direct mode
        {
            LOG(__WARN, "proxy", __FUNCTION__ << " Using direct mode already.");
            return;
        }
        if (!http_request_demo_)
        {
            LOG(__WARN, "proxy", __FUNCTION__ << " No http request demo, can not use direct mode.");
            return;
        }
        if (!download_driver || download_driver != download_driver_)
        {
            LOG(__WARN, "proxy", __FUNCTION__ << " Invalid download driver given.");
            return;
        }
        if (proxy_sender_->IsHeaderResopnsed())
        {
            LOG(__WARN, "proxy", __FUNCTION__ << " Header has been responsed.");
            return;
        }

        LOG(__EVENT, "proxy", __FUNCTION__ << " download_driver=" << download_driver);
        // release download driver
        download_driver_->Stop();
        download_driver_.reset();
        // release proxy sender
        proxy_sender_->Stop();  // be sure http_server_socket_ is not stopped
        proxy_sender_.reset();
        // start all with direct mode

        proxy_sender_ = DirectProxySender::create(io_svc_, http_server_socket_, true);
        proxy_sender_->Start(http_request_demo_, shared_from_this());
        proxy_sender_->SendHttpRequest();
    }
/*
    void ProxyConnection::OnAsyncGetBufferSucced(uint32_t start_position, protocol::SubPieceBuffer buffer)
    {
        OnAsyncGetSubPieceSucced(start_position, buffer);
    }
*/
    void ProxyConnection::CheckDeath()
    {
        if (false == is_running_)
            return;

        // 直播不主动断连接
        if (is_live_connection_)
        {
            return;
        }

        if (silent_time_counter_.elapsed() > DEFAULT_SILENT_TIME_LIMIT &&
            (!download_driver_ || download_driver_->IsOpenService() || false == download_driver_->IsRunning()) &&
            proxy_sender_->GetPlayingPosition() >= file_length_)
        {

            LOGX(__WARN, "proxy", " silent_time_counter_ > DEFAULT_SILENT_TIME_LIMIT; openservice; playingpos >= file_length");
            LOGX(__WARN, "proxy", " playingpos >= file_length " << proxy_sender_->GetPlayingPosition() << ">" << file_length_);
            WillStop();
            return;
        }

        if (
            silent_time_counter_.elapsed() > DEFAULT_SILENT_TIME_LIMIT &&
            download_driver_ && download_driver_->GetStatistic() &&
            download_driver_->GetStatistic()->GetSpeedInfo().MinuteDownloadSpeed == 0 &&
            download_driver_->GetInstance() && download_driver_->GetInstance()->GetRID().is_empty()
           )
        {

            LOG(__WARN, "proxy", __FUNCTION__ << " silent_time_counter_ > DEFAULT_SILENT_TIME_LIMIT; MinuteSpeed=0; RID=Empty");
            proxy_sender_->OnNotice403Header();
            WillStop();
            return;
        }

        if (
            silent_time_counter_.elapsed() > DEFAULT_SILENT_TIME_LIMIT &&
            download_driver_ && (
                !(download_driver_->GetHTTPControlTarget() || download_driver_->GetP2PControlTarget()) ||
                (
                    download_driver_->IsHttp403Header() &&
                    (
                        !download_driver_->GetP2PControlTarget() ||
                        (download_driver_->GetP2PControlTarget()->GetConnectedPeersCount() == 0 &&
                        download_driver_->GetP2PControlTarget()->GetRecentDownloadSpeed() == 0)
                   )
               )
           )
           )
        {

            LOG(__WARN, "proxy", __FUNCTION__ << " silent_time_counter_ > 20s; !Http&!P2P; 403&!P2P");
            proxy_sender_->OnNotice403Header();
            WillStop();
            return;
        }
    }

    /************************************************************************/
    /*
     *                   IKAN的无插start to range 算法
     *
     *                        2010/3/10 jeffreywu
     *
     * 1. 假设ikan发过来的请求 head=204091 & start = 1246217，这个意义就是这
     *    个影片204091只这个分段的MP4头部长度，start是drag里面的拖动点位置
     * 2. 内核首先去下载 [0 - 204091)，把完整的头部下载下来，然后根据这个头部
     *    的数据 调用内核Mp4HeadParse函数计算关键帧
     * 3. 因为flash播放器是拖动的，从中间播放的，所以内核发送给flash播放器的
     *    头部是需要精简的，而且需要计算出关键帧的位置，必须从关键帧发送数据
     *    才可以播放。
     * 4. Mp4HeadParse函数的第一个参数是 完整的头部数据，以下简称buffer; 第二
     *    个参数是start的值，这里应该是1246217，返回值是一个关键帧的位置。
     * 5. 调用GetHeader(buffer, 1246217)的返回值是1281575，也就是说内核的数据
     *    要从这个位置发送
     * 6. 但是！！1281575不是1024的倍数啊，所以内核会做这样的处理，
     *     (1281575 / 1024 ) * 1024 = 1281024
     * 7. 终于，内核发送给CDN请求的range应该从[1281024 - END)
     */
    /************************************************************************/
    base::AppBuffer ProxyConnection::GetHeader(
        uint32_t& key_frame_position,
        base::AppBuffer const & header_buffer)
    {
        // only used in OpenService
        if (false == is_running_)
            return base::AppBuffer();

        protocol::UrlInfo info = download_driver_->GetOriginalUrlInfo();
        network::Uri uri(info.url_);

        base::AppBuffer header;
        if (boost::algorithm::iends_with(uri.getfile(), ".mp4"))
        {
#undef PRINT_MP4_HEADER
// #define PRINT_MP4_HEADER
#ifdef PRINT_MP4_HEADER
            static int fid = 0;
            {
                ++fid;
                string filename = boost::lexical_cast<string>(fid) + "_head_" + boost::lexical_cast<string>(key_frame_position) + "_source.mp4";
                FILE * f = fopen(filename.c_str(), "wb");
                if (f != NULL) {
                    for (uint32_t i = 0; i < header_buffer.Length();) {
                        uint32_t c = fwrite(header_buffer.Data() + i, sizeof(boost::uint8_t), header_buffer.Length() - i, f);
                        i += c;
                    }
                    fclose(f);
                    f = NULL;
                }
            }
#endif
            LOGX(__EVENT, "proxy", "key_frame_position = " << key_frame_position << ", source_headlength = " << header_buffer.Length());
            header = Mp4Spliter::Mp4HeadParse(header_buffer, key_frame_position);
            LOGX(__EVENT, "proxy", "key_frame_position = " << key_frame_position << ", generated_headlength = " << header.Length());
            // 这里判断一下返回值，如果失败直接返回
            if (0 == header.Length())
            {
                return base::AppBuffer();
            }

#ifdef PRINT_MP4_HEADER
            {
                string filename = boost::lexical_cast<string>(fid) + "_head_" + boost::lexical_cast<string>(key_frame_position) + "_generate.mp4";
                FILE * f = fopen(filename.c_str(), "wb");
                if (f != NULL) {
                    for (uint32_t i = 0; i < header.Length();) {
                        uint32_t c = fwrite(header.Data() + i, sizeof(boost::uint8_t), header.Length() - i, f);
                        i += c;
                    }
                    fclose(f);
                    f = NULL;
                }
            }
#endif
        }
        else if (boost::algorithm::iends_with(uri.getfile(), ".flv"))
        {
            static boost::uint8_t flv_header[] = {0x46, 0x4C, 0x56, 0x01, 0x05, 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00};
            if (header_buffer.Length() == 0) {
                return base::AppBuffer(flv_header, sizeof(flv_header));
            }
            else {
                return header_buffer;
            }
        }
        else
        {
            // !
            assert(!"Not mp4 or flv");
        }

        return header;
    }

    void ProxyConnection::OnNoticeDownloadMode(const string& url, const string& refer_url, const string& user_agent, const string& qualifed_file_name)
    {
        if (false == is_running_) {
            return;
        }


        LOGX(__DEBUG, "downloadcenter", "SaveMode = " << save_mode_);
        if (true == save_mode_)
        {
            source_url_ = url;
            qualified_file_name_ = qualifed_file_name;

            network::Uri uri(url);
            string port = framework::string::format(ProxyModule::Inst()->GetHttpPort());
            string request =
                "GET " + uri.getrequest() + " HTTP/1.1\r\n"
                "Host: 127.0.0.1:" + port + "\r\n"
                "Referer: " + boost::algorithm::replace_all_copy(refer_url, "\r\n", "") + "\r\n"
                "User-Agent: " + boost::algorithm::replace_all_copy(user_agent, "\r\n", "") + "\r\n"
                "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n"
                "Connection: close\r\n"
                "\r\n";
            network::HttpRequest::p http_request = network::HttpRequest::ParseFromBuffer(request);

            if (http_request)
            {
                LOGX(__DEBUG, "downloadcenter", "SourceUrl = " << source_url_);
                OnHttpRecvSucced(http_request);
            }
            else
            {
                LOGX(__DEBUG, "downloadcenter", "network::HttpRequest Parse Error!");
                return;
            }

            if (false == will_stop_)
            {
                protocol::UrlInfo url_info = download_driver_->GetOriginalUrlInfo();
                LOG(__DEBUG, "downloadcenter", __FUNCTION__ << ":" << __LINE__ << " SourceUrl = " << source_url_
                    << ", Url = " << url_info.url_
                    << ", FileName = " << (qualified_file_name_));
#ifdef DISK_MODE
                if (qualified_file_name_.length() > 0) {
                    storage::Storage::Inst()->AttachSaveModeFilenameByUrl(url_info.url_, "", qualified_file_name_);
                }
#endif  // #ifdef DISK_MODE
            }
            else
            {
                LOG(__DEBUG, "downloadcenter", __FUNCTION__ << ":" << __LINE__ << " will_stop_ = true");
            }
        }
        else {
            assert(!"Should be SaveMode!!");
        }
    }

    void ProxyConnection::OnNoticeDownloadFileByRid(const protocol::RidInfo& rid_info, const protocol::UrlInfo& url_info, protocol::TASK_TYPE task_type, bool is_push)
    {
        if (false == is_running_) {
            return;
        }
        if (true == save_mode_) {
            proxy_sender_ = NullProxySender::create(shared_from_this());
            download_driver_ = DownloadDriver::create(io_svc_, shared_from_this());
            download_driver_->SetIsPush(is_push);
            if(is_push) 
            {
                //push任务如果没有设置source_type，source_type默认为255
                //它的dac日志会被上报到这个域名：upload-va.synacast.com
                //而source_type为0的日志，会被上报到pplive-va.synacast.com 这个域名。
                //而客户端被观看了的的push任务（不等于被下载的push任务），会上报到pplive-va.synacast.com。
                //为了方便统计，这两种情况（被下载和被观看的push任务）都应该上报到同一个域名，因此这里做出修改。
                download_driver_->SetSourceType(PlayInfo::SOURCE_PPLIVE); 
            }

            protocol::RidInfo local_rid_info = rid_info;
            if (false == local_rid_info.HasRID()) 
            {
                storage::Instance::p temp_inst = boost::static_pointer_cast<storage::Instance>(storage::Storage::Inst()->CreateInstance(url_info));
                if (temp_inst->HasRID()) 
                {
                    temp_inst->GetRidInfo(local_rid_info);
                }
            }

            proxy_sender_->Start();

            if (local_rid_info.HasRID())
            {
                download_driver_->SetRidInfo(local_rid_info);
            }

            SwitchController::ControlModeType mode;
            if (protocol::TASK_OPEN_SERVICE == task_type) {
                mode = SwitchController::CONTROL_MODE_PUSH_OPENSERVICE;
                download_driver_->Start(url_info, false, true, mode);
            }
            else if (/*TASK_TEST_OPENSERVICE*/2 == task_type) {
                mode = SwitchController::CONTROL_MODE_DOWNLOAD;
                download_driver_->Start(url_info, false, true, mode, 3/*FORCE_MODE_P2P_TEST*/);
            }
            else {
                mode = SwitchController::CONTROL_MODE_DOWNLOAD;
                download_driver_->Start(url_info, false, false, mode);
            }

            string file_name = ProxyModule::ParseOpenServiceFileName(network::Uri(url_info.url_));
#ifdef DISK_MODE
            storage::Storage::Inst()->AttachSaveModeFilenameByUrl(url_info.url_, "", file_name);
#endif  // #ifdef DISK_MODE
        }
    }

    string ProxyConnection::GetSourceUrl() const
    {
        if (false == is_running_) {
            return "";
        }
        return source_url_;
    }

    protocol::UrlInfo ProxyConnection::GetOriginalUrlInfo() const
    {
        if (false == is_running_) {
            return protocol::UrlInfo();
        }
        if (!download_driver_) {
            return protocol::UrlInfo();
        }
        return download_driver_->GetOriginalUrlInfo();
    }

    void ProxyConnection::OnNoticeChangeProxySender(ProxySender__p proxy_sender)
    {
        if (false == is_running_) {
            return;
        }
        proxy_sender_ = proxy_sender;
    }

    void ProxyConnection::OnNoticeStopDownloadDriver()
    {
        if (false == is_running_) {
            LOGX(__WARN, "proxy", "Not Running!!");
            return;
        }
        LOGX(__WARN, "proxy", "PlayingPosition = " << proxy_sender_->GetPlayingPosition() << ", FileLength = " << file_length_ << ", DownloadDriver = " << download_driver_);
        if (proxy_sender_->GetPlayingPosition() >= file_length_)
        {
            LOGX(__WARN, "proxy", "PlayingPosition >= file_length_, download_driver = " << download_driver_);
            StopDownloadDriver();
        }
    }

    void ProxyConnection::SetSendSpeedLimit(const boost::int32_t send_speed_limit)
    {
        send_speed_limit_ = send_speed_limit;
    }

    bool ProxyConnection::IsHeaderResopnsed()
    {
        return proxy_sender_->IsHeaderResopnsed();
    }

    // 直播请求的处理
    void ProxyConnection::OnLiveRequest(PlayInfo::p play_info)
    {
        LOG(__DEBUG, "proxy", "Recv live request start:" <<
            play_info->GetLiveStart() << ", interval: " << play_info->GetLiveStart());

        is_live_connection_ = true;

        proxy_sender_ = LiveProxySender::create(http_server_socket_);
        proxy_sender_->Start();

        boost::uint32_t time_elapsed_since_stop;
        bool is_two_vv_of_same_channel_too_near = false;

        if (ProxyModule::Inst()->TryGetTimeElapsedSinceStop(play_info->GetChannelID(), time_elapsed_since_stop))
        {
            if (time_elapsed_since_stop < BootStrapGeneralConfig::Inst()->GetIntervalOfTwoVVDelim())
            {
                is_two_vv_of_same_channel_too_near = true;
            }
        }

        live_download_driver_ = LiveDownloadDriver::create(io_svc_, shared_from_this());
        live_download_driver_->Start(play_info->GetUrlInfo().url_, play_info->GetLiveRIDs(),
            play_info->GetLiveStart(), play_info->GetLiveInterval(), play_info->IsLiveReplay(), play_info->GetDataRates(),
            play_info->GetChannelID(), static_cast<uint32_t>(play_info->GetSourceType()), (JumpBWType)play_info->GetBWType(), play_info->GetUniqueID(),
            is_two_vv_of_same_channel_too_near);

        if (play_info->GetRestTimeInMillisecond() > 0)
        {
            live_download_driver_->SetRestTimeInSecond(play_info->GetRestTimeInMillisecond() / 1000);
        }
    }

    void ProxyConnection::OnLivePause(const RID & rid, bool pause, boost::uint32_t unique_id)
    {
        ProxyModule::Inst()->OnLivePause(rid, pause, unique_id);

        proxy_sender_ = LiveProxySender::create(http_server_socket_);
        proxy_sender_->Start();

        // 成功收到通知暂停的请求
        proxy_sender_->OnNoticeGetContentLength(0, network::HttpResponse::p());
        // 断开连接
        WillStop();
    }

    // 直播收到数据
    bool ProxyConnection::OnRecvLivePiece(uint32_t block_id, std::vector<base::AppBuffer> const & buffers)
    {
        // TODO: proxy_sender_->OnRecvLivePiece(block_id, buffers);
        // 这里我打算把直播和点播的收数据统一起来，所以暂时先没写
        proxy_sender_->OnRecvSubPiece(block_id, buffers);

        LOG(__DEBUG, "proxy", "OnRecvLivePiece piece:" << block_id << ", buffer size:" << buffers.size());

        return true;
    }

    boost::uint32_t ProxyConnection::GetLiveRestPlayableTime() const
    {
        return live_download_driver_ ? live_download_driver_->GetRestPlayableTime() : 0;
    }

    boost::uint8_t ProxyConnection::GetLostRate() const
    {
        return live_download_driver_ ? live_download_driver_->GetLostRate() : 0;
    }

    boost::uint8_t ProxyConnection::GetRedundancyRate() const
    {
        return live_download_driver_ ? live_download_driver_->GetRedundancyRate() : 0;
    }

    boost::uint32_t ProxyConnection::GetLiveTotalRequestSubPieceCount() const
    {
        return live_download_driver_ ? live_download_driver_->GetTotalRequestSubPieceCount() : 0;
    }

    boost::uint32_t ProxyConnection::GetLiveTotalRecievedSubPieceCount() const
    {
        return live_download_driver_ ? live_download_driver_->GetTotalRecievedSubPieceCount() : 0;
    }

    
}
