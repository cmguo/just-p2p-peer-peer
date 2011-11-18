//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "statistic/UploadStatisticModule.h"
#include "statistic/StatisticUtil.h"

namespace statistic
{
    UploadStatisticModule::p UploadStatisticModule::inst_;

    UploadStatisticModule::UploadStatisticModule()
        : is_running_(false)
    {

    }

    void UploadStatisticModule::Start()
    {
        is_running_ = true;

        if (false == CreateSharedMemory())
        {
            // log
        }

        upload_speed_info_.Start();
    }

    void UploadStatisticModule::Stop()
    {
        is_running_ = false;
        inst_.reset();
        upload_speed_info_.Stop();
    }

    void UploadStatisticModule::SubmitUploadInfo(uint32_t upload_speed_limit, std::set<boost::asio::ip::address> uploading_peers_)
    {
        upload_info_.peer_upload_count = uploading_peers_.size();
        upload_info_.speed_limit = upload_speed_limit;

        if (m_upload_map.size() == 0)
        {
            for (std::set<boost::asio::ip::address>::iterator iter = uploading_peers_.begin();
                iter != uploading_peers_.end(); iter++)
            {
                if (m_upload_map.find(*iter) == m_upload_map.end())
                {
                    boost::asio::ip::address ip_address = *iter;
                    m_upload_map.insert(std::make_pair(ip_address, SpeedInfoStatistic()));
                    m_upload_map[*iter].Start();
                }
            }
        }
        else
        {
            for (std::map<boost::asio::ip::address, SpeedInfoStatistic>::iterator iter = m_upload_map.begin();
                iter != m_upload_map.end();)
            {
                if (uploading_peers_.find(iter->first) == uploading_peers_.end())
                {
                    // 不在上传集合中，删除
                    m_upload_map.erase(iter++);
                }
                else
                {
                    iter++;
                }
            }
        }
    }

    void UploadStatisticModule::SubmitUploadSpeedInfo(boost::asio::ip::address address, uint32_t size)
    {
        upload_speed_info_.SubmitUploadedBytes(size);

        std::map<boost::asio::ip::address, SpeedInfoStatistic>::iterator iter = m_upload_map.find(address);
        if (iter != m_upload_map.end())
        {
            // 找到了
            iter->second.SubmitUploadedBytes(size);
        }
        else
        {
            m_upload_map.insert(std::make_pair(address, SpeedInfoStatistic()));
            m_upload_map[address].Start();
            m_upload_map[address].SubmitUploadedBytes(size);
        }
    }

    bool UploadStatisticModule::CreateSharedMemory()
    {
#ifdef BOOST_WINDOWS_API
        shared_memory_.Create(GetSharedMemoryName().c_str(), GetSharedMemorySize());
        return shared_memory_.IsValid();
#else
        shared_memory_.Create(GetSharedMemoryName(), GetSharedMemorySize());
        return true;
#endif
    }

    void UploadStatisticModule::OnShareMemoryTimer(uint32_t times)
    {
        upload_info_.upload_speed = upload_speed_info_.GetSpeedInfo().NowUploadSpeed;

        int i = 0;
        for (std::map<boost::asio::ip::address, SpeedInfoStatistic>::iterator iter = m_upload_map.begin();
            iter != m_upload_map.end(); iter++)
        {
            uint32_t ip = iter->first.to_v4().to_ulong();
            upload_info_.peer_upload_info[i].ip = ip;
            upload_info_.peer_upload_info[i].port = 0;
            upload_info_.peer_upload_info[i].upload_speed = iter->second.GetSpeedInfo().NowUploadSpeed;
            i++;
            LOG(__DEBUG, "ppbug", "IP = " << upload_info_.peer_upload_info[i].ip << " PORT = " << upload_info_.peer_upload_info[i].port);
        }

        if (NULL != shared_memory_.GetView())
        {
            base::util::memcpy2(shared_memory_.GetView(), GetSharedMemorySize(), &upload_info_, sizeof(upload_info_));
        }
    }

    string UploadStatisticModule::GetSharedMemoryName()
    {
        return CreateUploadShareMemoryName(GetCurrentProcessID());
    }

    uint32_t UploadStatisticModule::GetSharedMemorySize()
    {
        return sizeof(upload_info_);
    }
}
