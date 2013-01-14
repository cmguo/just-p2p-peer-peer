//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "statistic/PeerConnectionStatistic.h"
#include "statistic/StatisticUtil.h"
#include "p2sp/p2p/P2SPConfigs.h"

namespace statistic
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_statistic = log4cplus::Logger::getInstance("[peer_connection_statistic]");
#endif
    PeerConnectionStatistic::PeerConnectionStatistic(const boost::asio::ip::udp::endpoint& end_point)
        : is_running_(false)
        , end_point_(end_point)
        , recent_average_rtt_(p2sp::P2SPConfigs::PEERCONNECTION_RTT_RANGE_SIZE)
    {

    }

    PeerConnectionStatistic::p PeerConnectionStatistic::Create(
        const boost::asio::ip::udp::endpoint& end_point)
    {
        return p(new PeerConnectionStatistic(end_point));
    }

    void PeerConnectionStatistic::Start()
    {
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "PeerConnectionStatistic::Start [IN]");

        if (is_running_ == true)
        {
            LOG4CPLUS_WARN_LOG(logger_statistic, "PeerConnectionStatistic is running, return.");
            return;
        }

        is_running_ = true;

        // recent_average_rtt_ = measure::CycleBuffer::Create(
        //    p2sp::P2SPConfigs::PEERCONNECTION_RTT_RANGE_SIZE, measure::CYCLE_MAX_VAL);

        Clear();

        speed_info_.Start();

        LOG4CPLUS_DEBUG_LOG(logger_statistic, "PeerConnectionStatistic::Start [OUT]");
    }

    void PeerConnectionStatistic::Stop()
    {
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "PeerConnectionStatistic::Stop [IN]");
        if (is_running_ == false)
        {
            LOG4CPLUS_WARN_LOG(logger_statistic, "PeerConnectionStatistic is not running, return.");
            return;
        }

        speed_info_.Stop();

        Clear();

        is_running_ = false;
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "PeerConnectionStatistic::Stop [OUT]");
    }

    void PeerConnectionStatistic::Clear()
    {
        if (false == is_running_)
            return;
        // recent_average_rtt_->Clear();
        speed_info_.Clear();
        peer_connection_info_.Clear();
    }

    inline bool PeerConnectionStatistic::IsRunning() const
    {
        return is_running_;
    }

    //////////////////////////////////////////////////////////////////////////
    // Speed Info

    void PeerConnectionStatistic::SubmitDownloadedBytes(boost::uint32_t downloaded_bytes)
    {
        if (false == is_running_)
            return;

        speed_info_.SubmitDownloadedBytes(downloaded_bytes);
    }

    void PeerConnectionStatistic::SubmitUploadedBytes(boost::uint32_t uploaded_bytes)
    {
        if (false == is_running_)
            return;
        speed_info_.SubmitUploadedBytes(uploaded_bytes);
    }

    SPEED_INFO PeerConnectionStatistic::GetSpeedInfo()
    {
        UpdateSpeedInfo();
        return peer_connection_info_.SpeedInfo;
    }

    void PeerConnectionStatistic::UpdateSpeedInfo()
    {
        if (false == is_running_)
            return;

        peer_connection_info_.SpeedInfo = speed_info_.GetSpeedInfo();
    }

    //////////////////////////////////////////////////////////////////////////
    // Peer Connection Info

    P2P_CONNECTION_INFO PeerConnectionStatistic::GetPeerConnectionInfo()
    {
        UpdateSpeedInfo();
        return peer_connection_info_;
    }

    //////////////////////////////////////////////////////////////////////////
    // Misc

    void PeerConnectionStatistic::SetPeerVersion(boost::uint32_t peer_version)
    {
        if (false == is_running_)
            return;
        peer_connection_info_.PeerVersion = peer_version;
    }

    void PeerConnectionStatistic::SetCandidatePeerInfo(const protocol::CandidatePeerInfo& peer_info)
    {
        if (false == is_running_)
            return;
        peer_connection_info_.PeerInfo = peer_info;
    }

    void PeerConnectionStatistic::SetPeerDownloadInfo(const protocol::PEER_DOWNLOAD_INFO& peer_download_info)
    {
        if (false == is_running_)
            return;
        peer_connection_info_.PeerDownloadInfo = peer_download_info;
    }

    void PeerConnectionStatistic::SetBitmap(protocol::SubPieceBuffer bitmap)
    {
        if (false == is_running_)
            return;
        assert(bitmap.Length() <= BITMAP_SIZE);
        base::util::memcpy2(peer_connection_info_.BitMap, sizeof(peer_connection_info_.BitMap), bitmap.Data(), (std::min)(BITMAP_SIZE, (boost::uint32_t)bitmap.Length()));
    }

    void PeerConnectionStatistic::SetBitmap(protocol::BlockMap block_map)
    {
        if (false == is_running_)
            return;
        block_map.GetBytes(peer_connection_info_.BitMap);
    }

    void PeerConnectionStatistic::SetWindowSize(boost::uint8_t window_size)
    {
        if (false == is_running_)
            return;
        peer_connection_info_.WindowSize = window_size;
    }

    void PeerConnectionStatistic::SubmitRTT(boost::uint16_t rtt)
    {
        if (false == is_running_)
            return;

        recent_average_rtt_.Push(rtt);

        peer_connection_info_.RTT_Total += rtt;
        peer_connection_info_.RTT_Count++;
        peer_connection_info_.RTT_Average =
            static_cast<boost::uint16_t>(recent_average_rtt_.Average() + 0.5);

        // (peer_connection_info_.RTT_Count == 0 ? 0 : peer_connection_info_.RTT_Total / peer_connection_info_.RTT_Count);
        boost::uint16_t now_max = peer_connection_info_.RTT_Max;
        peer_connection_info_.RTT_Max = (now_max > rtt ? now_max : rtt);

    }

    boost::uint16_t PeerConnectionStatistic::GetAverageRTT()
    {
        return peer_connection_info_.RTT_Average;
    }

    boost::uint16_t PeerConnectionStatistic::GetLongestRtt()
    {
        return recent_average_rtt_.MaxValue();
    }

    void PeerConnectionStatistic::SetElapsedTime(boost::uint16_t elapsed_time)
    {
        if (false == is_running_)
            return;
        peer_connection_info_.ElapseTime = elapsed_time;
    }

    void PeerConnectionStatistic::SetIsRidInfoValid(bool is_ridinfo_valid)
    {
        if (false == is_running_)
            return;
        peer_connection_info_.IsRidInfoValid = is_ridinfo_valid;
    }

    bool PeerConnectionStatistic::GetIsRidInfoValid() const
    {
        return 0 != peer_connection_info_.IsRidInfoValid;
    }

    void PeerConnectionStatistic::SetSentCount(boost::uint32_t sent_count)
    {
        if (false == is_running_)
            return;
        peer_connection_info_.Sent_Count = sent_count;
    }

    boost::uint32_t PeerConnectionStatistic::GetSentCount()
    {
        return peer_connection_info_.Sent_Count;
    }

    void PeerConnectionStatistic::SetRequestingCount(boost::uint32_t requesting_count)
    {
        if (false == is_running_)
            return;
        peer_connection_info_.Requesting_Count = requesting_count;
    }

    boost::uint32_t PeerConnectionStatistic::GetRequestingCount()
    {
        return peer_connection_info_.Requesting_Count;
    }

    void PeerConnectionStatistic::SetReceivedCount(boost::uint32_t received_count)
    {
        if (false == is_running_)
            return;
        peer_connection_info_.Received_Count = received_count;
    }

    boost::uint32_t PeerConnectionStatistic::GetReceivedCount()
    {
        return peer_connection_info_.Received_Count;
    }

    void PeerConnectionStatistic::SetAssignedLeftSubPieceCount(boost::uint32_t assigned_left_subpiece_count)
    {
        if (false == is_running_)
            return;
        peer_connection_info_.AssignedLeftSubPieceCount = assigned_left_subpiece_count;
    }

    boost::uint16_t PeerConnectionStatistic::GetAssignedLeftSubPieceCount()
    {
        return peer_connection_info_.AssignedLeftSubPieceCount;
    }

    //////////////////////////////////////////////////////////////////////////
    // Peer Guid

    boost::asio::ip::udp::endpoint PeerConnectionStatistic::GetEndpoint() const
    {
        return end_point_;
    }

//     Guid PeerConnectionStatistic::GetPeerGuid() const
//     {
//         return peer_guid_;
//     }
}
