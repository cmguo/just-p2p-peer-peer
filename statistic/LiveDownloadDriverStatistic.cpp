//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "statistic/LiveDownloadDriverStatistic.h"
#include "statistic/SpeedInfoStatistic.h"
#include "statistic/StatisticUtil.h"
#include "p2sp/download/LiveDownloadDriver.h"

namespace statistic
{
    LiveDownloadDriverStatistic::p LiveDownloadDriverStatistic::Create(boost::uint32_t id)
    {
        return LiveDownloadDriverStatistic::p(new LiveDownloadDriverStatistic(id));
    }

    LiveDownloadDriverStatistic::LiveDownloadDriverStatistic(boost::uint32_t id)
        : is_running_(false)
        , download_driver_id_(id)
    {
    }

    void LiveDownloadDriverStatistic::Start(p2sp::LiveDownloadDriver__p live_download_driver)
    {
        if (true == is_running_)
        {
            return;
        }

        is_running_ = true;
        live_download_driver_ = live_download_driver;

        Clear();

        if (!CreateSharedMemory())
        {
            assert(false);
        }
    }

    void LiveDownloadDriverStatistic::Stop()
    {
        if (false == is_running_)
        {
            return;
        }

        Clear();

#ifndef STATISTIC_OFF
        UpdateShareMemory();
#endif

        is_running_ = false;

        shared_memory_.Close();
        live_download_driver_.reset();
    }

    void LiveDownloadDriverStatistic::Clear()
    {
        download_driver_statistic_info_.Clear();
    }

#ifndef STATISTIC_OFF
    const LIVE_DOWNLOADDRIVER_STATISTIC_INFO& LiveDownloadDriverStatistic::TakeSnapshot()
    {
        download_driver_statistic_info_ = live_download_driver_->GetLiveDownloadDirverStatisticInfo();
        return download_driver_statistic_info_;
    }
#endif

#ifndef STATISTIC_OFF
    void LiveDownloadDriverStatistic::UpdateShareMemory()
    {
        if (false == is_running_)
        {
            return;
        }

        TakeSnapshot();

        // write to memory
        if (NULL != shared_memory_.GetView())
        {
            util::archive::ArchiveBuffer<> buf((char*)shared_memory_.GetView(), GetSharedMemorySize());
            util::archive::LittleEndianBinaryOArchive<> oa(buf);
            oa << this->download_driver_statistic_info_;
            assert(oa);
        }
    }
#endif

    //////////////////////////////////////////////////////////////////////////
    // Shared Memory
    bool LiveDownloadDriverStatistic::CreateSharedMemory()
    {
        shared_memory_.Create(GetSharedMemoryName(), GetSharedMemorySize());
        return shared_memory_.IsValid();
    }

    string LiveDownloadDriverStatistic::GetSharedMemoryName()
    {
        return CreateLiveDownloadDriverModuleSharedMemoryName(GetCurrentProcessID(), download_driver_id_);
    }

    boost::uint32_t LiveDownloadDriverStatistic::GetSharedMemorySize()
    {
        return sizeof(download_driver_statistic_info_);
    }
}
