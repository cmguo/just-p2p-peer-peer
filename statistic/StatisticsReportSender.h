//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _STATISTIC_STATISTICS_REPORT_SENDER_H_
#define _STATISTIC_STATISTICS_REPORT_SENDER_H_

#include "network/IHttpPostListener.h"
#include <iosfwd>

namespace network
{
    class HttpPost;
}

namespace statistic
{
    class ReportCondition;
    class StatisticsData;
    class StatisticsRequest;

    class StatisticsReportSender
        : public network::IHttpPostListener,
          public boost::enable_shared_from_this<StatisticsReportSender>
    {
        typedef boost::shared_ptr<ReportCondition> ReportConditionPointer;
        typedef boost::shared_ptr<StatisticsData> StatisticsPointer;

    public:
        StatisticsReportSender(const std::vector<string>& servers);

        void SendAll();
        void AddReport(const RID& rid, ReportConditionPointer report_condition, const std::vector<StatisticsPointer>& statistics);

    private:
        void OnTimerElapsed(framework::timer::PeriodicTimer* timer);
        void SendReportIfAppropriate();
        void OnPostResult(const boost::system::error_code& err);
        void CountAsSuccessfulRequest();
        bool BetterThan(const string& a, const string& b);

    private:
        framework::timer::PeriodicTimer timer_;
        std::vector<string> servers_;
        int selected_server_index_;

        std::deque<boost::shared_ptr<StatisticsRequest> > pending_reports_;
        boost::shared_ptr<network::HttpPost> http_post_;
        std::map<string, int> servers_successful_count_;
    };
}

#endif  // _STATISTIC_STATISTICS_REPORT_SENDER_H_
