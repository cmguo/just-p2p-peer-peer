//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "TrackerStation.h"
#include "TrackerClient.h"

namespace p2sp
{
    void TrackerStation::Start()
    {
        if (is_running_)
        {
            return;
        }

        StartAllClient();

        if (tracker_type_ == p2sp::REPORT)
        {
            report_timer_.start();
        }

        is_running_ = true;
    }

    void TrackerStation::Stop()
    {
        if (!is_running_)
        {
            return;
        }

        ClearAllClient();

        report_timer_.stop();
        is_running_ = false;
    }

    void TrackerStation::StartAllClient()
    {
        for (std::map<boost::asio::ip::udp::endpoint, boost::shared_ptr<TrackerClient> >::iterator
            iter = tracker_client_map_.begin(); iter != tracker_client_map_.end(); ++iter)
        {
            iter->second->Start();
        }
    }

    void TrackerStation::StopAllClient()
    {
        for (std::map<boost::asio::ip::udp::endpoint, boost::shared_ptr<TrackerClient> >::iterator
            iter = tracker_client_map_.begin(); iter != tracker_client_map_.end(); ++iter)
        {
            iter->second->Stop();
        }
    }

    void TrackerStation::ClearAllClient()
    {
        StopAllClient();
        tracker_client_map_.clear();
        tracker_client_list_.clear();
    }

    std::vector<protocol::TRACKER_INFO> TrackerStation::GetTrackers()
    {
        std::vector<protocol::TRACKER_INFO> ret_tracker_infos;

        for (std::map<boost::asio::ip::udp::endpoint, boost::shared_ptr<TrackerClient> >::iterator
            iter = tracker_client_map_.begin(); iter != tracker_client_map_.end(); ++iter)
        {
            ret_tracker_infos.push_back(iter->second->GetTrackerInfo());
        }

        return ret_tracker_infos;
    }

    void TrackerStation::SetTrackers(boost::uint32_t gropup_count,
        const std::set<protocol::TRACKER_INFO> & trackers)
    {
        // 首先检查当前正在使用的Tracker是否在最新列表
        TrackerClient::p using_tracker;
        if (current_report_tracker_)
        {
            using_tracker = current_report_tracker_;
        }
        else if(trying_report_tracker_)
        {
            using_tracker = trying_report_tracker_;
        }

        if (using_tracker)
        {
            if (trackers.find(using_tracker->GetTrackerInfo()) != trackers.end())
            {
                // 当前正在使用的Tracker在最新的列表里
                for (std::map<boost::asio::ip::udp::endpoint, boost::shared_ptr<TrackerClient> >::iterator
                    iter = tracker_client_map_.begin(); iter != tracker_client_map_.end(); ++iter)
                {
                    if (iter->second->GetTrackerInfo() == using_tracker->GetTrackerInfo())
                    {
                        continue;
                    }
                    iter->second->Stop();
                }

                // 这个clear，是因为当前正在使用的tracker的share_ptr被current_report_tracker_
                // 或者trying_report_tracker_里面有，不会析构
                tracker_client_map_.clear();
                tracker_client_list_.clear();
            }
            else
            {
                // 当前正在使用的Tracker不在列表了
                using_tracker->PPLeave();
                using_tracker.reset();
                ClearAllClient();
            }
        }
        else
        {
            ClearAllClient();
        }

        std::vector<boost::shared_ptr<TrackerClient> > tracker_client_vector;

        for (std::set<protocol::TRACKER_INFO>::const_iterator iter = trackers.begin();
            iter != trackers.end(); ++iter)
        {
            boost::asio::ip::udp::endpoint end_point = framework::network::Endpoint(
                iter->IP, iter->Port);

            boost::shared_ptr<TrackerClient> tracker_client = TrackerClient::Create(
                end_point, is_vod_, *iter);

            tracker_client->SetGroupCount(gropup_count);
            tracker_client->Start();

            tracker_client_map_.insert(std::make_pair(end_point, tracker_client));

            tracker_client_vector.push_back(tracker_client);
        }

        std::random_shuffle(tracker_client_vector.begin(), tracker_client_vector.end());

        for (std::vector<boost::shared_ptr<TrackerClient> >::iterator 
            iter = tracker_client_vector.begin(); 
            iter != tracker_client_vector.end(); ++iter)
        {
            tracker_client_list_.push_back(*iter);
        }

        trying_tracker_iterator_ = tracker_client_list_.end();
    }

    void TrackerStation::DoList(const RID& rid, bool list_for_live_udpserver)
    {
        std::map<RID,boost::uint32_t>::iterator it = list_get_response_.find(rid);
        if (is_vod_ && it != list_get_response_.end() && (time(NULL) -it->second < 30))
        {
             //如果有回复，超过30秒才可以重查
            return;
        }
        boost::uint32_t x = Random::GetGlobal().Next(tracker_client_map_.size());

        assert(x<=tracker_client_map_.size());

        std::map<boost::asio::ip::udp::endpoint, boost::shared_ptr<TrackerClient> >::iterator
            iter = tracker_client_map_.begin();

        std::advance(iter, x);

        iter->second->DoList(rid, list_for_live_udpserver);
    }

    void TrackerStation::DoReport()
    {
        assert(tracker_type_ == p2sp::REPORT);
        // 本地没有资源的情况下，不进行report
        // 对点播和直播都有效
        if (tracker_client_map_.empty())
        {
            return;
        }

        std::set<RID> local_resource = (*tracker_client_list_.begin())->GetClientResource();
        if (local_resource.empty())
        {
            return;
        }

        if (!is_report_response_)
        {
            no_report_response_times_++;

            if (no_report_response_times_ >= 3)
            {
                no_report_response_times_ = 0;

                report_failed_times_++;

                if (current_report_tracker_)
                {
                    statistic::StatisticModule::Inst()->SetIsSubmitTracker(current_report_tracker_->GetTrackerInfo(), false);
                }

                current_report_tracker_ = TrackerClient::p();

                boost::uint32_t interval_in_second = DEFAULT_INTERVAL_IN_SECONDS_;

                if (report_failed_times_ > 3)
                {
                    interval_in_second += (report_failed_times_ - 3) * 10;
                    if (interval_in_second > 300)
                    {
                        interval_in_second = 300;
                    }
                }

                report_timer_.interval(interval_in_second *1000);
            }
        }

        if (current_report_tracker_)
        {
            is_report_response_ = false;
            last_report_transaction_id_ = current_report_tracker_->DoSubmit();
        }
        else
        {
            assert(!tracker_client_map_.empty());

            if (trying_tracker_iterator_ != tracker_client_list_.end())
            {
                (*trying_tracker_iterator_)->PPLeave();
                ++trying_tracker_iterator_;
            }

            if (trying_tracker_iterator_ == tracker_client_list_.end())
            {
                trying_tracker_iterator_ = tracker_client_list_.begin();
            }

            trying_report_tracker_ = *trying_tracker_iterator_;
            is_report_response_ = false;

            // 使Submit时清空同步资源，并进行全新Report
            trying_report_tracker_->SetRidCount(0);
            last_report_transaction_id_ = trying_report_tracker_->DoSubmit();
        }
    }

    void TrackerStation::DoLeave()
    {
        if (current_report_tracker_)
        {
            current_report_tracker_->PPLeave();
        }
    }

    void TrackerStation::OnListResponsePacket(const protocol::ListPacket & packet)
    {
        std::map<boost::asio::ip::udp::endpoint, boost::shared_ptr<TrackerClient> >::iterator
            iter = tracker_client_map_.find(packet.end_point);

        if (iter != tracker_client_map_.end())
        {
            iter->second->OnListResponsePacket(packet);
            if (is_vod_)
            {               
                list_get_response_[packet.response.resource_id_] = time(NULL);
            }
        }
    }

    void TrackerStation::OnReportResponsePacket(const protocol::ReportPacket & packet)
    {
        std::map<boost::asio::ip::udp::endpoint, boost::shared_ptr<TrackerClient> >::iterator
            iter = tracker_client_map_.find(packet.end_point);

        if (iter != tracker_client_map_.end())
        {
            iter->second->OnReportResponsePacket(packet);
        }
        else
        {
            return;
        }

        if (packet.transaction_id_ != last_report_transaction_id_)
        {
            return;
        }

        if (packet.error_code_ == 0)
        {
            report_failed_times_ = 0;
            no_report_response_times_ = 0;
            is_report_response_ = true;

            if (tracker_client_map_[packet.end_point]->IsSync())
            {
                report_timer_.interval(packet.response.keep_alive_interval_ * 1000);
            }
            else
            {
                report_timer_.interval(DEFAULT_INTERVAL_IN_SECONDS_ * 1000);
            }

            if (!current_report_tracker_)
            {
                current_report_tracker_ = tracker_client_map_[packet.end_point];

                statistic::StatisticModule::Inst()->SetIsSubmitTracker(
                    current_report_tracker_->GetTrackerInfo(), true);
            }
        }
    }

    void TrackerStation::OnTimerElapsed(framework::timer::Timer * pointer)
    {
        assert(pointer == &report_timer_);

        if (tracker_client_map_.empty())
        {
            return;
        }

        DoReport();
    }

    void TrackerStation::DeleteRidRecord(const RID & rid)
    {
        list_get_response_.erase(rid);
    }
}