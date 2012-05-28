//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "statistic/DownloadDriverStatistic.h"
#include "statistic/SpeedInfoStatistic.h"
#include "statistic/StatisticUtil.h"
#include "statistic/DACStatisticModule.h"

#include "base/wsconvert.h"

#include <framework/string/Url.h>
#include <util/archive/ArchiveBuffer.h>
#include <util/archive/LittleEndianBinaryOArchive.h>

namespace statistic
{
    DownloadDriverStatistic::DownloadDriverStatistic(uint32_t id, bool is_create_share_memory)
        : is_running_(false)
        , download_driver_id_(id)
        , http_download_max_speed_(0)
        , is_flush_shared_memory_(is_create_share_memory)
    {
    }

    DownloadDriverStatistic::p DownloadDriverStatistic::Create(int id, bool is_create_share_memory)
    {
        return p(new DownloadDriverStatistic(id, is_create_share_memory));
    }

    void DownloadDriverStatistic::Start()
    {
        STAT_DEBUG("DownloadDriverStatistic::Start [IN]");
        if (is_running_ == true)
        {
            STAT_WARN("DownloadDriverStatistic is running, return.");
            return;
        }

        is_running_ = true;

        Clear();

        download_driver_statistic_info_.DownloadDriverID = download_driver_id_;
        SetOriginalUrl(original_url_);
        SetOriginalReferUrl(original_refer_url_);
        STAT_DEBUG("    Download Driver ID: " << download_driver_id_);

        download_driver_statistic_info_.IsHidden = false;

        speed_info_.Start();

        if (is_flush_shared_memory_)
        {
            if (CreateSharedMemory() == false)
            {
                STAT_ERROR("Shared Memory Creation Failed: DownloadDriverStatistic");
            }
        }

        STAT_DEBUG("DownloadDriverStatistic::Start [OUT]");
    }

    void DownloadDriverStatistic::Stop()
    {
        STAT_DEBUG("DownloadDriverStatistic::Stop [IN]");
        if (is_running_ == false)
        {
            STAT_WARN("DownloadDriverStatistic is not running, return.");
            return;
        }

        speed_info_.Stop();
        STAT_DEBUG("   Speed Info Stopped.");

        Clear();

        // 在停止之前将共享内存清0
        if (NULL != shared_memory_.GetView())
        {
            util::archive::ArchiveBuffer<> buf((char*)shared_memory_.GetView(), GetSharedMemorySize());
            util::archive::LittleEndianBinaryOArchive<> oa(buf);
            oa << this->download_driver_statistic_info_;
        }

        shared_memory_.Close();
        STAT_DEBUG("   Shared Memory Closed.");

        is_running_ = false;
        STAT_DEBUG("DownloadDriverStatistic::Stop [OUT]");
    }

    void DownloadDriverStatistic::Clear()
    {
        speed_info_.Clear();
        download_driver_statistic_info_.Clear();
        peer_info_.Clear();
        DetachAllHttpDownloaderStatistic();
        STAT_DEBUG("DownloadDriverStatistic::Clear");
    }

    bool DownloadDriverStatistic::IsRunning() const
    {
        return is_running_;
    }

    const DOWNLOADDRIVER_STATISTIC_INFO& DownloadDriverStatistic::TakeSnapshot()
    {
        UpdateSpeedInfo();
        UpdateHttpDownloaderInfo();
        return download_driver_statistic_info_;
    }

    void DownloadDriverStatistic::OnShareMemoryTimer(uint32_t times)
    {
        STAT_DEBUG("DownloadDriverStatistic::OnShareMemoryTimer [IN], times: " << times);
        if (is_running_ == false)
        {
            STAT_WARN("DownloadDriverStatistic is not running, return.");
            return;
        }

        // Update
        TakeSnapshot();

        // write to memory
        if (NULL != shared_memory_.GetView())
        {
            util::archive::ArchiveBuffer<> buf((char*)shared_memory_.GetView(), GetSharedMemorySize());
            util::archive::LittleEndianBinaryOArchive<> oa(buf);
            oa << this->download_driver_statistic_info_;
            assert(oa);

            // memcpy(shared_memory_.GetView(), &info, GetSharedMemorySize());
            STAT_DEBUG("Write DOWNLOADDRIVER_STATISTIC_INFO into SharedMemory");
        }
        STAT_DEBUG("DownloadDriverStatistic::OnShareMemoryTimer [OUT]");
    }

    //////////////////////////////////////////////////////////////////////////
    // Operations

    HttpDownloaderStatistic::p DownloadDriverStatistic::AttachHttpDownloaderStatistic(const string& url)
    {


        HttpDownloaderStatistic::p http_downloader_;

        if (is_running_ == false)
        {

            return http_downloader_;
        }

        // 判断MaxCount
        if (http_downloader_statistic_map_.size() == GetMaxHttpDownloaderCount())
        {

            return http_downloader_;
        }

        // 存在, 返回空
        if (http_downloader_statistic_map_.find(url) != http_downloader_statistic_map_.end())
        {
            http_downloader_ = http_downloader_statistic_map_[url];

            return http_downloader_;
        }

        // 创建并开始
        http_downloader_ = HttpDownloaderStatistic::Create(url, shared_from_this());
        http_downloader_statistic_map_[ url ] = http_downloader_;

        http_downloader_->Start();



        return http_downloader_;
    }

    bool DownloadDriverStatistic::DetachHttpDownloaderStatistic(const string& url)
    {
        STAT_DEBUG("DownloadDriverStatistic::DetachHttpDownloaderStatistic [IN], Url: " << url);

        if (is_running_ == false)
        {
            STAT_WARN("DownloadDriverStatistic is not running. return false.");
            return false;
        }

        // 不存在, 返回
        HttpDownloaderStatisticMap::iterator it = http_downloader_statistic_map_.find(url);
        if (it == http_downloader_statistic_map_.end())
        {
            STAT_WARN("Return false. Can not find given url: " << url);
            return false;
        }

        assert(it->second);

        it->second->Stop();
        http_downloader_statistic_map_.erase(it);
        STAT_DEBUG("Stopped HttpDownloader Statistic, and erased from std::map.");

        STAT_DEBUG("DownloadDriverStatistic::DetachHttpDownloaderStatistic [OUT]");
        return true;
    }

    bool DownloadDriverStatistic::DetachHttpDownloaderStatistic(const HttpDownloaderStatistic::p http_downloader_statistic)
    {
        return DetachHttpDownloaderStatistic(http_downloader_statistic->GetUrl());
    }

    bool DownloadDriverStatistic::DetachAllHttpDownloaderStatistic()
    {
        for (HttpDownloaderStatisticMap::iterator it = http_downloader_statistic_map_.begin();
            it != http_downloader_statistic_map_.end(); it++)
        {
            it->second->Stop();
        }
        // clear
        http_downloader_statistic_map_.clear();

        return true;
    }

    //////////////////////////////////////////////////////////////////////////
    // Url Info

    void DownloadDriverStatistic::SetOriginalUrl(
        const string& original_url)
    {
        original_url_ = original_url;
        framework::string::Url::truncate_to(original_url, download_driver_statistic_info_.OriginalUrl);
        STAT_DEBUG("DownloadDriverStatistic::SetOriginalReferUrl, Original Url: " << original_url);
    }

    void DownloadDriverStatistic::SetOriginalReferUrl(const string& original_refer_url)
    {
        original_refer_url_ = original_refer_url;
        framework::string::Url::truncate_to(original_refer_url, download_driver_statistic_info_.OriginalReferUrl);
        STAT_DEBUG("DownloadDriverStatistic::SetOriginalReferUrl, Original Refer Url: " << original_refer_url);
    }

    //////////////////////////////////////////////////////////////////////////
    // Misc

    uint32_t DownloadDriverStatistic::GetDownloadDriverID() const
    {
        return download_driver_id_;
    }

    uint32_t DownloadDriverStatistic::GetMaxHttpDownloaderCount() const
    {
        return MAX_HTTP_DOWNLOADER_COUNT;
    }

    //////////////////////////////////////////////////////////////////////////
    // Resource Info

    void DownloadDriverStatistic::SetResourceID(const RID& rid)
    {
        download_driver_statistic_info_.ResourceID = rid;
    }

    RID DownloadDriverStatistic::GetResourceID()
    {
        return download_driver_statistic_info_.ResourceID;
    }

    void DownloadDriverStatistic::SetFileLength(uint32_t file_length)
    {
        download_driver_statistic_info_.FileLength = file_length;
    }

    uint32_t DownloadDriverStatistic::GetFileLength()
    {
        return download_driver_statistic_info_.FileLength;
    }

    void DownloadDriverStatistic::SetBlockSize(uint32_t block_size)
    {
        download_driver_statistic_info_.BlockSize = block_size;
    }

    uint32_t DownloadDriverStatistic::GetBlockSize()
    {
        return download_driver_statistic_info_.BlockSize;
    }

    void DownloadDriverStatistic::SetBlockCount(boost::uint16_t block_count)
    {
        download_driver_statistic_info_.BlockCount = block_count;
    }

    boost::uint16_t DownloadDriverStatistic::GetBlockCount()
    {
        return download_driver_statistic_info_.BlockCount;
    }

    void DownloadDriverStatistic::SetFileName(const string& file_name)
    {
#ifdef PEER_PC_CLIENT
        std::wstring name = base::s2ws(file_name);
        if (name.length() * sizeof(wchar_t) >= 256) {
            name = name.substr(0, 256 / sizeof(wchar_t));
        }
#else
        string name = file_name;
#endif
        memset(download_driver_statistic_info_.FileName, 0, 256);
        base::util::memcpy2(
            download_driver_statistic_info_.FileName, 
            sizeof(download_driver_statistic_info_.FileName), 
            name.c_str(), 
            name.length() * sizeof(name[0]));
    }

    string DownloadDriverStatistic::GetFileName()
    {
        string name((char*)(download_driver_statistic_info_.FileName));
        return name;
    }

    //////////////////////////////////////////////////////////////////////////
    // Speed Info & HTTP Downloader Info

    void DownloadDriverStatistic::SubmitDownloadedBytes(uint32_t downloaded_bytes)
    {
        speed_info_.SubmitDownloadedBytes(downloaded_bytes);
    }

    void DownloadDriverStatistic::SubmitUploadedBytes(uint32_t uploaded_bytes)
    {
        speed_info_.SubmitUploadedBytes(uploaded_bytes);
    }

    SPEED_INFO DownloadDriverStatistic::GetSpeedInfo()
    {
        if (false == is_running_) {
            return SPEED_INFO();
        }
        UpdateSpeedInfo();
        return download_driver_statistic_info_.SpeedInfo;
    }

    void DownloadDriverStatistic::UpdateSpeedInfo()
    {
        STAT_DEBUG("DownloadDriverStatistic::UpdateSpeedInfo");
        if (is_running_ == false)
        {
            STAT_WARN("DownloadDriverStatistic Module is not running, return.");
            return;
        }

        download_driver_statistic_info_.SpeedInfo = speed_info_.GetSpeedInfo();
    }

    void DownloadDriverStatistic::UpdateHttpDownloaderInfo()
    {
        STAT_DEBUG("DownloadDriverStatistic::UpdateHttpDownloaderInfo [IN]");
        if (is_running_ == false)
        {
            STAT_WARN("DownloadDriverStatistic module is running, return.");
            return;
        }

        download_driver_statistic_info_.HttpDownloaderCount = http_downloader_statistic_map_.size();
        STAT_DEBUG("Http Downloader Count: " << http_downloader_statistic_map_.size());

        assert(download_driver_statistic_info_.HttpDownloaderCount <= GetMaxHttpDownloaderCount());

        HttpDownloaderStatisticMap::iterator it = http_downloader_statistic_map_.begin();
        for (uint32_t i = 0; it != http_downloader_statistic_map_.end(); it++, i++)
        {
            assert(it->second);
            download_driver_statistic_info_.HttpDownloaders[i] = it->second->GetHttpDownloaderInfo();
        }
        STAT_DEBUG("DownloadDriverStatistic::UpdateHttpDownloaderInfo [OUT]");
    }

    //////////////////////////////////////////////////////////////////////////
    // HTTP Data Bytes

    void DownloadDriverStatistic::SubmitHttpDataBytesWithRedundance(uint32_t http_data_bytes)
    {
        download_driver_statistic_info_.TotalHttpDataBytesWithRedundance += http_data_bytes;
    }

    void DownloadDriverStatistic::SubmitHttpDataBytesWithoutRedundance(uint32_t http_data_bytes)
    {
        download_driver_statistic_info_.TotalHttpDataBytesWithoutRedundance += http_data_bytes;
    }


    void DownloadDriverStatistic::SetLocalDataBytes(uint32_t local_data_bytes)
    {
        download_driver_statistic_info_.TotalLocalDataBytes = local_data_bytes;
    }
    //////////////////////////////////////////////////////////////////////////
    // HTTP Max Download Speed

    uint32_t DownloadDriverStatistic::GetHttpDownloadMaxSpeed()
    {
        STL_FOR_EACH_CONST(HttpDownloaderStatisticMap, http_downloader_statistic_map_, iter)
        {
            HttpDownloaderStatistic::p statistic = iter->second;
            uint32_t now_speed = statistic->GetSpeedInfo().NowDownloadSpeed;
            if (now_speed > http_download_max_speed_)
            {
                http_download_max_speed_ = now_speed;
            }
        }
        return http_download_max_speed_;
    }

    uint32_t DownloadDriverStatistic::GetHttpDownloadAvgSpeed()
    {
        uint32_t http_avg = 0;
        STL_FOR_EACH_CONST(HttpDownloaderStatisticMap, http_downloader_statistic_map_, iter)
        {
            HttpDownloaderStatistic::p statistic = iter->second;
            uint32_t avg_speed = statistic->GetSpeedInfo().AvgDownloadSpeed;
            // !
            http_avg += avg_speed;
        }
        return http_avg;
    }

    //////////////////////////////////////////////////////////////////////////
    // Shared Memory
    bool DownloadDriverStatistic::CreateSharedMemory()
    {

        shared_memory_.Create(GetSharedMemoryName(), GetSharedMemorySize());
        bool result = shared_memory_.IsValid();

        return result;
    }

    string DownloadDriverStatistic::GetSharedMemoryName()
    {
        return CreateDownloadDriverModuleSharedMemoryName(GetCurrentProcessID(), GetDownloadDriverID());
    }

    uint32_t DownloadDriverStatistic::GetSharedMemorySize()
    {
        return sizeof(download_driver_statistic_info_);
    }

    bool DownloadDriverStatistic::IsHidden()
    {
        return 0 != download_driver_statistic_info_.IsHidden;
    }

    void DownloadDriverStatistic::SetHidden(bool is_hidden)
    {
        download_driver_statistic_info_.IsHidden = is_hidden;
    }

    void DownloadDriverStatistic::SetSourceType(boost::uint32_t source_type)
    {
        this->download_driver_statistic_info_.SourceType = source_type;
    }
    //////////////////////////////////////////////////////////////////////////
    // State Machine

    void DownloadDriverStatistic::SetStateMachineType(boost::uint8_t state_machine_type)
    {
        if (false == is_running_)
            return;
        download_driver_statistic_info_.StateMachineType = state_machine_type;
    }

    boost::uint8_t DownloadDriverStatistic::GetStateMachineType()
    {
        return download_driver_statistic_info_.StateMachineType;
    }

    void DownloadDriverStatistic::SetStateMachineState(const string& state)
    {
        assert(sizeof(download_driver_statistic_info_.StateMachineState) /sizeof(uint8_t) > state.length());
        strcpy((char*)download_driver_statistic_info_.StateMachineState, state.c_str());
    }

    string DownloadDriverStatistic::GetStateMachineState()
    {
        return string((const char*)download_driver_statistic_info_.StateMachineState);
    }

    //////////////////////////////////////////////////////////////////////////
    // PlayingPosition
    uint32_t DownloadDriverStatistic::GetPlayingPosition()
    {
        return download_driver_statistic_info_.PlayingPosition;
    }

    void DownloadDriverStatistic::SetQueriedPeerCount(boost::uint16_t QueriedPeerCount)
    {
        if (QueriedPeerCount > peer_info_.uQueriedPeerCount)
        {
            peer_info_.uQueriedPeerCount = QueriedPeerCount;
        }
    }

    boost::uint16_t DownloadDriverStatistic::GetQueriedPeerCount()
    {
        return peer_info_.uQueriedPeerCount;
    }

    void DownloadDriverStatistic::SetConnectedPeerCount(boost::uint16_t ConnectedPeerCount)
    {
        if (ConnectedPeerCount > peer_info_.uConnectedPeerCount)
        {
            peer_info_.uConnectedPeerCount = ConnectedPeerCount;
        }
    }

    boost::uint16_t DownloadDriverStatistic::GetConnectedPeerCount()
    {
        return peer_info_.uConnectedPeerCount;
    }

    void DownloadDriverStatistic::SetFullPeerCount(boost::uint16_t FullPeerCount)
    {
        if (FullPeerCount > peer_info_.uFullPeerCount)
        {
            peer_info_.uFullPeerCount = FullPeerCount;
        }
    }

    boost::uint16_t DownloadDriverStatistic::GetFullPeerCount()
    {
        return peer_info_.uFullPeerCount;
    }

    void DownloadDriverStatistic::SetMaxActivePeerCount(boost::uint16_t MaxActivePeerCount)
    {
        if (MaxActivePeerCount > peer_info_.uMaxActivePeerCount)
        {
            peer_info_.uMaxActivePeerCount = MaxActivePeerCount;
        }
    }

    boost::uint16_t DownloadDriverStatistic::GetMaxActivePeerCount()
    {
        return peer_info_.uMaxActivePeerCount;
    }

    void DownloadDriverStatistic::SetDataRate(uint32_t date_rate)
    {
        download_driver_statistic_info_.DataRate = date_rate;
    }

    void DownloadDriverStatistic::SetHttpState(boost::uint8_t h)
    {
        download_driver_statistic_info_.http_state = h;
    }

    void DownloadDriverStatistic::SetP2PState(boost::uint8_t p)
    {
        download_driver_statistic_info_.p2p_state = p;
    }

    void DownloadDriverStatistic::SetSnState(boost::uint8_t sn)
    {
        download_driver_statistic_info_.sn_state = sn;
    }

    void DownloadDriverStatistic::SetTimerusingState(boost::uint8_t tu)
    {
        download_driver_statistic_info_.timer_using_state = tu;
    }

    void DownloadDriverStatistic::SetTimerState(boost::uint8_t t)
    {
        download_driver_statistic_info_.timer_state = t;
    }

    void DownloadDriverStatistic::SetWebUrl(string web_url)
    {
        int32_t max_length = sizeof(download_driver_statistic_info_.WebUrl) - 1;
        strncpy((char*)download_driver_statistic_info_.WebUrl, web_url.c_str(), max_length);
        download_driver_statistic_info_.WebUrl[max_length] = '\0';
    }

    void DownloadDriverStatistic::SetSmartPara(boost::int32_t t, boost::int32_t b, boost::int32_t speed_limit)
    {
        download_driver_statistic_info_.t = t;
        download_driver_statistic_info_.b = b;
        download_driver_statistic_info_.speed_limit = speed_limit;
    }
}
