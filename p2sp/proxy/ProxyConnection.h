//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// ProxyConnection.h

#ifndef _P2SP_PROXY_PROXYCONNECTION_H_
#define _P2SP_PROXY_PROXYCONNECTION_H_

#include "storage/IStorage.h"
#include "p2sp/proxy/PlayInfo.h"
#include "network/HttpAcceptor.h"
#include "network/HttpRequest.h"
#include "network/HttpResponse.h"
#include "network/HttpServer.h"
#include "p2sp/push/PlayHistoryManager.h"

namespace p2sp
{
    class LiveDownloadDriver;
    typedef boost::shared_ptr<LiveDownloadDriver> LiveDownloadDriver__p;

    struct ProxyType
    {
        PlayInfo::PlayType play_type;
        string player_id;

        ProxyType() : play_type(PlayInfo::PLAY_BY_NONE){}
        ProxyType(const PlayInfo::PlayType& pt, const string& pid)
            : play_type(pt), player_id(pid) {}

        bool operator == (const ProxyType& other) const
        {
            return (play_type == other.play_type)
                && (player_id == other.player_id)
                && player_id.length() != 0;
        }
    };

    inline std::ostream& operator << (std::ostream& output, const ProxyType& pt)
    {
        output << "[PlayType: \"" << PlayInfo::PlayType2Str(pt.play_type)
            << "\", PlayerID: " << pt.player_id << "]";
        return output;
    }

    // herain:2011-1-4:对deque<SubPieceBuffer>进行了简单的封装，用于统计deque中所有
    // SubPieceBuffer的长度
    class SubPieceBufferDeque
    {
    public:
        SubPieceBufferDeque()
        {
        }

        void push_back(boost::uint32_t postion, const base::AppBuffer &buf)
        {
            map_.insert(std::make_pair(postion, buf));
        }

        std::map<boost::uint32_t, base::AppBuffer>::const_reference front() const
        {
            return *(map_.begin());
        }

        void pop_front()
        {
            map_.erase(map_.begin());
        }

        uint32_t size() const
        {
            return map_.size();
        }

        bool empty() const
        {
            return map_.empty();
        }

        // 返回当前的播放位置
        int32_t GetPlayingPostion(boost::uint32_t position) const
        {
            std::map<boost::uint32_t, base::AppBuffer>::const_iterator iter = map_.find(position);
            if (iter != map_.end())
            {
                for (; iter != map_.end(); ++iter)
                {
                    // 当缓存数据的开始等于目前的播放位置
                    // 可以把播放位置往后移，因为这些数据下一时刻被会发送
                    if (iter->first == position)
                    {
                        position += iter->second.Length();
                    }
                }
            }
            
            return position;
        }

    private:
        std::map<boost::uint32_t, base::AppBuffer> map_;
    };

    class DownloadDriver;
    typedef boost::shared_ptr<DownloadDriver> DownloadDriver__p;

    class ProxySender;
    typedef boost::shared_ptr<ProxySender> ProxySender__p;

    class PlayInfo;
    typedef boost::shared_ptr<PlayInfo> PlayInfo__p;

    class ProxyConnection
        : public boost::noncopyable
        , public boost::enable_shared_from_this<ProxyConnection>
        , public network::IHttpServerListener
#ifdef DUMP_OBJECT
        , public count_object_allocate<ProxyConnection>
#endif
    {
    public:
        typedef boost::shared_ptr<ProxyConnection> p;
        static p create(
            boost::asio::io_service & io_svc,
            network::HttpServer::pointer http_server_socket)
        {
            return p(new ProxyConnection(io_svc, http_server_socket));
        }
        static p create(
            boost::asio::io_service & io_svc)
        {
            return p(new ProxyConnection(io_svc));
        }

    public:
        void Start();
        void Stop();
        void WillStop();
    public:
        virtual uint32_t GetPlayingPosition() const;
        virtual void SendHttpRequest();
        virtual void ResetPlayingPostion();
        virtual bool IsWillStop() const { return will_stop_; }
        virtual bool IsWillStopDownload() const { return will_stop_download_; }
        virtual bool IsSaveMode() const { return save_mode_; }
        virtual void OnRecvSubPiece(uint32_t position, std::vector<base::AppBuffer> const & buffers);
    public:
        // network::HttpServer
        virtual void OnHttpRecvSucced(network::HttpRequest::p http_request);
        virtual void OnHttpRecvFailed(uint32_t error_code);
        virtual void OnHttpRecvTimeout();
        virtual void OnTcpSendFailed();
        virtual void OnClose();
        void OnProxyTimer(uint32_t times);
        virtual void OnNoticeGetContentLength(uint32_t content_length, network::HttpResponse::p http_response);
        virtual void OnNoticeDirectMode(DownloadDriver__p download_driver);
        virtual void OnNoticeDownloadMode(const string& url, const string& refer_url, const string& web_url, const string& qualifed_file_name);

        virtual void OnNoticeDownloadFileByRid(const protocol::RidInfo& rid_info, const protocol::UrlInfo& url_info, protocol::TASK_TYPE task_type, bool is_push = false);

        // IHttpFetchListener
        // virtual void OnFetchComplete(protocol::SubPieceContent buffer);
        // virtual void OnFetchFailed(uint32_t error_code);
        virtual void OnNoticeOpenServiceHeadLength(uint32_t head_length);

        //
        virtual void OnNoticeChangeProxySender(ProxySender__p proxy_sender);

        virtual void OnNoticeStopDownloadDriver();

        // Header
        virtual base::AppBuffer GetHeader(uint32_t& key_frame_position, base::AppBuffer const & header_buffer);
        // void NotifyFetchHeader();
        // bool IsHeaderPrepared() const { return is_header_prepared_; }

        virtual protocol::UrlInfo GetUrlInfo() const { return url_info_; }

        virtual DownloadDriver__p GetDownloadDriver() { return download_driver_; }

        virtual string GetSourceUrl() const;
        virtual protocol::UrlInfo GetOriginalUrlInfo() const;

        void StopDownloadDriver();

        ProxyType GetProxyType() const { return proxy_type_; }

        PlayInfo::p GetPlayInfo() const { return play_info_; }

        bool IsMovieUrl() const { return is_movie_url_; }

        void SetSendSpeedLimit(const boost::int32_t send_speed_limit);

        bool IsHeaderResopnsed();

        // 直播收到数据
        bool OnRecvLivePiece(uint32_t block_id, std::vector<base::AppBuffer> const & buffers);

        bool IsLiveConnection() const
        {
            return is_live_connection_;
        }

        LiveDownloadDriver__p GetLiveDownloadDriver()
        {
            return live_download_driver_;
        }

        boost::uint32_t GetLiveRestPlayableTime() const;
        boost::uint8_t GetLostRate() const;
        boost::uint8_t GetRedundancyRate() const;
        boost::uint32_t GetLiveTotalRequestSubPieceCount() const;
        boost::uint32_t GetLiveTotalRecievedSubPieceCount() const;

        void StopEstimateIkanRestPlayTime() { need_estimate_ikan_rest_play_time_ = false; }

        boost::uint32_t GetSendPendingCount()
        {
            if(!http_server_socket_)
                return 0;
            else
            {
                return http_server_socket_->GetSendPendingCount();
            }
        }

    protected:
        virtual void initialize();
        virtual void clear();

    private:
        // 直播请求的处理函数
        void OnLiveRequest(PlayInfo::p play_info);
        void OnLivePause(const RID & rid, bool pause, boost::uint32_t unique_id);
        void OnUrlInfoRequest(const protocol::UrlInfo& url_info, const protocol::RidInfo& rid_info, network::HttpRequest::p http_request);
        void OnOpenServiceRequest(PlayInfo::p play_info);
        void OnDownloadByUrlRequest(PlayInfo::p play_info);

    private:
        void CheckDeath();

    private:
        boost::asio::io_service & io_svc_;
        network::HttpServer::pointer http_server_socket_;
        network::HttpRequest::p http_request_demo_;
        // framework::timer::OnceTimer subpiece_fail_timer_;
        DownloadDriver__p download_driver_;

        // 直播的DownloadDriver
        LiveDownloadDriver__p live_download_driver_;

        ProxySender__p proxy_sender_;
        // framework::timer::PeriodicTimer play_timer_;

        volatile bool is_running_;

        protocol::UrlInfo url_info_;

        uint32_t file_length_;
        uint32_t openservice_head_length_;

        bool metadata_parsed_;

        framework::timer::TickCounter silent_time_counter_;

        base::AppBuffer header_buffer_;
        uint32_t header_buffer_length_;

        bool will_stop_;
        bool will_stop_download_;

        bool save_mode_;

        bool is_movie_url_;

        string source_url_;
        string qualified_file_name_;

        ProxyType proxy_type_;
        PlayInfo::p play_info_;

        boost::int32_t rest_time;

        boost::int32_t send_count_;
        boost::int32_t send_speed_limit_;
        SubPieceBufferDeque buf_deque_;

        bool is_live_connection_;
#ifdef DISK_MODE
        //PlayHistoryManager play_history_mgr_;
        PlayHistoryManager::PlayHistoryItemHandle play_history_item_handle_;
#endif

        // TODO(herain):在flash p2p播放器全部上线后可以删掉估算相关的代码
        // 是否需要估算ikan player的剩余缓冲时间        
        bool need_estimate_ikan_rest_play_time_;
    private:
        ProxyConnection(
            boost::asio::io_service & io_svc,
            network::HttpServer::pointer http_server_socket);

        ProxyConnection(
            boost::asio::io_service & io_svc);

    private:
        static const uint32_t DEFAULT_SILENT_TIME_LIMIT = 300*1000;
        static const uint32_t HEADER_LENGTH = 2048;
    };
}

#endif  // _P2SP_PROXY_PROXYCONNECTION_H_
