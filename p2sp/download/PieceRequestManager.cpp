//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/download/PieceRequestManager.h"
#include "p2sp/download/Downloader.h"
#include "p2sp/download/DownloadDriver.h"
#include "p2sp/p2p/P2PDownloader.h"
#include "p2sp/p2p/P2SPConfigs.h"

#include "storage/Instance.h"

namespace p2sp
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_piece_request = log4cplus::Logger::getInstance("[piece_request]");
#endif

    bool PieceTask::IsTimeout() const
    {
        uint32_t get_elapsed_ = start_time_.elapsed();
        return get_elapsed_ > timeout_;
    }

    bool PieceTask::IsTimeout(VodDownloader__p downloader, const protocol::PieceInfoEx& piece_info) const
    {
        uint32_t get_elapsed_ = start_time_.elapsed();

        if (downloader != downloader_)
        {
            if (downloader_->IsP2PDownloader())
            {
                // downloader_  IS P2PDownloader
                // downloader   IS HTTPDownloader
                // HTTP抢P2P

                // P2P被暂停
                if (downloader_->IsPausing())
                {
                    LOG4CPLUS_DEBUG_LOG(logger_piece_request, "IsTimeout 0");
                    return true;
                }
                // P2P分的了Piece但是尚未开始下载
                else if (!bStartDownloading)
                {
                    LOG4CPLUS_DEBUG_LOG(logger_piece_request, "IsTimeout 1");
                    return true;
                }
                // P2P下载超时并且HTTP下载速度超过30KB/S
                else if (get_elapsed_ > 2 * timeout_)
                {
                    LOG4CPLUS_DEBUG_LOG(logger_piece_request, "IsTimeout 2");
                    return true;
                }
                else if (get_elapsed_ > timeout_ && downloader->GetSpeedInfoEx().SecondDownloadSpeed >= 30*1024)
                {
                    LOG4CPLUS_DEBUG_LOG(logger_piece_request, "IsTimeout 3");
                    P2PDownloader * p2p = (P2PDownloader*)downloader_.get();
                    return p2p->CanPreemptive(download_driver_, piece_info);
                }
            }
            else
            {
                // downloader_  IS HTTPDownloader
                // downloader   IS P2PDownloader
                // P2P抢HTTP

                // HTTP被暂停
                if (downloader_->IsPausing())
                {
                    LOG4CPLUS_DEBUG_LOG(logger_piece_request, "IsTimeout 4");
                    return true;
                }

                // HTTP下载超过8秒 且 HTTP速度 < 20KBps  或者  12秒超时
                if ((get_elapsed_ > timeout_ * 0.5 && downloader_->GetSpeedInfo().NowDownloadSpeed < 20*1024) || get_elapsed_ > timeout_)
                {
                    LOG4CPLUS_DEBUG_LOG(logger_piece_request, "IsTimeout 5");
                    return true;
                }
            }
        }
        else
        {
            LOG4CPLUS_DEBUG_LOG(logger_piece_request, "IsP2PDownloader = " << downloader_->IsP2PDownloader() << 
                " PieceInfo = " << piece_info);
        }
        LOG4CPLUS_DEBUG_LOG(logger_piece_request, "IsTimeout 6");
        return false;
    }

    PieceRequestManager::PieceRequestManager(DownloadDriver::p download_driver)
        : download_driver_(download_driver)
        , is_running_(false)
    {
    }

    PieceRequestManager::~PieceRequestManager()
    {
    }

    void PieceRequestManager::Start()
    {
        if (true == is_running_)
            return;
        is_running_ = true;
    }

    void PieceRequestManager::Stop()
    {
        if (false == is_running_)
            return;

        if (download_driver_)
        {
            download_driver_.reset();
        }
        requesting_map_.clear();

        is_running_ = false;
    }

    bool PieceRequestManager::IsRequesting(const protocol::PieceInfo& piece_info) const
    {
        // 如果 requesting_map_ 中找到了，则返回 true, 否则返回 false
        return requesting_map_.find(piece_info) != requesting_map_.end();
    }

    bool PieceRequestManager::AddPieceTask(const protocol::PieceInfo& piece_info, VodDownloader__p downloader)
    {
        if (false == is_running_)
            return false;

        // 如果 在 requesting_map_ 中找到该 piece_info
        //      则 ppassert
        if (IsRequesting(piece_info))
        {
            assert(0);
            return false;
        }
        // 根据 piece_info, downloader, 当前时间 生成一个 PieceTask 然后添加到 requesting_map_ 里面
        PieceTask::p piece_task = PieceTask::Create(download_driver_, downloader, P2SPConfigs::PIECE_TIME_OUT_IN_MILLISEC);
        requesting_map_[piece_info] = piece_task;

        return true;
    }

    void PieceRequestManager::ClearTasks()
    {
        if (false == is_running_)
            return;
        requesting_map_.clear();
    }

    bool PieceRequestManager::RemovePieceTask(const protocol::PieceInfo& piece_info, VodDownloader__p downloader)
    {
        if (false == is_running_)
            return false;
        // 如果 requesting_map_ 中找不到 piece_info
        //        则 返回 false
        if (requesting_map_.find(piece_info) == requesting_map_.end())
            return false;

        // 在 requesting_map_ 中 找到 piece_info 对应的 PieceTask
        // 如果 该 PieceTask->downloader_ 不是当前的 downloader
        //      则 返回 false
        //
        if ((*requesting_map_.find(piece_info)).second->downloader_ != downloader)
            return false;
        else
            requesting_map_.erase(piece_info);
        // 在 requesting_map_ 中 删除 该 piece_info
        // 返回true
        return true;
    }

    bool PieceRequestManager::HasNextPieceForDownload(uint32_t playing_possition, protocol::PieceInfoEx &piece_info_ex, VodDownloader__p downloader)
    {
        if (false == is_running_)
            return false;
        // 调用instance_ -> GetNextPieceForDownload  (instance_->GetNextPieceForDownload())
        //
        if (!download_driver_ || !(download_driver_->GetInstance()))
        {
            LOG4CPLUS_WARN_LOG(logger_piece_request, __FUNCTION__ << ":" << __LINE__ << " download_driver_ = " 
                << download_driver_);
            LOG4CPLUS_WARN_LOG(logger_piece_request, __FUNCTION__ << ":" << __LINE__ << 
                " download_driver_->GetInstance() = " << download_driver_->GetInstance());
            return false;
        }
        LOG4CPLUS_INFO_LOG(logger_piece_request, "PieceRequestManager::HasNextPieceForDownload   playing_possition:" 
            << playing_possition);

        uint32_t block_size = download_driver_->GetInstance()->GetBlockSize();

        uint32_t possition_for_download = playing_possition;

        while (true)
        {
            if (false == download_driver_->GetInstance()->GetNextPieceForDownload(possition_for_download, piece_info_ex))
            {
                LOG4CPLUS_INFO_LOG(logger_piece_request, "PieceRequest Failed" << possition_for_download);
                return false;
            }

            std::map<protocol::PieceInfo, PieceTask::p>::iterator iter = requesting_map_.find(piece_info_ex.GetPieceInfo());
            if (iter == requesting_map_.end())
            {
                // check here
                if (task_range_map_.empty()) {
                    return true;
                }
                if (block_size == 0) {
                    LOG4CPLUS_DEBUG_LOG(logger_piece_request, __FUNCTION__ << ":" << __LINE__ << 
                        " BlockSize = 0, protocol::PieceInfo = " << piece_info_ex << " True");
                    return true;
                }

                uint32_t piece_start = piece_info_ex.GetPosition(block_size);
                uint32_t piece_end = piece_info_ex.GetEndPosition(block_size);
                std::map<uint32_t, uint32_t>::iterator it;
                bool good_piece = false;
                for (it = task_range_map_.begin(); it != task_range_map_.end() && false == good_piece; ++it)
                {
                    uint32_t range_start = it->first;
                    uint32_t range_end = it->second;
                    if (range_end == 0) {
                        if (piece_end > range_start) {
                            good_piece = true;
                        }
                    }
                    else {
                        // overlapped
                        if ((range_start <= piece_start && piece_start <= range_end) ||
                            (range_start <= piece_end && piece_end <= range_end) ||
                            (piece_start <= range_start && range_start <= piece_end) ||
                            (piece_start <= range_end && range_end <= piece_end))
                        {
                            good_piece = true;
                        }
                    }
                }

                if (good_piece) {
                    return true;
                }
            }
            else
            {
                PieceTask::p piece_task = iter->second;
                LOG4CPLUS_INFO_LOG(logger_piece_request, "PieceRequestManager::GetNextPieceForDownload check timeout: " 
                    << iter->first);
                if (piece_task->IsTimeout(downloader, piece_info_ex) || piece_task->downloader_->IsPausing())
                {
                    return true;
                }
                else if (block_size == 0) {
                    LOG4CPLUS_DEBUG_LOG(logger_piece_request, __FUNCTION__ << ":" << __LINE__ << 
                        " BlockSize = 0, protocol::PieceInfo = " << piece_info_ex << " False");
                    return false;
                }
            }

            // move to next piece
            LOG4CPLUS_DEBUG_LOG(logger_piece_request, 
                "PieceRequestManager::GetNextPieceForDownload   PieceIsDownloading :" << piece_info_ex);
            possition_for_download = piece_info_ex.GetEndPosition(block_size);
            LOG4CPLUS_DEBUG_LOG(logger_piece_request, 
                "PieceRequestManager::GetNextPieceForDownload   PieceIsDownloading EndPosition:" << 
                possition_for_download << " blocksize: " << block_size);
        }
        return false;
    }

    bool PieceRequestManager::GetNextPieceForDownload(uint32_t playing_possition, protocol::PieceInfoEx &piece_info_ex, VodDownloader__p downloader)
    {
        if (false == is_running_)
            return false;
        //
        //
        if (!download_driver_ || !(download_driver_->GetInstance()))
        {
            LOG4CPLUS_WARN_LOG(logger_piece_request, __FUNCTION__ << ":" << __LINE__ << 
                " download_driver_ = " << download_driver_);
            LOG4CPLUS_WARN_LOG(logger_piece_request, __FUNCTION__ << ":" << __LINE__ << 
                " download_driver_->GetInstance() = " << download_driver_->GetInstance());
            return false;
        }
        LOG4CPLUS_INFO_LOG(logger_piece_request, "PieceRequestManager::GetNextPieceForDownload   playing_possition:" 
            << playing_possition);

        uint32_t block_size = download_driver_->GetInstance()->GetBlockSize();

        uint32_t possition_for_download = playing_possition;

        while (true)
        {
            if (false == download_driver_->GetInstance()->GetNextPieceForDownload(possition_for_download, piece_info_ex))
            {
                LOG4CPLUS_INFO_LOG(logger_piece_request, "PieceRequest Failed" << possition_for_download);
                return false;
            }

            std::map<protocol::PieceInfo, PieceTask::p>::iterator iter = requesting_map_.find(piece_info_ex.GetPieceInfo());
            if (iter == requesting_map_.end())
            {
                // check here
                if (task_range_map_.empty()) {
                    return true;
                }
                if (block_size == 0) {
                    LOG4CPLUS_DEBUG_LOG(logger_piece_request, __FUNCTION__ << ":" << __LINE__ << 
                        " BlockSize = 0, protocol::PieceInfo = " << piece_info_ex << " True");
                    return true;
                }

                uint32_t piece_start = piece_info_ex.GetPosition(block_size);
                uint32_t piece_end = piece_info_ex.GetEndPosition(block_size);
                std::map<uint32_t, uint32_t>::iterator it;
                bool good_piece = false;
                for (it = task_range_map_.begin(); it != task_range_map_.end() && false == good_piece; ++it)
                {
                    uint32_t range_start = it->first;
                    uint32_t range_end = it->second;
                    if (range_end == 0) {
                        if (piece_end > range_start) {
                            good_piece = true;
                        }
                    }
                    else {
                        // 仅存在唯一的一片piece满足在range_end两端的
                        if (piece_start < range_end && piece_end > range_end)
                        {
                            // 这个时候，piece下载的终点就不应该是sp:127
                            // 重新计算piece下载的终点
                            piece_info_ex.subpiece_index_end_ = (range_end % (SUB_PIECE_SIZE * SUB_PIECE_COUNT_PER_PIECE)) / SUB_PIECE_SIZE;
                        }

                        // overlapped
                        if ((range_start <= piece_start && piece_start <= range_end) ||
                            (range_start <= piece_end && piece_end <= range_end) ||
                            (piece_start <= range_start && range_start <= piece_end) ||
                            (piece_start <= range_end && range_end <= piece_end))
                        {
                            good_piece = true;
                        }
                    }
                }

                // modified by jeffrey 2010/3/22
                // 修复IKAN拖动，头部下完之后的媒体数据部分多下载无效数据的bug
                if (!requesting_map_.empty())
                {
                    // requesting_map_.begin()->first).GetPosition(block_size) < task_range_map_[0] 
                    // 解释如下:
                    // task_range_map_[0] 其实这里表示的意思就是头部的长度
                    // request_map是升序排序，那么排在最前面的必然是所有待下载任务的最前面

                    // 如果 最前面的下载任务的数据起点 < 头部的长度
                    // 即头部还没有下载完成
                    
                    // piece_start > task_range_map_.rbegin()->first
                    // 解释如下:
                    // piece_end 表示 当前分配的下载任务的终点
                    // task_range_map_.rbeing()->first 表示 需要下载的数据区间的起点
                    // 当 piece_end > task_range_map_.rbegin()->first 成立时
                    // 意味着 分配的任务是下载数据任务。

                    // 综上所述：
                    // 这个if条件的语义是:
                    // 如果媒体头部还没有下完，这个时候不应该去分配媒体数据任务的下载。

                    if ((requesting_map_.begin()->first).GetPosition(block_size) < task_range_map_[0] &&
                        piece_end > task_range_map_.rbegin()->first)
                    {
                        good_piece = false;
                    }
                }

                if (good_piece) {
                    return true;
                }
            }
            else
            {
                PieceTask::p piece_task = iter->second;
                LOG4CPLUS_INFO_LOG(logger_piece_request, "PieceRequestManager::GetNextPieceForDownload check timeout: " 
                    << iter->first);
                if (piece_task->IsTimeout(downloader, piece_info_ex) || piece_task->downloader_->IsPausing())
                {
                    if (!downloader->IsPausing())
                        download_driver_->SetDownloaderToDeath(iter->second->downloader_);
                    requesting_map_.erase(iter);
                    if (piece_task->downloader_)
                        piece_task->downloader_->OnPieceTimeout(download_driver_, piece_info_ex);
                    return true;
                }
                else if (block_size == 0) {
                    LOG4CPLUS_DEBUG_LOG(logger_piece_request, __FUNCTION__ << ":" << __LINE__ << 
                        " BlockSize = 0, protocol::PieceInfo = " << piece_info_ex << " False");
                    return false;
                }
            }

            // move to next piece
            LOG4CPLUS_DEBUG_LOG(logger_piece_request, 
                "PieceRequestManager::GetNextPieceForDownload   PieceIsDownloading :" << piece_info_ex);
            possition_for_download = piece_info_ex.GetEndPosition(block_size);
            LOG4CPLUS_DEBUG_LOG(logger_piece_request, 
                "PieceRequestManager::GetNextPieceForDownload   PieceIsDownloading EndPosition:" << 
                possition_for_download << " blocksize: " << block_size);
        }
        return false;
    }

    void PieceRequestManager::NoticePieceTaskTimeOut(const protocol::PieceInfoEx& piece_info_ex, VodDownloader__p downloader)
    {
        if (false == is_running_)
            return;

        std::map<protocol::PieceInfo, PieceTask::p>::iterator iter = requesting_map_.find(piece_info_ex.GetPieceInfo());
        if (iter != requesting_map_.end())
        {
            PieceTask::p piece_task = iter->second;
            if (piece_task->downloader_ == downloader)
            {
                // 起始时间设置成0, 就会Timeout了
                piece_task->start_time_ = framework::timer::TickCounter(0);
            }
        }
    }

    void PieceRequestManager::ClearTaskRangeMap()
    {
        if (false == is_running_) {
            return;
        }
        task_range_map_.clear();
    }

    // make sure the ranges do not overlap
    void PieceRequestManager::AddTaskRange(uint32_t start, uint32_t end)
    {
        if (false == is_running_) {
            return;
        }
        task_range_map_[start] = end;
    }
}
