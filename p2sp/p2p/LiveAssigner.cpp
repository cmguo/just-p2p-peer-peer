#include "Common.h"
#include "LiveAssigner.h"
#include "LiveP2PDownloader.h"
#include "p2sp/download/LiveDownloadDriver.h"


namespace p2sp
{
    void LiveAssigner::Start(LiveP2PDownloader__p p2p_downloader)
    {
        p2p_downloader_ = p2p_downloader;
        urgent_ = false;
    }

    void LiveAssigner::OnBlockTimeout(boost::uint32_t block_id)
    {
        block_tasks_.erase(block_id);
    }

    void LiveAssigner::PutBlockTask(const protocol::LiveSubPieceInfo & live_block)
    {
        uint32_t block_id = live_block.GetBlockId();
        if (block_tasks_.find(block_id) == block_tasks_.end())
        {
            block_tasks_.insert(std::make_pair(block_id, live_block));
        }
    }

    void LiveAssigner::OnP2PTimer(boost::uint32_t times, bool urgent, bool use_udpserver, bool paused)
    {
        CheckBlockComplete();
        
        if (paused)
        {
            return;
        }

		// 500ms分配一次
		if (times % 2 == 0)
		{			
			return;
		}

        urgent_ = urgent;

        // 根据已有的subpiece_cout和需要分配的capacity
        // 计算出还需要请求多少片piece才能到capacity
        CalcSubpieceTillCapacity();

        CaclPeerConnectionRecvTimeMap();

        AssignerPeers(use_udpserver);
    }

    void LiveAssigner::CheckBlockComplete()
    {
        // 检查block是否完成
        std::set<protocol::LiveSubPieceInfo> completed_block_set;
        for (std::map<uint32_t, protocol::LiveSubPieceInfo>::iterator iter = block_tasks_.begin();
            iter != block_tasks_.end();
            iter++)
        {
            if (p2p_downloader_->GetInstance()->HasCompleteBlock(iter->first))
            {
                completed_block_set.insert(iter->second);
            }
        }

        for (std::set<protocol::LiveSubPieceInfo>::iterator iter = completed_block_set.begin();
            iter != completed_block_set.end(); ++iter)
        {
            p2p_downloader_->OnBlockComplete(*iter);
        }
    }

    // 根据已有的subpiece_cout和需要分配的capacity
    // 计算出还需要请求多少片piece才能到capacity
    void LiveAssigner::CalcSubpieceTillCapacity()
    {
        // 按照1s 300KB的理论速度 (500ms 150KB)
        while (true)
        {
            boost::uint32_t subpiece_count = CaclSubPieceAssignMap();

            if (subpiece_count >= 150)
            {
                break;
            }

            if (!block_tasks_.empty())
            {
                const protocol::LiveSubPieceInfo & last_task = block_tasks_.rbegin()->second;
                if (!p2p_downloader_->HasSubPieceCount(last_task.GetBlockId()))
                {
                    break;
                }
            }

            if (!p2p_downloader_->RequestNextBlock())
            {
                break;
            }
        }
    }

    // 分配subpiece的任务
    boost::uint32_t LiveAssigner::CaclSubPieceAssignMap()
    {
        subpiece_assign_deque_.clear();
        subpiece_reassign_set_.clear();

        boost::uint32_t total_unique_subpiece_count = 0;
        boost::uint32_t block_task_index = 0;

        bool first_block_urgent_reassign = false;        

        for (std::map<uint32_t,protocol::LiveSubPieceInfo>::iterator iter = block_tasks_.begin(); 
            iter != block_tasks_.end(); ++iter, ++block_task_index)
        {
            uint32_t block_id = iter->first;
            total_unique_subpiece_count += AssignForMissingSubPieces(block_id, false);

            uint32_t missing_subpieces = CountMissingSubPieces(block_id);
            bool reassign = false;

            if (urgent_)
            {
                if (block_task_index == 0 && missing_subpieces < 32)
                {
                    first_block_urgent_reassign = true;
                    reassign = true;
                }
                else if (first_block_urgent_reassign && block_task_index == 1 && missing_subpieces < 16)
                {
                    reassign = true;
                }
            } 
            
            if ((block_task_index == 0 && missing_subpieces <= 2) ||
                (block_task_index == 1 && missing_subpieces <= 1))
            {
                reassign = true;
            }

            if (reassign)
            {
                AssignForMissingSubPieces(block_id, true);
            }
        }

        return total_unique_subpiece_count;
    }

    boost::uint32_t LiveAssigner::CountMissingSubPieces(boost::uint32_t block_id)
    {
        boost::uint32_t missing_subpiece_count_in_block = 0;

        if (p2p_downloader_->HasSubPieceCount(block_id))
        {
            for (boost::uint16_t i=0; i<p2p_downloader_->GetSubPieceCount(block_id); i++)
            {
                protocol::LiveSubPieceInfo live_subpiece_info(block_id, i);

                if (!p2p_downloader_->GetInstance()->HasSubPiece(live_subpiece_info))
                {
                    missing_subpiece_count_in_block++;
                }
            }
        }

        return missing_subpiece_count_in_block;
    }

    boost::uint32_t LiveAssigner::AssignForMissingSubPieces(boost::uint32_t block_id, bool ignore_requesting_subpieces)
    {
        boost::uint32_t missing_subpiece_count_in_block = 0;

        if (p2p_downloader_->HasSubPieceCount(block_id))
        {
            // 已经知道该片piece有多少个subpiece了
            // 因此从头到尾依次分配
            for (boost::uint16_t i=0; i<p2p_downloader_->GetSubPieceCount(block_id); i++)
            {
                // 构造 subpiece_info
                protocol::LiveSubPieceInfo live_subpiece_info(block_id, i);

                if (!p2p_downloader_->GetInstance()->HasSubPiece(live_subpiece_info))
                {
                    bool assign = false;
                    uint32_t requesting = p2p_downloader_->GetRequestingCount(live_subpiece_info);
                    if (requesting == 0)
                    {
                        assign = true;
                    }
                    else if (ignore_requesting_subpieces && requesting < 3)
                    {
                        assign = true;
                    }

                    if (assign)
                    {
                        missing_subpiece_count_in_block++;
                        subpiece_assign_deque_.push_back(live_subpiece_info);

                        if (ignore_requesting_subpieces)
                        {
                            subpiece_reassign_set_.insert(live_subpiece_info);
                        }
                    }
                }
            }
        }

        return missing_subpiece_count_in_block;
    }

    void LiveAssigner::CaclPeerConnectionRecvTimeMap()
    {
        peer_connection_recvtime_list_.clear();

        for (std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::const_iterator 
            iter = p2p_downloader_->GetPeers().begin();
            iter != p2p_downloader_->GetPeers().end(); ++iter)
        {
            LivePeerConnection__p peer = iter->second;
            peer->ClearTaskQueue();
            
            peer_connection_recvtime_list_.push_back(PEER_RECVTIME(peer->GetAvgDeltaTime(), peer));
        }

        peer_connection_recvtime_list_.sort();
    }

    void LiveAssigner::AssignerPeers(bool use_udpserver)
    {
        for (std::deque<protocol::LiveSubPieceInfo>::const_iterator subpiece_iter = subpiece_assign_deque_.begin();
            subpiece_iter != subpiece_assign_deque_.end(); ++subpiece_iter)
        {
            const protocol::LiveSubPieceInfo & subpiece = *subpiece_iter;
            std::list<PEER_RECVTIME>::iterator peer_iter;

            bool is_assign = false;

            if (use_udpserver && subpiece_reassign_set_.find(subpiece) != subpiece_reassign_set_.end())
            {
                for (peer_iter = peer_connection_recvtime_list_.begin();
                    peer_iter != peer_connection_recvtime_list_.end(); ++peer_iter)
                {
                    LivePeerConnection__p peer = peer_iter->peer;

                    if (peer->IsUdpServer())
                    {
                        if (peer->HasSubPieceInBitmap(subpiece) && !peer->HasSubPieceInTaskSet(subpiece))
                        {
                            is_assign = true;
                            break;
                        }
                    }
                }
                
                subpiece_reassign_set_.erase(subpiece);
            }
            
            if (!is_assign)
            {
                for (peer_iter = peer_connection_recvtime_list_.begin();
                    peer_iter != peer_connection_recvtime_list_.end(); ++peer_iter)
                {
                    LivePeerConnection__p peer = peer_iter->peer;

                    if (!use_udpserver && peer->IsUdpServer())
                    {
                        continue;
                    }

                    if (peer->HasSubPieceInBitmap(subpiece) && !peer->HasSubPieceInTaskSet(subpiece))
                    {
                        is_assign = true;
                        break;
                    }
                }
            }

            if (is_assign)
            {
                peer_iter->peer->AddAssignedSubPiece(subpiece);
                 
                peer_iter->recv_time += peer_iter->peer->GetAvgDeltaTime();

                // 从小到大排序peer_connection_recvtime_list_
                std::list<PEER_RECVTIME>::iterator curr_peer = peer_iter++;

                std::list<PEER_RECVTIME>::iterator i = peer_iter;
                for (; i != peer_connection_recvtime_list_.end(); ++i)
                {
                    if (*curr_peer < *i)
                    {
                        break;
                    }
                }

                peer_connection_recvtime_list_.splice(i, peer_connection_recvtime_list_, curr_peer);
            }
        }
    }
}