//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _TRACKER_STATION_H_
#define _TRACKER_STATION_H_

namespace p2sp
{
    class TrackerClient;

    enum TrackerType
    {
        LIST = 0,
        REPORT = 1
    };

    class TrackerStation
    {
    public:
        TrackerStation(bool is_vod, TrackerType tracker_type)
            : is_running_(false)
            , is_vod_(is_vod)
            , tracker_type_(tracker_type)
            , no_report_response_times_(0)
            , is_report_response_(true)
            , report_failed_times_(0)
            , report_timer_(global_second_timer(), 1000 * DEFAULT_INTERVAL_IN_SECONDS_, 
                boost::bind(&TrackerStation::OnTimerElapsed, this, &report_timer_))
        {

        }

        void Start();
        void Stop();

        void StartAllClient();
        void StopAllClient();
        void ClearAllClient();

        std::vector<protocol::TRACKER_INFO> GetTrackers();

        void SetTrackers(boost::uint32_t gropup_count, const std::set<protocol::TRACKER_INFO> & trackers);
        
        void DoList(const RID& rid, bool list_for_live_udpserver);
        void DoReport();
        void DoLeave();

        void OnListResponsePacket(const protocol::ListPacket & packet);
        void OnReportResponsePacket(const protocol::ReportPacket & packet);

        void OnTimerElapsed(framework::timer::Timer * pointer);

    private:
        static const uint32_t DEFAULT_INTERVAL_IN_SECONDS_ = 60;

    private:
        bool is_running_;

        std::map<boost::asio::ip::udp::endpoint, boost::shared_ptr<TrackerClient> > tracker_client_map_;

        std::list<boost::shared_ptr<TrackerClient> > tracker_client_list_;

        std::list<boost::shared_ptr<TrackerClient> >::iterator trying_tracker_iterator_;

        boost::shared_ptr<TrackerClient> current_report_tracker_;
        boost::shared_ptr<TrackerClient> trying_report_tracker_;

        bool is_vod_;

        TrackerType tracker_type_;

        bool is_report_response_;

        boost::uint32_t no_report_response_times_;

        boost::uint32_t last_report_transaction_id_;

        framework::timer::PeriodicTimer report_timer_;

        boost::uint32_t report_failed_times_;
    };
}

#endif