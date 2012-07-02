#include "Common.h"
#include "LiveDownloadDriver.h"
#include "LiveBlockRequestManager.h"

namespace p2sp
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_live_block_request_manager = log4cplus::Logger::
        getInstance("[live_block_request_manager]");
#endif

    LiveBlockTask::LiveBlockTask(const protocol::LiveSubPieceInfo & live_block, LiveDownloader__p & download, boost::uint32_t & timeout)
        : live_block_(live_block)
        , download_(download)
        , timeout_(timeout)
    {
        tick_count_.reset();
    }

    bool LiveBlockTask::IsTimeout(const LiveDownloader__p & request_download)
    {
        return ((download_ != request_download) && 
            (tick_count_.elapsed() > timeout_ || download_->IsPausing()));
    }

    void LiveBlockTask::Timeout()
    {
        download_->OnBlockTimeout(live_block_.GetBlockId());
    }

    void LiveBlockRequestManager::AddBlockTask(const protocol::LiveSubPieceInfo & live_block, LiveDownloader__p & download)
    {
        // 假设8秒超时
        boost::uint32_t timeout = 8*1000;

        // 增加记录
        LiveBlockTask::p block_task = LiveBlockTask::Create(live_block, download, timeout);
        live_block_requesting_map_[live_block.GetBlockId()] = block_task;

        // 往相应的downloader中增加
        download->PutBlockTask(live_block);
    }

    void LiveBlockRequestManager::RemoveBlockTask(boost::uint32_t block_id)
    {
        LOG4CPLUS_DEBUG_LOG(logger_live_block_request_manager, "Remove block = " << block_id);
        std::map<boost::uint32_t, LiveBlockTask__p>::iterator iter = live_block_requesting_map_.find(block_id);

        if (iter != live_block_requesting_map_.end())
        {
            // 删除downloader的block_task
            iter->second->Timeout();

            // 删除记录
            live_block_requesting_map_.erase(iter);
            return;
        }
    }

    // 判断标识为block_id的任务是否正在下载
    bool LiveBlockRequestManager::IsRequesting(const boost::uint32_t block_id)
    {
        std::map<boost::uint32_t, LiveBlockTask__p>::iterator iter = live_block_requesting_map_.find(block_id);
        if (iter != live_block_requesting_map_.end())
        {
            return true;
        }

        return false;
    }

    // 判断标识为block_id的任务是否超时
    bool LiveBlockRequestManager::IsTimeout(const boost::uint32_t block_id, const LiveDownloader__p & request_download)
    {
        std::map<boost::uint32_t, LiveBlockTask__p>::iterator iter = live_block_requesting_map_.find(block_id);
        if (iter != live_block_requesting_map_.end())
        {
            return iter->second->IsTimeout(request_download);
        }

        return false;
    }
}