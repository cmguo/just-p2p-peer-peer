//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "p2sp/tracker/TrackerManager.h"
#include <fstream>

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("tracker");

    void TrackerManager::Start(const string& config_path)
    {
        if (is_running_ == true)
        {
            LOG(__WARN, "tracker", "TrackerManager is running...");
            return;
        }

        // config path
        LOGX(__WARN, "tracker", "config_path = " << config_path);
        if (config_path.length() == 0)
        {
            string szPath;
#ifdef DISK_MODE
            if (base::util::GetAppDataPath(szPath))
            {
                tracker_list_save_path_ = szPath;
            }
#endif  // #ifdef DISK_MODE

        }
        else 
        {
            tracker_list_save_path_ = config_path;
        }
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

        group_count_ = 0;

        LOG(__EVENT, "tracker", "Tracker Manager has started successfully.");

        load_tracker_list_timer_.start();

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
        LOGX(__DEBUG, "tracker", "");
        uint32_t save_group_count;
        std::vector<protocol::TRACKER_INFO> save_tracker_info;
        uint32_t info_size = 0;

        try
        {
            std::ifstream ifs(tracker_list_save_path_.c_str(), std::ios_base::in|std::ios_base::binary);
            if (!ifs)
            {
                LOGX(__DEBUG, "tracker", " File Read Error");
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
                assert(ia);

                ia >> ti.Length;  // = framework::io::BytesToUI16(p_buf); p_buf += 2;
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
            LOGX(__DEBUG, "tracker", " File Read Error");
            return;
        }

        SetTrackerList(save_group_count, save_tracker_info, false);
    }

    void TrackerManager::SaveTrackerList()
    {
        LOGX(__DEBUG, "tracker", "");
        uint32_t save_group_count = group_count_;
        std::vector<protocol::TRACKER_INFO> save_tracker_info;

        for (ModIndexer::iterator it = mod_indexer_.begin();
            it != mod_indexer_.end(); ++it)
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
            LOGX(__DEBUG, "tracker", " File Write Error");
        }
    }

    void TrackerManager::SetTrackerList(uint32_t group_count, std::vector<protocol::TRACKER_INFO> tracker_s,
        bool is_got_tracker_list_from_bs)
    {
        // 去掉这个aassert, 连续2次收到BS回的查询Tracker列表的报文就会触发这个assert.
        // assert(!is_got_tracker_list_from_bs_);

        if (is_got_tracker_list_from_bs)
        {
            is_got_tracker_list_from_bs_ = is_got_tracker_list_from_bs;
        }

        // 验证输入
        if (group_count == 0 && tracker_s.empty())
        {
            LOG(__DEBUG, "tracker", "Group Count is zero and Tracker Infos is empty.");
            return;
        }
        else
        {
            uint32_t max_count = 0;
            for (uint32_t i = 0; i < tracker_s.size(); ++i)
            {
                uint32_t mod_num = static_cast<uint32_t>(tracker_s[i].ModNo);
                if (mod_num > max_count)
                    max_count = mod_num;
            }
            LOGX(__DEBUG, "tracker", "max_count = " << max_count);
            // check
            if (max_count + 1 != group_count)
            {
                LOG(__WARN, "tracker", __FUNCTION__ << " Invalid tracker std::list");
                return;
            }
        }
        LOGX(__DEBUG, "tracker", "local_group_count = " << group_count_ << ", set_group_count = " << group_count);

        // 发现group数减少了，删除本地多出的group
        if (group_count < group_count_)
        {
            for (ModIndexer::iterator it = mod_indexer_.begin(), eit = mod_indexer_.end(); it != eit;)
            {
                if ((uint32_t)it->first >= group_count) 
                {
                    (it->second)->Stop();
                    mod_indexer_.erase(it++);
                }
                else 
                {
                    ++it;
                }
            }
        }

        // sort the trackers by their ModNo
        std::sort(tracker_s.begin(), tracker_s.end(), TrackerInfoSorterByMod());

        // 统计信息
        statistic::StatisticModule::Inst()->SetTrackerInfo(group_count, tracker_s);

        uint32_t i, j;
        for (i = 0; i < tracker_s.size(); i = j)
        {
            std::set<protocol::TRACKER_INFO> trackers_in_group;

            uint32_t group_key = tracker_s[i].ModNo;
            for (j = i; j < tracker_s.size() && tracker_s[j].ModNo == group_key; j++)
                trackers_in_group.insert(tracker_s[j]);

            if (group_key < group_count_)
            {
                // group本地已经存在，只需要重新设置Tracker列表
                ModIndexer::iterator it = mod_indexer_.find(group_key);
                assert(it != mod_indexer_.end());
                TrackerGroup::p group = it->second;
                group->SetTrackers(group_count, trackers_in_group);
            }
            else
            {
                // group本地不存在，创建新Group并start
                TrackerGroup::p group = TrackerGroup::Create(is_vod_);
                mod_indexer_[group_key] = group;
                group->SetTrackers(group_count, trackers_in_group);
                group->Start();
            }
        }

        // endpoint
        endpoint_indexer_.clear();
        for (i = 0; i < tracker_s.size(); i++)
        {
            boost::asio::ip::udp::endpoint end_point =
                framework::network::Endpoint(tracker_s[i].IP, tracker_s[i].Port);
            endpoint_indexer_[end_point] = mod_indexer_[tracker_s[i].ModNo];
        }

        group_count_ = group_count;

        if (is_running_ == true)
        {
            LOG(__EVENT, "tracker", "TrackerManager::SetTrackerList    Start All Groups");
            SaveTrackerList();
        }
    }

    void TrackerManager::DoList(RID rid)
    {
        TRACK_INFO("TrackerManager::DoList, RID: " << rid);

        if (is_running_ == false)
        {
            LOG(__WARN, "tracker", "Tracker Manager is not running. Return.");
            return;
        }

        if (group_count_ == 0)
        {
            TRACK_WARN("Tracker List is not std::set. Return.");
            return;
        }

        // 根据 rid % group_count_ 从 mod_indexer_ 中定位出 TrackerGroup,
        // 然后 该 group 做 DoList
        uint32_t group_key = base::util::GuidMod(rid, group_count_);
        
        TrackerGroup::p tracker_group = mod_indexer_[group_key];

        if (tracker_group)
        {
            tracker_group->DoList(rid);
        }
        else
        {
            LOG(__ERROR, "tracker", "mod indexer data inconsistence");
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
                // LOG(__WARN, "tracker", "Ignoring unknown action: 0x" << std::hex << packet_header->GetAction());
            }
            break;
        }
    }

    void TrackerManager::OnReportResponsePacket(protocol::ReportPacket const & packet)
    {
        if (is_running_ == false) return;

        if (endpoint_indexer_.count(packet.end_point) != 0)
        {
            const TrackerGroup::p group = endpoint_indexer_[packet.end_point];  // ! shared_ptr
            group->OnReportResponsePacket(packet);
        }
    }

    void TrackerManager::OnListResponsePacket(protocol::ListPacket const & packet)
    {
        if (is_running_ == false)
        {
            LOG(__DEBUG, "tracker", "Tracker Manager is not running...");
            return;
        }

        // 根据 endpoint_indexer_ 索引 找到对应的 TrackerGroup
        // 然后对这个Group调用 OnCommitResponsePacket(end_point, p packet)
        // 如果找不到Group不管
        boost::asio::ip::udp::endpoint end_point = packet.end_point;
        if (endpoint_indexer_.count(packet.end_point) != 0)
        {
            const TrackerGroup::p group = endpoint_indexer_[packet.end_point];
            group->OnListResponsePacket(packet);
        }
        else
        {
            LOG(__DEBUG, "tracker", "No such end point");
        }
    }

    void TrackerManager::StopAllGroups()
    {
        LOG(__INFO, "tracker", "Stopping all tracker groups.");
        for (ModIndexer::iterator it = mod_indexer_.begin(), eit = mod_indexer_.end(); it != eit; it++)
        {
            it->second->Stop();
        }

        LOG(__EVENT, "tracker", "All tracker groups has been stopped.");
    }

    void TrackerManager::ClearAllGroups()
    {
        StopAllGroups();
        mod_indexer_.clear();
        endpoint_indexer_.clear();
        group_count_ = 0;
    }

    void TrackerManager::PPLeave()
    {
        if (false == is_running_)
        {
            return;
        }

        LOG(__INFO, "tracker", "Leave");

        for (std::map<int, TrackerGroup::p> ::iterator iter = mod_indexer_.begin(); iter != mod_indexer_.end(); iter++)
        {
            iter->second->PPLeave();
        }
    }

    void TrackerManager::OnLoadTrackerTimer(framework::timer::Timer::pointer timer)
    {
        if (!is_got_tracker_list_from_bs_)
        {
            LoadTrackerList();
        }
    }

    void TrackerManager::DoReport()
    {
        for (ModIndexer::iterator it = mod_indexer_.begin(); it != mod_indexer_.end(); ++it)
        {
            it->second->SelectTracker();
        }
    }
}
