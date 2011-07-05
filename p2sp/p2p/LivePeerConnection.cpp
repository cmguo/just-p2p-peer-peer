#include "Common.h"
#include "LiveP2PDownloader.h"
#include "p2sp/download/LiveDownloadDriver.h"
#include "LivePeerConnection.h"
#include "p2sp/p2p/P2PModule.h"

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("live_p2p");

    void LivePeerConnection::Start(protocol::ConnectPacket const & reconnect_packet, 
        const boost::asio::ip::udp::endpoint &end_point,
        const protocol::CandidatePeerInfo& peer_info)
    {
        is_running_ = true;
        end_point_ = end_point;
        DoAnnounce();
        speed_info_.Start();

        candidate_peer_info_ = reconnect_packet.peer_info_;
        if (candidate_peer_info_.IP == peer_info.IP && candidate_peer_info_.UdpPort == peer_info.UdpPort)
        {
            candidate_peer_info_ = peer_info;
        }
    }

    void LivePeerConnection::Stop()
    {
        speed_info_.Stop();
        is_running_ = false;
    }

    void LivePeerConnection::OnP2PTimer(boost::uint32_t times)
    {
        if (false == is_running_)
            return;

        if (times % 4 == 0)
        {
            no_announce_response_time_ += 1000;

            // 请求Announce
            DoAnnounce();

            // 更新window_size_
            boost::int32_t last_window_size = window_size_;

            window_size_ = GetSpeedInfo().NowDownloadSpeed / 1024;

            if (window_size_ < last_window_size)
            {
                window_size_ = (9 * last_window_size + 1 * window_size_) / 10;
            }

            LIMIT_MIN_MAX(window_size_, 4, 25);

            // 更新共享内存信息
            UpdatePeerConnectionInfo();
        }

        RequestTillFullWindow();
    }

    void LivePeerConnection::DoAnnounce()
    {
        // 发送 annouce 报文
        boost::uint32_t request_block_id = 0;
        boost::uint32_t interval = (*p2p_downloader_->GetDownloadDriverSet().begin())->GetInstance()->GetLiveInterval();

        // 计算上传带宽和能力值的较小值，填到LiveRequestAnnouncePacket中的upload_bandwidth_字段中
        boost::int32_t upload_bandwidth = p2sp::P2PModule::Inst()->GetUploadSpeedLimitInKBps();
        if (upload_bandwidth < (boost::int32_t)p2sp::P2PModule::Inst()->GetUploadBandWidthInKBytes())
        {
            upload_bandwidth = (boost::int32_t)p2sp::P2PModule::Inst()->GetUploadBandWidthInKBytes();
        }

        // 每个播放点都进行Announce
        for (std::set<LiveDownloadDriver__p>::const_iterator iter = p2p_downloader_->GetDownloadDriverSet().begin();
            iter != p2p_downloader_->GetDownloadDriverSet().end(); ++iter)
        {
            boost::uint32_t request_block_id = (*iter)->GetPlayingPosition().GetBlockId();

            protocol::LiveRequestAnnouncePacket live_request_annouce_packet(protocol::Packet::NewTransactionID(), 
                p2p_downloader_->GetRid(), request_block_id, upload_bandwidth, end_point_);

            p2p_downloader_->DoSendPacket(live_request_annouce_packet);
        }
    }

    void LivePeerConnection::OnAnnounce(protocol::LiveAnnouncePacket const & packet)
    {
        if (false == is_running_)
            return;

        if (packet.live_announce_map_.block_info_count_ > 0)
        {
            std::map<boost::uint32_t, boost::dynamic_bitset<boost::uint8_t> >::const_iterator iter;
            std::vector<boost::uint16_t> subpiece_no;

            iter = packet.live_announce_map_.subpiece_map_.begin();
            boost::uint32_t start_block_id = iter->first;
           
            for (std::map<boost::uint32_t, boost::uint16_t>::const_iterator subpiece_no_iter = packet.live_announce_map_.subpiece_nos_.begin();
                subpiece_no_iter != packet.live_announce_map_.subpiece_nos_.end(); ++subpiece_no_iter)
            {
                subpiece_no.push_back(subpiece_no_iter->second);
            }

            p2p_downloader_->SetBlockCountMap(start_block_id, subpiece_no);

            // 保存piece_bitmaps_
            // 删除request_block_id 与 subpiece_map_之间的已经被淘汰的bitmap
            for (boost::uint32_t block_id = packet.live_announce_map_.request_block_id_;
                !packet.live_announce_map_.subpiece_map_.empty() && 
                block_id < packet.live_announce_map_.subpiece_map_.begin()->first;
                block_id += p2p_downloader_->GetInstance()->GetLiveInterval())
            {
                map<boost::uint32_t, boost::dynamic_bitset<boost::uint8_t> >::iterator iter = 
                    block_bitmap_.find(block_id);
                
                if (iter != block_bitmap_.end())
                {
                    block_bitmap_.erase(iter);
                }
            }

            // 更新piece_bitmaps_
            map<boost::uint32_t, boost::dynamic_bitset<boost::uint8_t> >::const_iterator bitmap_iter = 
                packet.live_announce_map_.subpiece_map_.begin();

            for (; bitmap_iter != packet.live_announce_map_.subpiece_map_.end(); ++bitmap_iter)
            {
                block_bitmap_[bitmap_iter->first] = bitmap_iter->second;
            }

            peer_connection_info_.LastLiveBlockId = packet.live_announce_map_.subpiece_map_.rbegin()->first;
            peer_connection_info_.FirstLiveBlockId = packet.live_announce_map_.subpiece_map_.begin()->first;
        }

        // 收到Announce, 重置
        no_announce_response_time_ = 0;
    }

    void LivePeerConnection::ClearTaskQueue()
    {
        task_queue_.clear();
    }

    void LivePeerConnection::AddAssignedSubPiece(const protocol::LiveSubPieceInfo & subpiece_info)
    {
        task_queue_.push_back(subpiece_info);
    }

    void LivePeerConnection::OnSubPiece(uint32_t subpiece_rtt, uint32_t buffer_length)
    {
        requesting_count_--;

        // 计算最大rtt
        if (subpiece_rtt > rtt_max_)
        {
            rtt_max_ = subpiece_rtt;
        }

        // 计算最近10个包的平均rtt
        rtt_avg_ = (rtt_avg_ * 9 + subpiece_rtt) / 10;

        // 计算最近10个包的平均avg_delta
        avg_delta_time_ = (avg_delta_time_ * 9 + recv_subpiece_time_counter_.elapsed()) / 10;

        LIMIT_MIN_MAX(avg_delta_time_, 10, 1000);

        recv_subpiece_time_counter_.reset();

        LOG(__DEBUG, "live_p2p", "rtt_max= " << rtt_max_ << " rtt_avg= " << rtt_avg_ << " delta= " << avg_delta_time_);

        speed_info_.SubmitDownloadedBytes(buffer_length);
        ++peer_connection_info_.Received_Count;

        RequestTillFullWindow();
    }

    void LivePeerConnection::OnSubPieceTimeout()
    {
        requesting_count_--;
        RequestTillFullWindow();
    }

    void LivePeerConnection::RequestSubPieces(uint32_t block_count, bool need_check)
    {
        if (block_count == 0 || task_queue_.empty())
        {
            return;
        }

        if (requesting_count_ == 0)
        {
            recv_subpiece_time_counter_.reset();
        }

        std::vector<protocol::LiveSubPieceInfo> subpieces;
        for (uint32_t i = 0; i < block_count; ++i)
        {
            if (task_queue_.empty())
            {
                break;
            }

            const protocol::LiveSubPieceInfo & subpiece = task_queue_.front();
            if (!need_check || !p2p_downloader_->HasSubPiece(subpiece))
            {
                subpieces.push_back(subpiece);
            }
            task_queue_.pop_front();
        }

        if (subpieces.size() == 0)
        {
            return;
        }

        boost::uint32_t copy_count = subpieces.size() / 2;

        LIMIT_MIN_MAX(copy_count, 1, 3);

        protocol::LiveRequestSubPiecePacket packet(protocol::Packet::NewTransactionID(),
            p2p_downloader_->GetRid(), subpieces, protocol::LiveRequestSubPiecePacket::DEFAULT_LIVE_PRIORITY, end_point_);

        for (boost::uint32_t i=0; i < copy_count; i++)
        {
            p2p_downloader_->DoSendPacket(packet);
        }

        p2p_downloader_->SubmitAllRequestSubPieceCount(copy_count * packet.sub_piece_infos_.size());
        p2p_downloader_->SubmitRequestSubPieceCount(packet.sub_piece_infos_.size());

        for (uint32_t i = 0; i < subpieces.size(); ++i)
        {
            LOG(__DEBUG, "live_p2p", "request subpiece " << subpieces[i]);
            p2p_downloader_->AddRequestingSubpiece(subpieces[i], rtt_avg_ + GetTimeoutAdjustment(),
                shared_from_this());
        }

        requesting_count_ += subpieces.size();

        peer_connection_info_.Sent_Count += packet.sub_piece_infos_.size();
    }

    bool LivePeerConnection::HasSubPiece(const protocol::LiveSubPieceInfo & subpiece)
    {
        if (block_bitmap_.find(subpiece.GetBlockId()) != block_bitmap_.end())
        {
            if (subpiece.GetSubPieceIndex() == 0)
            {
                return block_bitmap_[subpiece.GetBlockId()].test(0);
            }
            else
            {
                return block_bitmap_[subpiece.GetBlockId()].test(((subpiece.GetSubPieceIndex() - 1) >> 4) + 1);
            }
        }
        return false;
    }

    void LivePeerConnection::RequestTillFullWindow()
    {
        while (requesting_count_ < window_size_ && !task_queue_.empty())
        {
            RequestSubPieces(6, false);
        }
    }

    void LivePeerConnection::EliminateBlockBitMap(uint32_t block_id)
    {
        map<boost::uint32_t, boost::dynamic_bitset<boost::uint8_t> >::iterator iter;
        iter = block_bitmap_.find(block_id);

        if (iter != block_bitmap_.end())
        {
            block_bitmap_.erase(iter);
        }

        LOG(__DEBUG, "live_p2p", "block_bitmap_.size() = " << block_bitmap_.size());
    }

    boost::uint32_t LivePeerConnection::GetAvgDeltaTime() const
    {
        return avg_delta_time_;
    }

    statistic::SPEED_INFO LivePeerConnection::GetSpeedInfo()
    {
        return speed_info_.GetSpeedInfo();
    }

    uint32_t LivePeerConnection::Get75PercentPointInBitmap(uint32_t live_interval)
    {
        if (block_bitmap_.empty())
        {
            return 0;
        }

        uint32_t first_block_id = block_bitmap_.begin()->first;
        uint32_t last_block_id = block_bitmap_.rbegin()->first;

        // 需要是live_interval的倍数
        return (first_block_id + (last_block_id - first_block_id) * 3 / 4) / live_interval * live_interval;
    }

    uint32_t LivePeerConnection::GetSubPieceCountInBitmap(uint32_t block_id)
    {
        if (block_bitmap_.find(block_id) == block_bitmap_.end())
        {
            return 0;
        }
        else
        {
            return block_bitmap_[block_id].count();
        }
    }

    const statistic::P2P_CONNECTION_INFO & LivePeerConnection::GetPeerConnectionInfo()
    {
        peer_connection_info_.SpeedInfo = speed_info_.GetSpeedInfo();
        return peer_connection_info_;
    }

    void LivePeerConnection::UpdatePeerConnectionInfo()
    {
        peer_connection_info_.PeerInfo.IP = end_point_.address().to_v4().to_ulong();
        peer_connection_info_.WindowSize = window_size_;
        peer_connection_info_.RTT_Average = rtt_avg_;
        peer_connection_info_.RTT_Max = rtt_max_;
        peer_connection_info_.AverageDeltaTime = avg_delta_time_;
        peer_connection_info_.Requesting_Count = requesting_count_;
        peer_connection_info_.AssignedSubPieceCount = task_queue_.size();
    }

    boost::uint32_t LivePeerConnection::GetTimeoutAdjustment()
    {
        uint32_t rest_time = p2p_downloader_->GetMinRestTimeInSeconds();

        if (rest_time > 10)
        {
            return 500 + rest_time * 100 > 5000 ? 5000 : 500 + rest_time * 100;
        }

        return 500;
    }

    bool LivePeerConnection::LongTimeNoAnnounceResponse()
    {
        // 连续 10 秒没有 AnnounceResponse 回包
        return no_announce_response_time_ > 10*1000;
    }
}