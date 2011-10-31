#include "Common.h"
#include "LiveAssigner.h"
#include "LiveP2PDownloader.h"
#include "p2sp/download/LiveDownloadDriver.h"


namespace p2sp
{
    void LiveAssigner::Start(LiveP2PDownloader__p p2p_downloader)
    {
        p2p_downloader_ = p2p_downloader;
    }

    void LiveAssigner::OnP2PTimer(boost::uint32_t times, bool urgent)
    {
		// 500ms分配一次
		if (times % 2 == 0)
		{			
			return;
		}

        // 根据已有的subpiece_cout和需要分配的capacity
        // 计算出还需要请求多少片piece才能到capacity
        CalcSubpieceTillCapacity();

        // 分配subpiece的任务
        CaclSubPieceAssignMap(urgent);

        CaclPeerConnectionRecvTimeMap();

        AssignerPeers();
    }

    // 根据已有的subpiece_cout和需要分配的capacity
    // 计算出还需要请求多少片piece才能到capacity
    void LiveAssigner::CalcSubpieceTillCapacity()
    {
        while (p2p_downloader_->GetBlockTaskNum() < 3)
        {
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
    void LiveAssigner::CaclSubPieceAssignMap(bool urgent)
    {
        subpiece_assign_deque_.clear();

        for (std::list<protocol::LiveSubPieceInfo>::iterator iter = p2p_downloader_->GetBlockTasks().begin();
            iter != p2p_downloader_->GetBlockTasks().end(); ++iter)
        {
            if (p2p_downloader_->HasSubPieceCount((*iter).GetBlockId()))
            {
                // 已经知道该片piece有多少个subpiece了
                // 因此从头到尾依次分配
                int retry = 1;
                for (int j = 0; j < retry; j++)
                {
                    for (boost::uint16_t i=0; i<p2p_downloader_->GetSubPieceCount((*iter).GetBlockId()); i++)
                    {
                    
                        // 构造 subpiece_info
                        protocol::LiveSubPieceInfo live_subpiece_info((*iter).GetBlockId(), i);

                        if (!p2p_downloader_->GetInstance()->HasSubPiece(live_subpiece_info) && 
                            !p2p_downloader_->IsRequesting(live_subpiece_info))
                        {
                            subpiece_assign_deque_.push_back(live_subpiece_info);
                        }
                    }

                    // 第一个Block的没有的subpiece数量<20, 冗余一次
                    if (urgent && subpiece_assign_deque_.size() < 20 && 
                        iter == p2p_downloader_->GetBlockTasks().begin())
                    {
                        // 冗余
                        retry = 3;
                    }
                }
            }
        }

        if (subpiece_assign_deque_.size() <= 30)
        {
            std::deque<protocol::LiveSubPieceInfo>::const_iterator end_iter = subpiece_assign_deque_.end();

            for (std::deque<protocol::LiveSubPieceInfo>::const_iterator iter = subpiece_assign_deque_.begin();
                iter != end_iter;
                ++iter)
            {
                subpiece_assign_deque_.push_back(*iter);
            }
        }
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