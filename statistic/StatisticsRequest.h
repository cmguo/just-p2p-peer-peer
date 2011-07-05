//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _STATISTIC_STATISTICS_REQUEST_H_
#define _STATISTIC_STATISTICS_REQUEST_H_

namespace statistic
{
    class ReportCondition;
    class StatisticsData;

    class StatisticsRequest
    {
        typedef boost::shared_ptr<ReportCondition> ReportConditionPointer;
        typedef boost::shared_ptr<StatisticsData> StatisticsPointer;
    public:
        StatisticsRequest(const RID& rid, ReportConditionPointer condition, const std::vector<StatisticsPointer>& statistics)
            : attempts_(0), rid_(rid), condition_(condition), statistics_(statistics)
        {
        }

        ReportConditionPointer GetReportCondition() const { return condition_; }

        bool GetCompressedData(std::ostream& data);

        void IncrementAttempts() { ++attempts_; }

        bool IsValid() const { return attempts_ < MaxAttempts; }

        string BuildPostRelativeUrl();
    private:
        static const int MaxAttempts;
        ReportConditionPointer condition_;
        std::vector<StatisticsPointer> statistics_;
        const RID rid_;
        int attempts_;
    };
}

#endif  // _STATISTIC_STATISTICS_REQUEST_H_
