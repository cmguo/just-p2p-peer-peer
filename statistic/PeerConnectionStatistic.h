//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// PeerConnectionStatistic.h

#ifndef _STATISTIC_PEER_CONNECTION_STATISTIC_H_
#define _STATISTIC_PEER_CONNECTION_STATISTIC_H_

#include "statistic/StatisticStructs.h"
#include "statistic/SpeedInfoStatistic.h"
#include "measure/CycleBuffer.h"

namespace statistic
{
    class PeerConnectionStatistic
        : public boost::noncopyable
        , public boost::enable_shared_from_this<PeerConnectionStatistic>
    {
    public:

        typedef boost::shared_ptr<PeerConnectionStatistic> p;

        static p Create(const boost::asio::ip::udp::endpoint& end_point);

    public:

        void Start();

        void Stop();

        void Clear();

        bool IsRunning() const;

    public:

        //////////////////////////////////////////////////////////////////////////
        // Speed Info

        void SubmitDownloadedBytes(boost::uint32_t downloaded_bytes);

        void SubmitUploadedBytes(boost::uint32_t uploaded_bytes);

        SPEED_INFO GetSpeedInfo();

        //////////////////////////////////////////////////////////////////////////
        // Peer Connection Info

        P2P_CONNECTION_INFO GetPeerConnectionInfo();

        //////////////////////////////////////////////////////////////////////////
        // Misc

        void SetPeerVersion(boost::uint32_t peer_version);

        void SetCandidatePeerInfo(const protocol::CandidatePeerInfo& peer_info);

        void SetPeerDownloadInfo(const protocol::PEER_DOWNLOAD_INFO& peer_download_info);

        void SetBitmap(protocol::SubPieceBuffer bitmap);

        void SetBitmap(protocol::BlockMap block_map);

        void SetWindowSize(boost::uint8_t window_size);

        void SetAssignedSubPieceCount(boost::uint8_t assigned_subpiece_count);

        void SetElapsedTime(boost::uint16_t elapsed_time);  // TODO: how?

        void SubmitRTT(boost::uint16_t rtt);

        void SetAverageDeltaTime(boost::uint32_t avg_delt_time);

        void SetSortedValue(boost::uint32_t sorted_value);

        boost::uint16_t GetAverageRTT();

        boost::uint16_t GetLongestRtt();

        boost::uint32_t GetTotalRTTCount();

        void SetIsRidInfoValid(bool is_ridinfo_valid);

        bool GetIsRidInfoValid() const;

        void SetSentCount(boost::uint32_t sent_count);

        boost::uint32_t GetSentCount();

        void SetRequestingCount(boost::uint32_t requesting_count);

        boost::uint32_t GetRequestingCount();

        void SetReceivedCount(boost::uint32_t received_count);

        boost::uint32_t GetReceivedCount();

        void SetAssignedLeftSubPieceCount(boost::uint32_t assigned_left_subpiece_count);

        boost::uint16_t GetAssignedLeftSubPieceCount();


        //////////////////////////////////////////////////////////////////////////
        // Peer Guid
        boost::asio::ip::udp::endpoint GetEndpoint() const;

    private:

        //////////////////////////////////////////////////////////////////////////
        // Speed Info

        void UpdateSpeedInfo();

    private:

        volatile bool is_running_;

        P2P_CONNECTION_INFO peer_connection_info_;

        SpeedInfoStatistic speed_info_;

        measure::CycleBuffer recent_average_rtt_;

        boost::asio::ip::udp::endpoint end_point_;
        //Guid peer_guid_;

    private:
        PeerConnectionStatistic(const boost::asio::ip::udp::endpoint& end_point);

    };

   inline  void PeerConnectionStatistic::SetAssignedSubPieceCount(boost::uint8_t assigned_subpiece_count)
    {
        peer_connection_info_.AssignedSubPieceCount = assigned_subpiece_count;
    }

   inline void PeerConnectionStatistic::SetAverageDeltaTime(boost::uint32_t avg_delt_time)
   {
       peer_connection_info_.AverageDeltaTime = avg_delt_time;
   }

   inline void PeerConnectionStatistic::SetSortedValue(boost::uint32_t sorted_value)
   {
       peer_connection_info_.SortedValue = sorted_value;
   }

   inline boost::uint32_t PeerConnectionStatistic::GetTotalRTTCount()
   {
       return peer_connection_info_.RTT_Count;
   }
}

#endif  // _STATISTIC_PEER_CONNECTION_STATISTIC_H_
