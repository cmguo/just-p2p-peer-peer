//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "statistic/StatisticModule.h"
#include "statistic/DownloadDriverStatistic.h"
#include "statistic/P2PDownloaderStatistic.h"
#include "statistic/LiveDownloadDriverStatistic.h"
#include "statistic/StatisticUtil.h"
#include "storage/Storage.h"
#include "statistic/DACStatisticModule.h"
#include "statistic/UploadStatisticModule.h"
#include "p2sp/p2p/P2PModule.h"

#include <util/archive/LittleEndianBinaryOArchive.h>
#include <util/archive/ArchiveBuffer.h>

namespace statistic
{
    StatisticModule::p StatisticModule::inst_;
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_statistic = log4cplus::Logger::getInstance("[statistic_module]");
#endif

    StatisticModule::StatisticModule()
        : share_memory_timer_(
            global_second_timer(),
            1000,
            boost::bind(&StatisticModule::OnTimerElapsed, this, &share_memory_timer_))
        , is_running_(false)
        , duration_online_time_in_second_(0)
        , time_push_stamp_(time(NULL))
    {
    }

    void StatisticModule::Start(uint32_t flush_interval_in_seconds, string config_path)
    {
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "StatisticModule::Start [IN]");
        LOG4CPLUS_INFO_LOG(logger_statistic, "StatisticModule is starting.");

        if (is_running_ == true)
        {
            LOG4CPLUS_WARN_LOG(logger_statistic, "StatisticModule::Start when module is running.");
            return;
        }

        is_running_ = true;

        // config path
        ppva_config_path_ = config_path;

        LoadBandwidth();

        Clear();

        statistic_info_.LocalPeerInfo.PeerVersion = protocol::PEER_VERSION;

        if (CreateSharedMemory() == false)
        {
            LOG4CPLUS_ERROR_LOG(logger_statistic, "Create Shared Memory Failed");
        }

        speed_info_.Start();
        upload_speed_meter_.Start();

        share_memory_timer_.interval(flush_interval_in_seconds * 1000);
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "framework::timer::Timer Created, Interval: " << 
            flush_interval_in_seconds << "(s)");

        share_memory_timer_.start();

        LOG4CPLUS_INFO_LOG(logger_statistic, "StatisticModule starts successfully.");
    }

    void StatisticModule::Stop()
    {
        LOG4CPLUS_INFO_LOG(logger_statistic, "StatisticModule::Stop [IN]");

        if (is_running_ == false)
        {
            LOG4CPLUS_WARN_LOG(logger_statistic, "StatisticModule::Stop when module is not running.");
            return;
        }

        SaveBandwidth();

        share_memory_timer_.stop();
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "framework::timer::Timer stopped.");

        upload_speed_meter_.Stop();

        Clear();
        shared_memory_.Close();

        speed_info_.Stop();

        is_running_ = false;
        inst_.reset();

        LOG4CPLUS_INFO_LOG(logger_statistic, "StatisticModule is stopped successfully.");
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "StatisticModule::Stop [OUT]");
    }

    LiveDownloadDriverStatistic::p StatisticModule::AttachLiveDownloadDriverStatistic(uint32_t id)
    {
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "StatisticModule::AttachLiveDownloadDriverStatistic [IN]");

        assert(is_running_ == true);

        if (live_download_driver_statistic_map_.find(id) != live_download_driver_statistic_map_.end())
        {
            LOG4CPLUS_WARN_LOG(logger_statistic, "ID " << id << " exists, return null.");
            if (live_download_driver_statistic_map_[id])
            {
                // 不为空，返回
                return live_download_driver_statistic_map_[id];
            }
        }

        LiveDownloadDriverStatistic::p live_download_driver_statistics = LiveDownloadDriverStatistic::Create(id);
        live_download_driver_statistic_map_[id] = live_download_driver_statistics;

        AddLiveDownloadDriverID(id);

        LOG4CPLUS_DEBUG_LOG(logger_statistic, "StatisticModule::AttachLiveDownloadDriverStatistic [OUT]");

        return live_download_driver_statistics;
    }

    DownloadDriverStatistic::p StatisticModule::AttachDownloadDriverStatistic(
        uint32_t id,
        bool is_create_shared_memory)
    {
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "StatisticModule::AttachDownloadDriverStatistic [IN]");

        assert(is_running_ == true);
        
        // Modified by jeffrey 2010.2.21
        // 已经存在, 直接返回
        // 如果这里出问题，最多造成的后果是2个downloaddriver写同一个statistic，不会挂
        if (download_driver_statistic_map_.find(id) != download_driver_statistic_map_.end())
        {
            LOG4CPLUS_WARN_LOG(logger_statistic, "ID " << id << " exists, return null.");
            if (download_driver_statistic_map_[id])
            {
                // 不为空，返回
                return download_driver_statistic_map_[id];
            }
            else
            {
                // 为空，首先删除
                download_driver_statistic_map_.erase(id);
            }
        }

        // Modified by jeffrey 2010.2.21
        // 判断最大个数
        // 如果这里出问题，最多造成的后果是内存增长，不会挂
        if (statistic_info_.DownloadDriverCount == UINT8_MAX_VALUE - 1)
        {
            LOG4CPLUS_WARN_LOG(logger_statistic, "Download Driver Map is Full, size: " << 
                statistic_info_.DownloadDriverCount << ". Return null");
            assert(false);
        }

        // 新建一个 DownloadDriverStatistic, 然后添加到 download_driver_statistic_s_ 中
        // 返回这个新建的 DownloadDriverStatistic
        DownloadDriverStatistic::p download_driver = DownloadDriverStatistic::Create(id, is_create_shared_memory);
        download_driver_statistic_map_[id] = download_driver;

        // start it
        download_driver->Start();

        LOG4CPLUS_DEBUG_LOG(logger_statistic, "StatisticModule::AttachDownloadDriverStatistic [OUT]");

        if (is_create_shared_memory)
        {
            // 将ID加入数组, 线性开地址
            if (AddDownloadDriverID(id) == true)
            {

            }
        }

        return download_driver;
    }

    bool StatisticModule::DetachDownloadDriverStatistic(int id)
    {
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "StatisticModule::DetachDownloadDriverStatistic [IN]"); 
        LOG4CPLUS_INFO_LOG(logger_statistic,"StatisticModule is detaching download driver with id " << id);

        if (is_running_ == false)
        {
            LOG4CPLUS_WARN_LOG(logger_statistic, "StatisticModule is not running, return false.");
            return false;
        }

        // 如果 不存该 DownloadDriverStatistic  返回false
        if (download_driver_statistic_map_.find(id) == download_driver_statistic_map_.end())
        {
            LOG4CPLUS_WARN_LOG(logger_statistic, "Download Driver with id " << id << " does not exist.");
            return false;
        }

        DownloadDriverStatistic::p download_driver = download_driver_statistic_map_[id];
        if (download_driver->IsFlushSharedMemory())
        {
            // 将ID移出数组
            RemoveDownloadDriverID(id);
        }

        // 停止该 DownloadDriverStatistic
        download_driver->Stop();
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "Download Driver " << id << " has been stopped.");

        // 删除该 DownloadDriverStatistic
        download_driver_statistic_map_.erase(id);
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "Download Driver " << id << " has been removed from statistic std::map.");

        LOG4CPLUS_DEBUG_LOG(logger_statistic, "StatisticModule::DetachDownloadDriverStatistic [OUT]");

        return true;
    }

    bool StatisticModule::DetachDownloadDriverStatistic(const DownloadDriverStatistic::p download_driver_statistic)
    {
        return DetachDownloadDriverStatistic(download_driver_statistic->GetDownloadDriverID());
    }

    bool StatisticModule::DetachLiveDownloadDriverStatistic(const LiveDownloadDriverStatistic::p live_download_driver_statistic)
    {
        uint32_t id = live_download_driver_statistic->GetLiveDownloadDriverId();

        LOG4CPLUS_DEBUG_LOG(logger_statistic, "StatisticModule::DetachLiveDownloadDriverStatistic [IN]");
        LOG4CPLUS_INFO_LOG(logger_statistic,"StatisticModule is detaching live download driver with id " << id);

        if (is_running_ == false)
        {
            LOG4CPLUS_WARN_LOG(logger_statistic, "StatisticModule is not running, return false.");
            return false;
        }

        if (live_download_driver_statistic_map_.find(id) == live_download_driver_statistic_map_.end())
        {
            LOG4CPLUS_WARN_LOG(logger_statistic, "Live Download Driver with id " << id << " does not exist.");
            return false;
        }

        RemoveLiveDownloadDriverID(id);
        
        // 删除该 DownloadDriverStatistic
        live_download_driver_statistic_map_.erase(id);
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "Live Download Driver " << id << 
            " has been removed from statistic std::map.");

        LOG4CPLUS_DEBUG_LOG(logger_statistic, "StatisticModule::DetachLiveDownloadDriverStatistic [OUT]");

        return true;
    }

    bool StatisticModule::DetachAllDownloadDriverStatistic()
    {
        for (DownloadDriverStatisticMap::iterator it = download_driver_statistic_map_.begin(); it
            != download_driver_statistic_map_.end(); it++)
        {
            RemoveDownloadDriverID(it->first);
            it->second->Stop();
        }
        // clear
        download_driver_statistic_map_.clear();

        return true;
    }

    void StatisticModule::DetachAllLiveDownloadDriverStatistic()
    {
        for (LiveDownloadDriverStatisticMap::iterator it = live_download_driver_statistic_map_.begin(); 
            it != live_download_driver_statistic_map_.end(); 
            it++)
        {
            RemoveLiveDownloadDriverID(it->first);
            it->second->Stop();
        }

        live_download_driver_statistic_map_.clear();
    }

    P2PDownloaderStatistic::p StatisticModule::AttachP2PDownloaderStatistic(const RID& rid)
    {
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "StatisticModule::AttachP2PDownloaderStatistic [IN]"); 
        LOG4CPLUS_INFO_LOG(logger_statistic,"StatisticModule is attaching p2p downloader with RID " << rid);
        P2PDownloaderStatistic::p p2p_downloader_ = P2PDownloaderStatistic::Create(rid);

        if (is_running_ == false)
        {
            assert(false);
            LOG4CPLUS_WARN_LOG(logger_statistic, "StatisticModule is not running, return null.");
            return p2p_downloader_;
        }

        // 如果已经存在 则返回 空
        if (p2p_downloader_statistic_map_.find(rid) != p2p_downloader_statistic_map_.end())
        {
            LOG4CPLUS_WARN_LOG(logger_statistic, "p2p downloader " << rid << " exists. Return null.");
            return p2p_downloader_statistic_map_[rid];
        }

        // 判断最大个数
        if (statistic_info_.P2PDownloaderCount == UINT8_MAX_VALUE - 1)
        {
            assert(false);
            LOG4CPLUS_WARN_LOG(logger_statistic, "p2p downloader std::map is full. size: " << 
                statistic_info_.P2PDownloaderCount << ". Return null.");
            return p2p_downloader_;
        }

        // 将RID加入数组, 线性开地址
        AddP2PDownloaderRID(rid);

        // 新建一个 P2PDownloaderStatistic::p, 然后添加到 download_driver_statistic_s_ 中
        p2p_downloader_statistic_map_[rid] = p2p_downloader_;

        LOG4CPLUS_DEBUG_LOG(logger_statistic, "Created P2P Downloader with RID: " << rid);

        // 该 P2PDownloadeStatisticr::p -> Start()
        p2p_downloader_->Start();

        LOG4CPLUS_DEBUG_LOG(logger_statistic, "Started P2P Downloader: " << rid);

        // 返回这个新建的 P2PDownloaderStatistic::p
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "StatisticModule::AttachP2PDownloaderStatistic [OUT]");
        return p2p_downloader_;
    }

    bool StatisticModule::DetachP2PDownloaderStatistic(const RID& rid)
    {
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "StatisticModule::DetachP2PDownloaderStatistic [IN]"); 
        LOG4CPLUS_INFO_LOG(logger_statistic,"StatisticModule is detaching P2PDownloader: " << rid);

        if (is_running_ == false)
        {
            LOG4CPLUS_WARN_LOG(logger_statistic, "StatisticModule is not running, return false.");
            return false;
        }

        P2PDownloadDriverStatisticMap::iterator it = p2p_downloader_statistic_map_.find(rid);

        // 不存在, 返回
        if (it == p2p_downloader_statistic_map_.end())
        {
            LOG4CPLUS_WARN_LOG(logger_statistic, "P2PDownloader does not exist. Return false.");
            return false;
        }

        assert(it->second);

        it->second->Stop();
        p2p_downloader_statistic_map_.erase(it);
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "P2PDownloader " << rid << " has been stopped and removed.");
        RemoveP2PDownloaderRID(rid);

        LOG4CPLUS_DEBUG_LOG(logger_statistic, "StatisticModule::DetachP2PDownloaderStatistic [OUT]");
        return true;
    }

    bool StatisticModule::DetachP2PDownloaderStatistic(const P2PDownloaderStatistic::p p2p_downloader_statistic)
    {
        return DetachP2PDownloaderStatistic(p2p_downloader_statistic->GetResourceID());
    }

    bool StatisticModule::DetachAllP2PDownaloaderStatistic()
    {
        for (P2PDownloadDriverStatisticMap::iterator it = p2p_downloader_statistic_map_.begin(); it
            != p2p_downloader_statistic_map_.end(); it++)
        {
            RemoveP2PDownloaderRID(it->first);
            it->second->Stop();
        }

        // clear
        p2p_downloader_statistic_map_.clear();

        return true;
    }

    void StatisticModule::OnTimerElapsed(
        framework::timer::Timer * pointer)
    {
        LOG4CPLUS_INFO_LOG(logger_statistic, "StatisticModule::OnTimerElapsed, times: " << pointer->times());

        if (is_running_ == false)
        {
            LOG4CPLUS_WARN_LOG(logger_statistic, "StatisticModule is not running, return.");
            return;
        }

        if (pointer == &share_memory_timer_)
        {
            OnShareMemoryTimer(pointer->times());
            SubmitDurationOnline();
        }
        else
        {
            LOG4CPLUS_ERROR_LOG(logger_statistic, "Invalid framework::timer::Timer Pointer.");
            assert(0);
        }
    }

    void StatisticModule::TakeSnapshot(STASTISTIC_INFO& statistics, 
        std::vector<DOWNLOADDRIVER_STATISTIC_INFO>& download_drivers_statistics, 
        std::vector<LIVE_DOWNLOADDRIVER_STATISTIC_INFO>& live_download_drivers_statistics,
        std::vector<P2PDOWNLOADER_STATISTIC_INFO>& p2p_downloaders_statistics)
    {
        DoTakeSnapshot();

        statistics = statistic_info_;

        download_drivers_statistics.clear();
        download_drivers_statistics.reserve(download_driver_statistic_map_.size());
        for (DownloadDriverStatisticMap::iterator it = download_driver_statistic_map_.begin(); 
            it != download_driver_statistic_map_.end(); 
            it++)
        {
            assert(it->second);
            download_drivers_statistics.push_back(it->second->TakeSnapshot());
        }

        live_download_drivers_statistics.clear();
        live_download_drivers_statistics.reserve(live_download_driver_statistic_map_.size());
        for (LiveDownloadDriverStatisticMap::iterator it = live_download_driver_statistic_map_.begin(); 
            it != live_download_driver_statistic_map_.end(); 
            it++)
        {
            assert(it->second);
            live_download_drivers_statistics.push_back(it->second->TakeSnapshot());
        }

        p2p_downloaders_statistics.clear();
        p2p_downloaders_statistics.reserve(p2p_downloader_statistic_map_.size());
        for (P2PDownloadDriverStatisticMap::iterator it = p2p_downloader_statistic_map_.begin(); 
            it != p2p_downloader_statistic_map_.end(); 
            it++)
        {
            assert(it->second);
            p2p_downloaders_statistics.push_back(it->second->TakeSnapshot());
        }
    }

    void StatisticModule::DoTakeSnapshot()
    {
        UpdateSpeedInfo();
        UpdateTrackerInfo();
        UpdateMaxHttpDownloadSpeed();
        UpdateBandWidth();
    }

    void StatisticModule::OnShareMemoryTimer(uint32_t times)
    {
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "StatisticModule::OnShareMemoryTimer [IN], times: " << times); 
        LOG4CPLUS_INFO_LOG(logger_statistic,"StatisticModule::OnShareMemoryTimer, Writing data into shared memory.");

        if (is_running_ == false)
        {
            LOG4CPLUS_WARN_LOG(logger_statistic, "StatisticModule is not running, return.");
            return;
        }

        // 将内部变量拷贝到共享内存中
        DoTakeSnapshot();

        LOG4CPLUS_INFO_LOG(logger_statistic, "Updated Statistic Information has been written to shared memory.");
        FlushSharedMemory();

        uint32_t upload_speed = (uint32_t) (statistic_info_.SpeedInfo.MinuteUploadSpeed / 1024.0 + 0.5);
        DACStatisticModule::Inst()->SubmitP2PUploadSpeedInKBps(upload_speed);

        UploadStatisticModule::Inst()->OnShareMemoryTimer(times);

        // 遍历所有的  DownloadDriverStatistic::p->OnShareMemoryTimer(times)
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "Starting to update downloader driver statistic.");
        for (DownloadDriverStatisticMap::iterator it = download_driver_statistic_map_.begin(); it
            != download_driver_statistic_map_.end(); it++)
        {
            assert(it->second);
            it->second->OnShareMemoryTimer(times);
        } LOG4CPLUS_INFO_LOG(logger_statistic, "All Downloader Drirvers shared memory has been updated.");

        // 遍历所有的  P2PDownloaderStatistic::p->OnShareMemoryTimer(times)
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "Starting to update p2p downloader statistic.");
        for (P2PDownloadDriverStatisticMap::iterator it = p2p_downloader_statistic_map_.begin(); it
            != p2p_downloader_statistic_map_.end(); it++)
        {
            assert(it->second);
            it->second->OnShareMemoryTimer(times);
        } LOG4CPLUS_INFO_LOG(logger_statistic, "All P2PDownloader shared memory has been updated.");

        // flush bandwidth info
        if (times % (10 * 60) == 0)  // 10mins
        {
            SaveBandwidth();
        }

        LOG4CPLUS_DEBUG_LOG(logger_statistic, "StatisticModule::OnShareMemoryTimer [OUT]");
    }

    STASTISTIC_INFO StatisticModule::GetStatisticInfo()
    {
        UpdateSpeedInfo();
        return statistic_info_;
    }

    void StatisticModule::Clear()
    {
        // Maps
        DetachAllP2PDownaloaderStatistic();
        DetachAllDownloadDriverStatistic();
        DetachAllLiveDownloadDriverStatistic();
        statistic_tracker_info_map_.clear();

        // Statistic Info
        statistic_info_.Clear();

        // SpeedInfo
        speed_info_.Clear();
    }

    string StatisticModule::GetSharedMemoryName()
    {
        return CreateStatisticModuleSharedMemoryName(GetCurrentProcessID());
    }

    inline uint32_t StatisticModule::GetSharedMemorySize()
    {
        return sizeof(STASTISTIC_INFO);
    }

    bool StatisticModule::CreateSharedMemory()
    {
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "StatisticModule::CreateSharedMemory Creating Shared Memory.");
        shared_memory_.Close();
        shared_memory_.Create(GetSharedMemoryName(), GetSharedMemorySize());
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "Created Shared Memory: " << 
            ((const char*) GetSharedMemoryName().c_str()) << ", size: " << GetSharedMemorySize() << " Byte(s).");

        return shared_memory_.IsValid();
    }

    void StatisticModule::FlushSharedMemory()
    {
        if (false == is_running_)
        {
            return;
        }
        if (NULL != shared_memory_.GetView())
        {
            util::archive::ArchiveBuffer<> buf((char*)shared_memory_.GetView(), sizeof(STASTISTIC_INFO));
            util::archive::LittleEndianBinaryOArchive<> oa(buf);
            oa << statistic_info_;
            // memcpy(shared_memory_.GetView(), &statistic_info_, sizeof(STASTISTIC_INFO));
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Speed Info

    void StatisticModule::SubmitDownloadedBytes(uint32_t downloaded_bytes)
    {
        speed_info_.SubmitDownloadedBytes(downloaded_bytes);
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "StatisticModule::SubmitDownloadedBytes added: " << downloaded_bytes);
    }

    void StatisticModule::SubmitUploadedBytes(uint32_t uploaded_bytes)
    {
        speed_info_.SubmitUploadedBytes(uploaded_bytes);
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "StatisticModule::SubmitUploadedBytes added: " << uploaded_bytes);
    }

    SPEED_INFO StatisticModule::GetSpeedInfo()
    {
        UpdateSpeedInfo();
        return statistic_info_.SpeedInfo;
    }

    SPEED_INFO_EX StatisticModule::GetSpeedInfoEx()
    {
        return speed_info_.GetSpeedInfoEx();
    }

    uint32_t StatisticModule::GetMaxHttpDownloadSpeed() const
    {
        return statistic_info_.MaxHttpDownloadSpeed;
    }

    uint32_t StatisticModule::GetTotalDownloadSpeed()
    {
        uint32_t total_speed = GetSpeedInfo().NowDownloadSpeed;
        for (DownloadDriverStatisticMap::const_iterator it = download_driver_statistic_map_.begin(); it
            != download_driver_statistic_map_.end(); ++it)
        {
            DownloadDriverStatistic::p download_driver = it->second;
            if (download_driver)
            {
                total_speed += download_driver->GetSpeedInfo().NowDownloadSpeed;
            }
        }

        for (P2PDownloadDriverStatisticMap::const_iterator it = p2p_downloader_statistic_map_.begin(); it
            != p2p_downloader_statistic_map_.end(); ++it)
        {
            P2PDownloaderStatistic::p p2p_download = it->second;
            if (p2p_download)
            {
                total_speed += p2p_download->GetSpeedInfo().NowDownloadSpeed;
            }
        }
        return total_speed;
    }

    uint32_t StatisticModule::GetBandWidth()
    {
        return (std::max)(statistic_info_.BandWidth, history_bandwith_);
    }

    int StatisticModule::GetBandWidthInKBps()
    {
        return GetBandWidth() / 1024;
    }

    //////////////////////////////////////////////////////////////////////////
    // Local Download Info

    protocol::PEER_DOWNLOAD_INFO StatisticModule::GetLocalPeerDownloadInfo()
    {
        protocol::PEER_DOWNLOAD_INFO local_download_info;

        if (is_running_ == false)
        {
            LOG4CPLUS_WARN_LOG(logger_statistic, 
                "StatisticModule::GetLocalPeerDownloadInfo Statistic is not running. Return. ");
            return local_download_info;
        }

        UpdateSpeedInfo();

        local_download_info.OnlineTime = speed_info_.GetElapsedTimeInMilliSeconds();
        local_download_info.AvgDownload = statistic_info_.SpeedInfo.AvgDownloadSpeed;
        local_download_info.AvgUpload = p2sp::P2PModule::Inst()->GetUploadSpeedLimitInKBps();
        local_download_info.NowDownload = statistic_info_.SpeedInfo.NowDownloadSpeed;
        local_download_info.NowUpload = GetUploadDataSpeedInKBps();
        local_download_info.IsDownloading = true;

        return local_download_info;
    }

    protocol::PEER_DOWNLOAD_INFO StatisticModule::GetLocalPeerDownloadInfo(const RID& rid)
    {
        protocol::PEER_DOWNLOAD_INFO peer_download_info;
        if (is_running_ == false)
        {
            return peer_download_info;
        }

        P2PDownloadDriverStatisticMap::iterator it = p2p_downloader_statistic_map_.find(rid);
        if (it == p2p_downloader_statistic_map_.end())
        {
            peer_download_info.IsDownloading = false;
            peer_download_info.AvgDownload = 0;
            peer_download_info.AvgUpload = p2sp::P2PModule::Inst()->GetUploadSpeedLimitInKBps();
            peer_download_info.NowDownload = 0;
            peer_download_info.NowUpload = GetUploadDataSpeed();
            peer_download_info.OnlineTime = 0;
            return peer_download_info;
        }

        SPEED_INFO info = it->second->GetSpeedInfo();
        peer_download_info.IsDownloading = true;
        peer_download_info.AvgDownload = info.AvgDownloadSpeed;
        peer_download_info.AvgUpload = p2sp::P2PModule::Inst()->GetUploadSpeedLimitInKBps();
        peer_download_info.NowDownload = info.NowDownloadSpeed;
        peer_download_info.NowUpload = GetUploadDataSpeed();
        peer_download_info.OnlineTime = it->second->GetElapsedTimeInMilliSeconds();

        return peer_download_info;
    }

    //////////////////////////////////////////////////////////////////////////
    // Updates

    void StatisticModule::UpdateSpeedInfo()
    {
        statistic_info_.SpeedInfo = speed_info_.GetSpeedInfo();
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "StatisticModule::UpdateSpeedInfo Speed Info has been updated.");
    }

    void StatisticModule::UpdateTrackerInfo()
    {
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "StatisticModule::UpdateTrackerInfo [IN]");

        statistic_info_.TrackerCount = statistic_tracker_info_map_.size();
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "Current Tracker Number: " << (uint32_t)statistic_info_.TrackerCount);

        assert(statistic_info_.TrackerCount <= UINT8_MAX_VALUE - 1);

        StatisticTrackerInfoMap::iterator it = statistic_tracker_info_map_.begin();
        for (uint32_t i = 0; it != statistic_tracker_info_map_.end(); it++, i++)
        {
            statistic_info_.TrackerInfos[i] = it->second;
        } LOG4CPLUS_DEBUG_LOG(logger_statistic, "StatisticModule::UpdateTrackerInfo [OUT]");
    }

    void StatisticModule::UpdateMaxHttpDownloadSpeed()
    {
        uint32_t max_http_download_speed = 0;
        STL_FOR_EACH_CONST(DownloadDriverStatisticMap, download_driver_statistic_map_, iter)
        {
            DownloadDriverStatistic::p dd_statistic_ = iter->second;
            if (dd_statistic_)
            {
                uint32_t http_speed = dd_statistic_->GetHttpDownloadMaxSpeed();
                if (max_http_download_speed < http_speed)
                    max_http_download_speed = http_speed;
            }
        }
        statistic_info_.MaxHttpDownloadSpeed = max_http_download_speed;
    }

    void StatisticModule::UpdateBandWidth()
    {
        uint32_t total_download_speed = GetTotalDownloadSpeed();  // max(0, storage::Performance::Inst()->GetDownloadKBps())*1024;
        if (total_download_speed > statistic_info_.BandWidth)
        {
            statistic_info_.BandWidth = total_download_speed;
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // IP Info

    void StatisticModule::SetLocalPeerInfo(const protocol::CandidatePeerInfo& local_peer_info)
    {
        statistic_info_.LocalPeerInfo = local_peer_info;
    }

    protocol::CandidatePeerInfo StatisticModule::GetLocalPeerInfo()
    {
        return protocol::CandidatePeerInfo(statistic_info_.LocalPeerInfo);
    }

    void StatisticModule::SetLocalPeerAddress(const protocol::PeerAddr& peer_addr)
    {
        statistic_info_.LocalPeerInfo.IP = peer_addr.IP;
        statistic_info_.LocalPeerInfo.UdpPort = peer_addr.UdpPort;
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "StatisticModule::SetLocalPeerAddress [" 
            << protocol::PeerAddr(peer_addr) << "].");
    }

    void StatisticModule::SetLocalPeerIp(uint32_t ip)
    {
        statistic_info_.LocalPeerInfo.IP = ip;
    }

    void StatisticModule::SetLocalPeerUdpPort(boost::uint16_t udp_port)
    {
        statistic_info_.LocalPeerInfo.UdpPort = udp_port;
    }

    void StatisticModule::SetLocalPeerTcpPort(boost::uint16_t tcp_port)
    {
        tcp_port_ = tcp_port;
    }

    boost::uint16_t StatisticModule::GetLocalPeerTcpPort()
    {
        return tcp_port_;
    }

    protocol::PeerAddr StatisticModule::GetLocalPeerAddress()
    {
        return protocol::PeerAddr(statistic_info_.LocalPeerInfo.IP, statistic_info_.LocalPeerInfo.UdpPort);
    }

    void StatisticModule::SetLocalDetectSocketAddress(const protocol::SocketAddr& socket_addr)
    {
        statistic_info_.LocalPeerInfo.DetectIP = socket_addr.IP;
        statistic_info_.LocalPeerInfo.DetectUdpPort = socket_addr.Port;
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "StatisticModule::SetLocalDetectSocketAddress [" 
            << protocol::SocketAddr(socket_addr) << "].");
    }

    protocol::SocketAddr StatisticModule::GetLocalDetectSocketAddress()
    {
        return protocol::SocketAddr(statistic_info_.LocalPeerInfo.DetectIP, statistic_info_.LocalPeerInfo.DetectUdpPort);
    }

    void StatisticModule::SetLocalStunSocketAddress(const protocol::SocketAddr& socket_addr)
    {
        statistic_info_.LocalPeerInfo.StunIP = socket_addr.IP;
        statistic_info_.LocalPeerInfo.StunUdpPort = socket_addr.Port;
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "StatisticModule::SetLocalStunSocketAddress [" 
            << protocol::SocketAddr(socket_addr) << "].");
    }
    protocol::SocketAddr StatisticModule::GetLocalStunSocketAddress()
    {
        return protocol::SocketAddr(statistic_info_.LocalPeerInfo.StunIP, statistic_info_.LocalPeerInfo.StunUdpPort);
    }
    void StatisticModule::SetLocalNatType(boost::uint8_t nat_type)
    {
        statistic_info_.LocalPeerInfo.PeerNatType = nat_type;
    }
    boost::uint8_t StatisticModule::GetLocalNatType()
    {
        return statistic_info_.LocalPeerInfo.PeerNatType;
    }
    void StatisticModule::SetLocalUploadPriority(boost::uint8_t upload_priority)
    {
        statistic_info_.LocalPeerInfo.UploadPriority = upload_priority;
    }
    boost::uint8_t StatisticModule::GetLocalUploadPriority()
    {
        return statistic_info_.LocalPeerInfo.UploadPriority;
    }
    void StatisticModule::SetLocalIdleTime(boost::uint8_t idle_time_in_min)
    {
        statistic_info_.LocalPeerInfo.IdleTimeInMins = idle_time_in_min;
    }
    boost::uint8_t StatisticModule::GetLocalIdleTime()
    {
        return statistic_info_.LocalPeerInfo.IdleTimeInMins;
    }
    void StatisticModule::SetLocalIPs(const std::vector<uint32_t>& local_ips)
    {
        statistic_info_.LocalIpCount = (std::min)((uint32_t)MAX_IP_COUNT, (uint32_t)local_ips.size());

        LOG4CPLUS_DEBUG_LOG(logger_statistic, "Local IP Count: " << local_ips.size() 
            << ", Max allowed count: " << MAX_IP_COUNT);

        for (uint32_t i = 0; i < statistic_info_.LocalIpCount; i++)
        {
            statistic_info_.LocalIPs[i] = local_ips[i];
        }
        LOG4CPLUS_DEBUG_LOG(logger_statistic,"All " << (uint32_t)statistic_info_.LocalIpCount << " IPs are stored.");
    }

    void StatisticModule::GetLocalIPs(std::vector<uint32_t>& local_ips)
    {
        local_ips.clear();
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "Local IP Count: " << (uint32_t)statistic_info_.LocalIpCount);
        for (uint32_t i = 0; i < statistic_info_.LocalIpCount; i++)
        {
            local_ips.push_back(statistic_info_.LocalIPs[i]);
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Peer Info

    void StatisticModule::SetLocalPeerVersion(uint32_t local_peer_version)
    {
        statistic_info_.LocalPeerVersion = local_peer_version;
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "StatisticModule::SetLocalPeerVersion " << local_peer_version);
    }

    //////////////////////////////////////////////////////////////////////////
    void StatisticModule::SetBsInfo(const boost::asio::ip::udp::endpoint bs_ep)
    {
        bootstrap_endpoint_ = bs_ep;
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "StatisticModule::SetBsInfo " << bootstrap_endpoint_);
    }

    //////////////////////////////////////////////////////////////////////////
    // Stun Server

    void StatisticModule::SetStunInfo(const std::vector<protocol::STUN_SERVER_INFO> &stun_infos)
    {
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "    Statistic Set Stun Count: " << stun_infos.size());
        if (stun_infos.empty())
        {
            return;
        }

        stun_server_infos_.clear();
        stun_server_infos_.assign(stun_infos.begin(), stun_infos.end());
    }

    //////////////////////////////////////////////////////////////////////////
    // Tracker Server

    inline STATISTIC_TRACKER_INFO& StatisticModule::GetTracker(const protocol::TRACKER_INFO& tracker_info)
    {
        return statistic_tracker_info_map_[tracker_info];
    }

    void StatisticModule::SetTrackerInfo(uint32_t group_count, const std::vector<protocol::TRACKER_INFO>& tracker_infos)
    {
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "StatisticModule::SetTrackerInfo [IN]");
        LOG4CPLUS_DEBUG_LOG(logger_statistic,"    Tracker Group Count: " << group_count << 
            ", Trackers Number: " << tracker_infos.size());

        statistic_info_.GroupCount = group_count;
        statistic_info_.TrackerCount = (std::min)((uint32_t)tracker_infos.size(), (uint32_t)UINT8_MAX_VALUE);
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "    Statistic Tracker Count: " << statistic_info_.TrackerCount);

        statistic_tracker_info_map_.clear();
        for (uint32_t i = 0; i < statistic_info_.TrackerCount; i++)
        {
            const protocol::TRACKER_INFO& tracker_info = tracker_infos[i];
            statistic_tracker_info_map_[tracker_info] = STATISTIC_TRACKER_INFO(tracker_info);
        } 
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "StatisticModule::SetTrackerInfo [OUT]");
    }

    void StatisticModule::SetIsSubmitTracker(const protocol::TRACKER_INFO& tracker_info, bool is_submit_tracker)
    {
        GetTracker(tracker_info).IsSubmitTracker = is_submit_tracker;
    }

    void StatisticModule::SubmitCommitRequest(const protocol::TRACKER_INFO& tracker_info)
    {
        GetTracker(tracker_info).CommitRequestCount++;
    }

    void StatisticModule::SubmitCommitResponse(const protocol::TRACKER_INFO& tracker_info)
    {
        GetTracker(tracker_info).CommitResponseCount++;
    }

    void StatisticModule::SubmitKeepAliveRequest(const protocol::TRACKER_INFO& tracker_info)
    {
        GetTracker(tracker_info).KeepAliveRequestCount++;
    }

    void StatisticModule::SubmitKeepAliveResponse(const protocol::TRACKER_INFO& tracker_info, boost::uint16_t keep_alive_interval)
    {
        STATISTIC_TRACKER_INFO& tracker = GetTracker(tracker_info);
        tracker.KeepAliveResponseCount++;
        tracker.KeepAliveInterval = keep_alive_interval;
    }

    void StatisticModule::SubmitListRequest(const protocol::TRACKER_INFO& tracker_info, const RID &rid)
    {
        GetTracker(tracker_info).ListRequestCount++;
        if (p2p_downloader_statistic_map_.find(rid) != p2p_downloader_statistic_map_.end())
        {
            p2p_downloader_statistic_map_[rid]->SubmitDoListRequestCount(tracker_info.StationNo);
        }
    }

    void StatisticModule::SubmitListResponse(const protocol::TRACKER_INFO& tracker_info, uint32_t peer_count, const RID &rid)
    {
        STATISTIC_TRACKER_INFO& tracker = GetTracker(tracker_info);
        tracker.ListResponseCount++;
        tracker.LastListReturnPeerCount = peer_count;
        if (p2p_downloader_statistic_map_.find(rid) != p2p_downloader_statistic_map_.end())
        {
            p2p_downloader_statistic_map_[rid]->SubmitDoListReponseCount(tracker_info.StationNo);
        }
    }

    void StatisticModule::SubmitErrorCode(const protocol::TRACKER_INFO& tracker_info, boost::uint8_t error_code)
    {
        GetTracker(tracker_info).ErrorCode = error_code;
    }

    //////////////////////////////////////////////////////////////////////////
    // Index Server

    void StatisticModule::SetIndexServerInfo(uint32_t ip, boost::uint16_t port, boost::uint8_t type)
    {
        statistic_info_.StatisticIndexInfo.IP = ip;
        statistic_info_.StatisticIndexInfo.Port = port;
        statistic_info_.StatisticIndexInfo.Type = type;
    }

    void StatisticModule::SetIndexServerInfo(const protocol::SocketAddr& socket_addr, boost::uint8_t type)
    {
        statistic_info_.StatisticIndexInfo.IP = socket_addr.IP;
        statistic_info_.StatisticIndexInfo.Port = socket_addr.Port;
        statistic_info_.StatisticIndexInfo.Type = type;
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "Set IndexServerInfo: " << socket_addr << ", type[0-U;1-T]: " << type);
    }

    void StatisticModule::SubmitQueryRIDByUrlRequest()
    {
        statistic_info_.StatisticIndexInfo.QueryRIDByUrlRequestCount++;
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "IndexServer, QueryRIDByUrlRequestCount: " << 
            statistic_info_.StatisticIndexInfo.QueryRIDByUrlRequestCount);
    }

    void StatisticModule::SubmitQueryRIDByUrlResponse()
    {
        statistic_info_.StatisticIndexInfo.QueryRIDByUrlResponseCount++;
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "IndexServer, QueryRIDByUrlResponseCount: " << 
            statistic_info_.StatisticIndexInfo.QueryRIDByUrlResponseCount);
    }

    void StatisticModule::SubmitQueryHttpServersByRIDRequest()
    {
        statistic_info_.StatisticIndexInfo.QueryHttpServersByRIDRequestCount++;
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "IndexServer, QueryHttpServersByRIDRequestCount: " << 
            statistic_info_.StatisticIndexInfo.QueryHttpServersByRIDRequestCount);
    }

    void StatisticModule::SubmitQueryHttpServersByRIDResponse()
    {
        statistic_info_.StatisticIndexInfo.QueryHttpServersByRIDResponseCount++;
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "IndexServer, QueryHttpServersByRIDResponseCount: " << 
            statistic_info_.StatisticIndexInfo.QueryHttpServersByRIDResponseCount);
    }

    void StatisticModule::SubmitQueryTrackerListRequest()
    {
        statistic_info_.StatisticIndexInfo.QueryTrackerListRequestCount++;
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "IndexServer, QueryTrackerListRequestCount: " << 
            statistic_info_.StatisticIndexInfo.QueryTrackerListRequestCount);
    }

    void StatisticModule::SubmitQueryTrackerListResponse()
    {
        statistic_info_.StatisticIndexInfo.QureyTrackerListResponseCount++;
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "IndexServer, QureyTrackerListResponseCount: " << 
            statistic_info_.StatisticIndexInfo.QureyTrackerListResponseCount);
    }

    void StatisticModule::SubmitAddUrlRIDRequest()
    {
        statistic_info_.StatisticIndexInfo.AddUrlRIDRequestCount++;
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "IndexServer, AddUrlRIDRequestCount: " << 
            statistic_info_.StatisticIndexInfo.AddUrlRIDRequestCount);
    }

    void StatisticModule::SubmitAddUrlRIDResponse()
    {
        statistic_info_.StatisticIndexInfo.AddUrlRIDResponseCount++;
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "IndexServer, AddUrlRIDResponseCount: " << 
            statistic_info_.StatisticIndexInfo.AddUrlRIDResponseCount);
    }

    //////////////////////////////////////////////////////////////////////////
    // P2PDownloader RID Address

    boost::uint8_t StatisticModule::Address(const RID& rid)  // Be careful when buffer FULL
    {
        boost::uint8_t pos = HashFunc(boost::hash_value(rid));
        uint32_t count = 0;
        while (statistic_info_.P2PDownloaderRIDs[pos] != Guid() && count < UINT8_MAX_VALUE)
        {
            if (statistic_info_.P2PDownloaderRIDs[pos] == rid)
                return pos;
            pos = (pos + 1) % HASH_SIZE;  // size 太小, 不适合用平方
            ++count;
        }
        return pos;
    }

    bool StatisticModule::AddP2PDownloaderRID(const RID& rid)
    {
        boost::uint8_t pos = Address(rid);
        if (statistic_info_.P2PDownloaderRIDs[pos] == Guid())
        {
            statistic_info_.P2PDownloaderCount++;
            statistic_info_.P2PDownloaderRIDs[pos] = rid;
            return true;
        }
        return false;
    }

    bool StatisticModule::RemoveP2PDownloaderRID(const RID& rid)
    {
        boost::uint8_t pos = Address(rid);
        if (statistic_info_.P2PDownloaderRIDs[pos] == rid)
        {
            statistic_info_.P2PDownloaderCount--;
            statistic_info_.P2PDownloaderRIDs[pos] = Guid();
            return true;
        }
        return false;
    }

    //////////////////////////////////////////////////////////////////////////
    // DownloaderDriverID Address

    uint32_t StatisticModule::Address(uint32_t id)
    {
        boost::uint8_t pos = HashFunc(id);
        uint32_t count = 0;
        while (statistic_info_.DownloadDriverIDs[pos] != 0 && count < UINT8_MAX_VALUE)
        {
            if (statistic_info_.DownloadDriverIDs[pos] == id)
                return pos;
            pos = (pos + 1) % HASH_SIZE;
            ++count;
        }
        return pos;
    }

    bool StatisticModule::AddDownloadDriverID(uint32_t id)
    {
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "AddDownloadDriverID id = " << id);
        bool added = false;
        for (boost::int32_t i = 0; i<UINT8_MAX_VALUE; ++i)
        {
            if (statistic_info_.DownloadDriverIDs[i] == 0)
            {
                LOG4CPLUS_DEBUG_LOG(logger_statistic, "AddDownloadDriverID return true DownloadDriverIDs[" << i 
                    << "] = " << id);
                statistic_info_.DownloadDriverCount++;
                LOG4CPLUS_DEBUG_LOG(logger_statistic, "BINGO AddDownloadDriverID count = " 
                    << (boost::int32_t)statistic_info_.DownloadDriverCount);
                statistic_info_.DownloadDriverIDs[i] = id;
                added = true;
                break;
            }
        }

        return added;
    }

    bool StatisticModule::AddLiveDownloadDriverID(uint32_t id)
    {
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "AddDownloadDriverID id = " << id);
        bool added = false;
        for (boost::int32_t i=0; i<LIVEDOWNLOADER_MAX_COUNT; ++i)
        {
            if (statistic_info_.LiveDownloadDriverIDs[i] == 0)
            {
                LOG4CPLUS_DEBUG_LOG(logger_statistic, "AddLiveDownloadDriverID return true LiveDownloadDriverIDs[" 
                    << i << "] = " << id);
                statistic_info_.LiveDownloadDriverCount++;
                LOG4CPLUS_DEBUG_LOG(logger_statistic, "BINGO AddLiveDownloadDriverID count = " << 
                    (boost::int32_t)statistic_info_.LiveDownloadDriverCount);
                statistic_info_.LiveDownloadDriverIDs[i] = id;
                added = true;
                break;
            }
        }

        return added;
    }

    bool StatisticModule::RemoveDownloadDriverID(uint32_t id)
    {
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "RemoveDownloadDriverID id = " << id);
        bool removed = false;
        for (boost::int32_t i = 0; i<UINT8_MAX_VALUE; ++i)
        {
            if (statistic_info_.DownloadDriverIDs[i] == id)
            {
                LOG4CPLUS_DEBUG_LOG(logger_statistic, "RemoveDownloadDriverID return true DownloadDriverIDs[" 
                    << i << "] = " << id);
                statistic_info_.DownloadDriverCount--;
                LOG4CPLUS_DEBUG_LOG(logger_statistic, "BINGO RemoveDownloadDriverID count = " << 
                    (boost::int32_t)statistic_info_.DownloadDriverCount);
                statistic_info_.DownloadDriverIDs[i] = 0;
                removed = true;
                break;
            }
        }

        return removed;
    }

    bool StatisticModule::RemoveLiveDownloadDriverID(uint32_t id)
    {
        LOG4CPLUS_DEBUG_LOG(logger_statistic, "RemoveLiveDownloadDriverID id = " << id);
        bool removed = false;
        for (boost::int32_t i=0; i<LIVEDOWNLOADER_MAX_COUNT; ++i)
        {
            if (statistic_info_.LiveDownloadDriverIDs[i] == id)
            {
                LOG4CPLUS_DEBUG_LOG(logger_statistic, "RemoveLiveDownloadDriverID return true LiveDownloadDriverIDs[" 
                    << i << "] = " << id);
                statistic_info_.LiveDownloadDriverCount--;
                LOG4CPLUS_DEBUG_LOG(logger_statistic, "BINGO RemoveLiveDownloadDriverID count = " << 
                    (boost::int32_t)statistic_info_.LiveDownloadDriverCount);
                statistic_info_.LiveDownloadDriverIDs[i] = 0;
                removed = true;
                break;
            }
        }

        return removed;
    }

    uint32_t StatisticModule::GetTotalDataBytes()
    {
        return statistic_info_.TotalP2PDataBytes + statistic_info_.TotalHttpOriginalDataBytes;
    }

    boost::uint16_t StatisticModule::GetTotalDataBytesInMB()
    {
        return ((GetTotalDataBytes() >> 20) & 0xFFFFU);
    }

    //////////////////////////////////////////////////////////////////////////
    // Upload Cache 统计

    void StatisticModule::SetUploadCacheRequest(uint32_t count)
    {
        statistic_info_.TotalUploadCacheRequestCount = count;
    }

    void StatisticModule::SetUploadCacheHit(uint32_t count)
    {
        statistic_info_.TotalUploadCacheHitCount = count;
    }

    float StatisticModule::GetUploadCacheHitRate()
    {
        if (statistic_info_.TotalUploadCacheRequestCount == 0)
            return 0.0f;
        return (statistic_info_.TotalUploadCacheHitCount + 0.0f) / statistic_info_.TotalUploadCacheRequestCount;
    }

    //////////////////////////////////////////////////////////////////////////
    // HttpProxyPort

    void StatisticModule::SetHttpProxyPort(boost::uint16_t port)
    {
        statistic_info_.HttpProxyPort = port;
    }

    //////////////////////////////////////////////////////////////////////////
    // IncomingPeer
    void StatisticModule::SubmitIncomingPeer()
    {
        statistic_info_.IncomingPeersCount++;
    }

    boost::uint16_t StatisticModule::GetIncomingPeersCount()
    {
        return statistic_info_.IncomingPeersCount;
    }

    //////////////////////////////////////////////////////////////////////////
    // DownloadDurationInSec

    void StatisticModule::SubmitDownloadDurationInSec(const boost::uint16_t& download_duration_in_sec)
    {
        statistic_info_.DownloadDurationInSec += download_duration_in_sec;
    }

    boost::uint16_t StatisticModule::GetDownloadDurationInSec()
    {
        return statistic_info_.DownloadDurationInSec;
    }

    //////////////////////////////////////////////////////////////////////////
    // Profile Data

    void StatisticModule::LoadBandwidth()
    {
        if (ppva_config_path_.length() > 0)
        {
#ifdef BOOST_WINDOWS_API
            boost::filesystem::path configpath(ppva_config_path_);
            configpath /= ("ppvaconfig.ini");
            string filename = configpath.file_string();

            try
            {
                // TODO:yujinwu:2011/1/18:BUGBUG:framework在某些情况下会抛异常
                // 目前先绕过去
                framework::configure::Config conf(filename);
                framework::configure::ConfigModule & ppva_s_conf =
                    conf.register_module("PPVA_S");

                // ip
                uint32_t ip_local = 0;
                ppva_s_conf(CONFIG_PARAM_NAME_RDONLY("I", ip_local));
                // date
                time_t time_stamp = time(NULL);
                uint32_t time_stamp_last = 0;
                ppva_s_conf(CONFIG_PARAM_NAME_RDONLY("T", time_stamp_last));
                uint32_t duration_in_sec = 10 * 3600;

                // 获得本机IP
                uint32_t ip_local_now = base::util::GetLocalFirstIP();

                // 如果IP地址与原来的不同，则带宽从最低值512Kbps(64KB)开始
                if (ip_local && ip_local == ip_local_now
                    && time_stamp >= time_stamp_last
                    && time_stamp <= time_stamp_last + duration_in_sec)
                {
                    // bandwidth
                    uint32_t bandwith_load = 0;
                    ppva_s_conf(CONFIG_PARAM_NAME_RDONLY("B", bandwith_load));

                    history_bandwith_ = bandwith_load;
                    LOG4CPLUS_DEBUG_LOG(logger_statistic, "bandwidth_load = " << bandwith_load << 
                        ", history_bandwith_ = " << history_bandwith_);
                }
                else
                {
                    history_bandwith_ = 0;
                }

                // 最低要求带宽为512kbps
                if (history_bandwith_ < 65535)
                {
                    history_bandwith_ = 65535;
                }
                framework::configure::ConfigModule & ppva_push_conf = 
                    conf.register_module("PPVA_PUSH");
                ppva_push_conf(CONFIG_PARAM_NAME_RDONLY("T_STAMP",time_push_stamp_));
                ppva_push_conf(CONFIG_PARAM_NAME_RDONLY("T_ONLINE",duration_online_time_in_second_));
            }
            catch(...)
            {
                // 最低要求带宽为512kbps
                history_bandwith_ = 65535;
                base::filesystem::remove_nothrow(filename);
            }
#endif
        }
    }

    void StatisticModule::SaveBandwidth()
    {
        if (ppva_config_path_.length() > 0)
        {
#ifdef BOOST_WINDOWS_API

            boost::filesystem::path configpath(ppva_config_path_);
            configpath /= ("ppvaconfig.ini");
            string filename = configpath.file_string();

            try
            {
                framework::configure::Config conf(filename);
                framework::configure::ConfigModule & ppva_s_conf =
                    conf.register_module("PPVA_S");

                // ip
                uint32_t ip_local;
                // date
                time_t time_stamp;
                uint32_t bandwidth;
                ppva_s_conf(CONFIG_PARAM_NAME_RDONLY("I", ip_local));
                ppva_s_conf(CONFIG_PARAM_NAME_RDONLY("T", time_stamp));
                ppva_s_conf(CONFIG_PARAM_NAME_RDONLY("B", bandwidth));

                // ip
                ip_local = base::util::GetLocalFirstIP();
                // date
                time_stamp = time(NULL);

                bandwidth = (std::max)(statistic_info_.BandWidth, history_bandwith_);

                // 最低要求带宽为512kbps
                if (history_bandwith_ < 65535)
                {
                    history_bandwith_ = 65535;
                }

                 framework::configure::ConfigModule & ppva_push_conf = 
                     conf.register_module("PPVA_PUSH");
                 boost::uint32_t duration_online_time_in_second;
                 ppva_push_conf(CONFIG_PARAM_NAME_RDONLY("T_STAMP",time_push_stamp_));
                 ppva_push_conf(CONFIG_PARAM_NAME_RDONLY("T_ONLINE",duration_online_time_in_second));
                 duration_online_time_in_second = duration_online_time_in_second_;

             // store
             conf.sync();
            }
            catch(...)
            {
                // 最低要求带宽为512kbps
                history_bandwith_ = 65535;
                base::filesystem::remove_nothrow(filename);
            }
#endif
        }
    }

    //////////////////////////////////////////////////////////////////////////

    void StatisticModule::SetMemoryPoolLeftSize(uint32_t memory_pool_left_size)
    {
        statistic_info_.MemoryPoolLeftSize = memory_pool_left_size;
    }

    //////////////////////////////////////////////////////////////////////////
    // Upload

    void StatisticModule::SubmitUploadDataBytes(uint32_t uploaded_bytes)
    {
        if (false == is_running_)
            return;

        upload_speed_meter_.SubmitBytes(uploaded_bytes);
    }

    uint32_t StatisticModule::GetUploadDataSpeed()
    {
        if (false == is_running_)
            return 0;

        return upload_speed_meter_.CurrentByteSpeed(framework::timer::TickCounter::tick_count());
    }

    uint32_t StatisticModule::GetMinuteUploadDataSpeed()
    {
        if (false == is_running_)
            return 0;

        return upload_speed_meter_.RecentMinuteByteSpeed(framework::timer::TickCounter::tick_count());
    }

    uint32_t StatisticModule::GetUploadDataSpeedInKBps()
    {
        return GetUploadDataSpeed() / 1024;
    }

    uint32_t StatisticModule::GetRecentMinuteUploadDataSpeedInKBps()
    {
        return upload_speed_meter_.RecentMinuteByteSpeed(framework::timer::TickCounter::tick_count()) / 1024;
    }

    uint32_t StatisticModule::GetUploadDataBytes() const
    {
        return upload_speed_meter_.TotalBytes();
    }

    void StatisticModule::QueryBasicPeerInfo(boost::function<void()> result_handler, BASICPEERINFO *bpi)
    {
        memset(bpi, 0, sizeof(BASICPEERINFO));

        if (false == is_running_)
        {
            result_handler();
            return;
        }

        boost::asio::ip::address_v4 addr = bootstrap_endpoint_.address().to_v4();

        bpi->bs_ip = addr.to_ulong();
        bpi->stun_count = stun_server_infos_.size();
        bpi->tracker_count =  statistic_info_.TrackerCount;
        bpi->tcp_port = tcp_port_;
        bpi->udp_port = statistic_info_.LocalPeerInfo.UdpPort;
        bpi->upload_speed = GetSpeedInfoEx().SecondUploadSpeed;

        result_handler();
    }

    void StatisticModule::QueryPeerInfoByRid(RID rid, boost::function<void()> result_handler, boost::int32_t *iListCount, boost::int32_t *iConnectCount, boost::int32_t *iAverSpeed)
    {
        P2PDownloadDriverStatisticMap::iterator it = p2p_downloader_statistic_map_.find(rid);
        if (it != p2p_downloader_statistic_map_.end())
        {
            P2PDownloaderStatistic::p p2p_stastic = it->second;
            if (p2p_stastic)
            {
                *iListCount = p2p_stastic->GetIpPoolPeerCount();
                *iConnectCount = p2p_stastic->GetConnectedPeerCount();
                *iAverSpeed = p2p_stastic->GetSnSpeedInfoEx().SecondDownloadSpeed;
                result_handler();
                return;
            }
            else
            {
                LOG4CPLUS_DEBUG_LOG(logger_statistic, "QueryPeerInfoByRid Rid: " << rid << 
                    ", P2PDownloaderStatistic NULL");
            }
        }
        else
        {
            LOG4CPLUS_DEBUG_LOG(logger_statistic, "QueryPeerInfoByRid can't find rid: " << rid);
        }

        *iListCount = 0;
        *iConnectCount = 0;
        *iAverSpeed = 0;
        result_handler();
    }

    void StatisticModule::SubmitDurationOnline()
    {
        duration_online_time_in_second_ ++;
    }

    uint32_t StatisticModule::GetOnlinePercent() const
    { 
        time_t time_push_current = time(NULL);
        return duration_online_time_in_second_ * 100 / (uint32_t)(time_push_current - time_push_stamp_);
    }
}
