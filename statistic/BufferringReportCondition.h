//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _STATISTIC_BUFFERRING_REPORT_CONDITION_H_
#define _STATISTIC_BUFFERRING_REPORT_CONDITION_H_

#include "ReportCondition.h"

namespace statistic
{
    //汇报卡顿的触发条件
    class BufferringReportCondition: public ReportCondition
    {
        size_t occurrences_to_trigger_condition_;
        size_t occurrence_count_;
        boost::uint32_t time_window_to_ignore_repeated_occurrences_;
        boost::uint32_t last_bufferring_position_;
        framework::timer::TickCounter ticks_since_last_bufferring_;
        size_t remaining_reports_count_;

    public:
        BufferringReportCondition(
            const StatisticsConfiguration& bufferring_statistics_configuration, 
            size_t occurrences_to_trigger_condition, 
            boost::uint32_t time_window_to_ignore_repeated_occurrences);

        bool IsTrue();
        void Reset();
        
        void BufferringOccurs(boost::uint32_t bufferring_position_in_seconds);
    };
}

#endif  // _STATISTIC_BUFFERRING_REPORT_CONDITION_H_
