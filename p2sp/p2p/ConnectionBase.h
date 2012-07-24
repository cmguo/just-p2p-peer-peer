//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef CONNECTION_BASE_H
#define CONNECTION_BASE_H

#include "statistic/PeerConnectionStatistic.h"
#include "base/intrusive_ptr.h"

namespace p2sp
{
    class P2PDownloader;
    typedef boost::shared_ptr<P2PDownloader> P2PDownloader__p;

    class ConnectionBase
        : public base::intrusive_ptr_base<ConnectionBase>
    {
    public:
        ConnectionBase(P2PDownloader__p p2p_downloader,
            const boost::asio::ip::udp::endpoint & end_point)
            : p2p_downloader_(p2p_downloader)
            , endpoint_(end_point)
            , is_running_(false)
            , requesting_count_(0)
            , last_receive_time_(0)
            , last_live_response_time_(0)
            , avg_delta_time_(0)
            , recent_avg_delt_times_(30)
            , longest_rtt_(0)
            , window_size_(0)
            , sent_count_(0)
            , received_count_(0)
            , curr_time_out_(0)
            , accumulative_subpiece_num(0)
        {

        }

        virtual void Stop() = 0;
        virtual void RequestTillFullWindow(bool need_check = false) = 0;
        virtual void OnP2PTimer(boost::uint32_t times) = 0;
        virtual bool HasRidInfo() const = 0;
        virtual bool IsRidInfoValid() const = 0;
        virtual bool CanKick() = 0;
        virtual bool HasSubPiece(const protocol::SubPieceInfo & subpiece_info) = 0;

        virtual bool HasBlock(boost::uint32_t block_index) = 0;
        virtual bool IsBlockFull() = 0;

        virtual bool LongTimeNoSee() = 0;
        virtual void KeepAlive() {}
        virtual void UpdateConnectTime() {}

        virtual void SendPacket(const std::vector<protocol::SubPieceInfo> & subpieces,
            boost::uint32_t copy_count) = 0;

        virtual void SubmitDownloadedBytes(boost::uint32_t length) = 0;

        virtual void SubmitP2PDataBytesWithoutRedundance(boost::uint32_t length) = 0;
        virtual void SubmitP2PDataBytesWithRedundance(boost::uint32_t length) = 0;

        virtual boost::uint32_t GetConnectRTT() const = 0;

        bool IsRunning() const;
        const boost::asio::ip::udp::endpoint & GetEndpoint();
        statistic::PeerConnectionStatistic::p GetStatistic() const;

        bool AddAssignedSubPiece(const protocol::SubPieceInfo & subpiece_info);
        uint32_t GetTaskQueueSize() const;
        void ClearTaskQueue();

        void OnSubPiece(uint32_t subpiece_rtt, uint32_t buffer_length);
        void OnTimeOut();

        uint32_t GetConnectedTime() const;
        boost::uint32_t GetSentCount() const;
        boost::uint32_t GetReceivedCount() const;

        uint32_t GetWindowSize() const;
        uint32_t GetLongestRtt() const;
        uint32_t GetAvgDeltaTime() const;

        const protocol::CandidatePeerInfo & GetCandidatePeerInfo() const;

        void RequestSubPieces(uint32_t subpiece_count, uint32_t copy_count, bool need_check);
        void RecordStatisticInfo();

    private:
        void RequestNextSubpiece();

    protected:
        bool is_running_;

        uint32_t sent_count_;
        uint32_t received_count_;

        framework::timer::TickCounter last_live_response_time_;
        framework::timer::TickCounter last_receive_time_;
        framework::timer::TickCounter connected_time_;
        framework::timer::TickCounter last_request_time_;

        statistic::PeerConnectionStatistic::p statistic_;
        measure::CycleBuffer recent_avg_delt_times_;

        boost::uint32_t window_size_;
        boost::uint32_t avg_delta_time_;
        boost::uint32_t requesting_count_;

        // 累计等待请求的subpiece数
        boost::uint32_t accumulative_subpiece_num;

        // 请求算法变量
        std::deque<protocol::SubPieceInfo> task_queue_;

        boost::asio::ip::udp::endpoint endpoint_;

        boost::uint32_t peer_version_;

        P2PDownloader__p p2p_downloader_;

        boost::uint32_t longest_rtt_;

        protocol::CandidatePeerInfo candidate_peer_info_;

        boost::uint32_t curr_time_out_;
    };
}

#endif