//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// UploadStatisticModule.h

#ifndef _STATISTIC_UPLOAD_STATISTIC_MODULE_H_
#define _STATISTIC_UPLOAD_STATISTIC_MODULE_H_

#include "message.h"

#include "statistic/StatisticStructs.h"
#include "statistic/SpeedInfoStatistic.h"

#include "interprocess/SharedMemory.h"

namespace statistic
{
    class UploadStatisticModule
        : public boost::noncopyable
        , public boost::enable_shared_from_this<UploadStatisticModule>
    {
    public:
        typedef boost::shared_ptr<UploadStatisticModule> p;
        void Start();
        void Stop();

        void OnShareMemoryTimer(uint32_t times);
        bool CreateSharedMemory();
        string GetSharedMemoryName();
        uint32_t GetSharedMemorySize();

        void SubmitUploadInfo(uint32_t upload_speed_limit, std::set<boost::asio::ip::address> uploading_peers_);
        void SubmitUploadSpeedInfo(boost::asio::ip::address address, uint32_t size);

        static UploadStatisticModule::p Inst()
        {
            if (!inst_)
            {
                inst_.reset(new UploadStatisticModule());
            }
            return inst_;
        }

    private:
        UploadStatisticModule();

        bool is_running_;
        static UploadStatisticModule::p inst_;
        interprocess::SharedMemory shared_memory_;
        UPLOAD_INFO upload_info_;

        std::map<boost::asio::ip::address, SpeedInfoStatistic> m_upload_map;

        SpeedInfoStatistic upload_speed_info_;
    };
}

#endif  // _STATISTIC_UPLOAD_STATISTIC_MODULE_H_
