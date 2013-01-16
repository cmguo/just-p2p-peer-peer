//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// P2PDownloaderStatistic.h

#ifndef _STATISTIC_P2P_DOWNLOADER_STATISTIC_H_
#define _STATISTIC_P2P_DOWNLOADER_STATISTIC_H_

#include "statistic/StatisticStructs.h"
#include "statistic/SpeedInfoStatistic.h"
#include "statistic/PeerConnectionStatistic.h"
#include "interprocess/SharedMemory.h"

namespace statistic
{
    class P2PDownloaderStatistic
        : public boost::noncopyable
        , public boost::enable_shared_from_this<P2PDownloaderStatistic>
    {
    public:

        typedef boost::shared_ptr<P2PDownloaderStatistic> p;

        static p Create(const RID& rid);

    public:

        void Start();

        void Stop();

        void Clear();

        bool IsRunning() const;

        void OnShareMemoryTimer(boost::uint32_t times);

        const P2PDOWNLOADER_STATISTIC_INFO& TakeSnapshot();

    public:

        //////////////////////////////////////////////////////////////////////////
        // Attach & Detach

        PeerConnectionStatistic::p AttachPeerConnectionStatistic(
            boost::asio::ip::udp::endpoint end_point);

        bool DetachPeerConnectionStatistic(
            const boost::asio::ip::udp::endpoint& end_point);

        bool DetachPeerConnectionStatistic(const PeerConnectionStatistic::p peer_connection_statistic);

        bool DetachAllPeerConnectionStatistic();

    public:

        //////////////////////////////////////////////////////////////////////////
        // Speed Info
        void SubmitDownloadedBytes(boost::uint32_t downloaded_bytes);
        void SubmitUploadedBytes(boost::uint32_t uploaded_bytes);

        void SubmitPeerDownloadedBytes(boost::uint32_t downloaded_bytes);
        void SubmitPeerUploadedBytes(boost::uint32_t uploaded_bytes);

        void SubmitSnDownloadedBytes(boost::uint32_t downloaded_bytes);
        void SubmitSnUploadedBytes(boost::uint32_t uploaded_bytes);

        void SubmitDoListRequestCount(boost::uint32_t station_no);
        void SubmitDoListReponseCount(boost::uint32_t station_no);

        SPEED_INFO GetSpeedInfo();

        SPEED_INFO_EX GetSpeedInfoEx();

        SPEED_INFO GetPeerSpeedInfo();

        SPEED_INFO_EX GetPeerSpeedInfoEx();

        SPEED_INFO GetSnSpeedInfo();

        SPEED_INFO_EX GetSnSpeedInfoEx();

        boost::uint32_t GetElapsedTimeInMilliSeconds();

        //////////////////////////////////////////////////////////////////////////
        // Resource Info

        void SetFileLength(boost::uint32_t file_length);

        void SetBlockNum(boost::uint16_t block_num);

        void SetBlockSize(boost::uint16_t block_size);

        //////////////////////////////////////////////////////////////////////////
        // IP Pool Info

        void SetIpPoolPeerCount(boost::uint16_t ip_pool_peer_count);

        void SetExchangingPeerCount(boost::uint8_t exchanging_peer_count);

        void SetConnectingPeerCount(boost::uint8_t connecting_peer_count);

        void SetFullBlockPeerCount(boost::uint16_t full_block_peer_count);

        boost::uint16_t GetConnectedPeerCount();

        boost::uint16_t GetIpPoolPeerCount();
        //////////////////////////////////////////////////////////////////////////
        // Algorithm Info

        void SetTotalWindowSize(boost::uint16_t total_window_size);

        void SetNonConsistentSize(boost::uint16_t non_consistent_size);

        void SubmitAssignedSubPieceCount(boost::uint16_t assigned_sub_piece_count);

        void SubmitUnusedSubPieceCount(boost::uint16_t unused_sub_piece_count);

        void SubmitRecievedSubPieceCount(boost::uint16_t received_sub_piece_count);

        void SubmitRequestSubPieceCount(boost::uint16_t request_sub_piece_count);

        // 提交每秒发起的连接数
        void SubmitConnectCount(boost::uint16_t connect_count);

        // 提交每秒踢掉的连接数
        void SubmitKickCount(boost::uint16_t kick_count);

        boost::uint16_t GetTotalAssignedSubPieceCount();

        boost::uint16_t GetSubPieceRetryRate();

        boost::uint16_t GetUDPLostRate();

        boost::uint32_t GetTotalListRequestCount() const { return total_list_request_count_;}

        boost::uint32_t GetTotalListResponseCount() const { return total_list_response_count_;}

        string GetTrackerListRequestAndResponseString();

        //////////////////////////////////////////////////////////////////////////
        // Misc

        boost::uint32_t GetMaxP2PConnectionCount() const;

        RID GetResourceID() const;

        //////////////////////////////////////////////////////////////////////////
        // P2P Data Bytes

        void SubmitP2PPeerDataBytesWithRedundance(boost::uint32_t p2p_data_bytes);

        void SubmitP2pLocationDataBytesWithRedundance(boost::uint8_t location,boost::uint32_t bytes);

        void SubmitP2PPeerDataBytesWithoutRedundance(boost::uint32_t p2p_data_bytes);

        void SubmitP2PSnDataBytesWithRedundance(boost::uint32_t p2p_data_bytes);

        void SubmitP2PSnDataBytesWithoutRedundance(boost::uint32_t p2p_data_bytes);

        void ClearP2PDataBytes();

        boost::uint32_t GetTotalP2PPeerDataBytesWithRedundance();

        boost::uint32_t GetTotalP2PPeerDataBytesWithoutRedundance();

        boost::uint32_t GetTotalP2PSnDataBytesWithRedundance();

        boost::uint32_t GetTotalP2PSnDataBytesWithoutRedundance();

        std::string GetP2PLocationDataBytesWithRedundance();

        void SetEmptySubpieceDistance(boost::uint32_t empty_subpiece_distance);

        void SubmitPeerConnectRequestCount(boost::uint8_t nat_type);
        void SubmitPeerConnectSuccessCount(boost::uint8_t nat_type);
        string GetPeerConnectString() const;

        boost::uint32_t GetConnectionStatisticSize() const
        {
            return peer_connection_statistic_map_.size();
        }

    private:

        //////////////////////////////////////////////////////////////////////////
        // Updates

        void UpdateSpeedInfo();

        void UpdatePeerConnectionInfo();

        void UpdateRate();

        //////////////////////////////////////////////////////////////////////////
        // Shared Memory

        bool CreateSharedMemory();

        string GetSharedMemoryName();

        boost::uint32_t GetSharedMemorySize();

        //////////////////////////////////////////////////////////////////////////
        // Misc

        void ClearMaps();

    private:

        typedef std::map<boost::asio::ip::udp::endpoint, PeerConnectionStatistic::p> PeerConnectionStatisticMap;

    private:

        volatile bool is_running_;

        P2PDOWNLOADER_STATISTIC_INFO p2p_downloader_statistic_info_;

        SpeedInfoStatistic speed_info_;
        SpeedInfoStatistic peer_speed_info_;
        SpeedInfoStatistic sn_speed_info_;

        boost::uint32_t total_list_request_count_;
        boost::uint32_t total_list_response_count_;
        std::map<boost::uint32_t, int> tracker_list_request_count_;
        std::map<boost::uint32_t, int> tracker_list_response_count_;

        std::map<boost::uint16_t, std::pair<boost::uint32_t, boost::uint32_t> > nat_type_connection_statistic_;

        RID resource_id_;

        interprocess::SharedMemory shared_memory_;

        PeerConnectionStatisticMap peer_connection_statistic_map_;

    private:

        P2PDownloaderStatistic() {}

        P2PDownloaderStatistic(const RID& rid);

    };

    //////////////////////////////////////////////////////////////////////////
    // P2P Data Bytes

    inline void P2PDownloaderStatistic::SubmitP2PPeerDataBytesWithRedundance(boost::uint32_t p2p_data_bytes)
    {
        p2p_downloader_statistic_info_.TotalP2PPeerDataBytesWithRedundance += p2p_data_bytes;
    }

    inline void P2PDownloaderStatistic::SubmitP2pLocationDataBytesWithRedundance(boost::uint8_t location,boost::uint32_t bytes)
    {
        p2p_downloader_statistic_info_.SubmitP2pLocationDataBytesWithRedundance(location,bytes);
    }

    inline void P2PDownloaderStatistic::SubmitP2PPeerDataBytesWithoutRedundance(boost::uint32_t p2p_data_bytes)
    {
        p2p_downloader_statistic_info_.TotalP2PPeerDataBytesWithoutRedundance += p2p_data_bytes;
    }

    inline void P2PDownloaderStatistic::SubmitP2PSnDataBytesWithRedundance(boost::uint32_t p2p_data_bytes)
    {
        p2p_downloader_statistic_info_.TotalP2PSnDataBytesWithRedundance += p2p_data_bytes;
    }

    inline void P2PDownloaderStatistic::SubmitP2PSnDataBytesWithoutRedundance(boost::uint32_t p2p_data_bytes)
    {
        p2p_downloader_statistic_info_.TotalP2PSnDataBytesWithoutRedundance += p2p_data_bytes;
    }

    inline  void P2PDownloaderStatistic::ClearP2PDataBytes()
    {
        p2p_downloader_statistic_info_.TotalP2PPeerDataBytesWithRedundance = 0;
        p2p_downloader_statistic_info_.TotalP2PPeerDataBytesWithoutRedundance = 0;
        p2p_downloader_statistic_info_.TotalP2PSnDataBytesWithRedundance = 0;
        p2p_downloader_statistic_info_.TotalP2PSnDataBytesWithoutRedundance = 0;
        memset(p2p_downloader_statistic_info_.P2pLocationDataBytesWithRedundance,
            0,sizeof(p2p_downloader_statistic_info_.P2pLocationDataBytesWithRedundance));
    }

    inline boost::uint32_t P2PDownloaderStatistic::GetTotalP2PPeerDataBytesWithRedundance()
    {
        return p2p_downloader_statistic_info_.TotalP2PPeerDataBytesWithRedundance;
    }

    inline std::string P2PDownloaderStatistic::GetP2PLocationDataBytesWithRedundance()
    {
        std::ostringstream location_ss; 
        //格式: 0:1024,1:4096,2:34892,4:34380,(地域分类):(下载字节数)
        //如果某一个地域分类的下载字节数为0，就不上报了。

        boost::uint32_t * location_bytes = &p2p_downloader_statistic_info_.P2pLocationDataBytesWithRedundance[0];
       
        for (int i=0; 
            i<(sizeof(p2p_downloader_statistic_info_.P2pLocationDataBytesWithRedundance) / sizeof(p2p_downloader_statistic_info_.P2pLocationDataBytesWithRedundance[0]));
            ++i)
        {
            if(location_bytes[i] != 0)
            {
                location_ss << i<<":"<<location_bytes[i]<<",";
            }          
        }

        std::string location_str = location_ss.str();
        if(location_str.size() > 0  && (','==location_str[location_str.size()-1] ))
        {
            //去掉最后一个 ，
            location_str = location_str.substr(0,location_str.size()-1);
        }
        return location_str;
    }

    inline boost::uint32_t P2PDownloaderStatistic::GetTotalP2PPeerDataBytesWithoutRedundance()
    {
        return p2p_downloader_statistic_info_.TotalP2PPeerDataBytesWithoutRedundance;
    }

    inline boost::uint32_t P2PDownloaderStatistic::GetTotalP2PSnDataBytesWithRedundance()
    {
        return p2p_downloader_statistic_info_.TotalP2PSnDataBytesWithRedundance;
    }

    inline boost::uint32_t P2PDownloaderStatistic::GetTotalP2PSnDataBytesWithoutRedundance()
    {
        return p2p_downloader_statistic_info_.TotalP2PSnDataBytesWithoutRedundance;
    }

    //////////////////////////////////////////////////////////////////////////
    // P2P Packets

    inline void P2PDownloaderStatistic::SubmitAssignedSubPieceCount(boost::uint16_t assigned_sub_piece_count)
    {
        p2p_downloader_statistic_info_.TotalAssignedSubPieceCount = assigned_sub_piece_count;
    }

    inline void P2PDownloaderStatistic::SubmitUnusedSubPieceCount(boost::uint16_t unused_sub_piece_count)
    {
        p2p_downloader_statistic_info_.TotalUnusedSubPieceCount += unused_sub_piece_count;
    }

    inline void P2PDownloaderStatistic::SubmitRecievedSubPieceCount(boost::uint16_t received_sub_piece_count)
    {
        p2p_downloader_statistic_info_.TotalRecievedSubPieceCount += received_sub_piece_count;
    }

    inline void P2PDownloaderStatistic::SubmitRequestSubPieceCount(boost::uint16_t request_sub_piece_count)
    {
        p2p_downloader_statistic_info_.TotalRequestSubPieceCount += request_sub_piece_count;
    }

    //////////////////////////////////////////////////////////////////////////
    // Speed Info
    inline void P2PDownloaderStatistic::SubmitDownloadedBytes(boost::uint32_t downloaded_bytes)
    {
        speed_info_.SubmitDownloadedBytes(downloaded_bytes);
    }

    inline void P2PDownloaderStatistic::SubmitUploadedBytes(boost::uint32_t uploaded_bytes)
    {
        speed_info_.SubmitUploadedBytes(uploaded_bytes);
    }

    inline void P2PDownloaderStatistic::SubmitPeerDownloadedBytes(boost::uint32_t downloaded_bytes)
    {
        peer_speed_info_.SubmitDownloadedBytes(downloaded_bytes);
    }

    inline void P2PDownloaderStatistic::SubmitPeerUploadedBytes(boost::uint32_t uploaded_bytes)
    {
        peer_speed_info_.SubmitUploadedBytes(uploaded_bytes);
    }

    inline void P2PDownloaderStatistic::SubmitSnDownloadedBytes(boost::uint32_t downloaded_bytes)
    {
        sn_speed_info_.SubmitDownloadedBytes(downloaded_bytes);
    }

    inline void P2PDownloaderStatistic::SubmitSnUploadedBytes(boost::uint32_t uploaded_bytes)
    {
        sn_speed_info_.SubmitUploadedBytes(uploaded_bytes);
    }

    inline void P2PDownloaderStatistic::SubmitDoListRequestCount(boost::uint32_t station_no)
    {
        total_list_request_count_++;

        if (tracker_list_request_count_.find(station_no) != tracker_list_request_count_.end())
        {
            tracker_list_request_count_[station_no]++;
        }
        else
        {
            tracker_list_request_count_[station_no] = 1;
        }
    }

    inline void P2PDownloaderStatistic::SubmitDoListReponseCount(boost::uint32_t station_no)
    {
        total_list_response_count_++;

        if (tracker_list_response_count_.find(station_no) != tracker_list_response_count_.end())
        {
            tracker_list_response_count_[station_no]++;
        }
        else
        {
            tracker_list_response_count_[station_no] = 1;
        }
    }
}

#endif  // _STATISTIC_P2P_DOWNLOADER_STATISTIC_H_
