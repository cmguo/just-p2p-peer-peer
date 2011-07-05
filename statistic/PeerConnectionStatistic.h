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

        static p Create(const Guid& peer_id);

    public:

        void Start();

        void Stop();

        void Clear();

        bool IsRunning() const;

    public:

        //////////////////////////////////////////////////////////////////////////
        // Speed Info

        void SubmitDownloadedBytes(uint32_t downloaded_bytes);

        void SubmitUploadedBytes(uint32_t uploaded_bytes);

        SPEED_INFO GetSpeedInfo();

        //////////////////////////////////////////////////////////////////////////
        // Peer Connection Info

        P2P_CONNECTION_INFO GetPeerConnectionInfo();

        //////////////////////////////////////////////////////////////////////////
        // Misc

        void SetPeerVersion(uint32_t peer_version);

        void SetCandidatePeerInfo(const protocol::CandidatePeerInfo& peer_info);

        void SetPeerDownloadInfo(const protocol::PEER_DOWNLOAD_INFO& peer_download_info);

        void SetBitmap(protocol::SubPieceBuffer bitmap);

        void SetBitmap(protocol::BlockMap block_map);

        void SetWindowSize(boost::uint8_t window_size);

        void SetAssignedSubPieceCount(boost::uint8_t assigned_subpiece_count);

        void SetElapsedTime(boost::uint16_t elapsed_time);  // TODO: how?

        void SubmitRTT(boost::uint16_t rtt);

        void SetAverageDeltaTime(uint32_t avg_delt_time);

        void SetSortedValue(uint32_t sorted_value);

        boost::uint16_t GetAverageRTT();

        boost::uint16_t GetLongestRtt();

        uint32_t GetTotalRTTCount();

        void SetIsRidInfoValid(bool is_ridinfo_valid);

        bool GetIsRidInfoValid() const;

        void SetSentCount(uint32_t sent_count);

        uint32_t GetSentCount();

        void SetRequestingCount(uint32_t requesting_count);

        uint32_t GetRequestingCount();

        void SetReceivedCount(uint32_t received_count);

        uint32_t GetReceivedCount();

        void SetAssignedLeftSubPieceCount(uint32_t assigned_left_subpiece_count);

        uint16_t GetAssignedLeftSubPieceCount();


        //////////////////////////////////////////////////////////////////////////
        // Peer Guid

        Guid GetPeerGuid() const;

    private:

        //////////////////////////////////////////////////////////////////////////
        // Speed Info

        void UpdateSpeedInfo();

    private:

        volatile bool is_running_;

        P2P_CONNECTION_INFO peer_connection_info_;

        SpeedInfoStatistic speed_info_;

        measure::CycleBuffer recent_average_rtt_;

        Guid peer_guid_;

    private:
        PeerConnectionStatistic(const Guid& peer_id);

    };

   inline  void PeerConnectionStatistic::SetAssignedSubPieceCount(boost::uint8_t assigned_subpiece_count)
    {
        peer_connection_info_.AssignedSubPieceCount = assigned_subpiece_count;
    }

   inline void PeerConnectionStatistic::SetAverageDeltaTime(uint32_t avg_delt_time)
   {
       peer_connection_info_.AverageDeltaTime = avg_delt_time;
   }

   inline void PeerConnectionStatistic::SetSortedValue(uint32_t sorted_value)
   {
       peer_connection_info_.SortedValue = sorted_value;
   }

   inline uint32_t PeerConnectionStatistic::GetTotalRTTCount()
   {
       return peer_connection_info_.RTT_Count;
   }
}

#endif  // _STATISTIC_PEER_CONNECTION_STATISTIC_H_
