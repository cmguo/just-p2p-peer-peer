//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _STATISTIC_DOWNLOAD_DRIVER_STATISTIC_H_
#define _STATISTIC_DOWNLOAD_DRIVER_STATISTIC_H_

#include "statistic/SpeedInfoStatistic.h"
#include "statistic/StatisticStructs.h"
#include "statistic/HttpDownloaderStatistic.h"
#include "interprocess/SharedMemory.h"

namespace statistic
{
    class DownloadDriverStatistic
        : public boost::noncopyable
        , public boost::enable_shared_from_this<DownloadDriverStatistic>
    {
    public:

        typedef boost::shared_ptr<DownloadDriverStatistic> p;

        static p Create(int id, bool is_create_share_memory = true);

    public:

        void Start();

        void Stop();

        void Clear();

        bool IsRunning() const;

        void OnShareMemoryTimer(uint32_t times);

        const DOWNLOADDRIVER_STATISTIC_INFO& TakeSnapshot();

    public:

        //////////////////////////////////////////////////////////////////////////
        // Operations

        HttpDownloaderStatistic::p AttachHttpDownloaderStatistic(const string& url);

        bool DetachHttpDownloaderStatistic(const string& url);

        bool DetachHttpDownloaderStatistic(const HttpDownloaderStatistic::p http_downloader_statistic);

        bool DetachAllHttpDownloaderStatistic();

        //////////////////////////////////////////////////////////////////////////
        // Speed Info

        void SubmitDownloadedBytes(uint32_t downloaded_bytes);

        void SubmitUploadedBytes(uint32_t uploaded_bytes);

        SPEED_INFO GetSpeedInfo();

        //////////////////////////////////////////////////////////////////////////
        // Url Info

        void SetOriginalUrl(const string& original_url);

        void SetOriginalReferUrl(const string& original_refer_url);

        //////////////////////////////////////////////////////////////////////////
        // Shared Memory

        string GetSharedMemoryName();

        uint32_t GetSharedMemorySize();

        //////////////////////////////////////////////////////////////////////////
        // Misc

        uint32_t GetDownloadDriverID() const;

        uint32_t GetMaxHttpDownloaderCount() const;

        //////////////////////////////////////////////////////////////////////////
        // Resource Info

        void SetResourceID(const RID& rid);

        RID GetResourceID();

        void SetFileLength(uint32_t file_length);

        uint32_t GetFileLength();

        void SetBlockSize(uint32_t block_size);

        uint32_t GetBlockSize();

        void SetBlockCount(boost::uint16_t block_count);

        boost::uint16_t GetBlockCount();

        void SetFileName(const string& file_name);

        string GetFileName();

        //////////////////////////////////////////////////////////////////////////
        // HTTP Data Bytes

        void SubmitHttpDataBytesWithRedundance(uint32_t http_data_bytes);

        void SubmitHttpDataBytesWithoutRedundance(uint32_t http_data_bytes);

        void SetLocalDataBytes(uint32_t local_data_bytes);

        uint32_t GetTotalHttpDataBytesWithRedundancy()
        {
            return download_driver_statistic_info_.TotalHttpDataBytesWithRedundance;
        }

        uint32_t GetTotalHttpDataBytesWithoutRedundancy()
        {
            return download_driver_statistic_info_.TotalHttpDataBytesWithoutRedundance;
        }

        uint32_t GetTotalLocalDataBytes() { return download_driver_statistic_info_.TotalLocalDataBytes; }

        //////////////////////////////////////////////////////////////////////////

        // HTTP Max Download Speed (历史最大瞬时速度)
        uint32_t GetHttpDownloadMaxSpeed();

        // HTTP 历史平均速度
        uint32_t GetHttpDownloadAvgSpeed();

        //////////////////////////////////////////////////////////////////////////
        // IsHidden
        bool IsHidden();

        void SetHidden(bool is_hidden);

        void SetSourceType(boost::uint32_t source_type);
        bool IsFlushSharedMemory() const { return is_flush_shared_memory_; }

        //////////////////////////////////////////////////////////////////////////
        // State Machine

        void SetStateMachineType(boost::uint8_t state_machine_type);

        boost::uint8_t GetStateMachineType();

        void SetStateMachineState(const string& state);

        string GetStateMachineState();

        //////////////////////////////////////////////////////////////////////////
        // PlayingPosition

        void SetPlayingPosition(uint32_t playing_position);

        uint32_t GetPlayingPosition();

        void SetDataRate(uint32_t date_rate);

        void SetHttpState(boost::uint8_t h);
        void SetP2PState(boost::uint8_t p);
        void SetTimerusingState(boost::uint8_t tu);
        void SetTimerState(boost::uint8_t t);

        //////////////////////////////////////////////////////////////////////////
        // extend

        void SetQueriedPeerCount(boost::uint16_t QueriedPeerCount);
        boost::uint16_t GetQueriedPeerCount();
        void SetConnectedPeerCount(boost::uint16_t ConnectedPeerCount);
        boost::uint16_t GetConnectedPeerCount();
        void SetFullPeerCount(boost::uint16_t FullPeerCount);
        boost::uint16_t GetFullPeerCount();
        void SetMaxActivePeerCount(boost::uint16_t MaxActivePeerCount);
        boost::uint16_t GetMaxActivePeerCount();

        void SetWebUrl(string web_url);

        void SetSmartPara(boost::int32_t t, boost::int32_t b, boost::int32_t speed_limit);

    private:

        //////////////////////////////////////////////////////////////////////////
        // Speed Info & HTTP Downloader Info

        void UpdateSpeedInfo();

        void UpdateHttpDownloaderInfo();

        //////////////////////////////////////////////////////////////////////////
        // Shared Memory

        bool CreateSharedMemory();

    private:

        typedef std::map<string, HttpDownloaderStatistic::p> HttpDownloaderStatisticMap;

    private:

        volatile bool is_running_;

        string original_url_;

        string original_refer_url_;

        SpeedInfoStatistic speed_info_;

        DOWNLOADDRIVER_STATISTIC_INFO download_driver_statistic_info_;

        uint32_t download_driver_id_;

        HttpDownloaderStatisticMap http_downloader_statistic_map_;

        interprocess::SharedMemory shared_memory_;

        uint32_t http_download_max_speed_;

        bool is_flush_shared_memory_;

        // extend
        struct _PeerInfo
        {
            boost::uint32_t uMaxP2PDownloadSpeed;   // 最大P2P下载速度
            boost::uint16_t uQueriedPeerCount;      // 查询到的节点数
            boost::uint16_t uConnectedPeerCount;    // 连接上的节点数
            boost::uint16_t uFullPeerCount;         // 资源全满节点数
            boost::uint16_t uMaxActivePeerCount;    // 活跃节点数峰值
            //
            void Clear()
            {
                memset(this, 0, sizeof(_PeerInfo));
            }
            _PeerInfo()
            {
                Clear();
            }
        } peer_info_;

    private:

        DownloadDriverStatistic()
            : http_download_max_speed_(0),
              is_flush_shared_memory_(true)
        {}

        DownloadDriverStatistic(uint32_t id, bool is_create_share_memory = true);
    };

    inline void DownloadDriverStatistic::SetPlayingPosition(uint32_t playing_position)
    {
        this->download_driver_statistic_info_.PlayingPosition = playing_position;
    }
}

#endif  // _STATISTIC_DOWNLOAD_DRIVER_STATISTIC_H_
