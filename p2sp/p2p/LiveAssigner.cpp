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

    void LiveAssigner::OnP2PTimer(boost::uint32_t times, bool urgent)
    {
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

        AssignerPeers();
    }

    // 根据已有的subpiece_cout和需要分配的capacity
    // 计算出还需要请求多少片piece才能到capacity
    void LiveAssigner::CalcSubpieceTillCapacity()
    {
        // 按照1s 300KB的理论速度 (500ms 150KB)
        boost::uint32_t last_subpiece_count = -1;
        while (true)
        {
            boost::uint32_t subpiece_count = CaclSubPieceAssignMap();

            if (subpiece_count >= 150 || subpiece_count == last_subpiece_count)
            {
                return;
            }

            last_subpiece_count = subpiece_count;

            bool added = false;

            for (std::set<LiveDownloadDriver__p>::const_iterator iter = p2p_downloader_->GetDownloadDriverSet().begin();
                iter != p2p_downloader_->GetDownloadDriverSet().end(); ++iter)
            {
                if ((*iter)->RequestNextBlock(p2p_downloader_))
                {
                    added = true;
                    break;
                }
            }

            if (added == false)
            {
                return;
            }
        }
    }

    // 分配subpiece的任务
    boost::uint32_t LiveAssigner::CaclSubPieceAssignMap()
    {
        subpiece_assign_deque_.clear();

        boost::uint32_t total_unique_subpiece_count = 0;

        for (std::list<protocol::LiveSubPieceInfo>::iterator iter = p2p_downloader_->GetBlockTasks().begin();
            iter != p2p_downloader_->GetBlockTasks().end(); ++iter)
        {
            total_unique_subpiece_count += AssignForMissingSubPieces(iter->GetBlockId(), false);

            if (urgent_ && iter == p2p_downloader_->GetBlockTasks().begin())
            {
                boost::uint32_t missing_subpieces = CountMissingSubPieces(iter->GetBlockId());
                if (missing_subpieces < 30)
                {
                    AssignForMissingSubPieces(iter->GetBlockId(), true);
                }
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

    boost::uint32_t LiveAssigner::AssignForMissingSubPieces(boost::uint32_t block_id, bool igore_requesting_subpieces)
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
                    if (igore_requesting_subpieces || !p2p_downloader_->IsRequesting(live_subpiece_info))
                    {
                        missing_subpiece_count_in_block++;
                        subpiece_assign_deque_.push_back(live_subpiece_info);
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
            
            peer_connection_recvtime_list_.push_back(PEER_RECVTIME(0, peer));
        }
    }

    void LiveAssigner::AssignerPeers()
    {
        for (std::deque<protocol::LiveSubPieceInfo>::const_iterator subpiece_iter = subpiece_assign_deque_.begin();
            subpiece_iter != subpiece_assign_deque_.end(); ++subpiece_iter)
        {
            for (std::list<PEER_RECVTIME>::iterator peer_iter = peer_connection_recvtime_list_.begin();
                peer_iter != peer_connection_recvtime_list_.end(); ++peer_iter)
            {
                const protocol::LiveSubPieceInfo & subpiece = *subpiece_iter;
                LivePeerConnection__p peer = (*peer_iter).peer;

                if (peer->HasSubPiece(subpiece))
                {
                    peer->AddAssignedSubPiece(subpiece);

                    peer_iter->recv_time += peer->GetAvgDeltaTime();

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

                    break;
                }
            }
        }
    }
}