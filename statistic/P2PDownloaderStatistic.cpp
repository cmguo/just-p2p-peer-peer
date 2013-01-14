//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "statistic/P2PDownloaderStatistic.h"
#include "statistic/StatisticUtil.h"

#include <util/archive/ArchiveBuffer.h>
#include <util/archive/LittleEndianBinaryOArchive.h>

namespace statistic
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_statistic = log4cplus::Logger::getInstance("[p2p_downloader_statistic]");
#endif
    P2PDownloaderStatistic::P2PDownloaderStatistic(const RID& rid)
        : is_running_(false)
        , resource_id_(rid)
        , total_list_request_count_(0)
        , total_list_response_count_(0)
    {
    }

    P2PDownloaderStatistic::p P2PDownloaderStatistic::Create(const RID& rid)
    {
        return p(new P2PDownloaderStatistic(rid));
    }

    void P2PDownloaderStatistic::Start()
    {
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "P2PDownloaderStatistic::Start [IN]");
        if (is_running_ == true)
        {
            LOG4CPLUS_WARN_LOG(logger_statistic, "P2PDownloaderStatistic is running, return.");
            return;
        }

        is_running_ = true;

        Clear();

        p2p_downloader_statistic_info_.ResourceID = resource_id_;

        speed_info_.Start();
        peer_speed_info_.Start();
        sn_speed_info_.Start();

        if (false == CreateSharedMemory())
        {
            LOG4CPLUS_ERROR_LOG(logger_statistic, "Create P2PDownloaderStatistic Shared Memory Failed.");
        }

        LOG4CPLUS_DEBUG_LOG(logger_statistic, "P2PDownloaderStatistic::Start [OUT]");
    }

    void P2PDownloaderStatistic::Stop()
    {
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "P2PDownloaderStatistic::Stop [IN]");
        if (is_running_ == false)
        {
            LOG4CPLUS_DEBUG_LOG(logger_statistic, "P2PDownloaderStatistic is not running. Return.");
            return;
        }

        speed_info_.Stop();
        peer_speed_info_.Stop();
        sn_speed_info_.Stop();

        Clear();
        shared_memory_.Close();

        is_running_ = false;
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "P2PDownloaderStatistic::Stop [OUT]");
    }

    void P2PDownloaderStatistic::Clear()
    {
        speed_info_.Clear();
        peer_speed_info_.Clear();
        sn_speed_info_.Clear();
        p2p_downloader_statistic_info_.Clear();
        DetachAllPeerConnectionStatistic();
    }

    bool P2PDownloaderStatistic::IsRunning() const
    {
        return is_running_;
    }

    const P2PDOWNLOADER_STATISTIC_INFO& P2PDownloaderStatistic::TakeSnapshot()
    {
        UpdateRate();
        UpdateSpeedInfo();
        UpdatePeerConnectionInfo();
        return p2p_downloader_statistic_info_;
    }

    void P2PDownloaderStatistic::OnShareMemoryTimer(boost::uint32_t times)
    {
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "P2PDownloaderStatistic::OnShareMemoryTimer [IN], times: " << times);

        if (is_running_ == false)
        {
            LOG4CPLUS_WARN_LOG(logger_statistic, "P2PDownloaderStatistic is not running. Return.");
            return;
        }

        TakeSnapshot();

        P2PDOWNLOADER_STATISTIC_INFO& info = p2p_downloader_statistic_info_;

        // copy
        if (NULL != shared_memory_.GetView())
        {
            util::archive::ArchiveBuffer<> buf((char*)shared_memory_.GetView(), GetSharedMemorySize());
            util::archive::LittleEndianBinaryOArchive<> oa(buf);
            oa << info;
            assert(oa);
            // memcpy(shared_memory_.GetView(), &info, GetSharedMemorySize());
        }
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "Copied P2PDOWNLOADER_STATISTIC_INFO into Shared Memory: " 
            << GetSharedMemoryName());
        LOG4CPLUS_DEBUG_LOG(logger_statistic,"P2PDownloaderStatistic::OnShareMemoryTimer [OUT]");
    }

    //////////////////////////////////////////////////////////////////////////
    // Attach & Detach

    PeerConnectionStatistic::p P2PDownloaderStatistic::AttachPeerConnectionStatistic(
        boost::asio::ip::udp::endpoint end_point)
    {
        PeerConnectionStatistic::p peer_connection_info_ = PeerConnectionStatistic::Create(end_point);

        if (is_running_ == false)
        {
            LOG4CPLUS_WARN_LOG(logger_statistic, "PeerConnectionStatistic is not running. Return null.");
            return peer_connection_info_;
        }

        // 判断个数
        if (peer_connection_statistic_map_.size() == GetMaxP2PConnectionCount())
        {
            LOG4CPLUS_WARN_LOG(logger_statistic, "Peer Connection Map is Full, size: " << 
                GetMaxP2PConnectionCount() << ". Return null.");
            return peer_connection_info_;
        }

        PeerConnectionStatisticMap::iterator it = peer_connection_statistic_map_.find(end_point);

        // 存在, 返回空
        if (it != peer_connection_statistic_map_.end())
        {
            return peer_connection_statistic_map_[end_point];
        }

        peer_connection_statistic_map_[end_point] = peer_connection_info_;

        // start
        peer_connection_info_->Start();
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "Peer Connection Statistic " << peer_connection_info_ << 
            " is created and started.");


        LOG4CPLUS_DEBUG_LOG(logger_statistic, "P2PDownloaderStatistic::AttachPeerConnectionStatistic [OUT]");
        return peer_connection_info_;
    }

    bool P2PDownloaderStatistic::DetachPeerConnectionStatistic(const boost::asio::ip::udp::endpoint& end_point)
    {
        if (is_running_ == false)
        {
            LOG4CPLUS_WARN_LOG(logger_statistic, "P2PDownloaderStatistic is not running. Return false.");
            return false;
        }

        PeerConnectionStatisticMap::iterator it = peer_connection_statistic_map_.find(end_point);

        // 不存在, 返回
        if (it == peer_connection_statistic_map_.end())
        {
            return false;
        }

        it->second->Stop();
        peer_connection_statistic_map_.erase(it);

        LOG4CPLUS_DEBUG_LOG(logger_statistic, "P2PDownloaderStatistic::DetachPeerConnectionStatistic [OUT]");
        return true;
    }

    bool P2PDownloaderStatistic::DetachPeerConnectionStatistic(const PeerConnectionStatistic::p peer_connection_statistic)
    {
        return DetachPeerConnectionStatistic(peer_connection_statistic->GetEndpoint());
    }

    bool P2PDownloaderStatistic::DetachAllPeerConnectionStatistic()
    {
        for (PeerConnectionStatisticMap::iterator it = peer_connection_statistic_map_.begin(); it
            != peer_connection_statistic_map_.end(); it++)
        {
            it->second->Stop();
        }
        // clear
        peer_connection_statistic_map_.clear();

        return true;
    }

    //////////////////////////////////////////////////////////////////////////
    // Misc

    boost::uint32_t P2PDownloaderStatistic::GetMaxP2PConnectionCount() const
    {
        return MAX_P2P_DOWNLOADER_COUNT;
    }

    RID P2PDownloaderStatistic::GetResourceID() const
    {
        return resource_id_;
    }

    SPEED_INFO P2PDownloaderStatistic::GetSpeedInfo()
    {
        if (false == is_running_)
        {
            return SPEED_INFO();
        }

        return speed_info_.GetSpeedInfo();
    }

    SPEED_INFO_EX P2PDownloaderStatistic::GetSpeedInfoEx()
    {
        if (false == is_running_)
        {
            return SPEED_INFO_EX();
        }
        return speed_info_.GetSpeedInfoEx();
    }

    SPEED_INFO P2PDownloaderStatistic::GetPeerSpeedInfo()
    {
        if (false == is_running_)
        {
            return SPEED_INFO();
        }

        return peer_speed_info_.GetSpeedInfo();
    }

    SPEED_INFO_EX P2PDownloaderStatistic::GetPeerSpeedInfoEx()
    {
        if (false == is_running_)
        {
            return SPEED_INFO_EX();
        }
        return peer_speed_info_.GetSpeedInfoEx();
    }

    SPEED_INFO P2PDownloaderStatistic::GetSnSpeedInfo()
    {
        if (false == is_running_)
        {
            return SPEED_INFO();
        }

        return sn_speed_info_.GetSpeedInfo();
    }

    SPEED_INFO_EX P2PDownloaderStatistic::GetSnSpeedInfoEx()
    {
        if (false == is_running_)
        {
            return SPEED_INFO_EX();
        }
        return sn_speed_info_.GetSpeedInfoEx();
    }

    boost::uint32_t P2PDownloaderStatistic::GetElapsedTimeInMilliSeconds()
    {
        if (false == is_running_)
        {
            return 0;
        }
        return peer_speed_info_.GetElapsedTimeInMilliSeconds();
    }

    void P2PDownloaderStatistic::UpdateSpeedInfo()
    {
        if (false == is_running_)
        {
            return;
        }

        p2p_downloader_statistic_info_.SpeedInfo = speed_info_.GetSpeedInfo();
        p2p_downloader_statistic_info_.PeerSpeedInfo = peer_speed_info_.GetSpeedInfo();
        p2p_downloader_statistic_info_.SnSpeedInfo = sn_speed_info_.GetSpeedInfo();
    }

    //////////////////////////////////////////////////////////////////////////
    // Peer Connection Info

    void P2PDownloaderStatistic::UpdatePeerConnectionInfo()
    {
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "P2PDownloaderStatistic::UpdatePeerConnectionInfo [IN]");

        p2p_downloader_statistic_info_.PeerCount = peer_connection_statistic_map_.size();

        LOG4CPLUS_DEBUG_LOG(logger_statistic, "Peer Count: " << p2p_downloader_statistic_info_.PeerCount);

        // assert(p2p_downloader_statistic_info_.PeerCount <= GetMaxP2PConnectionCount());
        if (p2p_downloader_statistic_info_.PeerCount > GetMaxP2PConnectionCount())
        {
            LOG4CPLUS_WARN_LOG(logger_statistic, "Peer Count exceeds max allowed count: " << 
                GetMaxP2PConnectionCount() << ", Reset PeerCount to that value.");
            p2p_downloader_statistic_info_.PeerCount = GetMaxP2PConnectionCount();
        }

        PeerConnectionStatisticMap::iterator it = peer_connection_statistic_map_.begin();
        for (boost::uint32_t i = 0; it != peer_connection_statistic_map_.end(); it++, i++)
        {
            p2p_downloader_statistic_info_.P2PConnections[i] = it->second->GetPeerConnectionInfo();
        }

        LOG4CPLUS_DEBUG_LOG(logger_statistic, "P2PDownloaderStatistic::UpdatePeerConnectionInfo [OUT]");
    }

    void P2PDownloaderStatistic::SubmitPeerConnectRequestCount(boost::uint8_t nat_type)
    {
        if (nat_type_connection_statistic_.find (nat_type) != nat_type_connection_statistic_.end())
        {
            nat_type_connection_statistic_[nat_type].first++;
        }
        else
        {
            nat_type_connection_statistic_[nat_type] = std::make_pair(1, 0);
        }
    }

    void P2PDownloaderStatistic::SubmitPeerConnectSuccessCount(boost::uint8_t nat_type)
    {
        assert(nat_type_connection_statistic_.find(nat_type) != nat_type_connection_statistic_.end());
        nat_type_connection_statistic_[nat_type].second++;
    }

    //////////////////////////////////////////////////////////////////////////
    // Resource Info

    void P2PDownloaderStatistic::SetFileLength(boost::uint32_t file_length)
    {
        p2p_downloader_statistic_info_.FileLength = file_length;
    }

    void P2PDownloaderStatistic::SetBlockNum(boost::uint16_t block_num)
    {
        p2p_downloader_statistic_info_.BlockNum = block_num;
    }

    void P2PDownloaderStatistic::SetBlockSize(boost::uint16_t block_size)
    {
        p2p_downloader_statistic_info_.BlockSize = block_size;
    }

    //////////////////////////////////////////////////////////////////////////
    // Ip Pool Info

    void P2PDownloaderStatistic::SetIpPoolPeerCount(boost::uint16_t ip_pool_peer_count)
    {
        p2p_downloader_statistic_info_.IpPoolPeerCount = ip_pool_peer_count;
    }

    void P2PDownloaderStatistic::SetExchangingPeerCount(boost::uint8_t exchanging_peer_count)
    {
        p2p_downloader_statistic_info_.ExchangingPeerCount = exchanging_peer_count;
    }

    void P2PDownloaderStatistic::SetConnectingPeerCount(boost::uint8_t connecting_peer_count)
    {
        p2p_downloader_statistic_info_.ConnectingPeerCount = connecting_peer_count;
    }

    void P2PDownloaderStatistic::SetFullBlockPeerCount(boost::uint16_t full_block_peer_count)
    {
        p2p_downloader_statistic_info_.FullBlockPeerCount = full_block_peer_count;
    }

    boost::uint16_t P2PDownloaderStatistic::GetIpPoolPeerCount()
    {
        return p2p_downloader_statistic_info_.IpPoolPeerCount;
    }

    boost::uint16_t P2PDownloaderStatistic::GetConnectedPeerCount()
    {
        return p2p_downloader_statistic_info_.PeerCount;
    }
    //////////////////////////////////////////////////////////////////////////
    // Algorithm Info

    void P2PDownloaderStatistic::SetTotalWindowSize(boost::uint16_t total_window_size)
    {
        p2p_downloader_statistic_info_.TotalWindowSize = total_window_size;
    }

    void P2PDownloaderStatistic::SetNonConsistentSize(boost::uint16_t non_consistent_size)
    {
      p2p_downloader_statistic_info_.NonConsistentSize = non_consistent_size;
    }

    // 提交每秒发起的连接数
    void P2PDownloaderStatistic::SubmitConnectCount(boost::uint16_t connect_count)
    {
        p2p_downloader_statistic_info_.ConnectCount = connect_count;
    }

    // 提交每秒踢掉的连接数
    void P2PDownloaderStatistic::SubmitKickCount(boost::uint16_t kick_count)
    {
        p2p_downloader_statistic_info_.KickCount = kick_count;
    }

    boost::uint16_t P2PDownloaderStatistic::GetTotalAssignedSubPieceCount()
    {
        return p2p_downloader_statistic_info_.TotalAssignedSubPieceCount;
    }

    boost::uint16_t P2PDownloaderStatistic::GetSubPieceRetryRate()
    {
        if (false == is_running_)
        {
            return 0;
        }
        UpdateRate();
        return p2p_downloader_statistic_info_.SubPieceRetryRate;
    }

    boost::uint16_t P2PDownloaderStatistic::GetUDPLostRate()
    {
        if (false == is_running_)
        {
            return 0;
        }
        UpdateRate();
        return p2p_downloader_statistic_info_.UDPLostRate;
    }

    void P2PDownloaderStatistic::UpdateRate()
    {
        p2p_downloader_statistic_info_.SubPieceRetryRate = (p2p_downloader_statistic_info_.TotalRecievedSubPieceCount == 0) ? 0 :
            static_cast<boost::uint16_t>(100.0 * (p2p_downloader_statistic_info_.TotalUnusedSubPieceCount - p2p_downloader_statistic_info_.TotalRecievedSubPieceCount) / p2p_downloader_statistic_info_.TotalUnusedSubPieceCount + 0.5);

        boost::uint32_t total_requestint_count = 0;
        for (int i = 0; i<p2p_downloader_statistic_info_.PeerCount; i++)
        {
            total_requestint_count += p2p_downloader_statistic_info_.P2PConnections[i].Requesting_Count;
        }

        p2p_downloader_statistic_info_.UDPLostRate = (p2p_downloader_statistic_info_.TotalRequestSubPieceCount == 0 || p2p_downloader_statistic_info_.TotalRequestSubPieceCount == total_requestint_count) ? 0 :
            static_cast<boost::uint16_t>(100.0 * (p2p_downloader_statistic_info_.TotalRequestSubPieceCount - total_requestint_count - p2p_downloader_statistic_info_.TotalUnusedSubPieceCount) / (p2p_downloader_statistic_info_.TotalRequestSubPieceCount - total_requestint_count + 0.000001) + 0.5);
    }

    string P2PDownloaderStatistic::GetTrackerListRequestAndResponseString()
    {
        std::ostringstream tracker_string;
        std::map<boost::uint32_t, int>::iterator iter = tracker_list_request_count_.begin();
        for (iter; iter != tracker_list_request_count_.end();)
        {
            tracker_string << (boost::uint32_t)iter->first << ":";
            tracker_string << (int)iter->second << ":";

            if (tracker_list_response_count_.find(iter->first) != tracker_list_response_count_.end())
            {
                tracker_string << (int)tracker_list_response_count_[iter->first];
            }
            else
            {
                tracker_string << 0;
            }

            if (++iter != tracker_list_request_count_.end())
            {
                tracker_string << ",";
            }
        }

        return tracker_string.str();
    }

    //////////////////////////////////////////////////////////////////////////
    // Shared Memory
    bool P2PDownloaderStatistic::CreateSharedMemory()
    {
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "Creating SharedMemory: [" << GetSharedMemoryName() << "] size: " 
            << GetSharedMemorySize() << " Bytes.");
        shared_memory_.Create(GetSharedMemoryName(), GetSharedMemorySize());
        return shared_memory_.IsValid();
    }

    string P2PDownloaderStatistic::GetSharedMemoryName()
    {
        return CreateP2PDownloaderModuleSharedMemoryName(GetCurrentProcessID(), GetResourceID());
    }

    boost::uint32_t P2PDownloaderStatistic::GetSharedMemorySize()
    {
        return sizeof(p2p_downloader_statistic_info_);
    }

    void P2PDownloaderStatistic::SetEmptySubpieceDistance(boost::uint32_t empty_subpiece_distance)
    {
        p2p_downloader_statistic_info_.empty_subpiece_distance = empty_subpiece_distance;
    }

    string P2PDownloaderStatistic::GetPeerConnectString() const
    {
        std::ostringstream os;
        std::map<boost::uint16_t, std::pair<boost::uint32_t, boost::uint32_t> >::const_iterator iter = nat_type_connection_statistic_.begin();
        for (iter; iter != nat_type_connection_statistic_.end(); )
        {
            os << (boost::uint16_t)(iter->first) << ":"
                << (boost::uint32_t)iter->second.first << ":"
                << (boost::uint32_t)iter->second.second;

            if (++iter != nat_type_connection_statistic_.end())
            {
                os << ",";
            }
        }
        return os.str();
    }
}
