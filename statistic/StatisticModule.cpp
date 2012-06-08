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
#include "p2sp/stun/StunClient.h"
#include "statistic/DACStatisticModule.h"
#include "statistic/UploadStatisticModule.h"
#include "p2sp/p2p/P2PModule.h"

#include <util/archive/LittleEndianBinaryOArchive.h>
#include <util/archive/ArchiveBuffer.h>

namespace statistic
{
    StatisticModule::p StatisticModule::inst_;

    StatisticModule::StatisticModule()
        : share_memory_timer_(
            global_second_timer(),
            1000,
            boost::bind(&StatisticModule::OnTimerElapsed, this, &share_memory_timer_))
        , is_running_(false)
    {
    }

    void StatisticModule::Start(uint32_t flush_interval_in_seconds, string config_path)
    {
        STAT_DEBUG("StatisticModule::Start [IN]"); STAT_EVENT("StatisticModule is starting.");

        if (is_running_ == true)
        {
            STAT_WARN("StatisticModule::Start when module is running.");
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
            STAT_ERROR("Create Shared Memory Failed");
        }

        speed_info_.Start();
        upload_speed_meter_.Start();

        share_memory_timer_.interval(flush_interval_in_seconds * 1000);
        STAT_DEBUG("framework::timer::Timer Created, Interval: " << flush_interval_in_seconds << "(s)");

        share_memory_timer_.start();

        STAT_EVENT("StatisticModule starts successfully.");
    }

    void StatisticModule::Stop()
    {
        STAT_INFO("StatisticModule::Stop [IN]");

        if (is_running_ == false)
        {
            STAT_WARN("StatisticModule::Stop when module is not running.");
            return;
        }

        SaveBandwidth();

        share_memory_timer_.stop();
        STAT_DEBUG("framework::timer::Timer stopped.");

        upload_speed_meter_.Stop();

        Clear();
        shared_memory_.Close();

        speed_info_.Stop();

        is_running_ = false;
        inst_.reset();

        STAT_EVENT("StatisticModule is stopped successfully.");
        STAT_DEBUG("StatisticModule::Stop [OUT]");
    }

    LiveDownloadDriverStatistic::p StatisticModule::AttachLiveDownloadDriverStatistic(uint32_t id)
    {
        STAT_DEBUG("StatisticModule::AttachLiveDownloadDriverStatistic [IN]");

        assert(is_running_ == true);

        if (live_download_driver_statistic_map_.find(id) != live_download_driver_statistic_map_.end())
        {
            STAT_WARN("ID " << id << " exists, return null.");
            if (live_download_driver_statistic_map_[id])
            {
                // 不为空，返回
                return live_download_driver_statistic_map_[id];
            }
        }

        LiveDownloadDriverStatistic::p live_download_driver_statistics = LiveDownloadDriverStatistic::Create(id);
        live_download_driver_statistic_map_[id] = live_download_driver_statistics;

        AddLiveDownloadDriverID(id);

        STAT_DEBUG("StatisticModule::AttachLiveDownloadDriverStatistic [OUT]");

        return live_download_driver_statistics;
    }

    DownloadDriverStatistic::p StatisticModule::AttachDownloadDriverStatistic(
        uint32_t id,
        bool is_create_shared_memory)
    {
        STAT_DEBUG("StatisticModule::AttachDownloadDriverStatistic [IN]");

        assert(is_running_ == true);
        
        // Modified by jeffrey 2010.2.21
        // 已经存在, 直接返回
        // 如果这里出问题，最多造成的后果是2个downloaddriver写同一个statistic，不会挂
        if (download_driver_statistic_map_.find(id) != download_driver_statistic_map_.end())
        {
            STAT_WARN("ID " << id << " exists, return null.");
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
            STAT_WARN("Download Driver Map is Full, size: " << statistic_info_.DownloadDriverCount << ". Return null");
            assert(false);
        }

        // 新建一个 DownloadDriverStatistic, 然后添加到 download_driver_statistic_s_ 中
        // 返回这个新建的 DownloadDriverStatistic
        DownloadDriverStatistic::p download_driver = DownloadDriverStatistic::Create(id, is_create_shared_memory);
        download_driver_statistic_map_[id] = download_driver;

        // start it
        download_driver->Start();

        STAT_DEBUG("StatisticModule::AttachDownloadDriverStatistic [OUT]");

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
        STAT_DEBUG("StatisticModule::DetachDownloadDriverStatistic [IN]"); STAT_EVENT("StatisticModule is detaching download driver with id " << id);

        if (is_running_ == false)
        {
            STAT_WARN("StatisticModule is not running, return false.");
            return false;
        }

        // 如果 不存该 DownloadDriverStatistic  返回false
        if (download_driver_statistic_map_.find(id) == download_driver_statistic_map_.end())
        {
            STAT_WARN("Download Driver with id " << id << " does not exist.");
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
        STAT_DEBUG("Download Driver " << id << " has been stopped.");

        // 删除该 DownloadDriverStatistic
        download_driver_statistic_map_.erase(id);
        STAT_DEBUG("Download Driver " << id << " has been removed from statistic std::map.");

        STAT_DEBUG("StatisticModule::DetachDownloadDriverStatistic [OUT]");

        return true;
    }

    bool StatisticModule::DetachDownloadDriverStatistic(const DownloadDriverStatistic::p download_driver_statistic)
    {
        return DetachDownloadDriverStatistic(download_driver_statistic->GetDownloadDriverID());
    }

    bool StatisticModule::DetachLiveDownloadDriverStatistic(const LiveDownloadDriverStatistic::p live_download_driver_statistic)
    {
        uint32_t id = live_download_driver_statistic->GetLiveDownloadDriverId();

        STAT_DEBUG("StatisticModule::DetachLiveDownloadDriverStatistic [IN]"); STAT_EVENT("StatisticModule is detaching live download driver with id " << id);

        if (is_running_ == false)
        {
            STAT_WARN("StatisticModule is not running, return false.");
            return false;
        }

        if (live_download_driver_statistic_map_.find(id) == live_download_driver_statistic_map_.end())
        {
            STAT_WARN("Live Download Driver with id " << id << " does not exist.");
            return false;
        }

        RemoveLiveDownloadDriverID(id);
        
        // 删除该 DownloadDriverStatistic
        live_download_driver_statistic_map_.erase(id);
        STAT_DEBUG("Live Download Driver " << id << " has been removed from statistic std::map.");

        STAT_DEBUG("StatisticModule::DetachLiveDownloadDriverStatistic [OUT]");

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
        STAT_DEBUG("StatisticModule::AttachP2PDownloaderStatistic [IN]"); STAT_EVENT("StatisticModule is attaching p2p downloader with RID " << rid);
        P2PDownloaderStatistic::p p2p_downloader_ = P2PDownloaderStatistic::Create(rid);

        if (is_running_ == false)
        {
            assert(false);
            STAT_WARN("StatisticModule is not running, return null.");
            return p2p_downloader_;
        }

        // 如果已经存在 则返回 空
        if (p2p_downloader_statistic_map_.find(rid) != p2p_downloader_statistic_map_.end())
        {
            STAT_WARN("p2p downloader " << rid << " exists. Return null.");
            return p2p_downloader_statistic_map_[rid];
        }

        // 判断最大个数
        if (statistic_info_.P2PDownloaderCount == UINT8_MAX_VALUE - 1)
        {
            assert(false);
            STAT_WARN("p2p downloader std::map is full. size: " << statistic_info_.P2PDownloaderCount << ". Return null.");
            return p2p_downloader_;
        }

        // 将RID加入数组, 线性开地址
        AddP2PDownloaderRID(rid);

        // 新建一个 P2PDownloaderStatistic::p, 然后添加到 download_driver_statistic_s_ 中
        p2p_downloader_statistic_map_[rid] = p2p_downloader_;

        STAT_DEBUG("Created P2P Downloader with RID: " << rid);

        // 该 P2PDownloadeStatisticr::p -> Start()
        p2p_downloader_->Start();

        STAT_DEBUG("Started P2P Downloader: " << rid);

        // 返回这个新建的 P2PDownloaderStatistic::p
        STAT_DEBUG("StatisticModule::AttachP2PDownloaderStatistic [OUT]");
        return p2p_downloader_;
    }

    bool StatisticModule::DetachP2PDownloaderStatistic(const RID& rid)
    {
        STAT_DEBUG("StatisticModule::DetachP2PDownloaderStatistic [IN]"); STAT_EVENT("StatisticModule is detaching P2PDownloader: " << rid);

        if (is_running_ == false)
        {
            STAT_WARN("StatisticModule is not running, return false.");
            return false;
        }

        P2PDownloadDriverStatisticMap::iterator it = p2p_downloader_statistic_map_.find(rid);

        // 不存在, 返回
        if (it == p2p_downloader_statistic_map_.end())
        {
            STAT_WARN("P2PDownloader does not exist. Return false.");
            return false;
        }

        assert(it->second);

        it->second->Stop();
        p2p_downloader_statistic_map_.erase(it);
        STAT_DEBUG("P2PDownloader " << rid << " has been stopped and removed.");
        RemoveP2PDownloaderRID(rid);

        STAT_DEBUG("StatisticModule::DetachP2PDownloaderStatistic [OUT]");
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
        STAT_EVENT("StatisticModule::OnTimerElapsed, times: " << pointer->times());

        if (is_running_ == false)
        {
            STAT_WARN("StatisticModule is not running, return.");
            return;
        }

        if (pointer == &share_memory_timer_)
        {
            OnShareMemoryTimer(pointer->times());
        }
        else
        {
            STAT_ERROR("Invalid framework::timer::Timer Pointer.");
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
        STAT_DEBUG("StatisticModule::OnShareMemoryTimer [IN], times: " << times); STAT_EVENT("StatisticModule::OnShareMemoryTimer, Writing data into shared memory.");

        if (is_running_ == false)
        {
            STAT_WARN("StatisticModule is not running, return.");
            return;
        }

        // 将内部变量拷贝到共享内存中
        DoTakeSnapshot();

        STAT_EVENT("Updated Statistic Information has been written to shared memory.");
        FlushSharedMemory();

        uint32_t upload_speed = (uint32_t) (statistic_info_.SpeedInfo.MinuteUploadSpeed / 1024.0 + 0.5);
        DACStatisticModule::Inst()->SubmitP2PUploadSpeedInKBps(upload_speed);

        UploadStatisticModule::Inst()->OnShareMemoryTimer(times);

        // 遍历所有的  DownloadDriverStatistic::p->OnShareMemoryTimer(times)
        STAT_DEBUG("Starting to update downloader driver statistic.");
        for (DownloadDriverStatisticMap::iterator it = download_driver_statistic_map_.begin(); it
            != download_driver_statistic_map_.end(); it++)
        {
            assert(it->second);
            it->second->OnShareMemoryTimer(times);
        } STAT_EVENT("All Downloader Drirvers shared memory has been updated.");

        // 遍历所有的  P2PDownloaderStatistic::p->OnShareMemoryTimer(times)
        STAT_DEBUG("Starting to update p2p downloader statistic.");
        for (P2PDownloadDriverStatisticMap::iterator it = p2p_downloader_statistic_map_.begin(); it
            != p2p_downloader_statistic_map_.end(); it++)
        {
            assert(it->second);
            it->second->OnShareMemoryTimer(times);
        } STAT_EVENT("All P2PDownloader shared memory has been updated.");

        // flush bandwidth info
        if (times % (10 * 60) == 0)  // 10mins
        {
            SaveBandwidth();
        }

        STAT_DEBUG("StatisticModule::OnShareMemoryTimer [OUT]");
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
        STAT_DEBUG("StatisticModule::CreateSharedMemory Creating Shared Memory.");
        shared_memory_.Close();
        shared_memory_.Create(GetSharedMemoryName(), GetSharedMemorySize());
        STAT_DEBUG("Created Shared Memory: " << ((const char*) GetSharedMemoryName().c_str()) << ", size: " << GetSharedMemorySize() << " Byte(s).");

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
        STAT_DEBUG("StatisticModule::SubmitDownloadedBytes added: " << downloaded_bytes);
    }

    void StatisticModule::SubmitUploadedBytes(uint32_t uploaded_bytes)
    {
        speed_info_.SubmitUploadedBytes(uploaded_bytes);
        STAT_DEBUG("StatisticModule::SubmitUploadedBytes added: " << uploaded_bytes);
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
            STAT_WARN("StatisticModule::GetLocalPeerDownloadInfo Statistic is not running. Return. ");
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
        STAT_DEBUG("StatisticModule::UpdateSpeedInfo Speed Info has been updated.");
    }

    void StatisticModule::UpdateTrackerInfo()
    {
        STAT_DEBUG("StatisticModule::UpdateTrackerInfo [IN]");

        statistic_info_.TrackerCount = statistic_tracker_info_map_.size();
        STAT_DEBUG("Current Tracker Number: " << (uint32_t)statistic_info_.TrackerCount);

        assert(statistic_info_.TrackerCount <= UINT8_MAX_VALUE - 1);

        StatisticTrackerInfoMap::iterator it = statistic_tracker_info_map_.begin();
        for (uint32_t i = 0; it != statistic_tracker_info_map_.end(); it++, i++)
        {
            statistic_info_.TrackerInfos[i] = it->second;
        } STAT_DEBUG("StatisticModule::UpdateTrackerInfo [OUT]");
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
        // STAT_DEBUG("StatisticModule::SetLocalPeerInfo [" << protocol::CandidatePeerInfo(local_peer_info) << "].");
    }

    protocol::CandidatePeerInfo StatisticModule::GetLocalPeerInfo()
    {
        return protocol::CandidatePeerInfo(statistic_info_.LocalPeerInfo);
    }

    void StatisticModule::SetLocalPeerAddress(const protocol::PeerAddr& peer_addr)
    {
        statistic_info_.LocalPeerInfo.IP = peer_addr.IP;
        statistic_info_.LocalPeerInfo.UdpPort = peer_addr.UdpPort;
        STAT_DEBUG("StatisticModule::SetLocalPeerAddress [" << protocol::PeerAddr(peer_addr) << "].");
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
        STAT_DEBUG("StatisticModule::SetLocalDetectSocketAddress [" << protocol::SocketAddr(socket_addr) << "].");
    }

    protocol::SocketAddr StatisticModule::GetLocalDetectSocketAddress()
    {
        return protocol::SocketAddr(statistic_info_.LocalPeerInfo.DetectIP, statistic_info_.LocalPeerInfo.DetectUdpPort);
    }

    void StatisticModule::SetLocalStunSocketAddress(const protocol::SocketAddr& socket_addr)
    {
        statistic_info_.LocalPeerInfo.StunIP = socket_addr.IP;
        statistic_info_.LocalPeerInfo.StunUdpPort = socket_addr.Port;
        STAT_DEBUG("StatisticModule::SetLocalStunSocketAddress [" << protocol::SocketAddr(socket_addr) << "].");
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

        STAT_DEBUG("Local IP Count: " << local_ips.size() << ", Max allowed count: " << MAX_IP_COUNT);

        for (uint32_t i = 0; i < statistic_info_.LocalIpCount; i++)
        {
            statistic_info_.LocalIPs[i] = local_ips[i];
            // STAT_DEBUG("    Set IP: " << IpPortToUdpEndpoint(local_ips[i], 0).address());
        }
        STAT_DEBUG("All " << (uint32_t)statistic_info_.LocalIpCount << " IPs are stored.");
    }

    void StatisticModule::GetLocalIPs(std::vector<uint32_t>& local_ips)
    {
        local_ips.clear();
        STAT_DEBUG("Local IP Count: " << (uint32_t)statistic_info_.LocalIpCount);
        for (uint32_t i = 0; i < statistic_info_.LocalIpCount; i++)
        {
            // STAT_DEBUG("    Get IP: " << IpPortToUdpEndpoint(statistic_info_.LocalIPs[i], 0).address());
            local_ips.push_back(statistic_info_.LocalIPs[i]);
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Peer Info

    void StatisticModule::SetLocalPeerVersion(uint32_t local_peer_version)
    {
        statistic_info_.LocalPeerVersion = local_peer_version;
        STAT_DEBUG("StatisticModule::SetLocalPeerVersion " << local_peer_version);
    }

    //////////////////////////////////////////////////////////////////////////
    void StatisticModule::SetBsInfo(const boost::asio::ip::udp::endpoint bs_ep)
    {
        bootstrap_endpoint_ = bs_ep;
        STAT_DEBUG("StatisticModule::SetBsInfo " << bootstrap_endpoint_);
    }

    //////////////////////////////////////////////////////////////////////////
    // Stun Server

    void StatisticModule::SetStunInfo(const std::vector<protocol::STUN_SERVER_INFO> &stun_infos)
    {
        STAT_DEBUG("    Statistic Set Stun Count: " << stun_infos.size());
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
        STAT_DEBUG("StatisticModule::SetTrackerInfo [IN]"); STAT_DEBUG("    Tracker Group Count: " << group_count << ", Trackers Number: " << tracker_infos.size());

        statistic_info_.GroupCount = group_count;
        statistic_info_.TrackerCount = (std::min)((uint32_t)tracker_infos.size(), (uint32_t)UINT8_MAX_VALUE);
        STAT_DEBUG("    Statistic Tracker Count: " << statistic_info_.TrackerCount);

        statistic_tracker_info_map_.clear();
        for (uint32_t i = 0; i < statistic_info_.TrackerCount; i++)
        {
            const protocol::TRACKER_INFO& tracker_info = tracker_infos[i];
            // STAT_DEBUG("    Add Tracker Info: " << tracker_info);
            statistic_tracker_info_map_[tracker_info] = STATISTIC_TRACKER_INFO(tracker_info);
        } STAT_DEBUG("StatisticModule::SetTrackerInfo [OUT]");
    }

    void StatisticModule::SetIsSubmitTracker(const protocol::TRACKER_INFO& tracker_info, bool is_submit_tracker)
    {
        GetTracker(tracker_info).IsSubmitTracker = is_submit_tracker;
        // STAT_DEBUG("Set IsSubmitTracker: " << is_submit_tracker << " to Tracker: " << tracker_info);
    }

    void StatisticModule::SubmitCommitRequest(const protocol::TRACKER_INFO& tracker_info)
    {
        GetTracker(tracker_info).CommitRequestCount++;
        // STAT_DEBUG("Current CommitRequestCount: " << GetTracker(tracker_info).CommitRequestCount << " of Tracker: " << tracker_info);
    }

    void StatisticModule::SubmitCommitResponse(const protocol::TRACKER_INFO& tracker_info)
    {
        GetTracker(tracker_info).CommitResponseCount++;
        // STAT_DEBUG("Current CommitResponseCount: " << GetTracker(tracker_info).CommitResponseCount << " of Tracker: " << tracker_info);
    }

    void StatisticModule::SubmitKeepAliveRequest(const protocol::TRACKER_INFO& tracker_info)
    {
        GetTracker(tracker_info).KeepAliveRequestCount++;
        // STAT_DEBUG("Current KeepAliveRequestCount: " << GetTracker(tracker_info).KeepAliveRequestCount << " of Tracker: " << tracker_info);
    }

    void StatisticModule::SubmitKeepAliveResponse(const protocol::TRACKER_INFO& tracker_info, boost::uint16_t keep_alive_interval)
    {
        STATISTIC_TRACKER_INFO& tracker = GetTracker(tracker_info);
        tracker.KeepAliveResponseCount++;
        tracker.KeepAliveInterval = keep_alive_interval;
        // STAT_DEBUG("Current KeepAliveResponseCount: " << GetTracker(tracker_info).KeepAliveResponseCount << ", KLPInterval: " << keep_alive_interval << " of Tracker: " << tracker_info);
    }

    void StatisticModule::SubmitListRequest(const protocol::TRACKER_INFO& tracker_info)
    {
        GetTracker(tracker_info).ListRequestCount++;
        // STAT_DEBUG("Current ListRequestCount: " << GetTracker(tracker_info).ListRequestCount << " of Tracker: " << tracker_info);
    }

    void StatisticModule::SubmitListResponse(const protocol::TRACKER_INFO& tracker_info, uint32_t peer_count)
    {
        STATISTIC_TRACKER_INFO& tracker = GetTracker(tracker_info);
        tracker.ListResponseCount++;
        tracker.LastListReturnPeerCount = peer_count;
        // STAT_DEBUG("Current ListResponseCount: " << GetTracker(tracker_info).ListResponseCount << ", PeerCount: " << peer_count << " of Tracker: " << tracker_info);
    }

    void StatisticModule::SubmitErrorCode(const protocol::TRACKER_INFO& tracker_info, boost::uint8_t error_code)
    {
        GetTracker(tracker_info).ErrorCode = error_code;
        // STAT_DEBUG("Current ErrorCode: " << error_code << " of Tracker: " << tracker_info);
    }

    //////////////////////////////////////////////////////////////////////////
    // Index Server

    void StatisticModule::SetIndexServerInfo(uint32_t ip, boost::uint16_t port, boost::uint8_t type)
    {
        statistic_info_.StatisticIndexInfo.IP = ip;
        statistic_info_.StatisticIndexInfo.Port = port;
        statistic_info_.StatisticIndexInfo.Type = type;
        // STAT_DEBUG("Set IndexServerInfo: " << IpPortToUdpEndpoint(ip, port) << ", type[0-U;1-T]: " << type);
    }

    void StatisticModule::SetIndexServerInfo(const protocol::SocketAddr& socket_addr, boost::uint8_t type)
    {
        statistic_info_.StatisticIndexInfo.IP = socket_addr.IP;
        statistic_info_.StatisticIndexInfo.Port = socket_addr.Port;
        statistic_info_.StatisticIndexInfo.Type = type;
        STAT_DEBUG("Set IndexServerInfo: " << socket_addr << ", type[0-U;1-T]: " << type);
    }

    void StatisticModule::SubmitQueryRIDByUrlRequest()
    {
        statistic_info_.StatisticIndexInfo.QueryRIDByUrlRequestCount++;
        STAT_DEBUG("IndexServer, QueryRIDByUrlRequestCount: " << statistic_info_.StatisticIndexInfo.QueryRIDByUrlRequestCount);
    }

    void StatisticModule::SubmitQueryRIDByUrlResponse()
    {
        statistic_info_.StatisticIndexInfo.QueryRIDByUrlResponseCount++;
        STAT_DEBUG("IndexServer, QueryRIDByUrlResponseCount: " << statistic_info_.StatisticIndexInfo.QueryRIDByUrlResponseCount);
    }

    void StatisticModule::SubmitQueryHttpServersByRIDRequest()
    {
        statistic_info_.StatisticIndexInfo.QueryHttpServersByRIDRequestCount++;
        STAT_DEBUG("IndexServer, QueryHttpServersByRIDRequestCount: " << statistic_info_.StatisticIndexInfo.QueryHttpServersByRIDRequestCount);
    }

    void StatisticModule::SubmitQueryHttpServersByRIDResponse()
    {
        statistic_info_.StatisticIndexInfo.QueryHttpServersByRIDResponseCount++;
        STAT_DEBUG("IndexServer, QueryHttpServersByRIDResponseCount: " << statistic_info_.StatisticIndexInfo.QueryHttpServersByRIDResponseCount);
    }

    void StatisticModule::SubmitQueryTrackerListRequest()
    {
        statistic_info_.StatisticIndexInfo.QueryTrackerListRequestCount++;
        STAT_DEBUG("IndexServer, QueryTrackerListRequestCount: " << statistic_info_.StatisticIndexInfo.QueryTrackerListRequestCount);
    }

    void StatisticModule::SubmitQueryTrackerListResponse()
    {
        statistic_info_.StatisticIndexInfo.QureyTrackerListResponseCount++;
        STAT_DEBUG("IndexServer, QureyTrackerListResponseCount: " << statistic_info_.StatisticIndexInfo.QureyTrackerListResponseCount);
    }

    void StatisticModule::SubmitAddUrlRIDRequest()
    {
        statistic_info_.StatisticIndexInfo.AddUrlRIDRequestCount++;
        STAT_DEBUG("IndexServer, AddUrlRIDRequestCount: " << statistic_info_.StatisticIndexInfo.AddUrlRIDRequestCount);
    }

    void StatisticModule::SubmitAddUrlRIDResponse()
    {
        statistic_info_.StatisticIndexInfo.AddUrlRIDResponseCount++;
        STAT_DEBUG("IndexServer, AddUrlRIDResponseCount: " << statistic_info_.StatisticIndexInfo.AddUrlRIDResponseCount);
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
        STAT_DEBUG("AddDownloadDriverID id = " << id);
        bool added = false;
        for (boost::int32_t i = 0; i<UINT8_MAX_VALUE; ++i)
        {
            if (statistic_info_.DownloadDriverIDs[i] == 0)
            {
                STAT_DEBUG("AddDownloadDriverID return true DownloadDriverIDs[" << i << "] = " << id);
                statistic_info_.DownloadDriverCount++;
                STAT_DEBUG("BINGO AddDownloadDriverID count = " << (boost::int32_t)statistic_info_.DownloadDriverCount);
                statistic_info_.DownloadDriverIDs[i] = id;
                added = true;
                break;
            }
        }

        return added;
    }

    bool StatisticModule::AddLiveDownloadDriverID(uint32_t id)
    {
        STAT_DEBUG("AddDownloadDriverID id = " << id);
        bool added = false;
        for (boost::int32_t i=0; i<LIVEDOWNLOADER_MAX_COUNT; ++i)
        {
            if (statistic_info_.LiveDownloadDriverIDs[i] == 0)
            {
                STAT_DEBUG("AddLiveDownloadDriverID return true LiveDownloadDriverIDs[" << i << "] = " << id);
                statistic_info_.LiveDownloadDriverCount++;
                STAT_DEBUG("BINGO AddLiveDownloadDriverID count = " << (boost::int32_t)statistic_info_.LiveDownloadDriverCount);
                statistic_info_.LiveDownloadDriverIDs[i] = id;
                added = true;
                break;
            }
        }

        return added;
    }

    bool StatisticModule::RemoveDownloadDriverID(uint32_t id)
    {
        STAT_DEBUG("RemoveDownloadDriverID id = " << id);
        bool removed = false;
        for (boost::int32_t i = 0; i<UINT8_MAX_VALUE; ++i)
        {
            if (statistic_info_.DownloadDriverIDs[i] == id)
            {
                STAT_DEBUG("RemoveDownloadDriverID return true DownloadDriverIDs[" << i << "] = " << id);
                statistic_info_.DownloadDriverCount--;
                STAT_DEBUG("BINGO RemoveDownloadDriverID count = " << (boost::int32_t)statistic_info_.DownloadDriverCount);
                statistic_info_.DownloadDriverIDs[i] = 0;
                removed = true;
                break;
            }
        }

        return removed;
    }

    bool StatisticModule::RemoveLiveDownloadDriverID(uint32_t id)
    {
        STAT_DEBUG("RemoveLiveDownloadDriverID id = " << id);
        bool removed = false;
        for (boost::int32_t i=0; i<LIVEDOWNLOADER_MAX_COUNT; ++i)
        {
            if (statistic_info_.LiveDownloadDriverIDs[i] == id)
            {
                STAT_DEBUG("RemoveLiveDownloadDriverID return true LiveDownloadDriverIDs[" << i << "] = " << id);
                statistic_info_.LiveDownloadDriverCount--;
                STAT_DEBUG("BINGO RemoveLiveDownloadDriverID count = " << (boost::int32_t)statistic_info_.LiveDownloadDriverCount);
                statistic_info_.LiveDownloadDriverIDs[i] = 0;
                removed = true;
                break;
            }
        }

        return removed;
    }

    //////////////////////////////////////////////////////////////////////////
    // 停止时数据上传相关
    void StatisticModule::SubmitP2PDownloaderDownloadBytes(uint32_t p2p_downloader_download_bytes)
    {
        statistic_info_.TotalP2PDownloadBytes += p2p_downloader_download_bytes;
        STAT_DEBUG("IndexServer, SubmitP2PDownloaderDownloadBytes: " << statistic_info_.TotalP2PDownloadBytes);
    }

    void StatisticModule::SubmitOtherServerDownloadBytes(uint32_t other_server_download_bytes)
    {
        statistic_info_.TotalOtherServerDownloadBytes += other_server_download_bytes;
        STAT_DEBUG("IndexServer, QueryRIDByUrlResponseCount: " << statistic_info_.TotalOtherServerDownloadBytes);
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
                uint32_t ip_local_now = p2sp::CStunClient::GetLocalFirstIP();

                // 如果IP地址与原来的不同，则带宽从最低值512Kbps(64KB)开始
                if (ip_local && ip_local == ip_local_now
                    && time_stamp >= time_stamp_last
                    && time_stamp <= time_stamp_last + duration_in_sec)
                {
                    // bandwidth
                    uint32_t bandwith_load = 0;
                    ppva_s_conf(CONFIG_PARAM_NAME_RDONLY("B", bandwith_load));

                    history_bandwith_ = bandwith_load;
                    LOGX(__DEBUG, "upload", "bandwidth_load = " << bandwith_load << ", history_bandwith_ = " << history_bandwith_);
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
                ip_local = p2sp::CStunClient::GetLocalFirstIP();
                // date
                time_stamp = time(NULL);

                bandwidth = (std::max)(statistic_info_.BandWidth, history_bandwith_);

                // 最低要求带宽为512kbps
                if (history_bandwith_ < 65535)
                {
                    history_bandwith_ = 65535;
                }

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

    // 设置全局window_size
    void StatisticModule::SetGlobalWindowSize(uint32_t global_window_size)
    {
        statistic_info_.GlobalWindowSize = global_window_size;
    }

    // 设置全局window_size
    void StatisticModule::SetGlobalRequestSendCount(uint32_t global_request_send_count)
    {
        statistic_info_.GlobalRequestSendCount = global_request_send_count;
    }

    // 设置全局window_size
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

        return upload_speed_meter_.CurrentByteSpeed();
    }

    uint32_t StatisticModule::GetMinuteUploadDataSpeed()
    {
        if (false == is_running_)
            return 0;

        return upload_speed_meter_.RecentMinuteByteSpeed();
    }

    uint32_t StatisticModule::GetUploadDataSpeedInKBps()
    {
        return GetUploadDataSpeed() / 1024;
    }

    uint32_t StatisticModule::GetRecentMinuteUploadDataSpeedInKBps()
    {
        return upload_speed_meter_.RecentMinuteByteSpeed() / 1024;
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
                STAT_DEBUG("QueryPeerInfoByRid Rid: " << rid << ", P2PDownloaderStatistic NULL");
            }
        }
        else
        {
            STAT_DEBUG("QueryPeerInfoByRid can't find rid: " << rid);
        }

        *iListCount = 0;
        *iConnectCount = 0;
        *iAverSpeed = 0;
        result_handler();
    }
}
