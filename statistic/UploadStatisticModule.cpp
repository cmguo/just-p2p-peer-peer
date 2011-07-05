//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "statistic/UploadStatisticModule.h"
#include "statistic/StatisticUtil.h"

namespace statistic
{
    UploadStatisticModule::p UploadStatisticModule::inst_(new UploadStatisticModule());

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
    }

    void UploadStatisticModule::Stop()
    {
        is_running_ = false;
        inst_.reset();
    }

    void UploadStatisticModule::SubmitUploadInfo(uint32_t upload_speed_limit, std::set<boost::asio::ip::udp::endpoint> uploading_peers_)
    {
        upload_info_.peer_upload_count = uploading_peers_.size();
        upload_info_.speed_limit = upload_speed_limit;

        if (m_upload_map.size() == 0)
        {
            for (std::set<boost::asio::ip::udp::endpoint>::iterator iter = uploading_peers_.begin();
                iter != uploading_peers_.end(); iter++)
            {
                if (m_upload_map.find(*iter) == m_upload_map.end())
                {
                    // 找不到
                    boost::asio::ip::udp::endpoint end_point = *iter;
                    m_upload_map.insert(std::make_pair(end_point, SpeedInfoStatistic()));
                    m_upload_map[*iter].Start();
                }
            }
        }
        else
        {
            for (std::map<boost::asio::ip::udp::endpoint, SpeedInfoStatistic>::iterator iter = m_upload_map.begin();
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

    void UploadStatisticModule::SubmitUploadSpeedInfo(boost::asio::ip::udp::endpoint end_point, uint32_t size)
    {
        std::map<boost::asio::ip::udp::endpoint, SpeedInfoStatistic>::iterator iter = m_upload_map.find(end_point);
        if (iter != m_upload_map.end())
        {
            // 找到了
            iter->second.SubmitUploadedBytes(size);
        }
        else
        {
            m_upload_map.insert(std::make_pair(end_point, SpeedInfoStatistic()));
            m_upload_map[end_point].Start();
            m_upload_map[end_point].SubmitUploadedBytes(size);
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
        int i = 0;
        for (std::map<boost::asio::ip::udp::endpoint, SpeedInfoStatistic>::iterator iter = m_upload_map.begin();
            iter != m_upload_map.end(); iter++)
        {
            framework::network::Endpoint ep(iter->first);
            uint32_t ip = ep.ip_v4();
            boost::uint16_t port = ep.port();
            upload_info_.peer_upload_info[i].ip = ip;
            upload_info_.peer_upload_info[i].port = port;
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
