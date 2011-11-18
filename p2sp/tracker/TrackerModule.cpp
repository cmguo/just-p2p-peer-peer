//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/tracker/TrackerModule.h"

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("tracker");

    TrackerModule::p TrackerModule::inst_;

    void TrackerModule::Start(const string& config_path)
    {
        if (is_running_ == true)
        {
            LOG(__WARN, "tracker", "TrackerModule is running...");
            return;
        }

        vod_tracker_manager_.Start(config_path);
        live_tracker_manager_.Start(config_path);

        is_running_ = true;
    }

    void TrackerModule::Stop()
    {
        if (is_running_ == false) return;

        vod_tracker_manager_.Stop();
        live_tracker_manager_.Stop();

        is_running_ = false;
        inst_.reset();
    }

    void TrackerModule::SetTrackerList(uint32_t group_count, const std::vector<protocol::TRACKER_INFO> & tracker_s, bool is_vod)
    {
        if (is_vod)
        {
            vod_tracker_manager_.SetTrackerList(group_count, tracker_s, true);
        }
        else
        {
            live_tracker_manager_.SetTrackerList(group_count, tracker_s, true);
        }
    }

    void TrackerModule::DoList(RID rid, bool is_vod, bool list_for_live_udpserver)
    {
        TRACK_INFO("TrackerModule::DoList, RID: " << rid);

        if (is_running_ == false)
        {
            LOG(__WARN, "tracker", "Tracker Manager is not running. Return.");
            return;
        }

        if (is_vod)
        {
            vod_tracker_manager_.DoList(rid, false);
        }
        else
        {
            live_tracker_manager_.DoList(rid, list_for_live_udpserver);
        }
    }

    void TrackerModule::OnUdpRecv(protocol::ServerPacket const &packet)
    {
        if (is_running_ == false)
        {
            return;
        }

        vod_tracker_manager_.OnUdpRecv(packet);
        live_tracker_manager_.OnUdpRecv(packet);
    }

    void TrackerModule::PPLeave()
    {
        vod_tracker_manager_.PPLeave();
        live_tracker_manager_.PPLeave();
    }

    void TrackerModule::DoReport(bool is_vod)
    {
        if (is_vod)
        {
            vod_tracker_manager_.DoReport();
        }
        else
        {
            live_tracker_manager_.DoReport();
        }
    }
}