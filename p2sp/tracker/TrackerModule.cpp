//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/tracker/TrackerModule.h"

namespace p2sp
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_tracker = log4cplus::Logger::getInstance("[tracker_module]");
#endif

    TrackerModule::p TrackerModule::inst_;

    void TrackerModule::Start(const string& config_path)
    {
        if (is_running_ == true)
        {
            LOG4CPLUS_WARN_LOG(logger_tracker, "TrackerModule is running...");
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

    void TrackerModule::SetTrackerList(uint32_t group_count, const std::vector<protocol::TRACKER_INFO> & tracker_s,
        bool is_vod, TrackerType tracker_type)
    {
        if (is_running_ == false)
        {
            return;
        }

        if (is_vod)
        {
            vod_tracker_manager_.SetTrackerList(group_count, tracker_s, true, tracker_type);
        }
        else
        {
            live_tracker_manager_.SetTrackerList(group_count, tracker_s, true, tracker_type);
        }
    }

    void TrackerModule::DoList(RID rid, bool is_vod, bool list_for_live_udpserver)
    {
        if (is_running_ == false)
        {
            LOG4CPLUS_WARN_LOG(logger_tracker, "Tracker Manager is not running. Return.");
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
        if (is_running_ == false)
        {
            return;
        }

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

    void TrackerModule::DeleteRidRecord(const RID & rid)
    {
        vod_tracker_manager_.DeleteRidRecord(rid);
    }
}