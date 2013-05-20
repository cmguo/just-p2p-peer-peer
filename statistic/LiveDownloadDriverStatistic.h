//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _STATISTIC_LIVE_DOWNLOAD_DRIVER_STATISTIC_H_
#define _STATISTIC_LIVE_DOWNLOAD_DRIVER_STATISTIC_H_

#include "statistic/SpeedInfoStatistic.h"
#include "statistic/StatisticStructs.h"
#include "interprocess/SharedMemory.h"

namespace p2sp
{
    class LiveDownloadDriver;
    typedef boost::shared_ptr<LiveDownloadDriver> LiveDownloadDriver__p;
}

namespace statistic
{
    class LiveDownloadDriverStatistic
        : public boost::noncopyable
#ifdef DUMP_OBJECT
        , public count_object_allocate<LiveDownloadDriverStatistic>
#endif
    {
    public:
        typedef boost::shared_ptr<LiveDownloadDriverStatistic> p;

        static p Create(boost::uint32_t id);

        void Start(p2sp::LiveDownloadDriver__p live_download_driver);
        void Stop();

        boost::uint32_t GetLiveDownloadDriverId() const { return download_driver_id_; }

#ifndef STATISTIC_OFF
        void UpdateShareMemory();
        const LIVE_DOWNLOADDRIVER_STATISTIC_INFO& TakeSnapshot();
#endif

    private:
        LiveDownloadDriverStatistic(boost::uint32_t id);

        //////////////////////////////////////////////////////////////////////////
        // Shared Memory
        bool CreateSharedMemory();
        string GetSharedMemoryName();
        boost::uint32_t GetSharedMemorySize();
        void Clear();

    private:

        volatile bool is_running_;
        LIVE_DOWNLOADDRIVER_STATISTIC_INFO download_driver_statistic_info_;
        boost::uint32_t download_driver_id_;
        interprocess::SharedMemory shared_memory_;
        p2sp::LiveDownloadDriver__p live_download_driver_;
    };
}
#endif  // _STATISTIC_LIVE_DOWNLOAD_DRIVER_STATISTIC_H_
