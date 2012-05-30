//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _STATISTIC_STATISTIC_MODULE_H_
#define _STATISTIC_STATISTIC_MODULE_H_

#include "statistic/SpeedInfoStatistic.h"
#include "statistic/P2PDownloaderStatistic.h"
#include "statistic/DownloadDriverStatistic.h"
#include "statistic/LiveDownloadDriverStatistic.h"
#include "statistic/StatisticStructs.h"

namespace statistic
{
    class StatisticModule
        : public boost::noncopyable
        , public boost::enable_shared_from_this<StatisticModule>
    {
    public:

        typedef boost::shared_ptr<StatisticModule> p;

    public:

        void Start(
            boost::uint32_t flush_interval_in_seconds,  // 写入共享内存时间周期(ms)
            string config_path);

        void Stop();

        DownloadDriverStatistic::p AttachDownloadDriverStatistic(uint32_t id, bool is_create_shared_memory = true);
        LiveDownloadDriverStatistic::p AttachLiveDownloadDriverStatistic(uint32_t id);

        bool DetachLiveDownloadDriverStatistic(const LiveDownloadDriverStatistic::p live_download_driver_statistic);
        bool DetachDownloadDriverStatistic(const DownloadDriverStatistic::p download_driver_statistic);

        P2PDownloaderStatistic::p AttachP2PDownloaderStatistic(const RID& rid);

        bool DetachP2PDownloaderStatistic(const P2PDownloaderStatistic::p p2p_downloader_statistic);

        // framework::timer::Timer
        void OnShareMemoryTimer(uint32_t times);

        bool IsRunning() const
        {
            return is_running_;
        }

        void TakeSnapshot(STASTISTIC_INFO& statistics,
            std::vector<DOWNLOADDRIVER_STATISTIC_INFO>& download_drivers_statistics,
            std::vector<LIVE_DOWNLOADDRIVER_STATISTIC_INFO>& live_download_drivers_statistics,
            std::vector<P2PDOWNLOADER_STATISTIC_INFO>& p2p_downloaders_statistics);

    public:

        //////////////////////////////////////////////////////////////////////////
        // Statistic Info Snapshot
        STASTISTIC_INFO GetStatisticInfoSnapshot() const { return statistic_info_; }

        //////////////////////////////////////////////////////////////////////////
        // Statistic Info

        STASTISTIC_INFO GetStatisticInfo();

        //////////////////////////////////////////////////////////////////////////
        // Speed Info

        void SubmitDownloadedBytes(uint32_t downloaded_bytes);

        void SubmitUploadedBytes(uint32_t uploaded_bytes);

        SPEED_INFO GetSpeedInfo();

        SPEED_INFO_EX GetSpeedInfoEx();

        uint32_t GetMaxHttpDownloadSpeed() const;

        uint32_t GetTotalDownloadSpeed();

        uint32_t GetBandWidth();
        int GetBandWidthInKBps();

        uint32_t GetUploadBandWidth();

        //////////////////////////////////////////////////////////////////////////
        // Upload

        void SubmitUploadDataBytes(uint32_t uploaded_bytes);

        uint32_t GetUploadDataSpeed();
        uint32_t GetUploadDataSpeedInKBps();
        uint32_t GetRecentMinuteUploadDataSpeedInKBps();
        uint32_t GetMinuteUploadDataSpeed();
        uint32_t GetUploadDataBytes() const;

        //////////////////////////////////////////////////////////////////////////
        // Local Download Info

        protocol::PEER_DOWNLOAD_INFO GetLocalPeerDownloadInfo();

        protocol::PEER_DOWNLOAD_INFO GetLocalPeerDownloadInfo(const RID& rid);

        //////////////////////////////////////////////////////////////////////////
        // 设置 和 获取 IP Info

        void SetLocalPeerInfo(const protocol::CandidatePeerInfo& local_peer_info);

        protocol::CandidatePeerInfo GetLocalPeerInfo();

        // 设置 和 获取 Local PeerInfo 中的 Local 部分

        void SetLocalPeerIp(uint32_t ip);

        void SetLocalPeerUdpPort(boost::uint16_t udp_port);

        void SetLocalPeerTcpPort(boost::uint16_t tcp_port);

        void SetLocalPeerAddress(const protocol::PeerAddr& peer_addr);

        boost::uint16_t GetLocalPeerTcpPort();

        protocol::PeerAddr GetLocalPeerAddress();

        // 设置 和 获取 Local PeerInfo 中的 Detect 部分

        void SetLocalDetectSocketAddress(const protocol::SocketAddr& socket_addr);

        protocol::SocketAddr GetLocalDetectSocketAddress();

        // 设置 和 获取 Local PeerInfo 中的 Stun 部分

        void SetLocalStunSocketAddress(const protocol::SocketAddr& socket_addr);

        protocol::SocketAddr GetLocalStunSocketAddress();

        void SetLocalNatType(boost::uint8_t nat_type);

        boost::uint8_t GetLocalNatType();

        void SetLocalUploadPriority(boost::uint8_t upload_priority);

        boost::uint8_t GetLocalUploadPriority();

        void SetLocalIdleTime(boost::uint8_t idle_time_in_min);

        boost::uint8_t GetLocalIdleTime();

        void SetLocalIPs(const std::vector<uint32_t>& local_ips);

        void GetLocalIPs(std::vector<uint32_t>& local_ips);

        //////////////////////////////////////////////////////////////////////////
        // Peer Info

        void SetLocalPeerVersion(uint32_t local_peer_version);

        //////////////////////////////////////////////////////////////////////////
        // Bootstrap Server

        void SetBsInfo(const boost::asio::ip::udp::endpoint bs_ep);

        //////////////////////////////////////////////////////////////////////////
        // Stun Server

        void SetStunInfo(const vector<protocol::STUN_SERVER_INFO>& stun_infos);
        //////////////////////////////////////////////////////////////////////////
        // Tracker Server

        void SetTrackerInfo(uint32_t group_count, const std::vector<protocol::TRACKER_INFO>& tracker_infos);

        void SetIsSubmitTracker(const protocol::TRACKER_INFO& tracker_info, bool is_submit_tracker);

        void SubmitCommitRequest(const protocol::TRACKER_INFO& tracker_info);

        void SubmitCommitResponse(const protocol::TRACKER_INFO& tracker_info);

        void SubmitKeepAliveRequest(const protocol::TRACKER_INFO& tracker_info);

        void SubmitKeepAliveResponse(const protocol::TRACKER_INFO& tracker_info, boost::uint16_t keep_alive_interval);

        void SubmitListRequest(const protocol::TRACKER_INFO& tracker_info);

        void SubmitListResponse(const protocol::TRACKER_INFO& tracker_info, uint32_t peer_count);

        void SubmitErrorCode(const protocol::TRACKER_INFO& tracker_info, boost::uint8_t error_code);

        //////////////////////////////////////////////////////////////////////////
        // Index Server

        void SetIndexServerInfo(uint32_t ip, boost::uint16_t port, boost::uint8_t type = 0);

        void SetIndexServerInfo(const protocol::SocketAddr& socket_addr, boost::uint8_t type = 0);

        void SubmitQueryRIDByUrlRequest();

        void SubmitQueryRIDByUrlResponse();

        void SubmitQueryHttpServersByRIDRequest();

        void SubmitQueryHttpServersByRIDResponse();

        void SubmitQueryTrackerListRequest();

        void SubmitQueryTrackerListResponse();

        void SubmitAddUrlRIDRequest();

        void SubmitAddUrlRIDResponse();

        //////////////////////////////////////////////////////////////////////////
        // 停止时数据上传相关

        void SubmitP2PDownloaderDownloadBytes(uint32_t p2p_downloader_download_bytes);

        void SubmitOtherServerDownloadBytes(uint32_t other_server_download_bytes);

        //////////////////////////////////////////////////////////////////////////
        // 下载中 数据下载 实时相关信息

        void SubmitTotalHttpNotOriginalDataBytes(boost::uint32_t bytes);  // 实时 下载的纯数据 字节数

        void SubmitTotalP2PDataBytes(boost::uint32_t bytes);              // 实时 P2P下载的纯数据 字节数

        void SubmitTotalHttpOriginalDataBytes(boost::uint32_t bytes);     // 实时 原生下载的纯数据 字节数

        uint32_t GetTotalDataBytes();

        boost::uint16_t GetTotalDataBytesInMB();

        //////////////////////////////////////////////////////////////////////////
        // Upload Cache

        void SetUploadCacheRequest(uint32_t count);

        void SetUploadCacheHit(uint32_t count);

        float GetUploadCacheHitRate();

        //////////////////////////////////////////////////////////////////////////
        // HttpProxyPort

        void SetHttpProxyPort(boost::uint16_t port);

        //////////////////////////////////////////////////////////////////////////
        // IncomingPeersCount

        void SubmitIncomingPeer();

        boost::uint16_t GetIncomingPeersCount();

        //////////////////////////////////////////////////////////////////////////
        // DownloadDuration

        void SubmitDownloadDurationInSec(const boost::uint16_t& download_duration_in_sec);

        boost::uint16_t GetDownloadDurationInSec();

        //////////////////////////////////////////////////////////////////////////
        // 设置全局window_size
        void SetGlobalWindowSize(uint32_t global_window_size);

        void SetGlobalRequestSendCount(uint32_t global_request_send_count);

        // 设置全局window_size
        void SetMemoryPoolLeftSize(uint32_t memory_pool_left_size);

        void QueryBasicPeerInfo(boost::function<void()> result_handler, BASICPEERINFO *para_bpi);
        void QueryPeerInfoByRid(RID rid, boost::function<void()> result_handler, boost::int32_t *iListCount, boost::int32_t *iConnectCount, boost::int32_t *iAverSpeed);
    private:
        bool DetachP2PDownloaderStatistic(const RID& rid);
        bool DetachDownloadDriverStatistic(int id);
        bool DetachAllDownloadDriverStatistic();
        void DetachAllLiveDownloadDriverStatistic();
        bool DetachAllP2PDownaloaderStatistic();

        void OnTimerElapsed(framework::timer::Timer * timer);
        void DoTakeSnapshot();

        //////////////////////////////////////////////////////////////////////////
        //

        void LoadBandwidth();
        void SaveBandwidth();

        void Clear();

        //////////////////////////////////////////////////////////////////////////
        // Updates

        void UpdateSpeedInfo();

        void UpdateTrackerInfo();

        void UpdateMaxHttpDownloadSpeed();

        void UpdateBandWidth();

        //////////////////////////////////////////////////////////////////////////
        // Shared Memory

        bool CreateSharedMemory();

        string GetSharedMemoryName();

        uint32_t GetSharedMemorySize();

        void FlushSharedMemory();

        //////////////////////////////////////////////////////////////////////////
        // Tracker Info

        STATISTIC_TRACKER_INFO& GetTracker(const protocol::TRACKER_INFO& tracker_info);

    private:

        static const uint32_t HASH_SIZE = UINT8_MAX_VALUE;

        static boost::uint8_t HashFunc(uint32_t value)
        {
            return value % HASH_SIZE;
        }

        //////////////////////////////////////////////////////////////////////////
        // P2PDownloader RID Address

        boost::uint8_t Address(const RID& rid);  // Be careful when buffer FULL

        bool AddP2PDownloaderRID(const RID& rid);

        bool RemoveP2PDownloaderRID(const RID& rid);

        //////////////////////////////////////////////////////////////////////////
        // DownloaderDriverID Address

        uint32_t Address(uint32_t id);

        bool AddDownloadDriverID(uint32_t id);
        bool RemoveDownloadDriverID(uint32_t id);

        bool AddLiveDownloadDriverID(uint32_t id);
        bool RemoveLiveDownloadDriverID(uint32_t id);

    private:
        // types

        typedef std::map<uint32_t, DownloadDriverStatistic::p> DownloadDriverStatisticMap;
        typedef std::map<uint32_t, LiveDownloadDriverStatistic::p> LiveDownloadDriverStatisticMap;

        typedef std::map<RID, P2PDownloaderStatistic::p> P2PDownloadDriverStatisticMap;

        typedef std::map<protocol::TRACKER_INFO, STATISTIC_TRACKER_INFO> StatisticTrackerInfoMap;

    private:

        LiveDownloadDriverStatisticMap live_download_driver_statistic_map_;

        DownloadDriverStatisticMap download_driver_statistic_map_;

        P2PDownloadDriverStatisticMap p2p_downloader_statistic_map_;

        StatisticTrackerInfoMap statistic_tracker_info_map_;

        vector<protocol::STUN_SERVER_INFO> stun_server_infos_;
        boost::asio::ip::udp::endpoint bootstrap_endpoint_;

        STATISTIC_TRACKER_INFO* hot_tracker_;  // optimization

        framework::timer::PeriodicTimer share_memory_timer_;

        volatile bool is_running_;

        interprocess::SharedMemory shared_memory_;

        STASTISTIC_INFO statistic_info_;

        // Speed Info
        SpeedInfoStatistic speed_info_;

        uint32_t history_bandwith_;
        ByteSpeedMeter upload_speed_meter_;

        // config path
        string ppva_config_path_;

        boost::uint16_t tcp_port_;

    private:

        StatisticModule();

        static StatisticModule::p inst_;

    public:

        static StatisticModule::p Inst()
        {
            if (!inst_)
            {
                inst_.reset(new StatisticModule());
            }
            return inst_;
        }

    };

    //////////////////////////////////////////////////////////////////////////
    inline void StatisticModule::SubmitTotalHttpNotOriginalDataBytes(boost::uint32_t bytes)
    {
        statistic_info_.TotalHttpNotOriginalDataBytes += bytes;
    }

    inline void StatisticModule::SubmitTotalP2PDataBytes(boost::uint32_t bytes)
    {
        statistic_info_.TotalP2PDataBytes += bytes;
    }

    inline void StatisticModule::SubmitTotalHttpOriginalDataBytes(boost::uint32_t bytes)
    {
        statistic_info_.TotalHttpOriginalDataBytes += bytes;
    }
}

#endif  // _STATISTIC_STATISTIC_MODULE_H_
