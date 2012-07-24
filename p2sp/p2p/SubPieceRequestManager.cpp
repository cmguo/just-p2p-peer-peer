//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "p2sp/p2p/SubPieceRequestManager.h"
#include "p2sp/download/DownloadDriver.h"
#include "storage/Instance.h"
#include "statistic/DACStatisticModule.h"
#include "statistic/StatisticModule.h"
#include "p2sp/p2p/P2PDownloader.h"

using namespace storage;

namespace p2sp
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_sub_piece_request_manager = log4cplus::Logger::
        getInstance("[sub_piece_request_manager]");
#endif

    void SubPieceRequestManager::Start(P2PDownloader__p p2p_downloader)
    {
        if (is_running_ == true) return;

        p2p_downloader_ = p2p_downloader;

        LOG4CPLUS_INFO_LOG(logger_sub_piece_request_manager, "SubPieceRequestManager::Start " << p2p_downloader_);

        is_running_ = true;

        block_size_ = p2p_downloader_->GetInstance()->GetBlockSize();

        assert(request_tasks_.size() == 0);
    }

    void SubPieceRequestManager::Stop()
    {
        if (is_running_ == false) return;

        LOG4CPLUS_INFO_LOG(logger_sub_piece_request_manager, "SubPieceRequestManager::Stop " << p2p_downloader_);

        std::multimap<protocol::SubPieceInfo, SubPieceRequestTask *>::const_iterator iter;

        for (iter = request_tasks_.begin(); iter != request_tasks_.end(); ++iter)
        {
            delete iter->second;
        }

        request_tasks_.clear();

        p2p_downloader_.reset();

        is_running_ = false;
    }

    void SubPieceRequestManager::OnP2PTimer(uint32_t times)
    {
        if (is_running_ == false) return;

        // if (times % 2 == 0)  // 500ms
        {    // 每个一秒检查超时
            CheckExternalTimeout();
        }
    }

    // 收到Subpiece
    void SubPieceRequestManager::OnSubPiece(protocol::SubPiecePacket const & packet)
    {
        if (is_running_ == false) return;


        // 首先入库  添加到Instance中
        // 在Ｉｎｓｔａｎｃｅ中 判断该 piece 是否已经 满了
        //     如果是 则p2pdownloader->OnPieceComplete();
        //
        // 然后在requesting_map中找到相应的 SubPieceInfo
        //     如果没有找到 直接 return
        //     peerconnection::p->OnSubPiece ();
        // 然后在requesting_map中删除此条记录
        //     Remove(protocol::SubPieceInfo)

        protocol::SubPieceInfo sub_piece(packet.sub_piece_info_);

        protocol::SubPieceBuffer buffer(packet.sub_piece_content_, packet.sub_piece_length_);
        assert(buffer.Length() <= SUB_PIECE_SIZE);

        LOG4CPLUS_DEBUG_LOG(logger_sub_piece_request_manager, "Check HasSubPiece");
        if (false == p2p_downloader_->HasSubPiece(sub_piece))
        {
            //assert(false == p2p_downloader_->GetInstance()->HasSubPiece(sub_piece));
            LOG4CPLUS_DEBUG_LOG(logger_sub_piece_request_manager, "Recv Subpiece = " << sub_piece);
            p2p_downloader_->NoticeSubPiece(sub_piece);

            if (p2p_downloader_->IsOpenService())
            {
                LOG4CPLUS_DEBUG_LOG(logger_sub_piece_request_manager, "SubmitP2PDownloadBytes");
                statistic::DACStatisticModule::Inst()->SubmitP2PDownloadBytes(buffer.Length());
            }
            p2p_downloader_->GetStatistic()->SubmitRecievedSubPieceCount(1);
        }

        // 统计p2p总收到报文数
        p2p_downloader_->GetStatistic()->SubmitUnusedSubPieceCount(1);

        // 统计p2p总下载字节数
        statistic::StatisticModule::Inst()->SubmitTotalP2PDataBytes(buffer.Length());

        p2p_downloader_->GetInstance()->AsyncAddSubPiece(sub_piece, buffer);

        if (p2p_downloader_->GetDownloadDrivers().size() != 0)
        {
             LOG4CPLUS_INFO_LOG(logger_sub_piece_request_manager, "SubPieceRequestManager::OnSubPiece " << 
                 (*(p2p_downloader_->GetDownloadDrivers().begin()))->GetDownloadDriverID() << " " << 0 << " " 
                 << 1 << " " << " " << sub_piece);
        }

        std::multimap<protocol::SubPieceInfo, SubPieceRequestTask *>::iterator iter;
        for (iter = request_tasks_.find(sub_piece); iter != request_tasks_.end() && iter->first == sub_piece;)
        {
            SubPieceRequestTask * sub_piece_request_task = iter->second;
            boost::intrusive_ptr<ConnectionBase> peer_connection = sub_piece_request_task->peer_connection_;
            if (peer_connection->GetEndpoint() == packet.end_point)
            {
                uint32_t response_time = sub_piece_request_task->request_time_elapse_;

                LOG4CPLUS_INFO_LOG(logger_sub_piece_request_manager, "bingo SubpieceInfo = " << sub_piece << 
                    " RTT = " << response_time);

                if (true == sub_piece_request_task->dead_)
                {
                    statistic::PeerConnectionStatistic::p statistic = peer_connection->GetStatistic();
                    if (statistic) {
                        statistic->SubmitRTT(response_time);
                    }
                }
                else
                {
                    peer_connection->OnSubPiece(response_time, packet.sub_piece_length_);
                }

                delete iter->second;
                request_tasks_.erase(iter++);
            }
            else
            {
                iter++;
            }
        }
    }

    void SubPieceRequestManager::CheckExternalTimeout()
    {
        if (is_running_ == false) return;

        // 遍历整个 request_task_ 找出超时的 (protocol::SubPieceInfo, PieceTask)
        //     PeerConnection->OnTimeOut();
        //     Remove();

        std::multimap<protocol::SubPieceInfo, SubPieceRequestTask *>::iterator iter;
        for (iter = request_tasks_.begin(); iter != request_tasks_.end();)
        {
            SubPieceRequestTask * subpiece_request_task = iter->second;

            subpiece_request_task->request_time_elapse_ += 250;

            if (true == subpiece_request_task->dead_ && subpiece_request_task->request_time_elapse_ >= 10 * 1000)
            {
                delete iter->second;
                request_tasks_.erase(iter++);
            }
            else if (false == subpiece_request_task->dead_ && subpiece_request_task->IsTimeOut())
            {
                subpiece_request_task->peer_connection_->OnTimeOut();
                subpiece_request_task->dead_ = true;
                ++iter;
            }
            else
            {
                ++iter;
            }
        }
    }

    uint32_t SubPieceRequestManager::GetRequestingCount(const protocol::SubPieceInfo& subpiece_info, boost::uint32_t time_elapsed) const
    {
        if (false == is_running_) return false;

        uint32_t count = 0;

        std::multimap<protocol::SubPieceInfo, SubPieceRequestTask *>::const_iterator it;
        for (it = request_tasks_.find(subpiece_info); it != request_tasks_.end() && it->first == subpiece_info; ++it)
        {
            SubPieceRequestTask * task = it->second;
            if (!task->dead_ && true == task->peer_connection_->IsRunning())
            {
                boost::uint32_t elapsed = task->GetTimeElapsed();
                if (elapsed < time_elapsed)
                {
                    ++count;
                }
            }
        }
        return count;
    }

    void SubPieceRequestManager::Add(const protocol::SubPieceInfo& subpiece_info, boost::uint32_t timeout,
        boost::intrusive_ptr<ConnectionBase> peer_connection)
    {
        if (is_running_ == false) return;

        // 如果 该 subpiece_info 在 request_tasks_ 里面找得到的
        //        assert(0);
        //
        // 然后更具 subpiece_info，timeout, peer_connection 构造出 一个 SubPieceRequestTask
        //        然后将这个Task添加到 request_tasks_ 中

        // if (request_tasks_.find(subpiece_info) != request_tasks_.end())
        //    assert(0);

        SubPieceRequestTask * subpiece_request_task = new SubPieceRequestTask(timeout, peer_connection);

        request_tasks_.insert(std::make_pair(subpiece_info, subpiece_request_task));

        protocol::PieceInfo piece;
        protocol::PieceInfo::MakeByPosition(subpiece_info.GetPosition(block_size_), block_size_, piece);
        p2p_downloader_->OnPieceRequest(piece);
    }
}
