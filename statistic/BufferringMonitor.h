//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _STATISTIC_BUFFERRING_MONITOR_H_
#define _STATISTIC_BUFFERRING_MONITOR_H_


namespace statistic
{
    class BufferringReportCondition;
    class StatisticsReporter;
    struct StatisticsConfiguration;
    class CollectionSpec;

    class BufferringMonitor
    {
        std::vector<boost::shared_ptr<BufferringReportCondition> > conditions_;
        boost::shared_ptr<StatisticsReporter> reporter_;
        RID rid_;

    public:
        BufferringMonitor(RID rid, boost::shared_ptr<StatisticsReporter> reporter);
        ~BufferringMonitor();

        void BufferringOccurs(uint32_t bufferring_position_in_seconds);

    private:
        boost::shared_ptr<BufferringReportCondition> CreateBufferringReportCondition(
            const StatisticsConfiguration& bufferring_statistics_config) const;

        boost::shared_ptr<CollectionSpec> CreateBufferringCollectionSpec(
            const StatisticsConfiguration& bufferring_statistics_config) const;
    };
}

#endif  // _STATISTIC_BUFFERRING_MONITOR_H_
