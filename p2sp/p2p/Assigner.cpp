//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include <limits>
#include <cmath>

#include "statistic/P2PDownloaderStatistic.h"
#include "p2sp/p2p/Assigner.h"
#include "p2sp/p2p/PeerConnection.h"
#include "p2sp/p2p/P2PDownloader.h"
#include "p2sp/p2p/SubPieceRequestManager.h"
#include "p2sp/download/DownloadDriver.h"
#include "p2sp/p2p/P2SPConfigs.h"

#include "storage/Instance.h"

#ifdef COUNT_CPU_TIME
#include "count_cpu_time.h"
#endif

#define P2P_DEBUG(s) LOG(__DEBUG, "P2P", s)
#define P2P_INFO(s)    LOG(__INFO, "P2P", s)
#define P2P_EVENT(s) LOG(__EVENT, "P2P", s)
#define P2P_WARN(s)    LOG(__WARN, "P2P", s)
#define P2P_ERROR(s) LOG(__ERROR, "P2P", s)

using namespace storage;
namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("assigner");
    void Assigner::Start()
    {
        if (is_running_ == true) return;

        P2P_EVENT("Assigner Start");

        is_running_ = true;

        // 两个std::std::map 的 assert
        assert(subpiece_assign_map_.size() == 0);

        block_size_ = p2p_downloader_->GetInstance()->GetBlockSize();
        file_length_ = p2p_downloader_->GetInstance()->GetFileLength();

        redundant_assign_count_ = 0;
        total_assign_count_ = 0;

        assign_peer_count_limit_ = 0;
        is_end_of_file_ = false;
    }

    void Assigner::Stop()
    {
        if (is_running_ == false) return;

        P2P_EVENT("Assigner Stop" << p2p_downloader_);
        // 两个std::std::map 的 clear

        subpiece_assign_map_.clear();
        peer_connection_recvtime_list_.clear();

        if (p2p_downloader_)
        {
            p2p_downloader_.reset();
        }

        is_running_ = false;
    }

    void Assigner::OnP2PTimer(uint32_t times)
    {
#ifdef COUNT_CPU_TIME
        count_cpu_time(__FUNCTION__);
#endif
        if (!is_running_ || times % 2 == 1)
        {
            return;
        }

        // 以下代码500ms执行一次

        // 设置共享内存中的每个连接经过上轮剩余的分配到的Subpiece个数
        p2p_downloader_->SetAssignedLeftSubPieceCount();

        // 根据已有的subpiece_cout和需要分配的capacity
        // 计算出还需要请求多少片piece才能到capacity
        CalcSubpieceTillCapatity();

        // 分配subpiece的任务
        CaclSubPieceAssignMap();

        CaclPeerConnectionRecvTimeMap();

        AssignerPeers();

        p2p_downloader_->GetStatistic()->SubmitAssignedSubPieceCount(subpiece_assign_map_.size());
    }

    // 预分配peer的队列生成，分配顺序保存在peer_connection_recvtime_list_中
    void Assigner::CaclPeerConnectionRecvTimeMap()
    {
#ifdef COUNT_CPU_TIME
        count_cpu_time(__FUNCTION__);
#endif
        if (is_running_ == false) return;

        P2P_EVENT("Assigner::CaclPeerConnectionRecvTimeMap " << p2p_downloader_);

        // 清空 peer_connection_recvtime_list_ 以便于重新计算 预测收报时间
        peer_connection_recvtime_list_.clear();

        std::map<Guid, PeerConnection::p> ::iterator iter;
        for (iter = p2p_downloader_->peers_.begin(); iter != p2p_downloader_->peers_.end(); ++iter)
        {
            PeerConnection::p peer;
            peer = iter->second;
            P2P_EVENT("Assigner::CaclPeerConnectionRecvTimeMap p2p_downloader:" << p2p_downloader_ << " peer:" << peer << ", TaskQueueRemaining:" << peer->GetTaskQueueSize());
            peer->GetStatistic()->SetAssignedLeftSubPieceCount(peer->GetTaskQueueSize());
            peer->ClearTaskQueue();
            if (peer->HasRidInfo() && peer->IsRidInfoValid())
            {
                peer_connection_recvtime_list_.push_back(PEER_RECVTIME(peer->GetSortedValueForAssigner(), peer));
            }
        }
        peer_connection_recvtime_list_.sort();

        // 控制调度大小
        if (peer_connection_recvtime_list_.size() > P2SPConfigs::P2P_DOWNLOAD_MAX_SCHEDULE_COUNT)
        {
            std::list<PEER_RECVTIME>::iterator it = peer_connection_recvtime_list_.begin();
            for (boost::int32_t i = 0; i<P2SPConfigs::P2P_DOWNLOAD_MAX_SCHEDULE_COUNT; ++i)
            {
                ++it;
            }
            peer_connection_recvtime_list_.erase(it, peer_connection_recvtime_list_.end());
        }
    }

    // 计算当前piece_task中，可以提供分配subpiece数
    // @return p2p_downloader_ 的 piece_tasks_ 中正在被请求的subpiece个数
    uint32_t Assigner::CaclSubPieceAssignCount()
    {
#ifdef COUNT_CPU_TIME
        count_cpu_time(__FUNCTION__);
#endif
        if (is_running_ == false)
        {
            return 0;
        }

        P2P_EVENT("Assigner::CaclSubPieceAssignCount" << p2p_downloader_);

        uint32_t subpiece_count = 0;
        protocol::PieceInfo piece;
        uint32_t sub_piece_end;
        uint32_t first_empty_offset_ = -1;
        uint32_t last_data_offset_ = -1;

        // 用一个std::set来过滤掉重复的Piece
        std::set <protocol::PieceInfoEx> piece_info_s;
        for (std::multimap<protocol::PieceInfoEx, DownloadDriver__p>::iterator iter = p2p_downloader_->piece_tasks_.begin();
            iter != p2p_downloader_->piece_tasks_.end(); ++iter)
        {
            if (piece_info_s.find(iter->first) == piece_info_s.end())
            {
                piece_info_s.insert(iter->first);

                piece = iter->first.GetPieceInfo();

                if (piece.GetEndPosition(block_size_) > file_length_)
                {
                    sub_piece_end = (file_length_ - piece.GetPosition(block_size_) - 1) / SUB_PIECE_SIZE;
                }
                else
                {
                    sub_piece_end = iter->first.subpiece_index_end_;
                }

                for (uint32_t i = iter->first.subpiece_index_; i <= sub_piece_end; ++i)
                {
                    protocol::SubPieceInfo sub_piece(piece.block_index_, piece.piece_index_ * SUB_PIECE_COUNT_PER_PIECE + i);

                    // 本地没有该subpiece AND 该subpiece没有正在请求
                    if (!p2p_downloader_->HasSubPiece(sub_piece))
                    {
                        if (first_empty_offset_ == -1)
                        {
                            first_empty_offset_ = sub_piece.GetEndPosition(block_size_);
                        }
                        if (!p2p_downloader_->subpiece_request_manager_.IsRequesting(sub_piece))
                        {
                            ++subpiece_count;
                        }
                    }
                    else
                    {
                        last_data_offset_ = sub_piece.GetEndPosition(block_size_);
                    }
                }
            }
        }

        p2p_downloader_->GetStatistic()->SetEmptySubpieceDistance((last_data_offset_-first_empty_offset_)/1024);
        return subpiece_count;
    }

    // 计算并分配需要去下载的subpiece, 将这些subpiece加入subpiece_assign_map_
    void Assigner::CaclSubPieceAssignMap()
    {
#ifdef COUNT_CPU_TIME
        count_cpu_time(__FUNCTION__);
#endif
        if (!is_running_)
        {
            return;
        }

        P2P_EVENT("Assigner::CaclSubPieceAssignMap " << p2p_downloader_);

        // 总的可供分配的subpiece数，与分段结尾的强冗余算法相关
        boost::uint32_t total_subpiece_count = 0;


        // 遍历 downloader 里面的 PieceTask
        // 依次找到需要下载的SubPiece, 然后添加到 subpiece_assign_map_ 里面
        // 记得从 SubPieceRequestManager 中记录了正在请求的SubPiece

        // 清空 subpiece_assign_map_ 以便于重新计算 SubPiece顺序
        subpiece_assign_map_.clear();
        std::multimap<protocol::PieceInfoEx, DownloadDriver__p>::iterator iter;
        // 用一个std::set来过滤掉重复的Piece
        std::set <protocol::PieceInfoEx> piece_info_s;
        // 待分配的piece中，最后一片subpiece(一般为128, 文件末尾另外计算)
        uint32_t subpiece_end;

        protocol::PieceInfo piece;

        uint32_t index = 0;
        bool eof_file = false;

        LOG(__DEBUG, "ppdebug", "piece task size = " << p2p_downloader_->piece_tasks_.size());

        if (p2p_downloader_->piece_tasks_.size() > 0)
        {
            piece = p2p_downloader_->piece_tasks_.rbegin()->first.GetPieceInfo();
            uint32_t end_position = piece.GetEndPosition(block_size_);

            if (end_position >= file_length_ && end_position < file_length_ + PIECE_SIZE)
            {
                is_end_of_file_ = eof_file = true;
            }
        }

        piece_info_s.clear();
        for (std::multimap<protocol::PieceInfoEx, DownloadDriver__p>::iterator iter = p2p_downloader_->piece_tasks_.begin();
            iter != p2p_downloader_->piece_tasks_.end(); ++iter, ++index)
        {
            if (piece_info_s.find(iter->first) == piece_info_s.end())
            {
                piece_info_s.insert(iter->first);
                piece = iter->first.GetPieceInfo();

                // 修正subpiece_end
                uint32_t end_position = piece.GetEndPosition(block_size_);
                if (end_position >= file_length_)
                {
                    // 文件末尾情况，subpiece_end 需要计算
                    P2P_EVENT("Assigner::CaclSubPieceAssignMap EndPiece: FileLength:" << file_length_ << " piece.getpossition:" << piece.GetPosition(block_size_));
                    assert(end_position < file_length_ + PIECE_SIZE);
                    subpiece_end = (file_length_ - (end_position - PIECE_SIZE) - 1) / SUB_PIECE_SIZE;
                }
                else
                {
                    // 通常情况，subpiece_end 为 127
                    subpiece_end = iter->first.subpiece_index_end_;
                }

                uint32_t block_p = piece.block_index_ * block_size_;
                uint32_t needdown_sp_num = 0;

                // 遍历piece中的每一个subpiece并正常分配
                for (boost::uint32_t i = iter->first.subpiece_index_; i <= subpiece_end; ++i)
                {
                    protocol::SubPieceInfo sub_piece(piece.block_index_, piece.piece_index_ * SUB_PIECE_COUNT_PER_PIECE + i);

                    if (false == p2p_downloader_->HasSubPiece(sub_piece))
                    {
                        ++needdown_sp_num;
                        // 正常分配
                        if (!p2p_downloader_->subpiece_request_manager_.IsRequesting(sub_piece))
                        {
                            subpiece_assign_map_.push_back(sub_piece);
                            total_subpiece_count++;
                        }
                    }
                }

                switch (p2p_downloader_->GetDownloadMode())
                {
                case IP2PControlTarget::NORMAL_MODE:

                    // 冗余下载，使得piece更快完成
                    if (index <= 5 || eof_file)
                    {
                        // 下载到文件尾部
                        if (eof_file && needdown_sp_num < P2SPConfigs::ASSIGN_REDUNTANT_DECISION_SIZE * 3)
                        {
                            for (boost::uint32_t i = iter->first.subpiece_index_; i <= subpiece_end; ++i)
                            {
                                protocol::SubPieceInfo sub_piece(piece.block_index_, piece.piece_index_ * SUB_PIECE_COUNT_PER_PIECE + i);
                                if (false == p2p_downloader_->HasSubPiece(sub_piece)
                                    && p2p_downloader_->subpiece_request_manager_.IsRequesting(sub_piece))
                                {
                                    // 下载到文件结尾所有正在请求且未到达的piece每次预分配冗余一次
                                    subpiece_assign_map_.push_back(sub_piece);
                                    P2P_DEBUG("redundant subpiece " << sub_piece);
                                }
                            }
                        }
                        // 当前下载的第一片Piece
                        else if (index == 0)
                        {
                            for (boost::uint32_t i = iter->first.subpiece_index_; i <= subpiece_end; ++i)
                            {
                                protocol::SubPieceInfo sub_piece(piece.block_index_, piece.piece_index_ * SUB_PIECE_COUNT_PER_PIECE + i);
                                if (false == p2p_downloader_->HasSubPiece(sub_piece))
                                {
                                    // 如果剩余subpiece数小于3片，每一次预分配冗余2次
                                    if (needdown_sp_num <= 3)
                                    {
                                        subpiece_assign_map_.push_back(sub_piece);
                                        subpiece_assign_map_.push_back(sub_piece);
                                        P2P_DEBUG("redundant subpiece twice: " << sub_piece);
                                        continue;
                                    }

                                    // 如果剩余subpiece数小于10片，每一次预分配冗余1次
                                    if (needdown_sp_num <= 10)
                                    {
                                        subpiece_assign_map_.push_back(sub_piece);
                                        P2P_DEBUG("redundant subpiece: " << sub_piece);
                                        continue;
                                    }

                                    // 正在请求的subpiece每一秒冗余一次
                                    if (p2p_downloader_->subpiece_request_manager_.IsRequesting(sub_piece)
                                        && (p2p_downloader_->subpiece_request_manager_.IsRequestingTimeout(sub_piece, P2SPConfigs::ASSIGN_CONTINUOUS_REDUNTANT_DECISION_TIMEOUT, 10)))
                                    {
                                        subpiece_assign_map_.push_back(sub_piece);
                                        P2P_DEBUG("redundant subpiece: " << sub_piece);
                                        continue;
                                    }
                                }
                            }
                        }  // else if (index == 0)
                        else
                        {
                            uint32_t redundant_decision_mod = index <= P2SPConfigs::ASSIGN_REDUNTANT_PIECE_COUNT ?  0 : P2SPConfigs::ASSIGN_REDUNTANT_DECISION_SIZE_INTERVAL;
                            if (needdown_sp_num < P2SPConfigs::ASSIGN_REDUNTANT_DECISION_SIZE - redundant_decision_mod)
                            {
                                for (boost::uint32_t i = iter->first.subpiece_index_; i <= subpiece_end; ++i)
                                {
                                    protocol::SubPieceInfo sub_piece(piece.block_index_, piece.piece_index_ * SUB_PIECE_COUNT_PER_PIECE + i);

                                    if (false == p2p_downloader_->HasSubPiece(sub_piece))
                                    {
                                        // 紧急的piece每一秒冗余一次，不紧急的piece每3秒冗余一次
                                        uint32_t redundant_times = index <= P2SPConfigs::ASSIGN_REDUNTANT_PIECE_COUNT ?
                                            P2SPConfigs::ASSIGN_REDUNTANT_TIMES_URGENT :  P2SPConfigs::ASSIGN_REDUNTANT_TIMES_NORMAL;
                                        if (p2p_downloader_->subpiece_request_manager_.IsRequesting(sub_piece)
                                            && (p2p_downloader_->subpiece_request_manager_.IsRequestingTimeout(sub_piece, P2SPConfigs::ASSIGN_REDUNTANT_DECISION_TIMEOUT, 10)
                                            && p2p_downloader_->subpiece_request_manager_.GetRequestingCount(sub_piece, 3000)  < redundant_times))
                                        {
                                            // 正在请求的subpiece按一定条件冗余
                                            subpiece_assign_map_.push_back(sub_piece);
                                            P2P_DEBUG("redundant subpiece " << sub_piece);
                                        }
                                    }
                                }
                            }
                        }
                    }  // 冗余下载
                    break;

                case IP2PControlTarget::CONTINUOUS_MODE:
                    if (index == 0)
                    {
                        for (boost::uint32_t i = iter->first.subpiece_index_; i <= subpiece_end; ++i)
                        {
                            protocol::SubPieceInfo sub_piece(piece.block_index_, piece.piece_index_ * SUB_PIECE_COUNT_PER_PIECE + i);
                            if (false == p2p_downloader_->HasSubPiece(sub_piece))
                            {
                                // 如果剩余subpiece数小于5片，每一次预分配冗余2次
                                if (needdown_sp_num <= 5)
                                {
                                    subpiece_assign_map_.push_back(sub_piece);
                                    subpiece_assign_map_.push_back(sub_piece);
                                    P2P_DEBUG("redundant subpiece twice: " << sub_piece);
                                }

                                // 如果剩余subpiece数小于5片，每一次预分配冗余1次
                                else if (needdown_sp_num <= 15)
                                {
                                    subpiece_assign_map_.push_back(sub_piece);
                                    P2P_DEBUG("redundant subpiece: " << sub_piece);
                                }

                                // 正在请求的subpiece每一秒冗余一次
                                else if (p2p_downloader_->subpiece_request_manager_.IsRequesting(sub_piece)
                                    && (p2p_downloader_->subpiece_request_manager_.IsRequestingTimeout(sub_piece, P2SPConfigs::ASSIGN_CONTINUOUS_REDUNTANT_DECISION_TIMEOUT, 10)))
                                {
                                    subpiece_assign_map_.push_back(sub_piece);
                                    P2P_DEBUG("redundant subpiece: " << sub_piece);
                                }
                            }
                        }
                    }
                    // 下载到文件尾
                    else if (eof_file && needdown_sp_num < P2SPConfigs::ASSIGN_REDUNTANT_DECISION_SIZE * 3)
                    {
                        for (boost::uint32_t i = iter->first.subpiece_index_; i <= subpiece_end; ++i)
                        {
                            protocol::SubPieceInfo sub_piece(piece.block_index_, piece.piece_index_ * SUB_PIECE_COUNT_PER_PIECE + i);
                            if (false == p2p_downloader_->HasSubPiece(sub_piece)
                                && p2p_downloader_->subpiece_request_manager_.IsRequesting(sub_piece))
                            {
                                // 下载到文件结尾所有正在请求且未到达的piece每次预分配冗余一次
                                subpiece_assign_map_.push_back(sub_piece);
                                P2P_DEBUG("redundant subpiece " << sub_piece);
                            }
                        }
                    }
                    else
                    {
                        if (needdown_sp_num < P2SPConfigs::ASSIGN_REDUNTANT_DECISION_SIZE * 3)
                        {
                            for (boost::uint32_t i = iter->first.subpiece_index_; i <= subpiece_end; ++i)
                            {
                                protocol::SubPieceInfo sub_piece(piece.block_index_, piece.piece_index_ * SUB_PIECE_COUNT_PER_PIECE + i);

                                if (false == p2p_downloader_->HasSubPiece(sub_piece))
                                {
                                    // 正在请求的subpiece每一秒冗余1次
                                    if (p2p_downloader_->subpiece_request_manager_.IsRequesting(sub_piece)
                                        && (p2p_downloader_->subpiece_request_manager_.IsRequestingTimeout(sub_piece, P2SPConfigs::ASSIGN_CONTINUOUS_REDUNTANT_DECISION_TIMEOUT, 10)))
                                    {
                                        subpiece_assign_map_.push_back(sub_piece);
                                        P2P_DEBUG("redundant subpiece: " << sub_piece);
                                        continue;
                                    }
                                }
                            }
                        }
                    }
                    break;
                case IP2PControlTarget::FAST_MODE:
                    if (eof_file && needdown_sp_num < P2SPConfigs::ASSIGN_REDUNTANT_DECISION_SIZE * 4)
                    {
                        for (boost::uint32_t i = iter->first.subpiece_index_; i <= subpiece_end; ++i)
                        {
                            protocol::SubPieceInfo sub_piece(piece.block_index_, piece.piece_index_ * SUB_PIECE_COUNT_PER_PIECE + i);
                            if (false == p2p_downloader_->HasSubPiece(sub_piece)
                                && p2p_downloader_->subpiece_request_manager_.IsRequesting(sub_piece))
                            {
                                // 下载到文件结尾所有正在请求且未到达的piece每次预分配冗余一次
                                subpiece_assign_map_.push_back(sub_piece);
                                P2P_DEBUG("FAST_MODE redundant subpiece " << sub_piece); 
                            }
                        }
                    }
                    break;
                default:
                    break;
                }
            }
        }

        // 如果可供分配的subpiece任务太少，则进行强冗余
        // 冗余次数
        boost::uint32_t repeat = 0;

        if (total_subpiece_count < 32)
        {
            // 当可供分配的subpiece数量 < 96, 全部冗余4次
            repeat = 4;
        }
        else if (total_subpiece_count < 64)
        {
            // 当可供分配的subpiece数量 < 96, 全部冗余2次
            repeat = 2;
        }
        else if (total_subpiece_count < 96)
        {
            // 当可供分配的subpiece数量 < 96, 全部冗余1次
            repeat = 1;
        }

        // 将待冗余的subpiece加入临时的map
        std::deque<protocol::SubPieceInfo> tmp_subpiece_assign_map;

        for (boost::uint32_t i = 0; i<repeat; i++)
        {
             for (std::deque<protocol::SubPieceInfo>::iterator iter = subpiece_assign_map_.begin();
                 iter != subpiece_assign_map_.end(); ++iter)
             {
                 tmp_subpiece_assign_map.push_back(*iter);
             }
        }

        // 将临时mp中的所有subpiece任务合并到subpiece_assign_map_中
        for (std::deque<protocol::SubPieceInfo>::iterator iter = tmp_subpiece_assign_map.begin();
            iter != tmp_subpiece_assign_map.end(); ++iter)
        {
            subpiece_assign_map_.push_back(*iter);
        }

        LOG(__DEBUG, "assigner", "subpiece_assign_map_.size() = " << subpiece_assign_map_.size());
    }

    // 预分配，将subpiece_assign_map_中的指定任务按照peer_connection_recvtime_list_的顺序依次分配
    void Assigner::AssignerPeers()
    {
#ifdef COUNT_CPU_TIME
        count_cpu_time(__FUNCTION__);
#endif
        if (!is_running_)
        {
            return;
        }

        // 遍历 subpiece_assign_map_
        // {
        //        遍历 peer_connection_recvtime_list_
        //      {
        //            // 如果 PeerConnection::p 没有这个资源, 那么 iter ++
        //          // PeerConnection::p 有这个资源，那么 与分配 PeerConnection::p->AddAssignedSubPiece();
        //          // 改变 PeerConnection::p 在 peer_connection_recvtime_list_ 中的Key 先 erase, 再insert
        //      }
        // }
        //
        // 遍历 peer_connection_recvtime_list_
        //      PeerConnection::p->RequestTillFullWindow()

        std::deque<protocol::SubPieceInfo>::iterator iter_subpiece;
        std::list<PEER_RECVTIME>::iterator iter_peer;
        uint32_t rcvtime;
        PeerConnection::p peer;
        std::map<PeerConnection::p, boost::uint32_t> assigned_peers;
        P2P_DEBUG("Assigner: subpiece_assign_map size = " << subpiece_assign_map_.size());
        for (iter_subpiece = subpiece_assign_map_.begin(); iter_subpiece != subpiece_assign_map_.end(); ++iter_subpiece)
        {
            for (iter_peer = peer_connection_recvtime_list_.begin(); iter_peer != peer_connection_recvtime_list_.end(); ++iter_peer)
            {
                rcvtime = iter_peer->recv_time;
                peer = iter_peer->peer;

                if (peer->HasRidInfo() && peer->IsRidInfoValid() && peer->HasSubPiece(*iter_subpiece))
                {
                    peer->AddAssignedSubPiece(*iter_subpiece);

                    if (peer->GetTaskQueueSize() >= P2SPConfigs::ASSIGN_SUBPIECE_MAX_COUNT_PER_PEER + 30)
                        iter_peer->recv_time =  rcvtime + 6 * peer->GetUsedTime();
                    else if (peer->GetTaskQueueSize() >= P2SPConfigs::ASSIGN_SUBPIECE_MAX_COUNT_PER_PEER)
                        iter_peer->recv_time =  rcvtime + 3 * peer->GetUsedTime();
                    else
                        iter_peer->recv_time =  rcvtime + peer->GetUsedTime();

                    // 从小到大排序peer_connection_recvtime_list_
                    std::list<PEER_RECVTIME>::iterator curr_peer = iter_peer++;

                    std::list<PEER_RECVTIME>::iterator i = iter_peer;
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

        // request all remaining
        for (std::map<Guid, PeerConnection::p> ::iterator iter = p2p_downloader_->peers_.begin(); iter != p2p_downloader_->peers_.end();iter++)
        {
            PeerConnection::p peer;
            peer = iter->second;
            peer->GetStatistic()->SetAverageDeltaTime(peer->GetUsedTime());
            peer->GetStatistic()->SetSortedValue(peer->GetSortedValueForAssigner());
            peer->GetStatistic()->SetAssignedSubPieceCount(peer->GetTaskQueueSize());
            peer->RequestTillFullWindow(false);
        }
    }

    // 根据已有的subpiece_cout和需要分配的capacity
    // 计算出还需要请求多少片piece才能到capacity
    void Assigner::CalcSubpieceTillCapatity()
    {
        // 请求piece的任务之前，首先计算subpiece的数量
        uint32_t subpiece_count_ = CaclSubPieceAssignCount();

        // Assigner 500ms分配一次
        uint32_t capacity_ = p2p_downloader_->GetSpeedLimitRestCount();

        if (p2p_downloader_->GetDownloadMode() == IP2PControlTarget::CONTINUOUS_MODE)
        {
            LIMIT_MAX(capacity_, P2SPConfigs::ASSIGN_MAX_SUBPIECE_COUNT);
        }
        else
        {
            LIMIT_MIN(capacity_, 200);
        }

        P2P_DEBUG(__FUNCTION__ << " AssignCapacity = " << capacity_ << " subpiece_count_ = " << subpiece_count_);

        // 根据已有的subpiece_cout和需要分配的capacity
        // 计算出还需要请求多少片piece才能到capacity
        if (capacity_ > subpiece_count_)
        {
            boost::int32_t piece_num_to_request = (capacity_ - subpiece_count_-1) / 128 + 1;
            P2P_DEBUG(__FUNCTION__ << " RequestNextPiece Num =" << piece_num_to_request);
            for (boost::int32_t i = piece_num_to_request; i > 0; --i)
            {
                // If there are multiple downloaders, it means the same RID is being played from two 
                // different places. We want to pick only one downloader to download (the first one is ok.)
                std::set<DownloadDriver__p>::iterator iter = p2p_downloader_->download_driver_s_.begin();
                if (iter != p2p_downloader_->download_driver_s_.end())
                {
                    // If the downloader failed to request a piece, it means download 
                    // of the RID is complete, all subpeices have been allocated 
                    // or the block size is unknown. 
                    // There is no need to download other pieces in these cases.
                    if (!(*iter)->RequestNextPiece(p2p_downloader_))
                    {
                        break;
                    }
                }
            }
        }
    }
}
