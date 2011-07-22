//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#pragma once

/**
* @file
* @brief TrackerModule 类的包含文件
*/

#include "p2sp/tracker/TrackerManager.h"

namespace p2sp
{
    class TrackerModule
        : public boost::noncopyable
        , public boost::enable_shared_from_this<TrackerModule>
#ifdef DUMP_OBJECT
        , public count_object_allocate<TrackerModule>
#endif
    {
    public:
        typedef boost::shared_ptr<TrackerModule> p;

        void Start(const string& config_path, bool need_report = true);

        void Stop();

        void SetTrackerList(uint32_t group_count, const std::vector<protocol::TRACKER_INFO> & tracker_s, bool is_vod);

        void DoList(RID rid, bool is_vod);

        void OnUdpRecv(protocol::ServerPacket const &packet);

        void PPLeave();

    private:
        bool is_running_;

        TrackerManager vod_tracker_manager_;
        TrackerManager live_tracker_manager_;

    private:
        static TrackerModule::p inst_;
    public:
        static TrackerModule::p Inst()
        {
            if (!inst_)
            {
                inst_.reset(new TrackerModule());
            }
            return inst_; 
        }

    private:
        TrackerModule()
            : is_running_(false)
            , vod_tracker_manager_(true)
            , live_tracker_manager_(false)
        {}
    };
}