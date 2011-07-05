//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "downloadcenter/DownloadCenterModule.h"
#include "statistic/StatisticUtil.h"
#include "p2sp/proxy/ProxyModule.h"
#include "p2sp/proxy/ProxyConnection.h"
#include "p2sp/download/DownloadDriver.h"
#include "p2sp/p2p/P2PDownloader.h"
#include "storage/storage_base.h"
#include "storage/IStorage.h"
#include "storage/Instance.h"

namespace downloadcenter
{

    DownloadCenterModule::p DownloadCenterModule::inst_(new DownloadCenterModule());

    DownloadCenterModule::DownloadCenterModule()
        : flush_timer_(global_second_timer(), 1000, boost::bind(&DownloadCenterModule::OnTimerElapsed, this, &flush_timer_))
        , is_running_(false)
    {
    }

    //     DownloadCenterModule::p DownloadCenterModule::Create()
    //     {
    //         DownloadCenterModule::p module = DownloadCenterModule::p(new DownloadCenterModule());
    //         return module;
    //     }

    void DownloadCenterModule::Start(uint32_t flush_interval_in_millisec)
    {
        if (true == is_running_)
        {
            return;
        }
        is_running_ = true;

        ClearAllData();

        CreateSharedMemory();

        flush_timer_.interval(flush_interval_in_millisec);
        flush_timer_.start();

        FlushData();

    }

    void DownloadCenterModule::Stop()
    {
        if (false == is_running_)
        {
            return;
        }

        CloseSharedMemory();

        flush_timer_.stop();

        is_running_ = false;
        inst_.reset();
    }

    void DownloadCenterModule::ClearAllData()
    {
        if (false == is_running_)
        {
            return;
        }

        next_id_ = 1;
        free_id_list_.clear();
        strings_.clear();
        download_resources_.clear();

    }

    void DownloadCenterModule::UpdateDownloadResourceData(const DownloadResourceData& download_resource_data)
    {
        if (false == is_running_)
        {
            return;
        }
        // remove
        RemoveDownloadResourceData(download_resource_data);
        // insert
        internal::DOWNLOAD_RESOURCE_DATA data;
        data.FileName.StringID = CreateString(download_resource_data.FileName);
        data.StorePath.StringID = CreateString(download_resource_data.StorePath);
        data.WebUrl.StringID = CreateString(download_resource_data.WebUrl);
        data.DownloadUrl.StringID = CreateString(download_resource_data.DownloadUrl);
        data.RefererUrl.StringID = CreateString(download_resource_data.RefererUrl);
        data.IsDownloading = download_resource_data.IsDownloading;
        data.IsFinished = download_resource_data.IsFinished;
        data.DownloadStatus = download_resource_data.DownloadStatus;
        data.FileLength = download_resource_data.FileLength;
        data.FileDuration = download_resource_data.FileDuration;
        data.DataRate = download_resource_data.DataRate;
        data.DownloadedBytes = download_resource_data.DownloadedBytes;
        data.P2PDownloadBytes = download_resource_data.P2PDownloadBytes;
        data.HttpDownloadBytes = download_resource_data.HttpDownloadBytes;
        data.DownloadSpeed = download_resource_data.DownloadSpeed;
        data.P2PDownloadSpeed = download_resource_data.P2PDownloadSpeed;
        data.HttpDownloadSpeed = download_resource_data.HttpDownloadSpeed;
        // insert
        download_resources_[data.DownloadUrl.StringID] = data;
        // log
        LOG(__DEBUG, "downloadcenter", __FUNCTION__ << ":" << __LINE__
            << ", Size = " << download_resources_.size()
            << ", DownloadedBytes = " << download_resource_data.DownloadedBytes
            << ", DownloadSpeed = " << download_resource_data.DownloadSpeed
            << ", Url = " << download_resource_data.DownloadUrl
            << ", Status = " << download_resource_data.DownloadStatus);
    }

    void DownloadCenterModule::RemoveDownloadResourceData(const DownloadResourceData& download_resource_data)
    {
        if (false == is_running_)
        {
            return;
        }
        boost::uint16_t id = GetString(download_resource_data.DownloadUrl);
        if (id != 0)
        {
            DownloadResourceMap::iterator it = download_resources_.find(id);
            if (it != download_resources_.end())
            {
                // remove
                download_resources_.erase(it);
                // remove
                RemoveString(download_resource_data.FileName);
                RemoveString(download_resource_data.StorePath);
                RemoveString(download_resource_data.WebUrl);
                RemoveString(download_resource_data.DownloadUrl);
                RemoveString(download_resource_data.RefererUrl);
            }
        }
    }

    bool DownloadCenterModule::HasUrl(const string& url) const
    {
        if (false == is_running_)
        {
            return false;
        }
        // fetch id
        boost::uint16_t url_id = GetString(url);
        if (url_id == 0)
        {
            return false;
        }

        // check
        DownloadResourceMap::const_iterator it;
        for (it = download_resources_.begin(); it != download_resources_.end(); ++it)
        {
            const internal::DOWNLOAD_RESOURCE_DATA& res_data = it->second;
            boost::uint16_t strId = res_data.DownloadUrl.StringID;
            if (strId == url_id)
            {
                return true;
            }
        }
        return false;
    }

    bool DownloadCenterModule::IsUrlDownloading(const string& url) const
    {
        if (false == is_running_)
        {
            return false;
        }
        // fetch id
        boost::uint16_t url_id = GetString(url);
        if (url_id == 0)
        {
            return false;
        }

        // check
        DownloadResourceMap::const_iterator it;
        for (it = download_resources_.begin(); it != download_resources_.end(); ++it)
        {
            const internal::DOWNLOAD_RESOURCE_DATA& res_data = it->second;
            boost::uint16_t strId = res_data.DownloadUrl.StringID;
            if (strId == url_id)
            {
                return res_data.IsDownloading || res_data.IsFinished;
            }
        }
        return false;
    }

    bool DownloadCenterModule::CreateSharedMemory()
    {
        if (false == is_running_)
        {
            return false;
        }
        shared_memory_name_ = statistic::CreateDownloadCenterModuleSharedMemoryName(
            statistic::GetCurrentProcessID());
        // close
        shared_memory_.Close();
        shared_memory_.Create(shared_memory_name_, GetMaxSharedMemorySize());
        // result
        return shared_memory_.IsValid();
    }

    void DownloadCenterModule::CloseSharedMemory()
    {
        if (false == is_running_)
        {
            return;
        }
        shared_memory_.Close();
    }

    void DownloadCenterModule::FlushData()
    {
        if (false == is_running_)
        {
            return;
        }
        if (NULL == shared_memory_.GetView())
        {
            return;
        }

        uint32_t offset = 0;
        // header
        internal::DATA_HEADER *header = (internal::DATA_HEADER*)((boost::uint8_t*)shared_memory_.GetView() + offset);
        header->Version = 0x00000001;
        header->EndOffset = GetMaxSharedMemorySize();
        offset += sizeof(internal::DATA_HEADER);
        // strings
        header->StringStoreOffset = offset;
        internal::STRING_STORE *string_store = (internal::STRING_STORE*)((boost::uint8_t*)shared_memory_.GetView() + offset);
        string_store->StringCount = strings_.size();
        offset += sizeof(internal::STRING_STORE);
        for (StringStoreMap::iterator it = strings_.begin(); it != strings_.end(); ++it)
        {
            // str
            const string& str_value = it->first;
            boost::uint16_t id = (it->second & 0xFFFFu);
            // store
            internal::STRING * s = (internal::STRING *)((boost::uint8_t*)shared_memory_.GetView() + offset);
            s->ID = id;
            s->Length = str_value.length();
            base::util::memcpy2(s->Data, s->Length, str_value.c_str(), str_value.length());
            //
            offset += sizeof(internal::STRING) + s->Length;
        }
        // data
        header->DownloadResourceStoreOffset = offset;
        internal::DOWNLOAD_RESOURCE_STORE *resource_store =
            (internal::DOWNLOAD_RESOURCE_STORE*)((boost::uint8_t*)shared_memory_.GetView() + offset);
        resource_store->ResourceCount = download_resources_.size();
        offset += sizeof(internal::DOWNLOAD_RESOURCE_STORE);
        for (DownloadResourceMap::iterator it = download_resources_.begin(); it != download_resources_.end(); ++it)
        {
            internal::DOWNLOAD_RESOURCE_DATA *data =
                (internal::DOWNLOAD_RESOURCE_DATA*)((boost::uint8_t*)shared_memory_.GetView() + offset);
            *data = it->second;
            offset += sizeof(internal::DOWNLOAD_RESOURCE_DATA);
        }
        // log
        LOG(__DEBUG, "downloadcenter", __FUNCTION__ << " DownloadResource Size: " << download_resources_.size());
        // assert
        assert(offset < header->EndOffset);
    }

    void DownloadCenterModule::OnTimerElapsed(framework::timer::Timer * timer_ptr)
    {
        if (false == is_running_)
        {
            return;
        }

        if (timer_ptr == &flush_timer_)
        {
            PullActiveProxyConnectionsData();
            FlushData();
        }

    }

    boost::uint16_t DownloadCenterModule::GetString(const string& str_value) const
    {
        if (false == is_running_)
        {
            return 0;
        }
        StringStoreMap::const_iterator it = strings_.find(str_value);
        if (it != strings_.end())
        {
            return (it->second & 0xFFFFu);
        }
        return 0;
    }

    boost::uint16_t DownloadCenterModule::CreateString(const string& str_value)
    {
        if (false == is_running_)
        {
            return 0;
        }
        StringStoreMap::iterator it = strings_.find(str_value);
        if (it != strings_.end())
        {
            it->second += (1u << 16);
            return (it->second & 0xFFFFu);
        }
        else
        {
            boost::uint16_t id = NextID();
            strings_[str_value] = (id + (1u << 16));
            return id;
        }
    }

    boost::uint16_t DownloadCenterModule::RemoveString(const string& str_value)
    {
        if (false == is_running_)
        {
            return 0;
        }
        StringStoreMap::iterator it = strings_.find(str_value);
        if (it != strings_.end())
        {
            boost::uint16_t id = (it->second & 0xFFFFu);
            it->second -= (1u << 16);
            if ((it->second >> 16) == 0)
            {
                free_id_list_.push_back(id);
                strings_.erase(it);
            }
            return id;
        }
        return 0;
    }

    boost::uint16_t DownloadCenterModule::NextID()
    {
        if (false == is_running_)
        {
            return 0;
        }
        boost::uint16_t id;
        if (false == free_id_list_.empty())
        {
            id = free_id_list_.front();
            free_id_list_.pop_front();
        }
        else
        {
            id = next_id_++;
        }
        return id;
    }

    void DownloadCenterModule::PullActiveProxyConnectionsData()
    {
        if (false == is_running_)
        {
            return;
        }

        p2sp::ProxyModule::Inst()->ForEachProxyConnection(
            boost::bind(&DownloadCenterModule::ProxyConnectionProcessor,
            shared_from_this(), _1));
    }

    void DownloadCenterModule::ProxyConnectionProcessor(
        p2sp::ProxyConnection::p conn)
    {
        if (false == is_running_)
        {
            return;
        }

        p2sp::DownloadDriver::p dd = conn->GetDownloadDriver();
        LOG(__DEBUG, "downloadcenter", __FUNCTION__ << ":" << __LINE__
            << " proxy_connection = " << conn << " DownloadDriver = " << dd
            << " Running = " << (dd&&dd->IsRunning() ? true : false));

        if (dd && dd->IsRunning())
        {
            storage::Instance::p inst = dd->GetInstance();
            LOG(__DEBUG, "downloadcenter", __FUNCTION__ << ":" << __LINE__ << " inst = " << inst);
            if (inst && inst->IsSaveMode())
            {
                LOG(__DEBUG, "downloadcenter", __FUNCTION__ << ":" << __LINE__ << " save mode inst!");
                DownloadResourceData res_data;
                inst->GetDownloadResourceData(res_data);

                statistic::DownloadDriverStatistic::p statistic = dd->GetStatistic();
                p2sp::P2PDownloader::p p2p_downloader = dd->GetP2PDownloader();

                res_data.DownloadUrl = conn->GetSourceUrl();
                LOG(__DEBUG, "downloadcenter", __FUNCTION__ << ":" << __LINE__ << " proxy_conn source_url = " << res_data.DownloadUrl);

                if (inst->IsComplete())
                {
                    res_data.IsDownloading = false;
                    if (res_data.DownloadStatus != DOWNLOAD_LOCAL)
                    {
                        res_data.DownloadStatus = DOWNLOAD_COMPLETE;
                    }
                }
                else if (false == res_data.IsFinished)
                {
                    if (true == conn->IsWillStopDownload())
                    {
                        res_data.IsDownloading = false;
                        if (true == conn->IsPausedByUser())
                        {
                            res_data.DownloadStatus = NOT_DOWNLOADING;
                        }
                        else
                        {
                            res_data.DownloadStatus = DOWNLOAD_FAILED;
                        }
                    }
                    else
                    {
                        res_data.IsDownloading = true;
                        res_data.DownloadStatus = IS_DOWNLOADING;
                    }
                }
                else
                {
                    res_data.IsDownloading = true;
                    res_data.DownloadStatus = IS_DOWNLOADING;
                }

                res_data.HttpDownloadBytes = statistic->GetTotalHttpDataBytes();
                res_data.HttpDownloadSpeed = statistic->GetSpeedInfo().AvgDownloadSpeed;
                res_data.P2PDownloadBytes
                    = (p2p_downloader && p2p_downloader->GetStatistic() ? (p2p_downloader->GetStatistic()->GetTotalP2PDataBytes())
                    : 0);
               ;
                res_data.P2PDownloadSpeed
                    = (p2p_downloader && p2p_downloader->GetStatistic() ? (p2p_downloader->GetStatistic()->GetSpeedInfo().AvgDownloadSpeed)
                    : 0);
                res_data.DownloadSpeed = res_data.HttpDownloadSpeed + res_data.P2PDownloadSpeed;

                LOG(__DEBUG, "downloadcenter", __FUNCTION__ << ":" << __LINE__ << " DownloadBytes = " << res_data.DownloadedBytes
                    << " DownloadSpeed = " << res_data.DownloadSpeed << " DownloadStatus = " << res_data.DownloadStatus
                    << " IsDownloading = " << res_data.IsDownloading << " IsFinished = " << res_data.IsFinished);

                // update
                UpdateDownloadResourceData(res_data);
            }
        }
    }

}
