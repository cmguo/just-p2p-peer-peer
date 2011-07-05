//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _STATISTIC_STATISTICS_CONFIGURATION_H_
#define _STATISTIC_STATISTICS_CONFIGURATION_H_

#include "CollectionCriteria.h"

namespace statistic
{
    struct StatisticsConfiguration
    {
        StatisticsConfiguration()
            :collection_length_(0), max_reporting_occurrences_(1), reporting_probability_(0)
        {
        }

        int collection_length_;
        int max_reporting_occurrences_;
        int reporting_probability_;
        string id_;

        std::map<string, string> misc_settings_;

        CollectionCriteria criteria_;
    };

    struct StatisticsConfigurations
    {
        StatisticsConfigurations()
            :expires_in_minutes_(-1)
        {
        }

        std::vector<StatisticsConfiguration> statistics_configurations_;
        int expires_in_minutes_;

        void RemoveInactiveStatisticsConfiguration();
    };
}

#endif  // _STATISTIC_STATISTICS_CONFIGURATION_H_
