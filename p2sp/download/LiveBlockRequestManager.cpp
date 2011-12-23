#include "Common.h"
#include "LiveDownloadDriver.h"
#include "LiveBlockRequestManager.h"

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("live_block_request_manager");

    LiveBlockTask::LiveBlockTask(const protocol::LiveSubPieceInfo & live_block, LiveDownloader__p & download, boost::uint32_t & timeout)
        : live_block_(live_block)
        , download_(download)
        , timeout_(timeout)
    {
        tick_count_.reset();
    }

    bool LiveBlockTask::IsTimeout(const LiveDownloader__p & request_download)
    {
        return (download_->IsP2PDownloader() != request_download->IsP2PDownloader() && 
            (tick_count_.elapsed() > timeout_ || download_->IsPausing()));
    }

    void LiveBlockTask::Timeout()
    {
        download_->OnBlockTimeout(live_block_.GetBlockId());
    }

    void LiveBlockRequestManager::Start(LiveDownloadDriver::p live_download_driver)
    {
        live_download_driver_ = live_download_driver;
    }

    void LiveBlockRequestManager::Stop()
    {
        live_block_requesting_map_.clear();
        live_download_driver_.reset();
    }

    bool LiveBlockRequestManager::GetNextBlockForDownload(boost::uint32_t start_block_id, LiveDownloader__p download)
    {
        while(1)
        {
            // 申请一片Block去下载
            protocol::LiveSubPieceInfo live_block;
            assert(live_download_driver_);
            live_download_driver_->GetInstance()->GetNextIncompleteBlock(start_block_id, live_block);

#ifdef USE_MEMORY_POOL
            // TODO: 如果使用内存池增加限制分配的逻辑，防止因为内存不足，卡住不播
            if (protocol::LiveSubPieceContent::get_left_capacity() < 2048 &&
                live_block.GetBlockId() > live_download_driver_->GetPlayingPosition().GetBlockId())
            {
                return false;
            }
#endif

            if (!IsRequesting(live_block.GetBlockId()))
            {
                // Block不在请求
                // 可以下载，加入
                LOG(__DEBUG, "live_block_request_manager", "Not Requesting, Add id = " << live_block.GetBlockId());
                AddBlockTask(live_block, download);
                return true;
            }
            else
            {
                // Block正在请求
                if (IsTimeout(live_block.GetBlockId(), download))
                {
                    // 超时了
                    LOG(__DEBUG, "live_block_request_manager", "Requesting & timeout Add id = " << live_block.GetBlockId());

                    // 再删除任务记录, 同时删除相应的downloader的block_task_
                    RemoveBlockTask(live_block.GetBlockId());

                    // 再加入
                    AddBlockTask(live_block, download);
                    return true;
                }
                else
                {
                    // 还没有超时
                    // 不能下载，寻找下一片
                    start_block_id += live_download_driver_->GetInstance()->GetLiveInterval();
                }
            }
        }

        return false;
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
        LOG(__DEBUG, "live_block_request_manager", "Remove block = " << block_id);
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