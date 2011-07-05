//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "statistic/StatisticsConfiguration.h"
#include "random.h"

namespace statistic
{
    void StatisticsConfigurations::RemoveInactiveStatisticsConfiguration()
    {
        if (statistics_configurations_.size() > 0)
        {
            //[0, 1000000)
            int random_value = Random::GetGlobal().Next(1000000);

            std::vector<StatisticsConfiguration> active_configurations;
            for(size_t i = 0; i < statistics_configurations_.size(); ++i)
            {
                assert(statistics_configurations_[i].reporting_probability_ <= 1000000);

                if (random_value < statistics_configurations_[i].reporting_probability_)
                {
                    active_configurations.push_back(statistics_configurations_[i]);
                }
            }

            statistics_configurations_ = active_configurations;
        }
    }
}

