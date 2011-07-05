//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _STATISTIC_REPORT_CONDITION_H_
#define _STATISTIC_REPORT_CONDITION_H_

#include "StatisticsConfiguration.h"

namespace statistic
{
    class ReportCondition
    {
    public:
        virtual bool IsTrue() = 0;
        virtual void Reset() = 0;
        virtual ~ReportCondition(){}
        string GetConditionId() const { return condition_id_; }

    protected:
        ReportCondition(const StatisticsConfiguration& statistics_configuration)

        {
            condition_id_ = statistics_configuration.id_;
        }

    private:
        string condition_id_;
    };
}

#endif  // _STATISTIC_REPORT_CONDITION_H_
