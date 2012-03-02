//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "ConnectionBase.h"
#include "p2sp/p2p/P2PModule.h"
#include "p2sp/p2p/P2PDownloader.h"

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("p2p");

    bool ConnectionBase::IsRunning() const
    {
        return is_running_;
    }

    const boost::asio::ip::udp::endpoint & ConnectionBase::GetEndpoint()
    {
        return endpoint_;
    }

    statistic::PeerConnectionStatistic::p ConnectionBase::GetStatistic() const
    {
        return statistic_;
    }

    bool ConnectionBase::AddAssignedSubPiece(const protocol::SubPieceInfo & subpiece_info)
    {
        if (is_running_ == false)
        {
            return false;
        }

        task_queue_.push_back(subpiece_info);
        return true;
    }

    uint32_t ConnectionBase::GetTaskQueueSize() const
    {
        if (is_running_ == false)
        {
            return 0;
        }

        return task_queue_.size();
    }

    void ConnectionBase::ClearTaskQueue()
    {
        if (is_running_ == false)
        {
            return;
        }
        task_queue_.clear();
    }

    uint32_t ConnectionBase::GetConnectedTime() const
    {
        return connected_time_.elapsed();
    }

    boost::uint32_t ConnectionBase::GetSentCount() const
    {
        return sent_count_;
    }

    boost::uint32_t ConnectionBase::GetReceivedCount() const
    {
        return received_count_;
    }

    void ConnectionBase::OnSubPiece(uint32_t subpiece_rtt, uint32_t buffer_length)
    {
        if (is_running_ == false)
        {
            return;
        }

        last_live_response_time_.reset();
        assert(statistic_);
        statistic_->SubmitDownloadedBytes(buffer_length);

        recent_avg_delt_times_.Push(last_receive_time_.elapsed());
        avg_delta_time_ = (uint32_t)recent_avg_delt_times_.Average();

        LIMIT_MAX(avg_delta_time_, 1000);
        last_receive_time_.reset();
        if (requesting_count_ > 0)
        {
            requesting_count_--;
        }
        P2PModule::Inst()->RemoveRequestCount();
        received_count_++;

        statistic_->SubmitRTT(subpiece_rtt);

        RequestNextSubpiece();
     }

    void ConnectionBase::RequestNextSubpiece()
    {
        ++accumulative_subpiece_num;
        if (accumulative_subpiece_num < 4)
        {
            return;
        }
        accumulative_subpiece_num = 0;
        RequestTillFullWindow(true);
    }

    void ConnectionBase::OnTimeOut(SubPieceRequestTask__p subpiece_request_task)
    {
        if (is_running_ == false)
        {
            return;
        }

        if (requesting_count_ > 0)
        {
            requesting_count_--;
        }

        P2PModule::Inst()->RemoveRequestCount();

        RequestNextSubpiece();
    }

    uint32_t ConnectionBase::GetWindowSize() const
    {
        if (is_running_ == false)
            return 0;
        return window_size_;
    }

    uint32_t ConnectionBase::GetLongestRtt() const
    {
        if (is_running_ == false)
            return 0;
        return longest_rtt_;
    }

    uint32_t ConnectionBase::GetAvgDeltaTime() const
    {
        if (is_running_ == false)
            return 0;
        return avg_delta_time_;
    }

    const protocol::CandidatePeerInfo & ConnectionBase::GetCandidatePeerInfo() const
    {
        return candidate_peer_info_;
    }


    boost::uint32_t ConnectionBase::GetSortedValueForAssigner()
    {
        if (is_running_ == false)
        {
            return 0;
        }

        return framework::timer::TickCounter::tick_count() + GetUsedTimeForAssigner();
    }

    boost::uint32_t ConnectionBase::GetUsedTimeForAssigner()
    {
        if (is_running_ == false)
        {
            return 0;
        }

        uint32_t t = 0;
        if (window_size_ < requesting_count_)
        {
            t = GetUsedTime();
            if (t > last_receive_time_.elapsed())
            {
                t -= last_receive_time_.elapsed();
            }
        }
        if (statistic_->GetTotalRTTCount() < 3)
        {
            t = 10 + statistic_->GetTotalRTTCount() * 30;
        }
        return t;
    }

    boost::uint32_t ConnectionBase::GetUsedTime()
    {
        if (is_running_ == false)
            return 0;

        uint32_t v = avg_delta_time_;
        if (avg_delt_time_init_ > 0 && v > avg_delt_time_init_)
        {
            v = (boost::uint32_t)((avg_delt_time_init_ * 7 + avg_delta_time_ * 3 + 5) / 10);
        }
        LIMIT_MIN(v, 10);
        return v;
    }

    void ConnectionBase::RequestSubPieces(uint32_t subpiece_count, uint32_t copy_count, bool need_check)
    {
        if (false == is_running_)
        {
            return;
        }

        if (subpiece_count == 0 || task_queue_.empty())
        {
            return;
        }

        if (requesting_count_ == 0)
        {
            last_receive_time_.reset();
        }

        std::vector<protocol::SubPieceInfo> subpieces;
        for (uint32_t i = 0; i < subpiece_count; ++i)
        {
            if (task_queue_.empty())
            {
                break;
            }
            const protocol::SubPieceInfo & sp = task_queue_.front();
            if (!need_check || !p2p_downloader_->HasSubPiece(sp))
            {
                subpieces.push_back(sp.GetSubPieceInfoStruct());
            }
            task_queue_.pop_front();
        }

        if (subpieces.size() == 0)
        {
            return;
        }

        // 检查对方Peer版本，旧版本的只发1个
        if (peer_version_ >= 0x00000101)
        {
            // check copy_count
            LIMIT_MIN_MAX(copy_count, 1, subpieces.size());
        }
        else
        {
            copy_count = 1;
        }

        SendPacket(subpieces, copy_count);

        last_request_time_.reset();

        // 打印日志，这是PPLogMonitor请求包的来源
        if (p2p_downloader_->GetDownloadDrivers().size() != 0)
        {
            for (uint32_t i = 0; i < subpieces.size(); ++i)
            {
                //LOG(__EVENT, "P2P", "PeerConnection::RequestSubPiece " << (*(p2p_downloader_->GetDownloadDrivers().begin()))->GetDownloadDriverID() << " " << 0 << " " << 1 << " " << shared_from_this() << " " << protocol::SubPieceInfo(subpieces[i]));
                //LOG(__EVENT, "P2P", "PeerConnection::SubpieceTimeOut: " << *(p2p_downloader_->GetDownloadDrivers().begin()) << " " << shared_from_this() << " " << protocol::SubPieceInfo(subpieces[i]) << " " << curr_time_out_);
            }
        }

        requesting_count_ += subpieces.size();
        sent_count_ += subpieces.size();
    }

    void ConnectionBase::RecordStatisticInfo()
    {
        statistic_->SetWindowSize(window_size_);
        statistic_->SetSentCount(sent_count_);
        statistic_->SetRequestingCount(requesting_count_);
        statistic_->SetReceivedCount(received_count_);
    }
}