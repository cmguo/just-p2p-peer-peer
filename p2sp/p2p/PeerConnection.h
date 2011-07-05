//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// PeerConnection.h

#ifndef _P2SP_P2P_PEER_CONNECTION_H_
#define _P2SP_P2P_PEER_CONNECTION_H_

#include "statistic/PeerConnectionStatistic.h"
#include "p2sp/p2p/P2PModule.h"
#include "p2sp/p2p/P2SPConfigs.h"

#include <protocol/PeerPacket.h>
namespace p2sp
{
    class P2PDownloader;
    typedef boost::shared_ptr<P2PDownloader> P2PDownloader__p;
    class SubPieceRequestManager;
    typedef boost::shared_ptr<SubPieceRequestManager> SubPieceRequestManager__p;
    class SubPieceRequestTask;
    typedef boost::shared_ptr<SubPieceRequestTask> SubPieceRequestTask__p;

    class PeerConnection
        : public boost::noncopyable
        , public boost::enable_shared_from_this<PeerConnection>
#ifdef DUMP_OBJECT
        , public count_object_allocate<PeerConnection>
#endif
    {
    public:
        typedef boost::shared_ptr<PeerConnection> p;
        static p create(P2PDownloader__p p2p_downloader, SubPieceRequestManager__p subpiece_request_manager) { return p(new PeerConnection(p2p_downloader, subpiece_request_manager)); }
    public:
        // 启停
        void Start(protocol::ConnectPacket const & reconnect_packet, const boost::asio::ip::udp::endpoint &end_point, const protocol::CandidatePeerInfo& peer_info);
        void Stop();
        bool IsRunning() const { return is_running_; }
        // 操作
        bool RequestTillFullWindow(bool need_check = false);
        void RequestNextSubpiece();
        bool RequestSubPiece(const protocol::SubPieceInfo& subpiece_info, bool need_check = false);
        bool RequestSubPieces(uint32_t piece_count, uint32_t copy_count, bool need_check = false);
        // bool RequestNextSubpiece(const protocol::SubPieceInfo& subpiece_info);
        bool AddAssignedSubPiece(const protocol::SubPieceInfo & subpiece_info);
        bool AddAssignedSubPiece(const protocol::SubPieceInfo & subpiece_info, uint32_t timeout);
        void ClearTaskQueue();
        void DoAnnounce();
        void DoRequestRIDInfo();
        void DoReportDownloadSpeed();
        void SetStatisticStatusAfterAssigned();
        bool CheckRidInfo(const protocol::RidInfo& rid_info);
        bool IsRidInfoValid() const;
        bool HasRidInfo() const;
        protocol::RidInfo GetRidInfo() const { return rid_info_; }
        void KeepAlive();

        // 消息
        void OnP2PTimer(boost::uint32_t times);
        void OnAnnounce(protocol::AnnouncePacket const & packet);
        void OnRIDInfoResponse(protocol::RIDInfoResponsePacket const & packet);
        void OnSubPiece(uint32_t subpiece_rtt, uint32_t buffer_length);
        void OnTimeOut(SubPieceRequestTask__p subpiece_request_task);
        // 属性
        bool IsRequesting() const;
        uint32_t GetUsedTime();
        uint32_t GetUsedTimeForAssigner();
        uint32_t GetSortedValueForAssigner();
        boost::uint32_t GetRequestedCount() const;

        boost::uint32_t GetWindowSize() const;
        boost::uint32_t GetConnectRTT() const {return connect_rtt_;}
        boost::uint32_t GetRtt() const;
        boost::uint32_t GetLongestRtt() const;
        boost::uint32_t GetAvgDeltaTime() const;
        boost::uint32_t GetSentCount() const { return sent_count_; }
        boost::uint32_t GetReceivedCount() const { return received_count_; }

        uint32_t GetTaskQueueSize() const;
        bool HasSubPiece(const protocol::SubPieceInfo& subpiece_info);
        protocol::BlockMap const & GetBlockMap() const {return block_map_;}
        const Guid& GetGuid() const {return peer_guid_;}
        const protocol::CandidatePeerInfo & GetCandidatePeerInfo() const{ return candidate_peer_info_;}
        bool LongTimeNoSee() {return P2SPConfigs::PEERCONNECTION_NORESPONSE_KICK_TIME_IN_MILLISEC < last_live_response_time_.elapsed();};
        bool CanKick();
        statistic::PeerConnectionStatistic::p GetPeerConnectionStatistic() const {assert(statistic_);return statistic_;}
        bool CanRequest() const;
        uint32_t GetConnectedTime() const { return connected_time_.elapsed(); }
        statistic::PeerConnectionStatistic::p GetStatistic() const { return statistic_; }

        uint32_t GetPeerVersion() const { return peer_version_; }

    private:
        // 模块
        P2PDownloader__p p2p_downloader_;
        SubPieceRequestManager__p subpiece_request_manager_;
        statistic::PeerConnectionStatistic::p statistic_;
        // 请求算法变量
        std::deque<protocol::SubPieceInfo> task_queue_;
        uint32_t window_size_;
        uint32_t window_size_init_;
        uint32_t requesting_count_;
        uint32_t curr_time_out_;
        uint32_t curr_delta_size_;
        uint32_t curr_request_count_;

        boost::uint32_t requested_size_;

        framework::timer::TickCounter last_receive_time_;
        framework::timer::TickCounter last_live_response_time_;
        framework::timer::TickCounter last_request_time_;
        framework::timer::TickCounter connected_time_;
        uint32_t avg_delt_time_;
        uint32_t avg_delt_time_init_;

        measure::CycleBuffer recent_avg_delt_times_;

        uint32_t sent_count_;
        uint32_t received_count_;

        // Peer对方的相关变量
        uint32_t connect_rtt_;
        uint32_t rtt_;
        uint32_t longest_rtt_;

        uint32_t peer_version_;
        protocol::CandidatePeerInfo candidate_peer_info_;
        protocol::PEER_DOWNLOAD_INFO peer_download_info_;
        protocol::BlockMap block_map_;
        boost::asio::ip::udp::endpoint end_point_;
        Guid peer_guid_;
        protocol::RidInfo rid_info_;
        bool is_rid_info_valid_;

        // 状态
        volatile bool is_running_;

        // 累计等待请求的subpiece数
        boost::uint32_t accumulative_subpiece_num;

    private:
        // 构造
        PeerConnection(P2PDownloader__p p2p_downloader, SubPieceRequestManager__p subpiece_request_manager)
            : p2p_downloader_(p2p_downloader)
            , subpiece_request_manager_(subpiece_request_manager)
            , requesting_count_(0)
            , last_receive_time_(0)
            , last_live_response_time_(0)
            , last_request_time_(0)
            , avg_delt_time_(0)
            , longest_rtt_(0)
            , is_rid_info_valid_(false)
            , is_running_(false)
            , recent_avg_delt_times_(30)
        {}

    };

    inline bool PeerConnection::CanRequest() const
    {
        return P2PModule::Inst()->CanRequest() && requesting_count_ < window_size_;
    }

    inline bool PeerConnection::CheckRidInfo(const protocol::RidInfo& rid_info)
    {
        if (false == is_running_)
            return false;

        // 兼容老Peer
        if (peer_version_ < 0x00000007)
            return true;

        is_rid_info_valid_ = (rid_info_ == rid_info);
        return is_rid_info_valid_;
    }

    inline bool PeerConnection::HasRidInfo() const
    {
        if (false == is_running_)
            return false;

        if (peer_version_ < 0x00000007)
            return true;

        return false == rid_info_.GetRID().is_empty();
    }

    inline bool PeerConnection::IsRidInfoValid() const
    {
        if (false == is_running_)
            return false;

        if (peer_version_ < 0x00000007)
            return true;
        // Modified by jeffrey 支持ppbox发给内核的请求不带block_md5
#ifndef PEER_PC_CLIENT
        return true;
#endif
        return is_rid_info_valid_;
    }

    inline bool PeerConnection::HasSubPiece(const protocol::SubPieceInfo & subpiece_info)
    {
        if (is_running_ == false)
            return false;
        return block_map_.HasBlock(subpiece_info.block_index_);
    }

    inline bool PeerConnection::AddAssignedSubPiece(const protocol::SubPieceInfo & subpiece_info)
    {
        if (is_running_ == false)
            return false;
        task_queue_.push_back(subpiece_info);
        return true;
    }

    inline void PeerConnection::ClearTaskQueue()
    {
        if (is_running_ == false)
            return;
        task_queue_.clear();
    }

    inline uint32_t PeerConnection::GetSortedValueForAssigner()
    {
        if (is_running_ == false)
            return 0;

        return framework::timer::TickCounter::tick_count() + GetUsedTimeForAssigner();
    }

    inline void PeerConnection::SetStatisticStatusAfterAssigned()
    {
        if (false == is_running_)
            return;
        assert(statistic_);
        // statistic_->SetAssignedSubPieceCount(task_queue_.size());
        statistic_->SetAverageDeltaTime(GetUsedTime());
        statistic_->SetSortedValue(GetSortedValueForAssigner());
    }

    inline void PeerConnection::OnAnnounce(protocol::AnnouncePacket const & packet)
    {
        if (is_running_ == false)
            return;

        last_live_response_time_.reset();
        statistic_->SubmitDownloadedBytes(sizeof(protocol::AnnouncePacket));
        statistic_->SetBitmap(/*protocol::BlockMap::p*/(packet.block_map_));
        block_map_ = packet.block_map_;
        peer_download_info_ = packet.peer_download_info_;
        end_point_ = packet.end_point;
    }

    inline uint32_t PeerConnection::GetWindowSize() const
    {
        if (is_running_ == false)
            return 0;
        return window_size_;
    }

    inline uint32_t PeerConnection::GetRtt() const
    {
        if (is_running_ == false)
            return 0;
        return rtt_;
    }

    inline uint32_t PeerConnection::GetLongestRtt() const
    {
        if (is_running_ == false)
            return 0;
        return longest_rtt_;
    }

    inline uint32_t PeerConnection::GetAvgDeltaTime() const
    {
        if (is_running_ == false)
            return 0;
        return avg_delt_time_;
    }

    inline uint32_t PeerConnection::GetTaskQueueSize() const
    {
        if (is_running_ == false)
            return 0;
        return task_queue_.size();
    }
}

#endif  // _P2SP_P2P_PEER_CONNECTION_H_
