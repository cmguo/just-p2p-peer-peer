//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "BufferringMonitor.h"
#include "BufferringReportCondition.h"
#include "CollectionSpec.h"
#include "StatisticsReporter.h"
#include "StatisticsConfiguration.h"

namespace statistic
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_buffering_monitor = log4cplus::Logger::getInstance("[buffering_monitor]");
#endif

    BufferringMonitor::BufferringMonitor(RID rid, boost::shared_ptr<StatisticsReporter> reporter)
        : rid_(rid), reporter_(reporter)
    {
        std::vector<StatisticsConfiguration> bufferring_configurations;
        reporter_->GetApplicableStatisticsConfigurations(rid, Bufferring, bufferring_configurations);

        for(size_t i = 0; i < bufferring_configurations.size(); ++i)
        {
            boost::shared_ptr<BufferringReportCondition> report_condition = CreateBufferringReportCondition(bufferring_configurations[i]);
            boost::shared_ptr<CollectionSpec> collection_spec = CreateBufferringCollectionSpec(bufferring_configurations[i]);

            reporter_->AddCondition(report_condition, collection_spec);
            conditions_.push_back(report_condition);
        }

        LOG4CPLUS_INFO_LOG(logger_buffering_monitor, __FUNCTION__ << " BufferringMonitor created.");
    }

    BufferringMonitor::~BufferringMonitor()
    {
        for(size_t i = 0; i < conditions_.size(); ++i)
        {
            reporter_->RemoveCondition(conditions_[i]);
        }
    }

    void BufferringMonitor::BufferringOccurs(boost::uint32_t bufferring_position_in_seconds)
    {
        for(size_t i = 0; i < conditions_.size(); ++i)
        {
            conditions_[i]->BufferringOccurs(bufferring_position_in_seconds);
        }

        LOG4CPLUS_INFO_LOG(logger_buffering_monitor, __FUNCTION__ << " Bufferring occurred");
    }

    boost::shared_ptr<BufferringReportCondition> BufferringMonitor::CreateBufferringReportCondition(
        const StatisticsConfiguration& bufferring_statistics_config) const
    {
        //目前仅检1次卡顿的发生，可以在此做扩展以支持检测多次卡顿发生的事件 
        return boost::shared_ptr<BufferringReportCondition>(
            new BufferringReportCondition(
                bufferring_statistics_config, 
                1/*发生一次卡顿就汇报*/, 
                5/*在距离上次汇报后，忽略多少秒内重复发生的事件*/));
    }

    boost::shared_ptr<CollectionSpec> BufferringMonitor::CreateBufferringCollectionSpec(
        const StatisticsConfiguration& bufferring_statistics_config) const
    {
        return boost::shared_ptr<CollectionSpec>(new CollectionSpec(rid_, bufferring_statistics_config.collection_length_, true, true));
    }
}

