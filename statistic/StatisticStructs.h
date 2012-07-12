//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _STATISTIC_STATISTIC_STRUCTS_H_
#define _STATISTIC_STATISTIC_STRUCTS_H_

#include "struct/Base.h"

namespace statistic
{
    //////////////////////////////////////////////////////////////////////////
    // Constants

    const uint32_t BITMAP_SIZE = 50;

    const uint32_t UINT8_MAX_VALUE = 256;

    const uint32_t LIVEDOWNLOADER_MAX_COUNT = 64;

    const uint32_t MAX_IP_COUNT = 10;

    //////////////////////////////////////////////////////////////////////////
    // 公共结构

    struct SPEED_INFO
    {
        boost::uint32_t StartTime;               // 开始时刻
        boost::uint32_t TotalDownloadBytes;      // 总共下载的字节数
        boost::uint32_t TotalUploadBytes;        // 总共上传时字节数
        boost::uint32_t NowDownloadSpeed;        // 当前下载速度 <5s>
        boost::uint32_t NowUploadSpeed;          // 当前上传速度 <5s>
        boost::uint32_t MinuteDownloadSpeed;     // 最近一分钟平均下载速度 <60s>
        boost::uint32_t MinuteUploadSpeed;       // 最近一分钟平均上传速度 <60s>
        boost::uint32_t AvgDownloadSpeed;        // 历史平均下载速度
        boost::uint32_t AvgUploadSpeed;          // 历史平均上传速度

        SPEED_INFO()
        {
            memset(this, 0, sizeof(SPEED_INFO));
        }

        template <typename Archive>
        void serialize(Archive & ar)
        {
            ar & StartTime;
            ar & TotalDownloadBytes;
            ar & TotalUploadBytes;
            ar & NowDownloadSpeed;
            ar & NowUploadSpeed;
            ar & MinuteDownloadSpeed;
            ar & MinuteUploadSpeed;
            ar & AvgDownloadSpeed;
            ar & AvgUploadSpeed;
        }
    };

    struct SPEED_INFO_EX : SPEED_INFO
    {
        boost::uint32_t RecentDownloadSpeed;     // 当前下载速度 <20s>
        boost::uint32_t RecentUploadSpeed;       // 当前上传速度 <20s>
        boost::uint32_t SecondDownloadSpeed;     // 当前1s的下载速度
        boost::uint32_t SecondUploadSpeed;       // 当前1s的上传速度

        SPEED_INFO_EX()
        {
            memset(this, 0, sizeof(SPEED_INFO_EX));
        }
    };

    /*
    struct CANDIDATE_PEER_INFO
    {
    boost::uint32_t IP;                      //
    boost::uint16_t UdpPort;                 //
    boost::uint16_t TcpPort;                 //
    boost::uint32_t DetectIP;                //
    boost::uint16_t DetectUdpPort;           //
    boost::uint32_t StunIP;                  //
    boost::uint16_t StunUdpPort;             //
    boost::uint8_t  Reversed[4];
    };
    */
    // typedef protocol::CANDIDATE_PEER_INFO CANDIDATE_PEER_INFO;

    /*
    struct TRACKER_INFO
    {
    boost::uint16_t Length;
    boost::uint8_t ModNo;
    uint32_t IP;
    boost::uint16_t Port;
    boost::uint8_t Type;
    }
    */
    struct STATISTIC_TRACKER_INFO
    {
        //
        protocol::TRACKER_INFO TrackerInfo;  // 该Tracker的基本信息

        //
        boost::uint16_t CommitRequestCount;       // 该Tracker Commit 的请求次数
        boost::uint16_t CommitResponseCount;      // 该Tracker Commit 的响应次数
        boost::uint16_t KeepAliveRequestCount;    // 该Tracker KeepAlive 的请求次数
        boost::uint16_t KeepAliveResponseCount;   // 该Tracker KeepAlive 的响应次数
        boost::uint16_t ListRequestCount;         // 该Tracker List 的请求次数
        boost::uint16_t ListResponseCount;        // 该Tracker List 的响应次数
        boost::uint8_t  LastListReturnPeerCount;  // 上一次成功的List的返回的Peer数
        boost::uint8_t  IsSubmitTracker;          // 在该Group中是否为当前选定Tracker
        boost::uint8_t  ErrorCode;                // 上次Tracker返回的错误码
        boost::uint16_t KeepAliveInterval;        // 上一次从服务器返回的Submit间隔

        STATISTIC_TRACKER_INFO()
        {
            Clear();
        }
        STATISTIC_TRACKER_INFO(const protocol::TRACKER_INFO& tracker_info)
        {
            Clear();
            TrackerInfo = tracker_info;
        }
        void Clear()
        {
            memset(this, 0, sizeof(STATISTIC_TRACKER_INFO));
        }

        template <typename Archive>
        void serialize(Archive & ar)
        {
            ar & TrackerInfo;
            ar & CommitRequestCount;       //
            ar & CommitResponseCount;      //
            ar & KeepAliveRequestCount;    //
            ar & KeepAliveResponseCount;   //
            ar & ListRequestCount;         //
            ar & ListResponseCount;        //
            ar & LastListReturnPeerCount;  //
            ar & IsSubmitTracker;          //
            ar & ErrorCode;                //
            ar & KeepAliveInterval;        //
        }
    };

    struct STATISTIC_INDEX_INFO
    {
        uint32_t IP;
        boost::uint16_t Port;
        boost::uint8_t  Type;
        boost::uint16_t QueryRIDByUrlRequestCount;
        boost::uint16_t QueryRIDByUrlResponseCount;
        boost::uint16_t QueryHttpServersByRIDRequestCount;
        boost::uint16_t QueryHttpServersByRIDResponseCount;
        boost::uint16_t QueryTrackerListRequestCount;
        boost::uint16_t QureyTrackerListResponseCount;
        boost::uint16_t AddUrlRIDRequestCount;
        boost::uint16_t AddUrlRIDResponseCount;

        STATISTIC_INDEX_INFO()
        {
            Clear();
        }

        void Clear()
        {
            memset(this, 0, sizeof(STATISTIC_INDEX_INFO));
        }

        template <typename Archive>
        void serialize(Archive & ar)
        {
            ar & IP;
            ar & Port;
            ar & Type;
            ar & QueryRIDByUrlRequestCount;
            ar & QueryRIDByUrlResponseCount;
            ar & QueryHttpServersByRIDRequestCount;
            ar & QueryHttpServersByRIDResponseCount;
            ar & QueryTrackerListRequestCount;
            ar & QureyTrackerListResponseCount;
            ar & AddUrlRIDRequestCount;
            ar & AddUrlRIDResponseCount;
        }

    };

    // struct PEER_DOWNLOAD_INFO
    // {
    //    boost::uint8_t  IsDownloading;
    //    uint32_t OnlineTime;
    //    boost::uint16_t AvgDownload;
    //    boost::uint16_t NowDownload;
    //    boost::uint16_t AvgUpload;
    //    boost::uint16_t NowUpload;
    // };

    struct PIECE_INFO_EX
    {
        boost::uint16_t BlockIndex;
        boost::uint16_t PieceIndexInBlock;
        boost::uint16_t SubPieceIndexInPiece;

        PIECE_INFO_EX()
        {
            memset(this, 0, sizeof(PIECE_INFO_EX));
        }
        PIECE_INFO_EX(boost::uint16_t block_index, boost::uint16_t piece_index_in_block, boost::uint16_t sub_piece_index_in_piece)
            : BlockIndex(block_index)
            , PieceIndexInBlock(piece_index_in_block)
            , SubPieceIndexInPiece(sub_piece_index_in_piece)
        {
        }

        template <typename Archive>
        void serialize(Archive & ar)
        {
            ar & BlockIndex;
            ar & PieceIndexInBlock;
            ar & SubPieceIndexInPiece;
        }
    };

    inline std::ostream& operator << (std::ostream& os, const PIECE_INFO_EX& info)
    {
        return os << "BlockIndex: " << info.BlockIndex << ", PieceIndexInBlock: " << info.PieceIndexInBlock << ", SubPieceIndexInBlock: " << info.SubPieceIndexInPiece;
    }

    //Peer信息，计划扩展它来包含类似于客户端OS版本/CPU占用率等信息
    struct PeerStatisticsInfo
    {
        protocol::VERSION_INFO version_;
        boost::uint16_t collected_statistics_size_in_seconds_;
        boost::uint8_t   resersed_[1018];  // 保留字段

        //for deserialization
        PeerStatisticsInfo()
        {
            Clear();
        }

        PeerStatisticsInfo(const protocol::VERSION_INFO& version, int collected_statistics_size_in_seconds)
        {
            Clear();
            version_ = version;
            collected_statistics_size_in_seconds_ = collected_statistics_size_in_seconds;
        }

        template <typename Archive>
        void serialize(Archive & ar)
        {
            ar & version_;
            ar & collected_statistics_size_in_seconds_;
            ar & framework::container::make_array(resersed_, sizeof(resersed_) / sizeof(resersed_[0]));
        }

    private:
        void Clear()
        {
            memset(this, 0, sizeof(PeerStatisticsInfo));
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // AppStop时统计上传
    // StatisticModule
    // 共享内存名: PPVIDEO_<PID>
    struct STASTISTIC_INFO
    {
        // 速度相关信息
        SPEED_INFO SpeedInfo;                  // 网络 字节数 和 速度

        // IP 相关信息
        protocol::CandidatePeerInfo LocalPeerInfo;     // 自己 的基本IP端口信息
        boost::uint8_t  LocalIpCount;                   // 本地通过API 获得的本地IP数
        boost::uint32_t LocalIPs[MAX_IP_COUNT];         // 本地通过API 获得的IP; (连续存放)

        // 自己其他相关信息
        boost::uint32_t LocalPeerVersion;               // 自己内核版本号
        boost::uint8_t  Reverse;                  // 保留

        // TrackerServer相关信息
        boost::uint8_t  TrackerCount;                       // Tracker的个数
        boost::uint8_t  GroupCount;                         // Group 的, 也是MOD的除数
        STATISTIC_TRACKER_INFO TrackerInfos[UINT8_MAX_VALUE];  // Tracker, (连续存放)

        // IndexServer 相关信息
        STATISTIC_INDEX_INFO StatisticIndexInfo;   // IndexServer的相关信息

        // P2PDownloader 相关信息
        boost::uint8_t  P2PDownloaderCount;                 // 有多少个正在P2P下载的资源
        RID    P2PDownloaderRIDs[UINT8_MAX_VALUE];  // 正在P2P下载的资源RID; 如果为 GUID_NULL 表示空; (不连续)

        // DownloadDriver 相关信息
        boost::uint8_t  DownloadDriverCount;               // 正在下载的视频
        boost::uint32_t DownloadDriverIDs[UINT8_MAX_VALUE];  // 正在下载的视频 驱动器ID; 如果为 0 表示不存在; (不连续)

        uint32_t Resersed2;
        uint32_t Resersed3;

        // 下载中 数据下载 实时相关信息
        boost::uint32_t  TotalHttpNotOriginalDataBytes;        // 实时 下载的纯数据 字节数
        boost::uint32_t  TotalP2PDataBytes;                    // 实时 P2P下载的纯数据 字节数
        boost::uint32_t  TotalHttpOriginalDataBytes;            // 实时 原生下载的纯数据 字节数
        boost::uint32_t  Resersed1;// 备用

        boost::uint32_t  TotalUploadCacheRequestCount;       // 总共的上传Cache请求数
        boost::uint32_t  TotalUploadCacheHitCount;           // 总共的上传Cache命中数

        boost::uint16_t  HttpProxyPort;                      // HTTP实际代理端口
        boost::uint32_t  MaxHttpDownloadSpeed;               // HTTP最大下载速度
        boost::uint16_t  IncomingPeersCount;                 // 总共连入的Peer个数
        boost::uint16_t  DownloadDurationInSec;              // 下载总共持续时长(秒)
        boost::uint32_t  BandWidth;                          // 预测的带宽

        boost::uint32_t  GlobalWindowSize;

        boost::uint16_t  GlobalRequestSendCount;             // 每秒发出的请求数

        boost::uint16_t  MemoryPoolLeftSize;                     // 内存池剩余大小

        // LiveDownloadDriver 相关信息
        boost::uint8_t  LiveDownloadDriverCount;               // 正在下载的直播视频
        boost::uint32_t LiveDownloadDriverIDs[LIVEDOWNLOADER_MAX_COUNT];// 正在下载的直播视频 驱动器ID; 如果为 0 表示不存在; (不连续)

        protocol::VERSION_INFO PeerVersion;
        boost::uint8_t   Resersed[937 - 4 * LIVEDOWNLOADER_MAX_COUNT];                      // 保留字段

        STASTISTIC_INFO();

        void Clear()
        {
            memset(this, 0, sizeof(STASTISTIC_INFO));  // Important!
        }

        template <typename Archive>
        void serialize(Archive & ar)
        {
            ar & SpeedInfo;

            ar & LocalPeerInfo;
            ar & LocalIpCount;
            ar & framework::container::make_array(LocalIPs, sizeof(LocalIPs) / sizeof(LocalIPs[0]));

            ar & LocalPeerVersion;
            ar & Reverse;

            ar & TrackerCount;
            ar & GroupCount;
            ar & framework::container::make_array(TrackerInfos, sizeof(TrackerInfos) / sizeof(TrackerInfos[0]));

            ar & StatisticIndexInfo;

            ar & P2PDownloaderCount;
            ar & framework::container::make_array(P2PDownloaderRIDs, sizeof(P2PDownloaderRIDs) / sizeof(P2PDownloaderRIDs[0]));

            ar & DownloadDriverCount;
            ar & framework::container::make_array(DownloadDriverIDs, sizeof(DownloadDriverIDs) / sizeof(DownloadDriverIDs[0]));

            ar & Resersed2;
            ar & Resersed3;

            ar &  TotalHttpNotOriginalDataBytes;        //
            ar &  TotalP2PDataBytes;                    //
            ar &  TotalHttpOriginalDataBytes;            //
            ar &  Resersed1;                        //

            ar &  TotalUploadCacheRequestCount;       //
            ar &  TotalUploadCacheHitCount;           //

            ar &  HttpProxyPort;                      //
            ar &  MaxHttpDownloadSpeed;               //
            ar &  IncomingPeersCount;                 //
            ar &  DownloadDurationInSec;              //
            ar &  BandWidth;                          //

            ar &  GlobalWindowSize;

            ar & GlobalRequestSendCount;
            ar & MemoryPoolLeftSize;

            ar & LiveDownloadDriverCount;
            ar & framework::container::make_array(LiveDownloadDriverIDs, sizeof(LiveDownloadDriverIDs) / sizeof(LiveDownloadDriverIDs[0]));

            ar & PeerVersion;

            ar & framework::container::make_array(Resersed, sizeof(Resersed) / sizeof(Resersed[0]));
        }
    };

    struct PEER_INFO
    {
        boost::uint8_t download_connected_count_;
        boost::uint8_t upload_connected_count_;
        boost::uint32_t mine_upload_speed_;
        boost::uint32_t max_upload_speed_;
        boost::uint32_t rest_playable_time_;
        boost::uint8_t lost_rate_;
        boost::uint8_t redundancy_rate_;

        PEER_INFO()
        {
            Clear();
        }

        PEER_INFO(boost::uint8_t download_connected_count, boost::uint8_t upload_connected_count, boost::uint32_t mine_upload_speed,
            boost::uint32_t max_upload_speed, boost::uint32_t rest_playable_time, boost::uint8_t lost_rate, boost::uint8_t redundancy_rate)
        {
            download_connected_count_ = download_connected_count;
            upload_connected_count_ = upload_connected_count;
            mine_upload_speed_ = mine_upload_speed;
            max_upload_speed_ = max_upload_speed;
            rest_playable_time_ = rest_playable_time;
            lost_rate_ = lost_rate;
            redundancy_rate_ = redundancy_rate;
        }

        void Clear()
        {
            memset(this, 0, sizeof(PEER_INFO));
        }

        template <typename Archive>
        void serialize(Archive & ar)
        {
            ar & download_connected_count_;
            ar & upload_connected_count_;
            ar & mine_upload_speed_;
            ar & max_upload_speed_;
            ar & rest_playable_time_;
            ar & lost_rate_;
            ar & redundancy_rate_;
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // P2PDownloaderStatistic 结构
    // 共享内存名: P2PDOWNLOADER_<PID>_<RID>
    // NOTE: RID格式{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}

    struct P2P_CONNECTION_INFO
    {
        Guid   PeerGuid;                       // 该Peer的PeerGuid
        SPEED_INFO SpeedInfo;                  // 该Peer 的速度信息
        boost::uint32_t PeerVersion;                    // 对方Peer的内核版本号
        boost::uint8_t  Reserve;                       // 保留
        protocol::CandidatePeerInfo PeerInfo;          // 该Peer的 IP信息
        protocol::PEER_DOWNLOAD_INFO PeerDownloadInfo;   // 对方的相关 下载 信息
        boost::uint8_t  BitMap[BITMAP_SIZE];            // 对方的BitMap
        boost::uint32_t RTT_Count;                      // Udp包的个数
        boost::uint16_t RTT_Now;                        // 当前的rtt
        boost::uint16_t RTT_Average;                    // 总共的平均rtt; 可画出所有udp包的收到时间分布图
        boost::uint16_t RTT_Max;                        // 收到的udp包中rtt最长的
        boost::uint32_t RTT_Total;                      // 总共的rtt
        boost::uint16_t ElapseTime;                     // 当前超时时间
        boost::uint8_t  WindowSize;                     // 窗口大小
        boost::uint8_t  AssignedSubPieceCount;          // 当前预分配SubPiece数
        uint32_t AverageDeltaTime;               // Average Delta Time
        uint32_t SortedValue;                    // Sorted Value

        //////////////////////////////////////////////////////////////////////////
        boost::uint8_t  IsRidInfoValid;                 // 0-Invalid; 1-Valid
        boost::uint16_t Sent_Count;
        boost::uint16_t Requesting_Count;
        boost::uint16_t Received_Count;
        boost::uint16_t AssignedLeftSubPieceCount;  // 经过一轮下载后，剩余的分配SubPiece个数
        boost::uint32_t LastLiveBlockId;            // 对方缓存的最后一个block的ID
        boost::uint32_t FirstLiveBlockId;           // 对方发过来的AnnounceMap中的第一片block的ID
        boost::uint8_t  ConnectType;                // 0 vod, 1 live peer, 2 live udpserver, 3 notify(只区分了1和2, 2011/7/27)
        PEER_INFO RealTimePeerInfo;
        boost::uint32_t ActualAssignedSubPieceCount;  // 当前1秒钟分配的SubPiece数
        boost::uint32_t RequestSubPieceCount;         // 当前1秒钟发出的SubPiece请求数
        boost::uint32_t SupplySubPieceCount;          // 可供下载的SubPiece数(我没有但是对方有的SuPiece数)
        boost::uint32_t TimeOfNoResponse;                 // 上一次收到该peer的包到现在为止过了多久
        boost::uint8_t  Reserved[150];                  //

        P2P_CONNECTION_INFO()
        {
            Clear();
        }

        void Clear()
        {
            memset(this, 0, sizeof(P2P_CONNECTION_INFO));
        }

        template <typename Archive>
        void serialize(Archive & ar)
        {
            ar & PeerGuid;
            ar & SpeedInfo;
            ar & PeerVersion;
            ar & Reserve;
            ar & PeerInfo;
            ar & PeerDownloadInfo;
            ar & framework::container::make_array(BitMap, sizeof(BitMap) / sizeof(BitMap[0]));
            ar & RTT_Count;
            ar & RTT_Now;
            ar & RTT_Average;
            ar & RTT_Max;
            ar & RTT_Total;
            ar & ElapseTime;
            ar & WindowSize;
            ar & AssignedSubPieceCount;
            ar & AverageDeltaTime;
            ar & SortedValue;

            ar & IsRidInfoValid;
            ar & Sent_Count;
            ar & Requesting_Count;
            ar & Received_Count;
            ar & AssignedLeftSubPieceCount;
            ar & LastLiveBlockId;
            ar & FirstLiveBlockId;
            ar & ConnectType;
            ar & RealTimePeerInfo;
            ar & ActualAssignedSubPieceCount;
            ar & RequestSubPieceCount;
            ar & SupplySubPieceCount;
            ar & TimeOfNoResponse;
            ar & framework::container::make_array(Reserved, sizeof(Reserved) / sizeof(Reserved[0]));
        }
    };

    struct P2PDOWNLOADER_STATISTIC_INFO
    {
        RID    ResourceID;                 // 对应的RID
        SPEED_INFO SpeedInfo;

        // 资源相关信息
        uint32_t FileLength;
        boost::uint16_t BlockNum;
        boost::uint16_t BlockSize;

        // IPPool信息
        boost::uint16_t IpPoolPeerCount;        // 备选IP
        boost::uint8_t  ExchangingPeerCount;    // 正在交换信息的IP
        boost::uint8_t  ConnectingPeerCount;    // 正在连接的IP

        // 算法相关信息
        boost::uint16_t TotalWindowSize;               // 总窗口大小
        boost::uint16_t TotalAssignedSubPieceCount;    // 当前与分配的总SubPiece数
        boost::uint16_t TotalUnusedSubPieceCount_;      // 冗余的Subpieces数
        boost::uint16_t TotalRecievedSubPieceCount_;    // 收到的Subpiece数
        boost::uint16_t TotalRequestSubPieceCount_;     // 发出的Subpiece请求数
        boost::uint16_t SubPieceRetryRate;             // 冗余率: 冗余 / 收到
        boost::uint16_t UDPLostRate;                   // 丢包率: (发出 - 收到) / 发出

        // PEER 下载的字节数(不含冗余)
        boost::uint32_t TotalP2PPeerDataBytesWithoutRedundance;
        boost::uint16_t FullBlockPeerCount;            // 在已经连接的Peer中，资源全满的Peer个数

        boost::uint32_t TotalUnusedSubPieceCount;      // 冗余的Subpieces数
        boost::uint32_t TotalRecievedSubPieceCount;    // 收到的Subpiece数
        boost::uint32_t TotalRequestSubPieceCount;     // 发出的Subpiece请求数

        boost::uint32_t NonConsistentSize;        // 当前下载的最后一个piece和第一个piece之间的距离
        boost::uint16_t ConnectCount;
        boost::uint16_t KickCount;

        boost::uint32_t empty_subpiece_distance;

        SPEED_INFO PeerSpeedInfo;
        SPEED_INFO SnSpeedInfo;
        
        // SN 下载的字节数(不含冗余)
        boost::uint32_t TotalP2PSnDataBytesWithoutRedundance;

        // PEER 下载的字节数(包含冗余)
        boost::uint32_t TotalP2PPeerDataBytesWithRedundance;

        // SN 下载的字节数(包含冗余)
        boost::uint32_t TotalP2PSnDataBytesWithRedundance;

        boost::uint8_t Reserved[886];                  // 保留

        boost::uint16_t PeerCount;                     // Peer的
        P2P_CONNECTION_INFO P2PConnections[MAX_P2P_DOWNLOADER_COUNT];  // 变长; (连续存放)

        void Clear()
        {
            memset(this, 0, sizeof(P2PDOWNLOADER_STATISTIC_INFO));
        }

        template <typename Archive>
        void serialize(Archive & ar)
        {
            ar & ResourceID;
            ar & SpeedInfo;

            ar & FileLength;
            ar & BlockNum;
            ar & BlockSize;

            ar & IpPoolPeerCount;
            ar & ExchangingPeerCount;
            ar & ConnectingPeerCount;

            ar & TotalWindowSize;
            ar & TotalAssignedSubPieceCount;
            ar & TotalUnusedSubPieceCount_;
            ar & TotalRecievedSubPieceCount_;
            ar & TotalRequestSubPieceCount_;
            ar & SubPieceRetryRate;
            ar & UDPLostRate;

            ar & TotalP2PPeerDataBytesWithoutRedundance;
            ar & FullBlockPeerCount;

            ar & TotalUnusedSubPieceCount;
            ar & TotalRecievedSubPieceCount;
            ar & TotalRequestSubPieceCount;

            ar & NonConsistentSize;
            ar & ConnectCount;
            ar & KickCount;

            ar & empty_subpiece_distance;

            ar & PeerSpeedInfo;
            ar & SnSpeedInfo;
            ar & TotalP2PSnDataBytesWithoutRedundance;

            ar & TotalP2PPeerDataBytesWithRedundance;
            ar & TotalP2PSnDataBytesWithRedundance;

            ar & framework::container::make_array(Reserved, sizeof(Reserved) / sizeof(Reserved[0]));
            ar & PeerCount;
            for (int i = 0; i<PeerCount; i++)
            {
                ar & P2PConnections[i];
            }
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // DownloadDriverStatistic 结构
    // 共享内存名： DOWNLOADDRIVER_<PID>_<DownloadDriverID>

    struct HTTP_DOWNLOADER_INFO
    {
        boost::uint8_t  Url[256];
        boost::uint8_t  ReferUrl[256];
        boost::uint8_t  RedirectUrl[256];  // 发生301或者302 重定向的Url

        SPEED_INFO SpeedInfo;

        PIECE_INFO_EX DownloadingPieceEx;   // 现在正在请求的 PieceEx
        PIECE_INFO_EX StartPieceEx;         // 从哪个Subpiece开始下载没断开连接过;
        boost::uint32_t LastConnectedTime;           // 最近建立HTTP连接的时刻
        boost::uint32_t LastRequestPieceTime;        // 最近请求PieceEx的时刻

        boost::uint16_t LastHttpStatusCode;          // 上次HTTP请求的返回值 (例如200.206)
        boost::uint16_t RetryCount;                  // Http重试次数
        boost::uint8_t  IsSupportRange;              // 是否支持Range

        boost::uint8_t  IsDeath;                     // 是否是死的
        boost::uint8_t  IsPause;                     // 是否停止状态
        boost::uint8_t  Resersed[399];               // 保留字段

        HTTP_DOWNLOADER_INFO()
        {
            Clear();
        }

        void Clear()
        {
            memset(this, 0, sizeof(HTTP_DOWNLOADER_INFO));
        }

        template <typename Archive>
        void serialize(Archive & ar)
        {
            ar & framework::container::make_array(Url, sizeof(Url) / sizeof(Url[0]));
            ar & framework::container::make_array(ReferUrl, sizeof(ReferUrl) / sizeof(ReferUrl[0]));
            ar & framework::container::make_array(RedirectUrl, sizeof(RedirectUrl) / sizeof(RedirectUrl[0]));

            ar & SpeedInfo;

            ar & DownloadingPieceEx;
            ar & StartPieceEx;
            ar & LastConnectedTime;
            ar & LastRequestPieceTime;

            ar & LastHttpStatusCode;
            ar & RetryCount;
            ar & IsSupportRange;

            ar & IsDeath;
            ar & IsPause;
            ar & framework::container::make_array(Resersed, sizeof(Resersed) / sizeof(Resersed[0]));
        }
    };

    struct DOWNLOADDRIVER_STATISTIC_INFO
    {
        uint32_t DownloadDriverID;
        SPEED_INFO SpeedInfo;

        boost::uint8_t  OriginalUrl[256];
        boost::uint8_t  OriginalReferUrl[256];

        RID       ResourceID;                 // 资源RID (可以为全0)
        boost::uint32_t    FileLength;                    // 文件的长度
        boost::uint32_t    BlockSize;                  // Block的大小
        boost::uint16_t    BlockCount;                 // Block的个数

        boost::uint32_t TotalHttpDataBytesWithoutRedundance; // 所有HttpDownloader下载的有效字节数, 不包含冗余
        boost::uint32_t TotalLocalDataBytes;         // 所有本地已经下载过的有效字节数
        boost::uint8_t FileName[256];                // 文件名, (TCHAR*)

        boost::uint8_t IsHidden;                      // 是否隐藏(不在界面上显示进度)
        boost::uint8_t SourceType;  // 标识是否是客户端
        boost::uint8_t Resersed1[256];  // 保留字段

        boost::uint8_t   StateMachineType;
        boost::uint8_t   StateMachineState[14];

        uint32_t PlayingPosition;
        uint32_t DataRate;
        boost::uint8_t http_state;
        boost::uint8_t p2p_state;
        boost::uint8_t timer_using_state;
        boost::uint8_t timer_state;
        boost::int32_t t;
        boost::int32_t b;
        boost::int32_t speed_limit;

        boost::uint32_t TotalHttpDataBytesWithRedundance; // 所有HttpDownloader下载的有效字节数, 包含冗余
        boost::uint8_t sn_state;

        boost::uint8_t Resersed[451-17];                // 保留字段

        boost::uint8_t  HttpDownloaderCount;
        HTTP_DOWNLOADER_INFO HttpDownloaders[MAX_HTTP_DOWNLOADER_COUNT];

        DOWNLOADDRIVER_STATISTIC_INFO()
        {
            Clear();
        }

        void Clear()
        {
            memset(this, 0, sizeof(DOWNLOADDRIVER_STATISTIC_INFO));
        }

        template <typename Archive>
        void serialize(Archive & ar)
        {
            ar & DownloadDriverID;
            ar & SpeedInfo;

            ar & framework::container::make_array(OriginalUrl, sizeof(OriginalUrl) / sizeof(OriginalUrl[0]));
            ar & framework::container::make_array(OriginalReferUrl, sizeof(OriginalReferUrl) / sizeof(OriginalReferUrl[0]));

            ar & ResourceID;
            ar & FileLength;
            ar & BlockSize;
            ar & BlockCount;

            ar & TotalHttpDataBytesWithoutRedundance;
            ar & TotalLocalDataBytes;
            ar & framework::container::make_array(FileName, sizeof(FileName) / sizeof(FileName[0]));

            ar & IsHidden;
            ar & SourceType;
            ar & framework::container::make_array(Resersed1, sizeof(Resersed1) / sizeof(Resersed1[0]));

            ar & StateMachineType;
            ar & framework::container::make_array(StateMachineState, sizeof(StateMachineState) / sizeof(StateMachineState[0]));


            ar & PlayingPosition;
            ar & DataRate;
            ar & http_state;
            ar & p2p_state;
            ar & timer_using_state;
            ar & timer_state;
            ar & t;
            ar & b;
            ar & speed_limit;
            ar & TotalHttpDataBytesWithRedundance;
            ar & sn_state;
            ar & framework::container::make_array(Resersed, sizeof(Resersed) / sizeof(Resersed[0]));

            ar & HttpDownloaderCount;
            for (int i = 0; i<HttpDownloaderCount; i++)
            {
                ar & HttpDownloaders[i];
            }
        }
    };

    //////////////////////////////////////////////////////////////////////////
    // LiveDownloadDriverStatistic 结构
    // 共享内存名： LIVEDOWNLOADDRIVER_<PID>_<DownloadDriverID>
    struct LIVE_DOWNLOADDRIVER_STATISTIC_INFO
    {
        uint32_t LiveDownloadDriverID;
        SPEED_INFO LiveHttpSpeedInfo;
        SPEED_INFO LiveP2PSpeedInfo;
        SPEED_INFO LiveP2PSubPieceSpeedInfo;
        boost::uint8_t http_state;
        boost::uint8_t p2p_state;
        boost::uint8_t  OriginalUrl[256];              // CDN IP
        boost::uint16_t LastHttpStatusCode;            // 上次HTTP请求的返回值 (例如200.206)
        boost::uint32_t TotalP2PDataBytes;             // 当前P2PDownloader下载的有效字节数
        boost::uint32_t TotalRecievedSubPieceCount;    // 收到的Subpiece数
        boost::uint32_t TotalRequestSubPieceCount;     // 发出的Subpiece请求数
        boost::uint32_t TotalAllRequestSubPieceCount;     // 发出的Subpiece请求数
        boost::uint32_t TotalUnusedSubPieceCount;      // 冗余的Subpieces数
        boost::uint16_t IpPoolPeerCount;               // 备选IP
        boost::uint32_t DataRate;                      // 码流率
        boost::uint16_t CacheSize;                      // 已缓存大小
        boost::uint32_t CacheFirstBlockId;             // 已缓存的第一片Block ID
        boost::uint32_t CacheLastBlockId;              // 已缓存的最后一片Block ID
        boost::uint32_t PlayingPosition;               // 播放点
        boost::uint32_t LeftCapacity;                  // 内存池剩余
        boost::int32_t  RestPlayTime;                  // 剩余时间
        RID       ResourceID;                          // 资源RID
        boost::uint8_t  IsPlayingPositionBlockFull; // 正在推给播放器的这一片Block是否满
        boost::uint32_t LivePointBlockId;              // 直播点的Block ID
        boost::uint32_t DataRateLevel;                  // 码流等级
        boost::int32_t reserved1;
        boost::uint8_t reserved2;
        boost::uint8_t reserved3;
        boost::uint8_t reserved4;
        boost::uint32_t JumpTimes;                      // 跳跃了多少次
        boost::uint32_t NumOfChecksumFailedPieces;      // 校验失败的piece个数
        boost::uint8_t reserved5;
        RID ChannelID;                                  // 频道ID
        boost::uint32_t TotalUdpServerDataBytes;        // 从UdpServer下载的字节数
        boost::uint8_t PmsStatus;                       // 0代表正常，1代表不正常
        boost::uint32_t UniqueID;                       // 播放器的ID
        SPEED_INFO UdpServerSpeedInfo;                  // UdpServer速度
        boost::uint8_t IsPaused;                        // 是否暂停，0代表播放，1代表暂停
        boost::uint8_t IsReplay;                        // 是否回拖，0代表不回拖，1代表回拖
        boost::uint32_t MissingSubPieceCountOfFirstBlock;  // 第一个不满的Block中空的SubPiece个数
        boost::uint32_t ExistSubPieceCountOfFirstBlock;  // 第一个不满的Block中存在的SubPiece个数
        boost::uint32_t P2PPeerSpeedInSecond;        // P2P Peer一秒的速度
        boost::uint32_t P2PUdpServerSpeedInSecond;  // UdpServer一秒的速度

        boost::uint8_t Reserved[896];

        boost::uint16_t PeerCount;                     // Peer的
        P2P_CONNECTION_INFO P2PConnections[MAX_P2P_DOWNLOADER_COUNT];  // 变长; (连续存放)

        LIVE_DOWNLOADDRIVER_STATISTIC_INFO()
        {
            Clear();
        }

        void Clear()
        {
            memset(this, 0, sizeof(LIVE_DOWNLOADDRIVER_STATISTIC_INFO));
        }

        template <typename Archive>
        void serialize(Archive & ar)
        {
            ar & LiveDownloadDriverID;
            ar & LiveHttpSpeedInfo;
            ar & LiveP2PSpeedInfo;
            ar & LiveP2PSubPieceSpeedInfo;
            ar & http_state;
            ar & p2p_state;
            ar & framework::container::make_array(OriginalUrl, sizeof(OriginalUrl) / sizeof(OriginalUrl[0]));
            ar & LastHttpStatusCode;
            ar & TotalP2PDataBytes;
            ar & TotalRecievedSubPieceCount;
            ar & TotalRequestSubPieceCount;
            ar & TotalAllRequestSubPieceCount;
            ar & TotalUnusedSubPieceCount;
            ar & IpPoolPeerCount;
            ar & DataRate;
            ar & CacheSize;
            ar & CacheFirstBlockId;
            ar & CacheLastBlockId;
            ar & PlayingPosition;
            ar & LeftCapacity;
            ar & RestPlayTime;
            ar & ResourceID;
            ar & IsPlayingPositionBlockFull;
            ar & LivePointBlockId;
            ar & DataRateLevel;
            ar & reserved1;
            ar & reserved2;
            ar & reserved3;
            ar & reserved4;
            ar & JumpTimes;
            ar & NumOfChecksumFailedPieces;
            ar & reserved5;
            ar & ChannelID;
            ar & TotalUdpServerDataBytes;
            ar & PmsStatus;
            ar & UniqueID;
            ar & UdpServerSpeedInfo;
            ar & IsPaused;
            ar & IsReplay;
            ar & MissingSubPieceCountOfFirstBlock;
            ar & ExistSubPieceCountOfFirstBlock;
            ar & P2PPeerSpeedInSecond;
            ar & P2PUdpServerSpeedInSecond;

            ar & framework::container::make_array(Reserved, sizeof(Reserved) / sizeof(Reserved[0]));

            ar & PeerCount;
            for (int i=0; i<PeerCount; i++)
            {
                ar & P2PConnections[i];
            }
        }
    };

    inline std::ostream& operator << (std::ostream& os, const DOWNLOADDRIVER_STATISTIC_INFO& info)
    {
        return os << "DownloadDriverID: " << info.DownloadDriverID
            << ", OriginalUrl: " << info.OriginalUrl
            << ", OriginalReferUrl: " << info.OriginalReferUrl
            << ", HttpDownloaderCount: " << info.HttpDownloaderCount;
    }

    struct PEER_UPLOAD_INFO
    {
        PEER_UPLOAD_INFO()
        {
            Clear();
        }

        uint32_t ip;
        boost::uint16_t port;
        boost::uint32_t upload_speed;
        PEER_INFO peer_info;
        boost::uint8_t resersed[126 - sizeof(PEER_INFO)];

        void Clear()
        {
            memset(this, 0, sizeof(PEER_UPLOAD_INFO));
        }
    };

    struct UPLOAD_INFO
    {
        UPLOAD_INFO()
        {
            Clear();
        }

        boost::uint8_t peer_upload_count;
        boost::uint8_t reserve;
        boost::uint32_t upload_speed;
        boost::uint32_t actual_speed_limit;
        boost::uint32_t upload_subpiece_count;
        boost::uint8_t resersed[116];
        PEER_UPLOAD_INFO peer_upload_info[256];

        void Clear()
        {
            memset(this, 0, sizeof(UPLOAD_INFO));
        }
    };

    typedef struct _BASICPEERINFO
    {
        boost::uint32_t tcp_port;
        boost::uint32_t udp_port;
        boost::uint32_t bs_ip;
        boost::uint32_t tracker_count;
        boost::uint32_t stun_count;
        boost::uint32_t upload_speed;
    }BASICPEERINFO, *LPBASICPEERINFO;
}

// #pragma pack(pop)

#endif  // _STATISTIC_STATISTIC_STRUCTS_H_
