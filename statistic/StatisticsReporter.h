//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _STATISTIC_STATISTICS_REPORTER_H_
#define _STATISTIC_STATISTICS_REPORTER_H_

#include "StatisticsReportingConfiguration.h"

namespace statistic
{
    class ReportCondition;
    class CollectionSpec;
    class StatisticsCollector;
    class StatisticsReportSender;

    class StatisticsReporter
    {
        typedef boost::shared_ptr<ReportCondition> ReportConditionPointer;
        typedef boost::shared_ptr<CollectionSpec> CollectionSpecPointer;

        boost::shared_ptr<StatisticsReportSender> report_sender_;
        boost::shared_ptr<StatisticsCollector> statistics_collector_;
        std::map<ReportConditionPointer, CollectionSpecPointer> conditions_;
        StatisticsReportingConfiguration reporting_configuration_;

        framework::timer::PeriodicTimer checking_timer_;

    public:
        StatisticsReporter(const StatisticsReportingConfiguration &reporting_configuration, 
            boost::shared_ptr<StatisticsCollector> statistics_collector,
            const std::vector<string>& servers);

        void AddCondition(ReportConditionPointer condition, CollectionSpecPointer collection_spec);

        bool RemoveCondition(ReportConditionPointer condition);

        void Start();

        void Stop();

        void GetApplicableStatisticsConfigurations(const RID& rid, ConditionType condition_type, std::vector<StatisticsConfiguration>& applicable_configurations) const
        {
            reporting_configuration_.GetApplicableStatisticsConfigurations(rid, condition_type, applicable_configurations);
        }

    private:
        void OnTimerElapsed(framework::timer::Timer * timer);
        void OnCollectionSpecsUpdated();
    };
}

#endif  // _STATISTIC_STATISTICS_REPORTER_H_
