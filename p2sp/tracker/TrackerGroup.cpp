//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/tracker/TrackerGroup.h"
#include "statistic/StatisticModule.h"
#include "p2sp/tracker/TrackerModule.h"

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("tracker");

    void TrackerGroup::Start()
    {
        if (is_running_)
        {
            return;
        }

        is_running_ = true;
        is_responsed_ = true;
        error_times_ = 0;

        // 启动所有的client
        StartAllClients();
        //
        // 创建定时器 timer_, 初始定时时间为 5秒，并且把该定时器启动起来

        OnTimerElapsed(&timer_);
        timer_.start();
    }

    void TrackerGroup::Stop()
    {
        error_times_ = 0;
        // 干掉定时器

        timer_.stop();

        // 停止所有的Client
        ClearAllClients();
        is_running_ = false;
    }

    void TrackerGroup::StartAllClients()
    {
        for (TrackerClientList::iterator it = tracker_list_.begin(),
            eit = tracker_list_.end(); it != eit; it++)
        {
            (*it)->Start();
        }
    }

    void TrackerGroup::ClearAllClients()
    {
        for (TrackerClientList::iterator it = tracker_list_.begin(),
            eit = tracker_list_.end(); it != eit; it++)
        {
            (*it)->Stop();
        }

        trackers_.clear();
        tracker_list_.clear();
        current_tracker_ = TrackerClient::p();
        trying_tracker_iterator_ = tracker_list_.end();
        trying_tracker_ = TrackerClient::p();
    }

    void TrackerGroup::DoList(const RID& rid)
    {
        // 遍历所有的 TrackerClient， 对每个TrackerClient DoList
        for (TrackerClientList::iterator it = tracker_list_.begin(),
            eit = tracker_list_.end(); it != eit; it++)
        {
            (*it)->DoList(rid);
        }
    }

    void TrackerGroup::SetTrackers(boost::uint32_t group_count, const std::set<protocol::TRACKER_INFO>& trackers)
    {
        // 第一步 添加新的Tracker
        // 第二步 删除旧的Tracker
        //   如果将 旧的Tracker 是 当前Tracker
        //   那么将 current_tracker_ 置为空
        // 第三步
        //   重组 tracker_list_

        if (trackers.empty()) 
        {
            ClearAllClients();
            return;
        }

        TrackerClient::p using_tracker;
        if (current_tracker_)
        {
            using_tracker = current_tracker_;
        }
        else if(trying_tracker_)
        {
            using_tracker = trying_tracker_;
        }

        if (using_tracker)
        {
            if (trackers.find(using_tracker->GetTrackerInfo()) != trackers.end())
            {
                for (TrackerClientList::iterator iter = tracker_list_.begin();
                    iter != tracker_list_.end(); ++iter)
                {
                    if ((*iter)->GetTrackerInfo() == using_tracker->GetTrackerInfo())
                    {
                        continue;
                    }
                    (*iter)->Stop();
                }
                tracker_list_.clear();
                trackers_.clear();
            }
            else
            {
                // 本地正在使用的tracker已经不在最新的tracker list中，放弃它
                using_tracker->PPLeave();
                using_tracker.reset();
                ClearAllClients();
            }
        }
        else
        {
            ClearAllClients();
        }

        // 打乱trackers
        std::vector<protocol::TRACKER_INFO> random_shuffled_trackers(trackers.begin(), trackers.end());
        std::random_shuffle(random_shuffled_trackers.begin(), random_shuffled_trackers.end());

        for (std::vector<protocol::TRACKER_INFO>::iterator it = random_shuffled_trackers.begin();
            it != random_shuffled_trackers.end(); it++)
        {
            protocol::TRACKER_INFO& info = *it;
            boost::asio::ip::udp::endpoint end_point = framework::network::Endpoint(info.IP, info.Port);

            if (using_tracker && info == using_tracker->GetTrackerInfo())
            {
                using_tracker->SetGroupCount(group_count);

                tracker_list_.push_back(using_tracker);
                trying_tracker_iterator_ = --tracker_list_.end();

                trackers_[end_point] = using_tracker;
            }
            else
            {
                assert (info.Type);  // udp
                TrackerClient::p tracker_client = TrackerClient::Create(end_point, is_vod_);
                tracker_client->SetTrackerInfo(info);
                tracker_client->SetGroupCount(group_count);

                tracker_list_.push_back(tracker_client);
                trackers_[end_point] = tracker_client;

                if (is_running_)
                {
                    tracker_client->Start();
                }
            }
        }

        if (is_running_)
        {
            // 当前没有正在使用的tracker，重启定时器并且立即执行一次SelectTracker
            if (!using_tracker)
            {
                timer_.stop();
                timer_.interval(DEFAULT_INTERVAL_IN_SECONDS_ *1000);
                timer_.start();
                SelectTracker();
            }
        }
    }

    const std::vector<protocol::TRACKER_INFO> TrackerGroup::GetTrackers()
    {
        std::vector<protocol::TRACKER_INFO> ret_tracker_infos;
        for (TrackerClientList::iterator it = tracker_list_.begin();
            it != tracker_list_.end(); ++it)
        {
            ret_tracker_infos.push_back((*it)->GetTrackerInfo());
        }
        return ret_tracker_infos;
    }

    void TrackerGroup::OnTimerElapsed(framework::timer::Timer * pointer)
    {
        TRACK_INFO("TrackerGroup::OnTimerElapsed times = " << pointer->times());
        uint32_t times = pointer->times();
         // 确认是 Commit/KeepAlive 定时器
        if (pointer != &timer_)
        {
            assert(0);
            return;
        }

        if (tracker_list_.empty())
        {
            TRACK_WARN("Tracker List is empty.");
            return;
        }

        SelectTracker();
    }

    void TrackerGroup::SelectTracker()
    {
        // 如果 is_responsed_ == false 上次发的Commit/KeepAlive 没有收到回包或者回包有问题
        //     error_time ++
        //   如果 error_times >= 3 那么就
        //        current_tacker_ 赋值为0
        //        timer_->SetInterval( 10秒);  定时器时间设短
        if (!is_responsed_)
        {
            error_times_++;
            LOG(__DEBUG, "tracker", "No Response, Times: " << error_times_);
            if (error_times_ >= 3)
            {
                error_times_ = 0;
                // 统计信息
                if (current_tracker_)
                {
                    statistic::StatisticModule::Inst()->SetIsSubmitTracker(current_tracker_->GetTrackerInfo(), false);
                }
                // 设为空
                current_tracker_ = TrackerClient::p();
                timer_.interval(DEFAULT_INTERVAL_IN_SECONDS_ *1000);
                LOG(__DEBUG, "tracker", "No Response, Set Interval to default: ");
            }
        }

        // 如果 current_tacker_ 存在
        //   current_tacker_->DoSubmit()     里面会根据资源情况决定是KeepAlive还是Commit
        //   DoSubmit 后 记下 TransactionID 的值，is_responsed_ = false
        // 否则
        //   如果 trying_tracker 为空， trying_tracker 就为 tracker_list_ 的第一个。
        //   否则 为 tracker_list_ 的下一个
        //   last_response_rid_count = 0;
        //   然后 DoSubmit()  后 记下 TransactionID 的值，is_responsed_ = false
        if (current_tracker_)
        {
            is_responsed_ = false;
            last_transcation_id_ = current_tracker_->DoSubmit();
        }
        else if (!tracker_list_.empty())  // tracker_list could not be empty
        {
            if (trying_tracker_iterator_ != tracker_list_.end())
            {
                (*trying_tracker_iterator_)->PPLeave();
                ++trying_tracker_iterator_;
            }
            
            if (trying_tracker_iterator_ == tracker_list_.end())
            {
                trying_tracker_iterator_ = tracker_list_.begin();
            }
            
            trying_tracker_ = *trying_tracker_iterator_;
            is_responsed_ = false;
            trying_tracker_->SetRidCount(0);  // ! trick，使Submit时清空同步资源，并进行全新Report
            last_transcation_id_ = trying_tracker_->DoSubmit();
        }
    }

    void TrackerGroup::DoCommit()
    {
        LOG(__INFO, "tracker", "TrackerGroup::DoCommit:");
        if (current_tracker_)
        {
            last_transcation_id_ = current_tracker_->DoSubmit();
        }
    }

    void TrackerGroup::OnCommitResponsePacket(protocol::CommitPacket const & packet)
    {
        LOG(__DEBUG, "tracker", "TrackerGroup::OnCommitPacket from: ");
        boost::asio::ip::udp::endpoint end_point = packet.end_point;
        // 在 trackers_ 中根据 end_point 找到对应的 TrackerClient, 如果找不到，直接返回
        // 该 TrackerClient->OnCommitResponsePacket(packet);
        if (trackers_.count(end_point) == 0)
        {
            LOG(__DEBUG, "tracker", "No such end_point: " << end_point);
            return;
        }
        trackers_[end_point]->OnCommitResponsePacket(packet);

        // 如果 packet::GetTranscationID() != trans_id_
        //      直接返回
        if (packet.transaction_id_ != last_transcation_id_)
        {
            LOG(__DEBUG, "tracker", "TrasactionID is " << packet.transaction_id_ << ", Expect: " << last_transcation_id_);
            return;
        }

        // 如果 成功 packet->ErrorCode() == 0
        //        error_time = 0    is_responsed_ = true;
        //        timer_->SetInterval( packet->GetInterval());        定时器时间设长 （设为Tracker的返回值）
        //        如果 current_tracker_ 为空
        //            那么 current_tracker_ = 当前end_point对应 的 TrackerClient
        //        current_tracker->Set_Rid_Count(packet->GetRidCount());
        // 如果 失败 packet->ErrorCode() != 0
        //      直接返回

        if (packet.error_code_ == 0)  // success
        {
            error_times_ = 0;
            is_responsed_ = true;
            LOG(__DEBUG, "tracker", "TrackerGroup::OnCommitPacket  Interval:" << packet.response.keep_alive_interval_);
            timer_.interval(packet.response.keep_alive_interval_ *1000);

            if (!current_tracker_)
            {
                current_tracker_ = trackers_[end_point];
                statistic::StatisticModule::Inst()->SetIsSubmitTracker(current_tracker_->GetTrackerInfo(), true);
            }

            // TODO commit's response does not contain resource count
        }
        else
        {
            LOG(__DEBUG, "tracker", "Error Code is: " << packet.error_code_);
        }
    }

    void TrackerGroup::OnReportResponsePacket(protocol::ReportPacket const & packet)
    {
        boost::asio::ip::udp::endpoint  end_point = packet.end_point;
        TRACK_DEBUG("TrackerGroup::OnReportPacket from: " << end_point);

        // 在 trackers_ 中根据 end_point 找到对应的 TrackerClient, 如果找不到，直接返回
        // 该 TrackerClient->OnCommitResponsePacket(packet);
        if (trackers_.count(end_point) == 0)
        {
            LOG(__DEBUG, "tracker", "No such end_point: " << end_point);
            return;
        }
        trackers_[end_point]->OnReportResponsePacket(packet);

        // 如果 packet::GetTranscationID() != trans_id_
        //      直接返回
        if (packet.transaction_id_ != last_transcation_id_)
        {
            LOG(__DEBUG, "tracker", "TrasactionID is " << packet.transaction_id_ << ", Expect: " << last_transcation_id_);
            return;
        }

        // 如果 成功 packet->ErrorCode() == 0
        //        error_time = 0    is_responsed_ = true;
        //        timer_->SetInterval( packet->GetInterval());        定时器时间设长 （设为Tracker的返回值）
        //        如果 current_tracker_ 为空
        //            那么 current_tracker_ = 当前end_point对应 的 TrackerClient
        //        current_tracker->Set_Rid_Count(packet->GetRidCount());
        // 如果 失败 packet->ErrorCode() != 0
        //      直接返回

        if (packet.error_code_ == 0)  // success
        {
            error_times_ = 0;
            is_responsed_ = true;
            if (trackers_[end_point]->IsSync()) {
                LOG(__DEBUG, "tracker", "TrackerGroup::OnReportResponsePacket  Interval:" << packet.response.keep_alive_interval_ << " ResourceCount: " << (uint32_t) packet.response.resource_count_);
                timer_.interval(packet.response.keep_alive_interval_*1000);
            }
            else {
                // do not set timer, use default
                LOGX(__DEBUG, "upload", "Tracker Not Synchronized. Do Not Use TrackerTime");
            }

            if (!current_tracker_)
            {
                current_tracker_ = trackers_[end_point];

                statistic::StatisticModule::Inst()->SetIsSubmitTracker(current_tracker_->GetTrackerInfo(), true);
            }
        }
        else
        {
            LOG(__DEBUG, "tracker", "Error Code is: " << packet.error_code_);
        }
    }

    void TrackerGroup::OnKeepAliveResponsePacket(protocol::KeepAlivePacket const & packet)
    {
        boost::asio::ip::udp::endpoint end_point = packet.end_point;
        LOG(__DEBUG, "tracker", "TrackerGroup::OnKeepAlivePacket from: " << end_point);

        // 在 trackers_ 中根据 end_point 找到对应的 TrackerClient, 如果找不到，直接返回
        // 该 TrackerClient->OnKeepAliveResponsePacket(packet);
        if (trackers_.count(end_point) == 0)
        {
            LOG(__DEBUG, "tracker", "No such end_point: " << end_point);
            return;
        }
        trackers_[end_point]->OnKeepAliveResponsePacket(packet);

        // 如果 packet::GetTranscationID() != trans_id_
        //      直接返回
        if (packet.transaction_id_ != last_transcation_id_)
        {
            LOG(__DEBUG, "tracker", "TrasactionID is " << packet.transaction_id_ << ", Expect: " << last_transcation_id_);
            return;
        }

        // 如果 成功 packet->ErrorCode() == 0
        //        error_time = 0    is_responsed_ = true;
        //        timer_->SetInterval( packet->GetInterval());        定时器时间设长 （设为Tracker的返回值）
        //        如果 current_tracker_ 为空
        //            那么 current_tracker_  =  当前end_point对应 的 TrackerClient
        //        current_tracker->Set_Rid_Count(packet->GetRidCount());
        // 如果 失败 packet->ErrorCode() != 0
        //      直接返回

        if (packet.error_code_ == 0)  // success
        {
            error_times_ = 0;
            is_responsed_ = true;
            LOG(__DEBUG, "tracker", "TrackerGroup::OnKeepAlivePacket  Interval:" << packet.response.keep_alive_interval_ << " ResourceCount: " << (uint32_t) packet.response.resource_count_);
            timer_.interval(1000* packet.response.keep_alive_interval_);

            if (!current_tracker_)
            {
                current_tracker_ = trackers_[end_point];
                // 统计信息
                statistic::StatisticModule::Inst()->SetIsSubmitTracker(current_tracker_->GetTrackerInfo(), true);
            }
            current_tracker_->SetRidCount(packet.response.resource_count_);
        }
        else
        {
            // 统计信息
            statistic::StatisticModule::Inst()->SubmitErrorCode(current_tracker_->GetTrackerInfo(), packet.error_code_);
            LOG(__DEBUG, "tracker", "Error Code is: " << packet.error_code_);
        }
    }

    void TrackerGroup::OnListResponsePacket(protocol::ListPacket const & packet)
    {
        boost::asio::ip::udp::endpoint end_point = packet.end_point;
        // 在 trackers_ 中根据 end_point 找到对应的 TrackerClient, 如果找不到，直接返回
        // 该 TrackerClient->OnListResponsePacket(packet);
        if (trackers_.count(end_point) != 0)
        {
            trackers_[end_point]->OnListResponsePacket(packet);
        }
    }

    void TrackerGroup::PPLeave()
    {
        if (current_tracker_)
        {
            current_tracker_->PPLeave();
        }
    }

}
