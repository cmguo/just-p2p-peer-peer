//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/tracker/TrackerGroup.h"
#include "statistic/StatisticModule.h"
#include "p2sp/tracker/TrackerModule.h"

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("tracker");

    void TrackerGroup::Start()
    {
        if (is_running_)
        {
            return;
        }

        StartAllStation();

        is_running_ = true;
    }

    void TrackerGroup::Stop()
    {
        if (!is_running_)
        {
            return;
        }

        StopAllStation();
        ClearAllStation();

        is_running_ = false;
    }

    void TrackerGroup::StartAllStation()
    {
        for (std::map<boost::uint32_t, boost::shared_ptr<TrackerStation> >::iterator
            iter = mod_station_map_.begin(); iter != mod_station_map_.end(); ++iter)
        {
            iter->second->Start();
        }
    }

    void TrackerGroup::StopAllStation()
    {
        for (std::map<boost::uint32_t, boost::shared_ptr<TrackerStation> >::iterator
            iter = mod_station_map_.begin(); iter != mod_station_map_.end(); ++iter)
        {
            iter->second->Stop();
        }
    }

    void TrackerGroup::ClearAllStation()
    {
        StopAllStation();

        mod_station_map_.clear();
        endpoint_station_map_.clear();
    }

    void TrackerGroup::SetTrackers(boost::uint32_t group_count, const std::set<protocol::TRACKER_INFO> & trackers)
    {
        if (trackers.empty()) 
        {
            ClearAllStation();
            return;
        }

        std::map<boost::uint32_t, std::set<protocol::TRACKER_INFO> > station_map;

        for (std::set<protocol::TRACKER_INFO>::const_iterator iter = trackers.begin();
            iter != trackers.end(); ++iter)
        {
            station_map[iter->StationNo].insert(*iter);
        }

        for (std::map<boost::uint32_t, boost::shared_ptr<TrackerStation> >::iterator
            iter = mod_station_map_.begin(); iter != mod_station_map_.end(); )
        {
            if (station_map.find(iter->first) == station_map.end())
            {
                mod_station_map_.erase(iter++);
            }
            else
            {
                ++iter;
            }
        }

        for (std::map<boost::uint32_t, std::set<protocol::TRACKER_INFO> >::iterator
            iter = station_map.begin(); iter != station_map.end(); ++iter)
        {
            if (mod_station_map_.find(iter->first) == mod_station_map_.end())
            {
                mod_station_map_.insert(std::make_pair(iter->first,
                    boost::shared_ptr<TrackerStation>(new TrackerStation(is_vod_, tracker_type_))));
            }

            mod_station_map_[iter->first]->SetTrackers(group_count, iter->second);
            mod_station_map_[iter->first]->Start();
        }

        endpoint_station_map_.clear();

        for (std::set<protocol::TRACKER_INFO>::const_iterator iter = trackers.begin();
            iter != trackers.end(); ++iter)
        {
            boost::asio::ip::udp::endpoint endpoint = framework::network::Endpoint(iter->IP,
                iter->Port);

            endpoint_station_map_[endpoint] = mod_station_map_[iter->StationNo];
        }

        StartAllStation();
    }

    void TrackerGroup::DoList(const RID& rid, bool list_for_live_udpserver)
    {
        for (std::map<boost::uint32_t, boost::shared_ptr<TrackerStation> >::iterator 
            iter = mod_station_map_.begin(); iter != mod_station_map_.end(); ++iter)
        {
            iter->second->DoList(rid, list_for_live_udpserver);
        }
    }

    void TrackerGroup::DoReport()
    {
        for (std::map<boost::uint32_t, boost::shared_ptr<TrackerStation> >::iterator 
            iter = mod_station_map_.begin(); iter != mod_station_map_.end(); ++iter)
        {
            iter->second->DoReport();
        }
    }

    void TrackerGroup::DoLeave()
    {
        for (std::map<boost::uint32_t, boost::shared_ptr<TrackerStation> >::iterator 
            iter = mod_station_map_.begin(); iter != mod_station_map_.end(); ++iter)
        {
            iter->second->DoLeave();
        }
    }

    std::vector<protocol::TRACKER_INFO> TrackerGroup::GetTrackers()
    {
        std::vector<protocol::TRACKER_INFO> ret_tracker_infos;

        for (std::map<boost::uint32_t, boost::shared_ptr<TrackerStation> >::iterator 
            it = mod_station_map_.begin(); it != mod_station_map_.end(); ++it)
        {
            std::vector<protocol::TRACKER_INFO> vector = it->second->GetTrackers();
            ret_tracker_infos.insert(ret_tracker_infos.end(), vector.begin(), vector.end());
        }

        return ret_tracker_infos;
    }

    void TrackerGroup::OnListResponsePacket(protocol::ListPacket const & packet)
    {
        std::map<boost::asio::ip::udp::endpoint, boost::shared_ptr<TrackerStation> >::iterator
            iter = endpoint_station_map_.find(packet.end_point);

        if (iter != endpoint_station_map_.end())
        {
            iter->second->OnListResponsePacket(packet);
        }
    }

    void TrackerGroup::OnReportResponsePacket(protocol::ReportPacket const & packet)
    {
        // Report型的Tracker只有一个机房
        assert(mod_station_map_.size() == 1);

        std::map<boost::asio::ip::udp::endpoint, boost::shared_ptr<TrackerStation> >::iterator
            iter = endpoint_station_map_.find(packet.end_point);

        if (iter != endpoint_station_map_.end())
        {
            iter->second->OnReportResponsePacket(packet);
        }
    }
}
