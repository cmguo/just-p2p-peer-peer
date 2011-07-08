//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "statistic/StatisticsReporter.h"
#include "statistic/ReportCondition.h"
#include "statistic/StatisticsData.h"
#include "statistic/StatisticsCollector.h"
#include "statistic/StatisticsReportSender.h"
#include "statistic/CollectionSpec.h"

namespace statistic
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("statistics_collection");

    StatisticsReporter::StatisticsReporter(
        const StatisticsReportingConfiguration &reporting_configuration, 
        boost::shared_ptr<StatisticsCollector> statistics_collector,
        const std::vector<string>& servers)
        : reporting_configuration_(reporting_configuration), statistics_collector_(statistics_collector), 
        checking_timer_(global_second_timer(), 3*1000, boost::bind(&StatisticsReporter::OnTimerElapsed, this, &checking_timer_))
    {
        report_sender_.reset(new StatisticsReportSender(servers));
    }

    void StatisticsReporter::Start()
    {
        checking_timer_.start();
        statistics_collector_->Start();
    }

    void StatisticsReporter::Stop()
    {
        statistics_collector_->Stop();
        checking_timer_.stop();
    }

    void StatisticsReporter::OnCollectionSpecsUpdated()
    {
        std::vector<CollectionSpecPointer> collection_specs;

        for(std::map<ReportConditionPointer, CollectionSpecPointer>::iterator iter = conditions_.begin();
            iter != conditions_.end();
            ++iter)
        {
            collection_specs.push_back(iter->second);    
        }

        statistics_collector_->SetActiveCollectionSpecs(collection_specs);
    }

    void StatisticsReporter::AddCondition(ReportConditionPointer condition, CollectionSpecPointer collection_spec)
    {
        conditions_.insert(std::make_pair(condition, collection_spec));
        OnCollectionSpecsUpdated();
    }

    bool StatisticsReporter::RemoveCondition(ReportConditionPointer condition)
    {
        assert(condition);

        size_t conditions_removed = conditions_.erase(condition);
        assert(conditions_removed == 1);

        OnCollectionSpecsUpdated();

        return conditions_removed > 0;
    }

    void StatisticsReporter::OnTimerElapsed(framework::timer::Timer * timer)
    {
        if (timer == &checking_timer_)
        {
            for(std::map<ReportConditionPointer, CollectionSpecPointer>::iterator iter = conditions_.begin();
                iter != conditions_.end();
                ++iter)
            {
                if (iter->first->IsTrue())
                {
                    LOG(__DEBUG, "statistics_collection", __FUNCTION__ << " Condition '" << iter->first->GetConditionId() << "' is TRUE.");

                    std::vector<boost::shared_ptr<StatisticsData> > statistics_data = statistics_collector_->Collect(*(iter->second));
                    if (statistics_data.size() > 0)
                    {
                        LOG(__DEBUG, "statistics_collection", __FUNCTION__ << " Sending statistics data to report sender");
                        report_sender_->AddReport(iter->second->GetResourceId(), iter->first, statistics_data);
                    }
                    else
                    {
                        LOG(__DEBUG, "statistics_collection", __FUNCTION__ << " statistics data is empty and will NOT be sent.");
                    }

                    iter->first->Reset();
                }
            }

            report_sender_->SendAll();
        }
    }
}