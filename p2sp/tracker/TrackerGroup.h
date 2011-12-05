//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _TRACKER_GROUP_H_
#define _TRACKER_GROUP_H_

#include "TrackerStation.h"

namespace p2sp
{
    class TrackerGroup
        : public boost::noncopyable
#ifdef DUMP_OBJECT
        , public count_object_allocate<TrackerGroup>
#endif
    {
    public:
        TrackerGroup(bool is_vod)
            : is_vod_(is_vod)
            , is_running_(false)
        {}

        void Start();

        void Stop();

        void StartAllStation();

        void StopAllStation();

        void ClearAllStation();

        void SetTrackers(boost::uint32_t group_count, const std::set<protocol::TRACKER_INFO>& trackers);

        std::vector<protocol::TRACKER_INFO> GetTrackers();

        void DoList(const RID& rid, bool list_for_live_udpserver);

        void DoReport();

        void DoLeave();

        void OnListResponsePacket(protocol::ListPacket const & packet);

        void OnReportResponsePacket(protocol::ReportPacket const & packet);

    private:
        bool is_running_;

        std::map<boost::uint32_t, boost::shared_ptr<TrackerStation> > mod_station_map_;
        std::map<boost::asio::ip::udp::endpoint, boost::shared_ptr<TrackerStation> > endpoint_station_map_;

        bool is_vod_;
    };
}


#endif
