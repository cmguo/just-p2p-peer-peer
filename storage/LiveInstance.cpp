//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "storage_base.h"
#include "IStorage.h"
#include "LiveInstance.h"
#include "LivePosition.h"
#include <p2sp/download/LiveDownloadDriver.h>
#include "LiveAnnounceMapBuilder.h"

using namespace base;
using namespace p2sp;

namespace storage
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("live_storage");

    LiveInstance::LiveInstance(const RID& rid_info, boost::uint16_t live_interval)
        : cache_manager_(live_interval, rid_info)
        , play_timer_(global_second_timer(), 1000, boost::bind(&LiveInstance::OnTimerElapsed, this, &play_timer_))
        , stop_timer_(global_second_timer(), ExpectedP2PCoverageInSeconds * 1000, boost::bind(&LiveInstance::OnTimerElapsed, this, &stop_timer_))
    {
    }

    void LiveInstance::Start(boost::shared_ptr<ILiveInstanceStoppedListener> stopped_event_listener)
    {
        stopped_event_listener_ = stopped_event_listener;
        play_timer_.start();
    }

    void LiveInstance::Stop()
    {
        play_timer_.stop();

        if (stopped_event_listener_)
        {
            stopped_event_listener_->OnLiveInstanceStopped(shared_from_this());
            //通知stopped_event_listener的目的在于通知它把自己拿掉，也就是说此时的LiveInstance已经被析构，
            //不应该再去尝试主动reset stopped_event_listener_
        }
    }

    void LiveInstance::GetNextIncompleteBlock(
        uint32_t start_piece_id, protocol::LiveSubPieceInfo & next_incomplete_block) const
    {
        STORAGE_DEBUG_LOG("start_piece_id:" << start_piece_id);
        cache_manager_.GetNextMissingSubPiece(start_piece_id, next_incomplete_block);

        STORAGE_DEBUG_LOG("need piece " << next_incomplete_block);
    }

    void LiveInstance::AddSubPiece(const protocol::LiveSubPieceInfo & subpiece, const protocol::LiveSubPieceBuffer & buff)
    {
        if (!cache_manager_.AddSubPiece(subpiece, buff))
        {
            STORAGE_DEBUG_LOG("Add failed. subpiece " << subpiece << " is exist.");
            return;
        }

        STORAGE_DEBUG_LOG("a new subpiece is added for block " << subpiece.GetBlockId() << ", will PushDataToDownloaderDriver.");

        PushDataToDownloaderDrivers();
    }

    void LiveInstance::PushDataToDownloaderDrivers()
    {
        // PushDataToDownloaderDriver可能导致码流切换
        // 会把当前的DownloadDriver从download_drivers_删除
        for (std::set<ILiveDownloadDriverPointer>::iterator iter = download_drivers_.begin(); 
             iter != download_drivers_.end(); )
        {
            std::set<ILiveDownloadDriverPointer>::iterator dd_iter = iter++;
            PushDataToDownloaderDriver(*dd_iter);
        }
    }

    void LiveInstance::BuildAnnounceMap(uint32_t request_block_id, protocol::LiveAnnounceMap& announce_map)
    {
        LiveAnnouceMapBuilder builder(cache_manager_, announce_map);
        builder.Build(request_block_id);
    }

    void LiveInstance::PushDataToDownloaderDriver(ILiveDownloadDriverPointer download_driver)
    {
        LivePosition& current_play_position = download_driver->GetPlayingPosition();

        if (cache_manager_.HasSubPiece(current_play_position))
        {
            //至少当前位置所对应那个subpiece已经存在

            if (cache_manager_.HasCompleteBlock(current_play_position.GetBlockId()))
            {
                //send range [subpiece_index, last_subpiece_of_block]
                uint32_t subpieces_count = cache_manager_.GetSubPiecesCount(current_play_position.GetBlockId());
                if (false == SendSubPieces(download_driver, static_cast<uint16_t>(subpieces_count - 1)))
                {
                    return;
                }

                assert(cache_manager_.GetLiveInterval()>0);
                uint32_t next_block_id = current_play_position.GetBlockId() + cache_manager_.GetLiveInterval();
                current_play_position.SetBlockId(next_block_id);
            }
            else
            {
                //i.e. 当前block还有空缺
                protocol::LiveSubPieceInfo next_missing_subpiece;
                cache_manager_.GetNextMissingSubPiece(current_play_position.GetBlockId(), next_missing_subpiece);

                if (next_missing_subpiece.GetBlockId() == current_play_position.GetBlockId() &&
                    next_missing_subpiece.GetSubPieceIndex() > current_play_position.GetSubPieceIndex())
                {
                    uint16_t containing_piece_of_missing_subpiece = LiveBlockNode::GetPieceIndex(next_missing_subpiece.GetSubPieceIndex());

                    uint32_t last_subpiece_to_send = LiveBlockNode::GetFirstSubPieceIndex(containing_piece_of_missing_subpiece) - 1;
                    if (last_subpiece_to_send >= current_play_position.GetSubPieceIndex())
                    {
                        //这里不必再校验数据，因为每个piece变成完整那一刻都会触发validation，而如果validation不过，数据已被丢掉
                        //send range [subpiece_index, last_subpiece_to_send]
                        if (SendSubPieces(download_driver, last_subpiece_to_send))
                        {
                            current_play_position.AdvanceSubPieceIndexTo(last_subpiece_to_send + 1);
                        }
                    }
                }
                else
                {
                    //既然当前block未下载完整，一定有空缺的subpiece
                    //而且第一个空缺的subpiece应该出现在current_play_position之后
                    assert(false);
                }

                //新的position对应的subpiece已经知道是空缺着的，不必尝试了
            }
        }
    }

    bool LiveInstance::SendSubPieces(ILiveDownloadDriverPointer download_driver, boost::uint16_t last_subpiece_index)
    {
        const LivePosition current_play_position = download_driver->GetPlayingPosition();
        
        if (current_play_position.GetSubPieceIndex() == 0 && last_subpiece_index == 0)
        {
            return true;
        }

        assert(current_play_position.GetSubPieceIndex() <= last_subpiece_index);

        bool is_first_block = current_play_position.GetBlockId() == download_driver->GetStartPosition().GetBlockId();

        uint16_t start_subpiece_index = current_play_position.GetSubPieceIndex();
        //block的第一个subpiece不推送
        if (start_subpiece_index == 0)
        {
            start_subpiece_index = 1;
        }

        std::vector<protocol::LiveSubPieceBuffer> subpiece_buffers;
        cache_manager_.GetSubPieces(current_play_position.GetBlockId(), start_subpiece_index, last_subpiece_index, subpiece_buffers);
        
        assert(subpiece_buffers.size() > 0);

        uint32_t total_subpieces_count = cache_manager_.GetSubPiecesCount(current_play_position.GetBlockId());
        assert(total_subpieces_count > 0);
        uint32_t subpieces_downloaded = current_play_position.GetSubPieceIndex() + subpiece_buffers.size();
        assert(subpieces_downloaded <= total_subpieces_count);
        uint8_t block_progress_percentage =  static_cast<uint8_t>(subpieces_downloaded * 100 / total_subpieces_count);

        //除了第一个block，其他block的subpiece[1]的前面的FLV头应该去除
        if (false == is_first_block && 
            start_subpiece_index == 1)
        {
            RemoveFlashHeader(subpiece_buffers);
        }

        return download_driver->OnRecvLivePiece(current_play_position.GetBlockId(), subpiece_buffers, block_progress_percentage);
    }

    void LiveInstance::RemoveFlashHeader(std::vector<protocol::LiveSubPieceBuffer> & subpiece_buffers)
    {
        const size_t FlvHeaderSizeInBytes = 13;

        assert(subpiece_buffers.size() > 0);

        //注意: 此时的subpiece_buffer[0]其实是block的subpieces_[1]
        assert(subpiece_buffers[0].Length() > FlvHeaderSizeInBytes);

        if (subpiece_buffers[0].Length() > FlvHeaderSizeInBytes)
        {
            protocol::LiveSubPieceBuffer first_subpiece(
                new protocol::LiveSubPieceContent(), 
                subpiece_buffers[0].Length() - FlvHeaderSizeInBytes);
            memcpy(first_subpiece.Data(), subpiece_buffers[0].Data() + FlvHeaderSizeInBytes, first_subpiece.Length());
            subpiece_buffers[0] = first_subpiece;
        }
    }

    void LiveInstance::AttachDownloadDriver(ILiveDownloadDriverPointer download_driver)
    {
        STORAGE_DEBUG_LOG("AttachDownloadDriver " << download_driver);

        download_drivers_.insert(download_driver);

        stop_timer_.stop();
    }

    void LiveInstance::DetachDownloadDriver(ILiveDownloadDriverPointer download_driver)
    {
        STORAGE_DEBUG_LOG("DeAttachDownloadDirver " << download_driver);

        std::set<ILiveDownloadDriverPointer>::iterator iter = download_drivers_.find(download_driver);
        if (iter == download_drivers_.end())
        {
            assert(false);
            return;
        }

        download_drivers_.erase(iter);

        if (download_drivers_.size() == 0)
        {
            stop_timer_.start();
        }
    }

    void LiveInstance::OnTimerElapsed(framework::timer::Timer * timer)
    {
        if (timer == &play_timer_)
        {
            // 数据推送
            PushDataToDownloaderDrivers();
            return;
        }

        if (timer == &stop_timer_)
        {
            Stop();
        }
    }

    void LiveInstance::GetAllPlayPoints(std::vector<LivePosition>& play_points) const
    {
        play_points.clear();
        for(std::set<p2sp::ILiveDownloadDriverPointer>::const_iterator iter = download_drivers_.begin();
            iter != download_drivers_.end();
            ++iter)
        {
            play_points.push_back((*iter)->GetPlayingPosition());
        }
    }

    boost::uint32_t LiveInstance::GetMissingSubPieceCount(boost::uint32_t block_id) const
    {
        return cache_manager_.GetMissingSubPieceCount(block_id);
    }

    boost::uint32_t LiveInstance::GetExistSubPieceCount(boost::uint32_t block_id) const
    {
        return cache_manager_.GetExistSubPieceCount(block_id);
    }
}
