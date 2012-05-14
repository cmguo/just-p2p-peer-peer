//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// ProxyModule.h

#ifndef _P2SP_PROXY_PROXY_MODULE_H_
#define _P2SP_PROXY_PROXY_MODULE_H_

#include "network/HttpAcceptor.h"
#include "network/Uri.h"
#include "struct/RidInfo.h"
#include "struct/Structs.h"
#include "peer.h"

#include <boost/function.hpp>

namespace storage
{
    class LiveInstance;
}

namespace p2sp
{
    class ProxyConnection;
    typedef boost::shared_ptr<ProxyConnection> ProxyConnection__p;

    struct ProxyType;

    class ProxyModule
        : public boost::noncopyable
        , public boost::enable_shared_from_this<ProxyModule>
        , public network::IHttpAcceptorListener
#ifdef DUMP_OBJECT
        , public count_object_allocate<ProxyModule>
#endif
    {
        friend class AppModule;
    public:
        typedef boost::shared_ptr<ProxyModule> p;
    public:
        // 方法
        void Start(const string& config_path, boost::uint16_t local_http_proxy_port);
        void Stop();
        bool IsRunning() const { return is_running_; }
        // 属性
        boost::uint16_t GetHttpPort() const;

        // 消息
        void OnHttpAccept(boost::shared_ptr<network::HttpServer> http_server_for_accept);
        void OnHttpAcceptFailed();
        void RemoveProxyConnection(boost::shared_ptr<ProxyConnection> server_socket);
        // 定时器
        virtual void OnTimerElapsed(framework::timer::Timer * pointer);
        void OnProxyTimer(uint32_t times);
        void OnUdpRecv(protocol::Packet const & packet);

        //
        void StartDownloadFile(const string& url, const string& refer_url, const string& web_url, const string& qualified_file_name);

        ProxyConnection__p GetProxyConnection(const string& url);
        void StopProxyConnection(const string& url);
        void ForEachProxyConnection(boost::function<void(ProxyConnection__p)> processor);
        void StartDownloadFileByRid(
            const protocol::RidInfo& rid_info,
            const protocol::UrlInfo& url_info,
            protocol::TASK_TYPE task_type,
            bool is_push = false);

        void StartDownloadFileByRidWithTimeout(const protocol::RidInfo& rid_info, const protocol::UrlInfo& url_info, boost::uint32_t timeout_sec);

        void LimitDownloadSpeedInKBps(const string& url, boost::int32_t speed_limit_KBps);

        void StopProxyDownload(const ProxyType& proxy_type, ProxyConnection__p proxy_connection);
        void StopProxyConnection(const ProxyType& proxy_type, ProxyConnection__p proxy_connection);

        int GetProxyConnectionSize()
        {
            return proxy_connections_.size();
        }

        void QueryDownloadProgress(RID rid, boost::function<void()> result_handler, boost::int32_t *file_length, boost::int32_t *download_bytes);
        void QueryDownloadProgressByUrl(string url, boost::int32_t * file_length, boost::int32_t * downloaded_bytes,
            boost::int32_t * position, boost::function<void ()> result_handler);
        void QueryDownloadSpeed(RID rid, boost::function<void()> result_handler, boost::int32_t *download_speed);
        void QueryDownloadSpeedByUrl(string url, boost::function<void()> result_handler, boost::int32_t *download_speed);

        void QueryProgressBitmap(string url, char * bitmap, boost::uint32_t * bitmap_size,
            boost::function<void ()> result_handler);

        void QueryDownloadProgress2(string url, boost::uint32_t start_pos, boost::uint32_t * last_pos,
            boost::function<void ()> result_handler);

        void SetRestPlayTime(RID rid, boost::uint32_t rest_play_time);
        void SetRestPlayTimeByUrl(string url, boost::uint32_t rest_play_time_in_millisecond);
        void SetDownloadMode(RID rid, boost::uint32_t download_mode);
        void SetDownloadModeByUrl(string url, boost::uint32_t download_mode);

        void QueryPeerStateMachine(RID rid, 
            boost::function<void()> result_handler,PEERSTATEMACHINE *peer_state);
        void QueryPeerStateMachineByUrl(const char * url, 
            boost::function<void()> result_handler, PEERSTATEMACHINE *peer_state);
        void QueryDragState(RID rid, boost::int32_t *state, boost::function<void ()> fun);
        void QueryDragStateByUrl(const char * url, boost::int32_t *state, boost::function<void ()> fun);
        bool IsHttpDownloading();
        bool IsP2PDownloading();
        bool IsWatchingMovie();
        bool IsDownloadingMovie();

        bool IsDownloadWithSlowMode();
        int GetLastSegno(string sessionid);
        void SetSegno(string sessionid, int segno);
        void ExpireSegno();

        void SetLastDragPrecent(uint32_t drag_precent);
        uint32_t GetDragPrecent();

        boost::uint32_t GetHistoryMaxDwonloadSpeed() {return history_max_download_speed_ini_;}
        void LoadHistoricalMaxDownloadSpeed();
        void SaveHistoricalMaxDownloadSpeed();


        // 设置内核推送数据的速度
        void SetSendSpeedLimitByUrl(string url, boost::int32_t send_speed_limit);
        // 全局限速管理
        void GlobalSpeedLimit();

        // 查询某个rid的校验失败的次数
#ifdef DISK_MODE
        void GetBlockHashFailed(const RID & rid, 
            boost::int32_t * failed_num, boost::function<void ()> fun);
        void GetBlockHashFailedByUrl(const char * url, 
            boost::int32_t * failed_num, boost::function<void ()> fun);
#endif
#ifdef PEER_PC_CLIENT
        void GetCompeletedFilePath(const RID & rid, string & file_path, boost::function<void ()> fun);
        void GetCompeletedFilePathByUrl(const char * url, string & file_path, boost::function<void ()> fun);
#endif

        static string ParseOpenServiceFileName(const network::Uri & uri);

        bool IsWatchingLive();
        void OnLivePause(const RID & channel_id, bool pause, boost::uint32_t unique_id);

        boost::uint32_t GetLiveRestPlayableTime() const;
        boost::uint8_t GetLostRate() const;
        boost::uint8_t GetRedundancyRate() const;

        void SetReceiveConnectPacket(boost::shared_ptr<storage::LiveInstance> instance);
        void SetSendSubPiecePacket(boost::shared_ptr<storage::LiveInstance> instance);

        const std::string & GetConfigPath() const
        {
            return ppva_config_path_;
        }

        void UpdateStopTime(const RID & channel_id);
        bool TryGetTimeElapsedSinceStop(const RID & channel_id, boost::uint32_t & time_elapsed) const;

    private:
        boost::asio::io_service & io_svc_;

        network::HttpAcceptor::pointer acceptor_;
        network::HttpAcceptor::pointer acceptor_place_holder_;
        std::set<ProxyConnection__p> proxy_connections_;

        framework::timer::PeriodicTimer proxy_timer_;
        framework::timer::TickCounter speed_query_counter_;

        std::map<string, std::pair<int, time_t> > drag_record_;

        uint32_t last_drag_precent;

        string ppva_config_path_;
        boost::uint32_t local_ip_from_ini_;
        boost::uint32_t history_max_download_speed_ini_;
        boost::uint32_t history_max_download_speed_;

        string last_session_id_;
        int last_rest_time_;
        // 状态
        volatile bool is_running_;

        std::map<RID, boost::uint32_t> time_elapsed_since_stop_;
        framework::timer::TickCounter tick_counter_;

    private:
        static ProxyModule::p inst_;
        ProxyModule(boost::asio::io_service & io_svc);

    public:
        static ProxyModule::p CreateInst(boost::asio::io_service & io_svc)
        {
            inst_.reset(new ProxyModule(io_svc));
            return inst_;
        }

        static ProxyModule::p Inst() { return inst_; }
        static string RemovePpvakeyFromUrl(const string& url);
    };
}

#endif  // _P2SP_PROXY_PROXY_MODULE_H_
