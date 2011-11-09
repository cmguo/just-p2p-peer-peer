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
        TrackerManager(bool is_vod)
            : group_count_(0)
            , is_running_(false)
            , is_vod_(is_vod)
            , is_got_tracker_list_from_bs_(false)
            , load_tracker_list_timer_(global_second_timer(), 60*1000, 
            boost::bind(&TrackerManager::OnLoadTrackerTimer, this, &load_tracker_list_timer_))
        {}

        void Start(const string& config_path);

        void Stop();

        void SetTrackerList(uint32_t group_count, std::vector<protocol::TRACKER_INFO> tracker_s,
            bool is_got_tracker_list_from_bs);

        void DoList(RID rid);

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

        void OnLoadTrackerTimer(framework::timer::Timer::pointer timer);

    private:
        struct TrackerInfoSorterByMod
        {
            bool operator() (const protocol::TRACKER_INFO& a, const protocol::TRACKER_INFO& b)
            {
                return a.ModNo < b.ModNo;
            }
        };
    private:
        /**
        * @brief 根据 RID 的 模值 对 TrackerGroup进行索引
        *    [Key]   模的余数
        *    [Value] TrackerGroup 智能指针
        */
        typedef std::map<int, TrackerGroup::p> ModIndexer;
        ModIndexer mod_indexer_;

        /**
        * @brief 根据 Tracker 的endpoint 对TrackerGroup 进行索引
        *    [Key]   Tracker的enpoint
        *    [Value] TrackerGroup 智能指针
        */
        std::map<boost::asio::ip::udp::endpoint, TrackerGroup::p> endpoint_indexer_;

        /**
        * @brief Group数，也是 RID 模的除数
        */
        boost::uint32_t group_count_;

        volatile bool is_running_;

        string tracker_list_save_path_;

        bool is_vod_;

        bool is_got_tracker_list_from_bs_;

        framework::timer::OnceTimer load_tracker_list_timer_;
    };
}