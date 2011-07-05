//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _STATISTIC_STATISTICS_REPORTING_CONFIGURATION_H_
#define _STATISTIC_STATISTICS_REPORTING_CONFIGURATION_H_

#include "statistic/StatisticsConfiguration.h"

namespace statistic
{
    class StatisticsReportingConfiguration
    {
    public:
        StatisticsConfigurations statistics_configurations_;

        StatisticsReportingConfiguration(const StatisticsConfigurations& statistics_configurations)
            : statistics_configurations_(statistics_configurations)
        {
        }

        void GetApplicableStatisticsConfigurations(const RID& rid, ConditionType condition_type, std::vector<StatisticsConfiguration>& applicable_configurations) const
        {
            applicable_configurations.clear();
            for(size_t i = 0; i < statistics_configurations_.statistics_configurations_.size(); ++i)
            {
                const CollectionCriteria& criteria = statistics_configurations_.statistics_configurations_[i].criteria_;
                if (criteria.condition_type_ == condition_type && criteria.IsApplicable(rid))
                {
                    applicable_configurations.push_back(statistics_configurations_.statistics_configurations_[i]);
                }
            }
        }
    };
}

#endif  // _STATISTIC_STATISTICS_REPORTING_CONFIGURATION_H_
