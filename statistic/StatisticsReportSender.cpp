//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "statistic/StatisticsReportSender.h"
#include "statistic/ReportCondition.h"
#include "statistic/StatisticsRequest.h"
#include "network/HttpPost.h"
#include "p2sp/AppModule.h"
#include <boost/interprocess/streams/vectorstream.hpp>

namespace statistic
{
    StatisticsReportSender::StatisticsReportSender(const std::vector<string>& servers)
        : servers_(servers),selected_server_index_(-1),
          timer_(global_second_timer(), 1000, boost::bind(&StatisticsReportSender::OnTimerElapsed, this, &timer_))
    {
        for(size_t index = 0; index < servers_.size(); ++index)
        {
            servers_successful_count_[servers_[index]] = 0;
        }
    }

    void StatisticsReportSender::SendAll()
    {
        if (pending_reports_.size() > 0)
        {
            timer_.start();
        }
    }

    void StatisticsReportSender::AddReport(const RID& rid, ReportConditionPointer report_condition, const std::vector<StatisticsPointer>& statistics)
    {
        if (pending_reports_.size() >= 10)
        {
            return;
        }

        pending_reports_.push_back(
            boost::shared_ptr<StatisticsRequest>(
                new StatisticsRequest(rid, report_condition, statistics)));

        SendReportIfAppropriate();
    }

    void StatisticsReportSender::OnTimerElapsed(framework::timer::PeriodicTimer* timer)
    {
        if (timer == &timer_)
        {
            SendReportIfAppropriate();
        }
    }

    void StatisticsReportSender::SendReportIfAppropriate()
    {
        if (http_post_ || servers_.size() == 0)
        {
            return;
        }

        while(pending_reports_.size() > 0)
        {
            if (pending_reports_.front()->IsValid())
            {
                break;
            }
            
            pending_reports_.pop_front();
        }

        if (pending_reports_.size() == 0)
        {
            return;
        }

        boost::shared_ptr<StatisticsRequest> request_to_send = pending_reports_.front();

        request_to_send->IncrementAttempts();

        if (selected_server_index_ < 0)
        {
            selected_server_index_ = 0;
        }

        http_post_.reset(
            new network::HttpPost(
                global_io_svc(), 
                servers_[selected_server_index_], 
                request_to_send->BuildPostRelativeUrl(),
                shared_from_this()));

        boost::interprocess::basic_vectorstream<vector<char> > data;
        if (request_to_send->GetCompressedData(data))
        {
            http_post_->AsyncPost(data);
        }
        else
        {
            //give up now
            http_post_.reset();
            assert(pending_reports_.size() > 0);
            pending_reports_.pop_front();
        }
    }

    void StatisticsReportSender::CountAsSuccessfulRequest()
    {
        assert(servers_.size() > 0);
        assert(selected_server_index_ >= 0 && selected_server_index_ < static_cast<int>(servers_.size()));

        string current_server = servers_[selected_server_index_];
        ++(servers_successful_count_[current_server]);
    }

    bool StatisticsReportSender::BetterThan(const string& a, const string& b)
    {
        return servers_successful_count_[a] > servers_successful_count_[b];
    }

    void StatisticsReportSender::OnPostResult(const boost::system::error_code& err)
    {
        if (!err)
        {
            assert(pending_reports_.size() > 0);
            assert(http_post_);

            CountAsSuccessfulRequest();

            http_post_.reset();

            if (pending_reports_.size() > 0)
            {
                this->pending_reports_.pop_front();
            }
        }
        else
        {
            if (selected_server_index_ + 1 < static_cast<int>(servers_.size()))
            {
                ++selected_server_index_;
            }
            else
            {
                //全部尝试过至少一次了
                if (servers_.size() > 1)
                {
                    std::sort(servers_.begin(), servers_.end(), boost::bind(&StatisticsReportSender::BetterThan, this, _1, _2));
                }

                selected_server_index_ = 0;
            }
        }
    }
}