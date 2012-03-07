//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// P2PModule.h

#ifndef _P2SP_P2P_P2PMODULE_H_
#define _P2SP_P2P_P2PMODULE_H_

#include <protocol/PeerPacket.h>
#include <p2sp/bootstrap/BootStrapGeneralConfig.h>
#include <p2sp/p2p/UploadModule.h>

namespace storage
{
    class LiveInstance;
    typedef boost::shared_ptr<LiveInstance> LiveInstance__p;
}

namespace p2sp
{
    class P2PDownloader;
    typedef boost::shared_ptr<P2PDownloader> P2PDownloader__p;

    class LiveP2PDownloader;
    typedef boost::shared_ptr<LiveP2PDownloader> LiveP2PDownloader__p;

    class SessionPeerCache;
    typedef boost::shared_ptr<SessionPeerCache> SessionPeerCache__p;

    class P2PModule
        : public boost::noncopyable
        , public boost::enable_shared_from_this<P2PModule>
        , public ConfigUpdateListener
#ifdef DUMP_OBJECT
        , public count_object_allocate<P2PModule>
#endif
    {
        friend class AppModule;
    public:
        typedef boost::shared_ptr<P2PModule> p;
    public:
        // 启动 停止
        void Start(const string& config_path);
        void Stop();

        // 操作
        P2PDownloader__p CreateP2PDownloader(const RID& rid, boost::uint32_t vip);
        // 消息
        void SetMaxUploadSpeedInKBps(boost::int32_t MaxUploadP2PSpeed);

        boost::int32_t GetUploadSpeedLimitInKBps() const;
        boost::int32_t GetMaxConnectLimitSize() const;
        boost::int32_t GetMaxUploadLimitSize() const;

        void OnUdpRecv(protocol::Packet const & packet);
        void OnTimerElapsed(framework::timer::Timer * pointer);
        void OnP2PTimer(uint32_t times);

        void AddCandidatePeers(RID rid, const std::vector<protocol::CandidatePeerInfo>& peers, bool is_live_udpserver);

        void OnP2PDownloaderWillStop(P2PDownloader__p p2p_downloader);

        // Window Size
        bool CanRequest() const { return request_count_ < window_size_; }
        void AddRequestCount() { ++request_count_; ++sent_count_; global_request_send_count_++;}
        void RemoveRequestCount() { if (request_count_ > 0) -- request_count_; }
        void RemoveRequestCount(uint32_t delta_request_count) { if (delta_request_count <= request_count_) request_count_ -= delta_request_count; else request_count_ = 0; }

        // PeerCountInfo
        P2PDownloader__p GetP2PDownloader(const RID& rid);
        SessionPeerCache__p GetSessionPeerCache() const {return session_cache_;}

        // 设置上传开关，用于控制是否启用上传
        void SetUploadSwitch(bool is_disable_upload);

        boost::uint32_t GetUploadBandWidthInBytes();
        boost::uint32_t GetUploadBandWidthInKBytes();

        bool NeedUseUploadPingPolicy();

        // Live
        // 创建直播的P2PDownloader
        void OnLiveP2PDownloaderCreated(LiveP2PDownloader__p live_p2p_downloader);
        void OnLiveP2PDownloaderDestroyed(LiveP2PDownloader__p live_p2p_downloader);

        virtual void OnConfigUpdated();

        bool IsConnectionPolicyEnable();

        boost::uint32_t GetDownloadConnectedCount() const;

    private:
        // 变量
        typedef std::map<RID, P2PDownloader__p> RIDIndexerMap;
        RIDIndexerMap rid_indexer_;
        boost::shared_ptr<UploadModule> upload_module_;
        SessionPeerCache__p session_cache_;
        framework::timer::PeriodicTimer p2p_timer_;
        // 状态
        volatile bool is_running_;
        // 全局窗口
        uint32_t window_size_;
        uint32_t request_count_;
        uint32_t sent_count_;
        uint32_t max_download_speed_;

        boost::uint16_t  global_request_send_count_;

        // 直播索引，直播的LiveP2PDownloader集合
        std::multimap<RID, LiveP2PDownloader__p> live_rid_index_;

        bool is_connection_policy_enable_;

    private:
        static P2PModule::p inst_;
        P2PModule();

    public:
        static P2PModule::p Inst()
        {
            if (!inst_)
            {
                inst_.reset(new P2PModule());
            }
            return inst_; 
        }
    };
}
#endif  // _P2SP_P2P_P2PMODULE_H_
