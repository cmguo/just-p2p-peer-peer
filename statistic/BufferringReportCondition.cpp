//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "statistic/BufferringReportCondition.h"

namespace statistic
{
    BufferringReportCondition::BufferringReportCondition(
        const StatisticsConfiguration& bufferring_statistics_configuration, 
        size_t occurrences_to_trigger_condition, 
        boost::uint32_t time_window_to_ignore_repeated_occurrences)
        : ReportCondition(bufferring_statistics_configuration),
          occurrences_to_trigger_condition_(occurrences_to_trigger_condition),
          time_window_to_ignore_repeated_occurrences_(time_window_to_ignore_repeated_occurrences),
          remaining_reports_count_(bufferring_statistics_configuration.max_reporting_occurrences_),
          ticks_since_last_bufferring_(false),
          occurrence_count_(0),
          last_bufferring_position_(0)
    {
        assert(occurrences_to_trigger_condition > 0);
    }

    bool BufferringReportCondition::IsTrue()
    {
        return remaining_reports_count_ > 0 && 
               occurrence_count_ >= occurrences_to_trigger_condition_;
    }

    void BufferringReportCondition::Reset()
    {
        if  (IsTrue())
        {
            --remaining_reports_count_;
        }

        occurrence_count_ = 0;
    }
    
    void BufferringReportCondition::BufferringOccurs(boost::uint32_t bufferring_position_in_seconds)
    {
        if (ticks_since_last_bufferring_.running() && 
            ticks_since_last_bufferring_.elapsed() < time_window_to_ignore_repeated_occurrences_*1000)
        {
            return;
        }

        //如果距离上次卡顿汇报位置太近，就忽略
        if (bufferring_position_in_seconds >= last_bufferring_position_ && 
            bufferring_position_in_seconds <= last_bufferring_position_ + 2)
        {
            return;
        }

        ++occurrence_count_;

        last_bufferring_position_ = bufferring_position_in_seconds;
        
        //reset & start
        ticks_since_last_bufferring_.start();
    }
}
