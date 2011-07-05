//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "statistic/StatisticsCollector.h"
#include "statistic/CollectionSpec.h"
#include "statistic/StatisticModule.h"
#include "statistic/StatisticsData.h"

namespace statistic
{
    StatisticsCollector::StatisticsCollector(const StatisticsCollectorSettings& collector_settings)
        :collection_timer_(global_second_timer(), 1000, boost::bind(&StatisticsCollector::OnTimerElapsed, this, &collection_timer_))
    {
    }

    std::vector<boost::shared_ptr<StatisticsData> > StatisticsCollector::Collect(const CollectionSpec& collection_spec)
    {
        RID rid = collection_spec.GetResourceId();

        std::map<RID, std::deque<boost::shared_ptr<StatisticsSnapshot> > >::iterator snapshot_iter = 
            rid_to_statistics_snapshots_mapping_.find(rid);

        std::vector<boost::shared_ptr<StatisticsData> > result;

        if (snapshot_iter != rid_to_statistics_snapshots_mapping_.end())
        {
            const std::deque<boost::shared_ptr<StatisticsSnapshot> >& snapshots = snapshot_iter->second;
            for(size_t i = 0; i < snapshots.size(); ++i)
            {
                if (snapshots[i]->ticks_since_collected.elapsed() < collection_spec.GetCollectionLength()*1000)
                {
                    result.push_back(ConvertSnapshotToStatisticsData(snapshots[i], collection_spec));
                }
            }
        }

        return result;
    }

    void StatisticsCollector::Start()
    {
        collection_timer_.start();
    }

    void StatisticsCollector::Stop()
    {
        collection_timer_.stop();
    }

    void StatisticsCollector::SetActiveCollectionSpecs(const std::vector<boost::shared_ptr<CollectionSpec> > & active_collection_specs)
    {
        active_collection_specs_.clear();

        for(size_t i = 0; i < active_collection_specs.size(); ++i)
        {
            const CollectionSpec& spec = *(active_collection_specs[i]);
            std::map<RID, CollectionSpec>::iterator iter = active_collection_specs_.find(spec.GetResourceId());
            if (iter == active_collection_specs_.end())
            {
                active_collection_specs_.insert(std::make_pair(spec.GetResourceId(), spec));
            }
            else
            {
                active_collection_specs_[spec.GetResourceId()].Union(spec);
            }
        }
        
        RemoveExpiredStatistics();
    }

    boost::shared_ptr<StatisticsData> StatisticsCollector::ConvertSnapshotToStatisticsData(const boost::shared_ptr<StatisticsSnapshot> snapshot, const CollectionSpec& collection_spec)
    {
        boost::shared_ptr<StatisticsData> statistics_data(new StatisticsData(snapshot->statistics_info));
        if (collection_spec.IncludesDownloadDrivers())
        {
            for(size_t index = 0; index < snapshot->live_download_driver_statistics_infos.size(); ++index)
            {
                statistics_data->AddLiveDownloadDriverStatistics(
                    boost::shared_ptr<LiveDownloadDriverStatisticsData>(
                        new LiveDownloadDriverStatisticsData(
                            snapshot->live_download_driver_statistics_infos[index])));
            }

            for(size_t index = 0; index < snapshot->vod_download_driver_statistics_infos.size(); ++index)
            {
                statistics_data->AddVodDownloadDriverStatistics(
                    boost::shared_ptr<VodDownloadDriverStatisticsData>(
                        new VodDownloadDriverStatisticsData(
                            snapshot->vod_download_driver_statistics_infos[index])));
            }
        }

        if (collection_spec.IncludesDownloaders())
        {
            for(size_t index = 0; index < snapshot->p2p_downloader_statistics_infos.size(); ++index)
            {
                statistics_data->AddP2PDownloaderStatistics(
                    boost::shared_ptr<P2PDownloaderStatisticsData>(
                        new P2PDownloaderStatisticsData(
                            snapshot->p2p_downloader_statistics_infos[index])));
            }
        }

        return statistics_data;
    }

    void StatisticsCollector::OnTimerElapsed(framework::timer::Timer* timer)
    {
        if (timer == &collection_timer_)
        {
            RemoveExpiredStatistics();

            if (active_collection_specs_.size() == 0)
            {
                return;
            }
            
            //save a snapshot of statistics data
            StatisticsSnapshot snapshot;
            StatisticModule::Inst()->TakeSnapshot(snapshot.statistics_info, snapshot.vod_download_driver_statistics_infos, snapshot.live_download_driver_statistics_infos, snapshot.p2p_downloader_statistics_infos);

            for(std::map<RID, CollectionSpec>::iterator iter = active_collection_specs_.begin();
                iter != active_collection_specs_.end();
                ++iter)
            {
                RID rid = iter->second.GetResourceId();
                rid_to_statistics_snapshots_mapping_[rid].push_back(GetSnapshotByRid(rid, snapshot));
            }
        }
    }

    boost::shared_ptr<StatisticsCollector::StatisticsSnapshot> StatisticsCollector::GetSnapshotByRid(const RID& rid, const StatisticsSnapshot& complete_snapshot)
    {
        boost::shared_ptr<StatisticsSnapshot> resource_specific_snapshot(new StatisticsSnapshot());
        resource_specific_snapshot->statistics_info = complete_snapshot.statistics_info;
        for(size_t i = 0; i < complete_snapshot.vod_download_driver_statistics_infos.size(); ++i)
        {
            if (complete_snapshot.vod_download_driver_statistics_infos[i].ResourceID == rid)
            {
                resource_specific_snapshot->vod_download_driver_statistics_infos.push_back(complete_snapshot.vod_download_driver_statistics_infos[i]);
            }
        }

        for(size_t i = 0; i < complete_snapshot.live_download_driver_statistics_infos.size(); ++i)
        {
            if (complete_snapshot.live_download_driver_statistics_infos[i].ResourceID == rid)
            {
                resource_specific_snapshot->live_download_driver_statistics_infos.push_back(complete_snapshot.live_download_driver_statistics_infos[i]);
            }
        }

        for(size_t i = 0; i < complete_snapshot.p2p_downloader_statistics_infos.size(); ++i)
        {
            if (complete_snapshot.p2p_downloader_statistics_infos[i].ResourceID == rid)
            {
                resource_specific_snapshot->p2p_downloader_statistics_infos.push_back(complete_snapshot.p2p_downloader_statistics_infos[i]);
            }
        }
        
        return resource_specific_snapshot;
    }

    void StatisticsCollector::RemoveExpiredStatistics()
    {
        std::map<RID, std::deque<boost::shared_ptr<StatisticsSnapshot> > >::iterator snapshot_iter;

        std::vector<RID> snapshots_to_remove;

        for(snapshot_iter = rid_to_statistics_snapshots_mapping_.begin();
            snapshot_iter != rid_to_statistics_snapshots_mapping_.end();
            ++snapshot_iter)
        {
            std::map<RID, CollectionSpec>::iterator spec_iter = active_collection_specs_.find(snapshot_iter->first);
            if (spec_iter == active_collection_specs_.end())
            {
                snapshots_to_remove.push_back(snapshot_iter->first);
            }
            else
            {
                RemoveExpiredStatistics(snapshot_iter->second, spec_iter->second);
            }
        }

        for(size_t i = 0; i < snapshots_to_remove.size(); ++i)
        {
            rid_to_statistics_snapshots_mapping_.erase(snapshots_to_remove[i]);
        }
    }

    void StatisticsCollector::RemoveExpiredStatistics(std::deque<boost::shared_ptr<StatisticsSnapshot> >& snapshots, 
        const CollectionSpec& collection_spec)
    {
        while(snapshots.size() > 0)
        {
            if (snapshots.front()->ticks_since_collected.elapsed() < 1000*collection_spec.GetCollectionLength())
            {
                break;
            }
            
            snapshots.pop_front();
        }
    }
}