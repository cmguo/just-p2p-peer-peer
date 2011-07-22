//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/tracker/TrackerModule.h"

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("tracker");

    TrackerModule::p TrackerModule::inst_;

    // need_report暂时没有用
    void TrackerModule::Start(const string& config_path, bool need_report)
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
            vod_tracker_manager_.SetTrackerList(group_count, tracker_s);
        }
        else
        {
            live_tracker_manager_.SetTrackerList(group_count, tracker_s);
        }
    }

    void TrackerModule::DoList(RID rid, bool is_vod)
    {
        TRACK_INFO("TrackerModule::DoList, RID: " << rid);

        if (is_running_ == false)
        {
            LOG(__WARN, "tracker", "Tracker Manager is not running. Return.");
            return;
        }

        if (is_vod)
        {
            vod_tracker_manager_.DoList(rid);
        }
        else
        {
            live_tracker_manager_.DoList(rid);
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
}