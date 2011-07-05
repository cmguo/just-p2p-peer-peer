//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _STATISTIC_STATISTICS_COLLECTOR_H_
#define _STATISTIC_STATISTICS_COLLECTOR_H_

#include "statistic/StatisticStructs.h"
#include "statistic/CollectionSpec.h"

namespace statistic
{
    class StatisticsData;
    class CollectionSpec;
    class StatisticsCollectorSettings;

    class IStatisticsCollector
    {
    public:
        //根据CollectionSpec来进行日志数据的收集
        virtual std::vector<boost::shared_ptr<StatisticsData> > Collect(const CollectionSpec& spec) = 0;
        virtual ~IStatisticsCollector(){}
    };

    class StatisticsCollector: public IStatisticsCollector
    {
    public:
        StatisticsCollector(const StatisticsCollectorSettings& collector_settings);

        std::vector<boost::shared_ptr<StatisticsData> > Collect(const CollectionSpec& collection_spec);

        void Start();
        void Stop();

        void SetActiveCollectionSpecs(const std::vector<boost::shared_ptr<CollectionSpec> > & active_collection_specs);

    private:
        void OnTimerElapsed(framework::timer::Timer * timer);

        framework::timer::PeriodicTimer collection_timer_;

        struct StatisticsSnapshot
        {
            STASTISTIC_INFO statistics_info;
            std::vector<DOWNLOADDRIVER_STATISTIC_INFO> vod_download_driver_statistics_infos;
            std::vector<LIVE_DOWNLOADDRIVER_STATISTIC_INFO> live_download_driver_statistics_infos;
            std::vector<P2PDOWNLOADER_STATISTIC_INFO> p2p_downloader_statistics_infos;

            framework::timer::TickCounter ticks_since_collected;
        };

        static boost::shared_ptr<StatisticsSnapshot> GetSnapshotByRid(const RID& rid, const StatisticsSnapshot& complete_snapshot);

        static boost::shared_ptr<StatisticsData> ConvertSnapshotToStatisticsData(const boost::shared_ptr<StatisticsSnapshot> snapshot, const CollectionSpec& collection_spec);

        void RemoveExpiredStatistics();

        static void RemoveExpiredStatistics(std::deque<boost::shared_ptr<StatisticsSnapshot> >& snapshots, 
                const CollectionSpec& collection_spec);

    private:
        std::map<RID, std::deque<boost::shared_ptr<StatisticsSnapshot> > > rid_to_statistics_snapshots_mapping_;

        std::map<RID, CollectionSpec> active_collection_specs_;
    };
}

#endif  // _STATISTIC_STATISTICS_COLLECTOR_H_
