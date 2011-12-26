#include "Common.h"
#include "LiveSubPieceRequestManager.h"
#include "LiveP2PDownloader.h"
#include "statistic/DACStatisticModule.h"

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("live_subpiece_request_manager");

    void LiveSubPieceRequestManager::Start(LiveP2PDownloader__p p2p_downloader)
    {
        p2p_downloader_ = p2p_downloader;
    }

    void LiveSubPieceRequestManager::Add(
        const protocol::LiveSubPieceInfo & subpiece_info, 
        boost::uint32_t timeout, 
        LivePeerConnection__p peer_connection,
        uint32_t transaction_id)
    {
        LiveSubPieceRequestTask::p live_subpiece_request_task = 
            LiveSubPieceRequestTask::create(timeout, peer_connection, transaction_id);
        request_tasks_.insert(std::make_pair(subpiece_info, live_subpiece_request_task));
    }

    void LiveSubPieceRequestManager::CheckExternalTimeout()
    {
        for (std::multimap<protocol::LiveSubPieceInfo, LiveSubPieceRequestTask::p>::iterator iter = request_tasks_.begin();
            iter != request_tasks_.end(); )
        {
            if (iter->second->IsTimeout())
            {
                LOG(__DEBUG, "live_p2p", "subpiece timeout " << iter->first);
                iter->second->peer_connection_->OnSubPieceTimeout();
                request_tasks_.erase(iter++);
            }
            else
            {
                ++iter;
            }
        }
    }

    void LiveSubPieceRequestManager::OnSubPiece(const protocol::LiveSubPiecePacket & packet)
    {
        LOG(__DEBUG, "live_p2p", "recvive subpiece " << packet.sub_piece_info_);

        std::pair<std::multimap<protocol::LiveSubPieceInfo, LiveSubPieceRequestTask::p>::iterator,
            std::multimap<protocol::LiveSubPieceInfo, LiveSubPieceRequestTask::p>::iterator> range = 
            request_tasks_.equal_range(packet.sub_piece_info_);

        protocol::LiveSubPieceBuffer buffer(packet.sub_piece_content_, packet.sub_piece_length_);
        boost::uint8_t connect_type = protocol::CONNECT_LIVE_PEER;

        LiveSubPieceRequestTask::p matched_task;
        std::vector<LiveSubPieceRequestTask::p> unmatched_tasks;

        for (std::multimap<protocol::LiveSubPieceInfo, LiveSubPieceRequestTask::p>::iterator iter=range.first; 
            iter!= range.second;)
        {
            LiveSubPieceRequestTask::p task = iter->second;
            if (task->peer_connection_->GetEndpoint() == packet.end_point &&
                task->GetTransactionId() == packet.transaction_id_)
            {
                matched_task = task;
            }
            else
            {
                unmatched_tasks.push_back(task);
            }

            request_tasks_.erase(iter++);
        }

        std::multimap<uint32_t, protocol::LiveSubPieceInfo> to_delete;
        if (matched_task)
        {
            matched_task->peer_connection_->DeleteLostPackets(packet.transaction_id_, to_delete);
            // 找到，删除
            matched_task->peer_connection_->OnSubPiece(matched_task->GetTimeElapsed(), buffer.Length());
            connect_type = matched_task->peer_connection_->GetConnectType(); 
            matched_task->peer_connection_->UpdateLastReceived(packet.transaction_id_);
        }

        for(std::vector<LiveSubPieceRequestTask::p>::iterator task_iter = unmatched_tasks.begin();
            task_iter != unmatched_tasks.end();
            task_iter++)
        {   
            // 冗余任务，立即超时
            (*task_iter)->peer_connection_->OnSubPieceTimeout();
        }

        uint32_t loss_count = 0;

        for(std::multimap<uint32_t, protocol::LiveSubPieceInfo>::iterator lost_iter = to_delete.begin();
            lost_iter != to_delete.end(); ++lost_iter)
        {
            std::pair<std::multimap<protocol::LiveSubPieceInfo, LiveSubPieceRequestTask::p>::iterator,
                std::multimap<protocol::LiveSubPieceInfo, LiveSubPieceRequestTask::p>::iterator> range2 = 
                request_tasks_.equal_range(lost_iter->second);

            for (std::multimap<protocol::LiveSubPieceInfo, LiveSubPieceRequestTask::p>::iterator iter2=range2.first; 
                iter2!= range2.second;)
            {
                LiveSubPieceRequestTask::p task2 = iter2->second;
                if (task2->GetTransactionId() == lost_iter->first)
                {
                    request_tasks_.erase(iter2++);
                    loss_count++;
                    break;
                }
                else
                {
                    iter2++;
                }
            }
        }

        for(uint32_t k = 0; k < loss_count; k++)
        {
            matched_task->peer_connection_->OnSubPieceTimeout();
        }

        if (false == p2p_downloader_->HasSubPiece(packet.sub_piece_info_))
        {
            ++total_received_subpiece_count_;

            if (connect_type == protocol::CONNECT_LIVE_UDPSERVER)
            {
                statistic::DACStatisticModule::Inst()->SubmitLiveUdpServerDownloadBytes(buffer.Length());
                total_udpserver_data_bytes_ += buffer.Length();
            }
            else
            {
                statistic::DACStatisticModule::Inst()->SubmitLiveP2PDownloadBytes(buffer.Length());
                total_p2p_data_bytes_ += buffer.Length();
            }
        }

        p2p_downloader_->GetInstance()->AddSubPiece(packet.sub_piece_info_, buffer);

        ++total_unused_subpiece_count_;
    }

    // 每秒执行一次
    void LiveSubPieceRequestManager::OnP2PTimer(uint32_t times)
    {
        // 检查超时
        CheckExternalTimeout();
    }

    bool LiveSubPieceRequestManager::IsRequesting(const protocol::LiveSubPieceInfo& subpiece_info) const
    {
        std::multimap<protocol::LiveSubPieceInfo, LiveSubPieceRequestTask::p>::const_iterator 
            iter = request_tasks_.find(subpiece_info);

        if (iter != request_tasks_.end())
        {
            // 找到
            return true;
        }

        return false;
    }

    boost::uint32_t LiveSubPieceRequestManager::GetTotalUnusedSubPieceCount() const
    {
        return total_unused_subpiece_count_;
    }

    boost::uint32_t LiveSubPieceRequestManager::GetTotalRecievedSubPieceCount() const
    {
        return total_received_subpiece_count_;
    }

    boost::uint32_t LiveSubPieceRequestManager::GetTotalP2PDataBytes() const
    {
        return total_p2p_data_bytes_;
    }

    boost::uint32_t LiveSubPieceRequestManager::GetTotalUdpServerDataBytes() const
    {
        return total_udpserver_data_bytes_;
    }

    uint32_t LiveSubPieceRequestManager::GetRequestingCount(const protocol::LiveSubPieceInfo & subpiece_info) const
    {
        return request_tasks_.count(subpiece_info);
    }
}
