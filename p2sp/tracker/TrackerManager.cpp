//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "p2sp/tracker/TrackerManager.h"
#include "p2sp/AppModule.h"
#include <fstream>

namespace p2sp
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_tracker = log4cplus::Logger::getInstance("[tracker_manager]");
#endif

    void TrackerManager::Start(const string& config_path)
    {
        if (is_running_ == true)
        {
            LOG4CPLUS_WARN_LOG(logger_tracker, "TrackerManager is running...");
            return;
        }

        // config path
        LOG4CPLUS_WARN_LOG(logger_tracker, "config_path = " << config_path);
        tracker_list_save_path_ = config_path;

        boost::filesystem::path list_path(tracker_list_save_path_);

        if (is_vod_)
        {
            list_path /= ("pptl");
        }
        else
        {
            list_path /= ("pptl2");
        }
        
        tracker_list_save_path_ = list_path.file_string();

        LOG4CPLUS_INFO_LOG(logger_tracker, "Tracker Manager has started successfully.");

        LoadTrackerList();

        is_running_ = true;
    }

    void TrackerManager::Stop()
    {
        if (is_running_ == false)
        {
            return;
        }

        ClearAllGroups();

        is_running_ = false;
    }

    void TrackerManager::LoadTrackerList()
    {
        LOG4CPLUS_DEBUG_LOG(logger_tracker, "");
        uint32_t save_group_count;
        std::vector<protocol::TRACKER_INFO> save_tracker_info;
        uint32_t info_size = 0;

        try
        {
            std::ifstream ifs(tracker_list_save_path_.c_str());
            if (!ifs)
            {
                LOG4CPLUS_DEBUG_LOG(logger_tracker, " File Read Error");
                return;
            }
            util::archive::BinaryIArchive<>  ia(ifs);

            ia >> save_group_count;
            ia >> info_size;

            // 防止2.0内核和1.5内核pptl文件格式不一致的错误
            // 正常的group_count一般固定为4，超过100即可认为是文件错误
            if (save_group_count > 100)
            {
                return;
            }



            for (uint32_t i = 0; i != info_size; ++i)
            {
                protocol::TRACKER_INFO ti;

                if (!ia)
                {
                    return;
                }

                ia >> ti.StationNo;  // = framework::io::BytesToUI16(p_buf); p_buf += 2;
                ia >> ti.Reserve;
                ia >> ti.ModNo;  // = framework::io::BytesToUI08(p_buf); p_buf += 1;
                ia >> ti.IP;  // = framework::io::BytesToUI32(p_buf); p_buf += 4;
                ia >> ti.Port;  // = framework::io::BytesToUI16(p_buf); p_buf += 2;
                ia >> ti.Type;  // = framework::io::BytesToUI08(p_buf); p_buf += 1;
                save_tracker_info.push_back(ti);
            }
            ifs.close();
        }
        catch(...)
        {
            LOG4CPLUS_DEBUG_LOG(logger_tracker, " File Read Error");
            return;
        }

        SetTrackerList(save_group_count, save_tracker_info, false,
            list_mod_indexer_, list_endpoint_indexer_, p2sp::LIST);
    }

    void TrackerManager::SaveTrackerList()
    {
        LOG4CPLUS_DEBUG_LOG(logger_tracker, "");
        uint32_t save_group_count = list_mod_indexer_.size();
        std::vector<protocol::TRACKER_INFO> save_tracker_info;

        for (ModIndexer::iterator it = list_mod_indexer_.begin();
            it != list_mod_indexer_.end(); ++it)
        {
            std::vector<protocol::TRACKER_INFO> group_tracker_info = it->second->GetTrackers();
            save_tracker_info.insert(save_tracker_info.end(), group_tracker_info.begin(), group_tracker_info.end());
        }

        try
        {
            std::ofstream ifs(tracker_list_save_path_.c_str());
            util::archive::BinaryOArchive<> ia(ifs);
            ia << save_group_count;
            ia << save_tracker_info;
            ifs.close();

        }
        catch(...)
        {
            LOG4CPLUS_DEBUG_LOG(logger_tracker, " File Write Error");
        }
    }

    void TrackerManager::SetTrackerList(uint32_t group_count, const std::vector<protocol::TRACKER_INFO> & trackers,
        bool is_got_tracker_list_from_bs, TrackerType tracker_type)
    {
        if (tracker_type == p2sp::LIST)
        {
            SetTrackerList(group_count, trackers, is_got_tracker_list_from_bs, list_mod_indexer_,
                list_endpoint_indexer_, tracker_type);
        }
        else
        {
            assert(tracker_type == p2sp::REPORT);
            SetTrackerList(group_count, trackers, is_got_tracker_list_from_bs, report_mod_indexer_,
                report_endpoint_indexer_, tracker_type);
        }
    }

    void TrackerManager::SetTrackerList(uint32_t group_count, const std::vector<protocol::TRACKER_INFO> & trackers,
        bool is_got_tracker_list_from_bs, ModIndexer & mod_indexer, EndpointIndexer & endpoint_indexer,
        TrackerType tracker_type)
    {
        if (is_got_tracker_list_from_bs)
        {
            is_got_tracker_list_from_bs_ = is_got_tracker_list_from_bs;
        }

        if (trackers.empty()) 
        {
            mod_indexer.clear();
            endpoint_indexer.clear();
            return;
        }

        std::map<boost::uint32_t, std::set<protocol::TRACKER_INFO> > mod_map;

        for (std::vector<protocol::TRACKER_INFO>::const_iterator iter = trackers.begin();
            iter != trackers.end(); ++iter)
        {
            // 只有当直播 && REPORT类型 && 返回的Tracker用于保存UDP SERVER的Tracker
            // 不加入列表
            if (is_vod_ || tracker_type == p2sp::LIST || !iter->IsTrackerForLiveUdpServer())
            {
                mod_map[iter->ModNo].insert(*iter);
            }
        }

        for (ModIndexer::iterator iter = mod_indexer.begin(); iter != mod_indexer.end(); )
        {
            if (mod_map.find(iter->first) == mod_map.end())
            {
                mod_indexer.erase(iter++);
            }
            else
            {
                ++iter;
            }
        }

        for (std::map<boost::uint32_t, std::set<protocol::TRACKER_INFO> >::iterator
            iter = mod_map.begin(); iter != mod_map.end(); ++iter)
        {
            if (mod_indexer.find(iter->first) == mod_indexer.end())
            {
                mod_indexer.insert(std::make_pair(iter->first,
                    boost::shared_ptr<TrackerGroup>(new TrackerGroup(is_vod_, tracker_type))));
            }

            mod_indexer[iter->first]->SetTrackers(group_count, iter->second);
            mod_indexer[iter->first]->Start();
        }

        endpoint_indexer.clear();

        for (std::vector<protocol::TRACKER_INFO>::const_iterator iter = trackers.begin();
            iter != trackers.end(); ++iter)
        {
            boost::asio::ip::udp::endpoint endpoint = framework::network::Endpoint(iter->IP,
                iter->Port);

            if (is_vod_ || tracker_type == p2sp::LIST || !iter->IsTrackerForLiveUdpServer())
            {
                endpoint_indexer[endpoint] = mod_indexer[iter->ModNo];
            }
        }

        if (is_vod_ && tracker_type == p2sp::REPORT)
        {
            DoReport();
        }

        statistic::StatisticModule::Inst()->SetTrackerInfo(group_count, trackers);

        if (is_running_ == true && tracker_type == p2sp::LIST)
        {
            LOG4CPLUS_INFO_LOG(logger_tracker, "TrackerManager::SetTrackerList    Start All Groups");
            SaveTrackerList();
        }
    }

    void TrackerManager::DoList(RID rid, bool list_for_live_udpserver)
    {
        if (is_running_ == false)
        {
            return;
        }

        if (list_mod_indexer_.size() == 0)
        {
            return;
        }

        // 根据 rid % group_count_ 从 mod_indexer_ 中定位出 TrackerGroup,
        // 然后 该 group 做 DoList
        uint32_t group_key = base::util::GuidMod(rid, list_mod_indexer_.size());
        
        boost::shared_ptr<TrackerGroup> tracker_group = list_mod_indexer_[group_key];

        if (tracker_group)
        {
            tracker_group->DoList(rid, list_for_live_udpserver);
        }
        else
        {
            assert(0);
        }
    }

    void TrackerManager::OnUdpRecv(protocol::ServerPacket const &packet)
    {
        if (is_running_ == false) return;

        switch (packet.PacketAction)
        {
        case protocol::ListPacket::Action:
            OnListResponsePacket((protocol::ListPacket const &)packet);
            break;
        case protocol::ReportPacket::Action:
            OnReportResponsePacket((protocol::ReportPacket const &)packet);
            break;
        default:
            {
            }
            break;
        }
    }

    void TrackerManager::OnReportResponsePacket(protocol::ReportPacket const & packet)
    {
        if (is_running_ == false) return;

        if (report_endpoint_indexer_.count(packet.end_point) != 0)
        {
            boost::shared_ptr<TrackerGroup> group = report_endpoint_indexer_[packet.end_point];
            group->OnReportResponsePacket(packet);
        }
    }

    void TrackerManager::OnListResponsePacket(protocol::ListPacket const & packet)
    {
        if (is_running_ == false)
        {
            LOG4CPLUS_DEBUG_LOG(logger_tracker, "Tracker Manager is not running...");
            return;
        }

        // 根据 endpoint_indexer_ 索引 找到对应的 TrackerGroup
        // 然后对这个Group调用 OnCommitResponsePacket(end_point, p packet)
        // 如果找不到Group不管
        boost::asio::ip::udp::endpoint end_point = packet.end_point;
        if (list_endpoint_indexer_.count(packet.end_point) != 0)
        {
            boost::shared_ptr<TrackerGroup> group = list_endpoint_indexer_[packet.end_point];
            group->OnListResponsePacket(packet);
        }
        else
        {
            LOG4CPLUS_DEBUG_LOG(logger_tracker, "No such end point");
        }
    }

    void TrackerManager::StopAllGroups()
    {
        LOG4CPLUS_INFO_LOG(logger_tracker, "Stopping all tracker groups.");
        for (ModIndexer::iterator it = list_mod_indexer_.begin(); it != list_mod_indexer_.end(); ++it)
        {
            it->second->Stop();
        }

        for (EndpointIndexer::iterator it = list_endpoint_indexer_.begin(); it != list_endpoint_indexer_.end(); ++it)
        {
            it->second->Stop();
        }

        for (ModIndexer::iterator it = report_mod_indexer_.begin(); it != report_mod_indexer_.end(); ++it)
        {
            it->second->Stop();
        }

        for (EndpointIndexer::iterator it = report_endpoint_indexer_.begin(); it != report_endpoint_indexer_.end(); ++it)
        {
            it->second->Stop();
        }

        LOG4CPLUS_INFO_LOG(logger_tracker, "All tracker groups has been stopped.");
    }

    void TrackerManager::ClearAllGroups()
    {
        StopAllGroups();
        list_endpoint_indexer_.clear();
        list_mod_indexer_.clear();

        report_endpoint_indexer_.clear();
        report_mod_indexer_.clear();
    }

    void TrackerManager::PPLeave()
    {
        if (false == is_running_)
        {
            return;
        }

        LOG4CPLUS_INFO_LOG(logger_tracker, "Leave");

        for (std::map<int, boost::shared_ptr<TrackerGroup> > ::iterator iter = report_mod_indexer_.begin();
            iter != report_mod_indexer_.end(); ++iter)
        {
            iter->second->DoLeave();
        }
    }

    void TrackerManager::DoReport()
    {
        for (ModIndexer::iterator it = report_mod_indexer_.begin();
            it != report_mod_indexer_.end(); ++it)
        {
            it->second->DoReport();
        }
    }

    void TrackerManager::DeleteRidRecord(const RID & rid)
    {
        uint32_t group_key = base::util::GuidMod(rid, list_mod_indexer_.size());

        boost::shared_ptr<TrackerGroup> tracker_group = list_mod_indexer_[group_key];

        if (tracker_group)
        {
            tracker_group->DeleteRidRecord(rid);
        }
    }
}
