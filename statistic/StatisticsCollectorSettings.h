//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _STATISTIC_STATISTICS_COLLECTOR_SETTINGS_H_
#define _STATISTIC_STATISTICS_COLLECTOR_SETTINGS_H_

namespace statistic
{
    struct StatisticsConfigurations;

    class StatisticsCollectorSettings
    {
    public:
        StatisticsCollectorSettings(const StatisticsConfigurations& statistics_configurations);
    };
}

#endif  // _STATISTIC_STATISTICS_COLLECTOR_SETTINGS_H_
