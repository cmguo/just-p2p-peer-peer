//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "statistic/UploadStatisticModule.h"
#include "statistic/StatisticUtil.h"
#include "DACStatisticModule.h"
#include <util/archive/LittleEndianBinaryOArchive.h>
#include <util/archive/ArchiveBuffer.h>

namespace statistic
{
    UploadStatisticModule::p UploadStatisticModule::inst_;
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_upload_statistic = log4cplus::Logger::getInstance("[upload_statistic]");
#endif

    UploadStatisticModule::UploadStatisticModule()
        : is_running_(false)
#ifdef STATISTIC_OFF
        : upload_count_(0)
#endif
    {

    }

    void UploadStatisticModule::Start()
    {
        is_running_ = true;

#ifndef STATISTIC_OFF
        if (false == CreateSharedMemory())
        {
            // log
        }
#endif

        upload_speed_info_.Start();
    }

    void UploadStatisticModule::Stop()
    {
        is_running_ = false;
        inst_.reset();
        upload_speed_info_.Stop();
    }

    void UploadStatisticModule::SubmitUploadInfo(boost::uint32_t upload_speed_limit, std::set<boost::asio::ip::address> uploading_peers_)
    {
#ifdef STATISTIC_OFF
        upload_count_ = uploading_peers_.size();
#else
        upload_info_.peer_upload_count = uploading_peers_.size();
        upload_info_.actual_speed_limit = upload_speed_limit;
#endif

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

    void UploadStatisticModule::SubmitUploadSpeedInfo(boost::asio::ip::address address, boost::uint32_t size)
    {
        upload_speed_info_.SubmitUploadedBytes(size);
        boost::uint32_t upload_speed = upload_speed_info_.GetSpeedInfo().NowUploadSpeed / 1024;
        DACStatisticModule::Inst()->SubmitP2PUploadSpeedInKBps(upload_speed);

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

#ifndef STATISTIC_OFF
    bool UploadStatisticModule::CreateSharedMemory()
    {
#ifdef PEER_PC_CLIENT
        shared_memory_.Create(GetSharedMemoryName().c_str(), GetSharedMemorySize());
        return shared_memory_.IsValid();
#else
        shared_memory_.Create(GetSharedMemoryName(), GetSharedMemorySize());
        return true;
#endif
    }

    void UploadStatisticModule::OnShareMemoryTimer(boost::uint32_t times)
    {
        if (false == is_running_)
        {
            return;
        }

        upload_info_.upload_speed = upload_speed_info_.GetSpeedInfo().NowUploadSpeed;

        int i = 0;
        for (std::map<boost::asio::ip::address, SpeedInfoStatistic>::iterator iter = m_upload_map.begin();
            iter != m_upload_map.end(); iter++)
        {
            boost::uint32_t ip = iter->first.to_v4().to_ulong();
            upload_info_.peer_upload_info[i].ip = ip;
            upload_info_.peer_upload_info[i].port = 0;
            upload_info_.peer_upload_info[i].upload_speed = iter->second.GetSpeedInfo().NowUploadSpeed;
            upload_info_.peer_upload_info[i].peer_info = upload_peer_info_[iter->first];
            i++;
            LOG4CPLUS_DEBUG_LOG(logger_upload_statistic, "IP = " << upload_info_.peer_upload_info[i].ip << " PORT = " 
                << upload_info_.peer_upload_info[i].port);
        }
        if (NULL != shared_memory_.GetView())
        {
            util::archive::ArchiveBuffer<> buf((char*)shared_memory_.GetView(), sizeof(UPLOAD_INFO));
            util::archive::LittleEndianBinaryOArchive<> oa(buf);
            oa << upload_info_;
        }
    }

    string UploadStatisticModule::GetSharedMemoryName()
    {
        return CreateUploadShareMemoryName(GetCurrentProcessID());
    }

    boost::uint32_t UploadStatisticModule::GetSharedMemorySize()
    {
        return sizeof(upload_info_);
    }
#endif

    boost::uint8_t UploadStatisticModule::GetUploadCount() const
    {
#ifdef STATISTIC_OFF
        return upload_count_;
#else
        return upload_info_.peer_upload_count;
#endif
    }

    boost::uint32_t UploadStatisticModule::GetUploadSpeed()
    {
        return upload_speed_info_.GetSpeedInfo().NowUploadSpeed;
    }

    boost::uint32_t UploadStatisticModule::GetUploadAvgSpeed()
    {
        return upload_speed_info_.GetSpeedInfo().AvgUploadSpeed;
    }

    void UploadStatisticModule::SubmitUploadPeerInfo(const boost::asio::ip::address & address, const statistic::PEER_INFO & peer_info)
    {
        upload_peer_info_[address] = peer_info;
    }

    boost::uint32_t UploadStatisticModule::GetUploadSpeed(const boost::asio::ip::address & address)
    {
        std::map<boost::asio::ip::address, SpeedInfoStatistic>::iterator iter = m_upload_map.find(address);

        if (iter == m_upload_map.end())
        {
            return 0;
        }

        return iter->second.GetSpeedInfo().NowUploadSpeed;
    }

#ifndef STATISTIC_OFF
    void UploadStatisticModule::SubmitUploadOneSubPiece()
    {
        ++upload_info_.upload_subpiece_count;
    }
#endif
}
