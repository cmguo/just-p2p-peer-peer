//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#pragma once

/**
* @file
* @brief TrackerManager 类的包含文件
*/

#include "p2sp/tracker/TrackerGroup.h"

namespace p2sp
{
    class TrackerManager
        : public boost::noncopyable
#ifdef DUMP_OBJECT
        , public count_object_allocate<TrackerManager>
#endif
    {
    public:
        typedef std::map<int, boost::shared_ptr<TrackerGroup> > ModIndexer;
        typedef std::map<boost::asio::ip::udp::endpoint, boost::shared_ptr<TrackerGroup> > EndpointIndexer;

        TrackerManager(bool is_vod)
            : is_running_(false)
            , is_vod_(is_vod)
            , is_got_tracker_list_from_bs_(false)
        {}

        void Start(const string& config_path);

        void Stop();

        void SetTrackerList(uint32_t group_count, const std::vector<protocol::TRACKER_INFO> & trackers,
            bool is_got_tracker_list_from_bs, TrackerType tracker_type);

        void DoList(RID rid, bool list_for_live_udpserver);

        void OnUdpRecv(protocol::ServerPacket const &packet);

        void PPLeave();

        void DoReport();

    private:

        void OnListResponsePacket(protocol::ListPacket const & packet);

        void OnReportResponsePacket(protocol::ReportPacket const & packet);

        void LoadTrackerList();

        void SaveTrackerList();

        void StopAllGroups();

        void ClearAllGroups();

        void SetTrackerList(uint32_t group_count, const std::vector<protocol::TRACKER_INFO> & trackers,
            bool is_got_tracker_list_from_bs, ModIndexer & mod_indexer, EndpointIndexer & endpoint_indexer,
            TrackerType tracker_type);

    private:
        /**
        * @brief 根据 RID 的 模值 对 TrackerGroup进行索引
        *    [Key]   模的余数
        *    [Value] TrackerGroup 智能指针
        */
        ModIndexer list_mod_indexer_;
        ModIndexer report_mod_indexer_;

        /**
        * @brief 根据 Tracker 的endpoint 对TrackerGroup 进行索引
        *    [Key]   Tracker的enpoint
        *    [Value] TrackerGroup 智能指针
        */
        EndpointIndexer list_endpoint_indexer_;
        EndpointIndexer report_endpoint_indexer_;

        volatile bool is_running_;

        string tracker_list_save_path_;

        bool is_vod_;

        bool is_got_tracker_list_from_bs_;
    };
}