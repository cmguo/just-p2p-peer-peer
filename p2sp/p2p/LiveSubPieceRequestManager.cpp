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

    void LiveSubPieceRequestManager::Add(const protocol::LiveSubPieceInfo & subpiece_info, boost::uint32_t timeout, LivePeerConnection__p peer_connection)
    {
        LiveSubPieceRequestTask::p live_subpiece_request_task = LiveSubPieceRequestTask::create(timeout, peer_connection);
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
        std::multimap<protocol::LiveSubPieceInfo, LiveSubPieceRequestTask::p>::iterator 
            iter = request_tasks_.find(packet.sub_piece_info_);

        LOG(__DEBUG, "live_p2p", "recvive subpiece " << packet.sub_piece_info_);

        protocol::LiveSubPieceBuffer buffer(packet.sub_piece_content_, packet.sub_piece_length_);

        if (iter != request_tasks_.end())
        {
            // 找到，删除
            iter->second->peer_connection_->OnSubPiece(iter->second->GetTimeElapsed(), buffer.Length());
            request_tasks_.erase(iter);
            boost::uint32_t peer_ip = packet.end_point.address().to_v4().to_ulong();
            LOG(__DEBUG, "live_subpiece_request_manager", "receive subpiece from p2p, block id = " << packet.sub_piece_info_.GetBlockId()
                << ", subpiece index = " << packet.sub_piece_info_.GetSubPieceIndex()
                << ". ip = " << peer_ip / 256 / 256 / 256
                << "." << (peer_ip % (256 * 256 * 256)) / 256 / 256
                << "." << (peer_ip % (256 * 256)) / 256
                << "." << peer_ip % 256);
        }

        if (false == p2p_downloader_->HasSubPiece(packet.sub_piece_info_))
        {
            total_p2p_data_bytes_ += buffer.Length();
            ++total_received_subpiece_count_;
            statistic::DACStatisticModule::Inst()->SubmitLiveP2PDownloadBytes(buffer.Length());
        }

        p2p_downloader_->GetInstance()->AddSubPiece(packet.sub_piece_info_, buffer);

        ++total_unused_subpiece_count_;
    }

    // 每秒执行一次
    void LiveSubPieceRequestManager::OnP2PTimer(uint32_t times)
    {
        for (std::multimap<protocol::LiveSubPieceInfo, LiveSubPieceRequestTask::p>::iterator iter = request_tasks_.begin();
            iter != request_tasks_.end(); ++iter)
        {
            iter->second->request_time_elapse_ += 1000;
        }

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
}