#ifndef LIVE_DAC_STOP_DATA_STRUCT_H
#define LIVE_DAC_STOP_DATA_STRUCT_H

namespace p2sp
{
    // 内核在看完一个直播频道后提交的数据
    typedef struct _LIVE_DAC_STOP_DATA_STRUCT
    {
        static const boost::uint8_t MaxHttpStatisticCount = 5;

        vector<RID>             ResourceIDs;            // 资源ID
        boost::uint32_t         PeerVersion[4];         // 内核版本：major, minor, micro, extra
        vector<boost::uint32_t> DataRates;              // 码流率
        string                  OriginalUrl;            // Url
        boost::uint32_t         P2PDownloadBytes;       // P2P下载字节数(不包括UdpServer)
        boost::uint32_t         HttpDownloadBytes;      // Http下载字节数
        boost::uint32_t         TotalDownloadBytes;     // 总下载字节数
        boost::uint32_t         AvgP2PDownloadSpeed;    // P2P平均速度
        boost::uint32_t         MaxP2PDownloadSpeed;    // P2P最大速度
        boost::uint32_t         MaxHttpDownloadSpeed;   // Http最大速度
        boost::uint32_t         ConnectedPeerCount;     // 连接上的节点数目
        boost::uint32_t         QueriedPeerCount;       // 查询到的节点数目
        boost::uint32_t         StartPosition;          // 开始播放点
        boost::uint32_t         JumpTimes;              // 跳跃次数
        boost::uint32_t         NumOfCheckSumFailedPieces;// 校验失败的piece个数
        boost::uint32_t         SourceType;             //
        RID                     ChannelID;              // 频道ID
        boost::uint32_t         UdpDownloadBytes;       // 从UdpServer下载的字节数
        boost::uint32_t         MaxUdpServerDownloadSpeed;  // 从UdpServer下载的最大速度
        boost::uint32_t         UploadBytes;            // 上传字节数
        boost::uint32_t         DownloadTime;           // 下载时间
        boost::uint32_t         TimesOfUseCdnBecauseLargeUpload;
        boost::uint32_t         TimeElapsedUseCdnBecauseLargeUpload;
        boost::uint32_t         DownloadBytesUseCdnBecauseLargeUpload;
        boost::uint32_t         TimesOfUseUdpServerBecauseUrgent;
        boost::uint32_t         TimeElapsedUseUdpServerBecauseUrgent;
        boost::uint32_t         DownloadBytesUseUdpServerBecauseUrgent;
        boost::uint32_t         TimesOfUseUdpServerBecauseLargeUpload;
        boost::uint32_t         TimeElapsedUseUdpServerBecauseLargeUpload;
        boost::uint32_t         DownloadBytesUseUdpServerBecauseLargeUpload;
        boost::uint32_t         MaxUploadSpeedIncludeSameSubnet;
        boost::uint32_t         MaxUploadSpeedExcludeSameSubnet;
        boost::uint32_t         MaxUnlimitedUploadSpeedInRecord;
        boost::uint8_t          ChangeToP2PConditionWhenStart;
        boost::uint32_t         ChangedToHttpTimesWhenUrgent;
        boost::uint32_t         BlockTimesWhenUseHttpUnderUrgentSituation;
        boost::uint32_t         MaxUploadSpeedDuringThisConnection;
        boost::uint32_t         AverageUploadConnectionCount;
        boost::uint32_t         TimeOfReceivingFirstConnectRequest;
        boost::uint32_t         TimeOfSendingFirstSubPiece;
        boost::uint32_t         TimeOfNonblankUploadConnections;
        boost::uint8_t          NatType;
        boost::uint32_t         HttpDownloadBytesWhenStart;
        boost::uint32_t         UploadBytesDuringThisConnection;
        boost::uint32_t         IsNotifyRestart;
        boost::uint32_t         MaxPushDataInterval;
        boost::uint32_t         AverageOfRestPlayableTime;
        boost::uint32_t         VarianceOfRestPlayableTime;
        boost::uint32_t         AverageConnectPeersCountInMinute;
        boost::uint32_t         TotalReceivedSubPiecePacketCount;
        boost::uint32_t         ReverseSubPiecePacketCount;
        boost::uint32_t         BandWidth;
        boost::uint32_t         UploadBytesWhenUsingCDNBecauseOfLargeUpload;
        boost::uint32_t         MinUdpServerCountWhenNeeded;
        boost::uint32_t         MaxUdpServerCountWhenNeeded;
        boost::uint32_t         MinConnectUdpServerCountWhenNeeded;
        boost::uint32_t         MaxConnectUdpServerCountWhenNeeded;
        boost::uint32_t         MinAnnounceResponseFromUdpServer;
        boost::uint32_t         MaxAnnounceResponseFromUdpServer;
        boost::uint32_t         MinRatioOfResponseToRequestFromUdpserver;
        boost::uint32_t         MaxRatioOfResponseToRequestFromUdpserver;
        boost::uint8_t          BWType;
        boost::uint32_t         PPbscfLength;  // ppbscf文件长度
        string                  HttpDownloadStartTimeEveryTime;  // 开始使用http的时间
        string                  HttpDownloadEndTimeEveryTime;  // 结束使用http的时间
        string                  ReasonOfUsingHttpEveryTime;  // 使用http的原因
        string                  ReasonOfStopingHttpEveryTime;  // 结束使用http的原因
        string                  HttpDownloadBytesEveryTime;  // 使用http的下载量

        string ToString()
        {
            std::ostringstream log_stream;

            log_stream << "C=";
            for (boost::uint32_t i = 0; i < ResourceIDs.size(); ++i)
            {
                if (i != 0)
                {
                    log_stream << "@";
                }
                log_stream << ResourceIDs[i].to_string();
            }

            log_stream << "&D=" << PeerVersion[0] << "." << PeerVersion[1] << "."
                << PeerVersion[2] << "." << PeerVersion[3];

            log_stream << "&E=";
            for (boost::uint32_t i = 0; i < DataRates.size(); ++i)
            {
                if (i != 0)
                {
                    log_stream << "@";
                }
                log_stream << DataRates[i];
            }

            log_stream << "&F=" << OriginalUrl;
            log_stream << "&G=" << P2PDownloadBytes;
            log_stream << "&H=" << HttpDownloadBytes;
            log_stream << "&I=" << TotalDownloadBytes;
            log_stream << "&J=" << AvgP2PDownloadSpeed;
            log_stream << "&K=" << MaxP2PDownloadSpeed;
            log_stream << "&L=" << MaxHttpDownloadSpeed;
            log_stream << "&M=" << ConnectedPeerCount;
            log_stream << "&N=" << QueriedPeerCount;
            log_stream << "&O=" << StartPosition;
            log_stream << "&P=" << JumpTimes;
            log_stream << "&Q=" << NumOfCheckSumFailedPieces;
            log_stream << "&R=" << SourceType;
            log_stream << "&S=" << ChannelID;
            log_stream << "&T=" << UdpDownloadBytes;
            log_stream << "&U=" << MaxUdpServerDownloadSpeed;
            log_stream << "&V=" << UploadBytes;
            log_stream << "&W=" << DownloadTime;
            log_stream << "&X=" << TimesOfUseCdnBecauseLargeUpload;
            log_stream << "&Y=" << TimeElapsedUseCdnBecauseLargeUpload;
            log_stream << "&Z=" << DownloadBytesUseCdnBecauseLargeUpload;
            log_stream << "&A1=" << TimesOfUseUdpServerBecauseUrgent;
            log_stream << "&B1=" << TimeElapsedUseUdpServerBecauseUrgent;
            log_stream << "&C1=" << DownloadBytesUseUdpServerBecauseUrgent;
            log_stream << "&D1=" << TimesOfUseUdpServerBecauseLargeUpload;
            log_stream << "&E1=" << TimeElapsedUseUdpServerBecauseLargeUpload;
            log_stream << "&F1=" << DownloadBytesUseUdpServerBecauseLargeUpload;
            log_stream << "&G1=" << MaxUploadSpeedIncludeSameSubnet;
            log_stream << "&H1=" << MaxUploadSpeedExcludeSameSubnet;
            log_stream << "&I1=" << MaxUnlimitedUploadSpeedInRecord;
            log_stream << "&J1=" << (uint32_t)ChangeToP2PConditionWhenStart;
            log_stream << "&K1=" << ChangedToHttpTimesWhenUrgent;
            log_stream << "&L1=" << BlockTimesWhenUseHttpUnderUrgentSituation;
            log_stream << "&M1=" << MaxUploadSpeedDuringThisConnection;
            log_stream << "&N1=" << AverageUploadConnectionCount;
            log_stream << "&O1=" << TimeOfReceivingFirstConnectRequest;
            log_stream << "&P1=" << TimeOfSendingFirstSubPiece;
            log_stream << "&Q1=" << TimeOfNonblankUploadConnections;
            log_stream << "&R1=" << (uint32_t)NatType;
            log_stream << "&S1=" << HttpDownloadBytesWhenStart;
            log_stream << "&T1=" << UploadBytesDuringThisConnection;
            log_stream << "&U1=" << IsNotifyRestart;
            log_stream << "&V1=" << MaxPushDataInterval;
            log_stream << "&W1=" << AverageOfRestPlayableTime;
            log_stream << "&X1=" << VarianceOfRestPlayableTime;
            log_stream << "&Y1=" << AverageConnectPeersCountInMinute;
            log_stream << "&Z1=" << TotalReceivedSubPiecePacketCount;
            log_stream << "&A2=" << ReverseSubPiecePacketCount;
            log_stream << "&B2=" << BandWidth;
            log_stream << "&C2=" << UploadBytesWhenUsingCDNBecauseOfLargeUpload;
            log_stream << "&D2=" << MinUdpServerCountWhenNeeded;
            log_stream << "&E2=" << MaxUdpServerCountWhenNeeded;
            log_stream << "&F2=" << MinConnectUdpServerCountWhenNeeded;
            log_stream << "&G2=" << MaxConnectUdpServerCountWhenNeeded;
            log_stream << "&H2=" << MinAnnounceResponseFromUdpServer;
            log_stream << "&I2=" << MaxAnnounceResponseFromUdpServer;
            log_stream << "&J2=" << MinRatioOfResponseToRequestFromUdpserver;
            log_stream << "&K2=" << MaxRatioOfResponseToRequestFromUdpserver;
            log_stream << "&L2=" << (uint32_t)BWType;
            log_stream << "&M2=" << PPbscfLength;
            log_stream << "&N2=" << HttpDownloadStartTimeEveryTime;
            log_stream << "&O2=" << HttpDownloadEndTimeEveryTime;
            log_stream << "&P2=" << ReasonOfUsingHttpEveryTime;
            log_stream << "&Q2=" << ReasonOfStopingHttpEveryTime;
            log_stream << "&R2=" << HttpDownloadBytesEveryTime;

            return log_stream.str();
        }
    } LIVE_DAC_STOP_DATA_STRUCT;
}

#endif