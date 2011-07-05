//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "statistic/StatisticsData.h"
#include <util/archive/ArchiveBuffer.h>
#include <util/archive/LittleEndianBinaryOArchive.h>
#include <util/archive/LittleEndianBinaryIArchive.h>

namespace statistic
{
    StatisticsData::StatisticsData()
    {
    }

    StatisticsData::StatisticsData(const STASTISTIC_INFO& statistics_info)
        : statistics_info_(statistics_info)
    {
    }

    int StatisticsData::Serialize(boost::uint8_t bytes[], size_t max_size)
    {
        util::archive::ArchiveBuffer<> buffer(reinterpret_cast<char*>(bytes), max_size);
        util::archive::LittleEndianBinaryOArchive<> output(buffer);
        
        output << statistics_info_;

        int num_of_p2p_downloaders = this->p2p_downloader_statistics_.size();
        output << num_of_p2p_downloaders;
        for(int i = 0; 
            i < num_of_p2p_downloaders && output; 
            ++i)
        {
            int bytes_serialized = p2p_downloader_statistics_[i]->Serialize(
                bytes + buffer.size(), 
                max_size - buffer.size());
            if (bytes_serialized > 0)
            {
                buffer.commit(bytes_serialized);
            }
        }

        int num_of_vod_downloader_drivers = this->vod_download_driver_statistics_.size();
        output << num_of_vod_downloader_drivers;
        for(int i = 0; 
            i < num_of_vod_downloader_drivers && output;
            ++i)
        {
            int bytes_serialized = vod_download_driver_statistics_[i]->Serialize(
                bytes + buffer.size(),
                max_size - buffer.size());
            if (bytes_serialized > 0)
            {
                buffer.commit(bytes_serialized);
            }
        }

        int num_of_live_downloader_drivers = this->live_download_driver_statistics_.size();
        output << num_of_live_downloader_drivers;
        for(int i = 0; 
            i < num_of_live_downloader_drivers && output;
            ++i)
        {
            int bytes_serialized = live_download_driver_statistics_[i]->Serialize(
                bytes + buffer.size(),
                max_size - buffer.size());
            if (bytes_serialized > 0)
            {
                buffer.commit(bytes_serialized);
            }
        }

        return output ? buffer.size() : -1;
    }

    int StatisticsData::Deserialize(const protocol::VERSION_INFO& version, const boost::uint8_t bytes[], size_t buffer_size)
    {
        util::archive::ArchiveBuffer<> buffer(reinterpret_cast<char*>(const_cast<boost::uint8_t*>(bytes)), buffer_size, buffer_size);

        util::archive::LittleEndianBinaryIArchive<> input(buffer);

        vod_download_driver_statistics_.clear();
        live_download_driver_statistics_.clear();
        p2p_downloader_statistics_.clear();

        input >> statistics_info_;
        
        int num_of_p2p_downloaders = 0;
        input >> num_of_p2p_downloaders;
        for(int i = 0; i < num_of_p2p_downloaders && input; ++i)
        {
            boost::shared_ptr<P2PDownloaderStatisticsData> downloader_statistics(new P2PDownloaderStatisticsData());
            int bytes_read = downloader_statistics->Deserialize(
                version,
                bytes + buffer_size - buffer.size(), 
                buffer.size());

            if (bytes_read > 0)
            {
                buffer.consume(bytes_read);
                p2p_downloader_statistics_.push_back(downloader_statistics);
            }
        }

        int num_of_vod_downloader_drivers = 0;
        input >> num_of_vod_downloader_drivers;
        for(int i = 0; i < num_of_vod_downloader_drivers && input; ++i)
        {
            boost::shared_ptr<VodDownloadDriverStatisticsData> download_driver_statistics(new VodDownloadDriverStatisticsData());
            int bytes_read = download_driver_statistics->Deserialize(
                version,
                bytes + buffer_size - buffer.size(),
                buffer.size());
            if (bytes_read > 0)
            {
                buffer.consume(bytes_read);
                vod_download_driver_statistics_.push_back(download_driver_statistics);
            }
        }

        int num_of_live_downloader_drivers = 0;
        input >> num_of_live_downloader_drivers;
        for(int i = 0; i < num_of_live_downloader_drivers && input; ++i)
        {
            boost::shared_ptr<LiveDownloadDriverStatisticsData> download_driver_statistics(new LiveDownloadDriverStatisticsData());
            int bytes_read = download_driver_statistics->Deserialize(
                version,
                bytes + buffer_size - buffer.size(),
                buffer.size());

            if (bytes_read > 0)
            {
                buffer.consume(bytes_read);
                live_download_driver_statistics_.push_back(download_driver_statistics);
            }
        }

        return input ? (buffer_size - buffer.size()) : -1;
    }

    size_t StatisticsData::GetSize()
    {
        size_t total_size = sizeof(statistics_info_) + 
               sizeof(int)*3;

        for(size_t i = 0; i < vod_download_driver_statistics_.size(); ++i)
        {
            total_size += vod_download_driver_statistics_[i]->GetSize();
        }

        for(size_t i = 0; i < live_download_driver_statistics_.size(); ++i)
        {
            total_size += live_download_driver_statistics_[i]->GetSize();
        }
        
        for(size_t i = 0; i < this->p2p_downloader_statistics_.size(); ++i)
        {
            total_size += p2p_downloader_statistics_[i]->GetSize();
        }

        return total_size;
    }

    int VodDownloadDriverStatisticsData::Serialize(boost::uint8_t bytes[], size_t max_size)
    {
        util::archive::ArchiveBuffer<> buffer(reinterpret_cast<char*>(bytes), max_size);
        util::archive::LittleEndianBinaryOArchive<> output(buffer);

        output << download_driver_statistics_info_;

        return output ? buffer.size() : -1;
    }

    int VodDownloadDriverStatisticsData::Deserialize(const protocol::VERSION_INFO& version, const boost::uint8_t bytes[], size_t buffer_size)
    {
        util::archive::ArchiveBuffer<> buffer(reinterpret_cast<char*>(const_cast<boost::uint8_t*>(bytes)), buffer_size, buffer_size);

        util::archive::LittleEndianBinaryIArchive<> input(buffer);

        input >> download_driver_statistics_info_;

        return input ? (buffer_size - buffer.size()) : -1;
    }

    size_t VodDownloadDriverStatisticsData::GetSize()
    {
        return sizeof(download_driver_statistics_info_);
    }
    
    int P2PDownloaderStatisticsData::Serialize(boost::uint8_t bytes[], size_t max_size)
    {
        util::archive::ArchiveBuffer<> buffer(reinterpret_cast<char*>(bytes), max_size);
        util::archive::LittleEndianBinaryOArchive<> output(buffer);

        output << p2p_downloader_statistics_info_;

        return output ? buffer.size() : -1;
    }

    int P2PDownloaderStatisticsData::Deserialize(const protocol::VERSION_INFO& version, const boost::uint8_t bytes[], size_t buffer_size)
    {
        util::archive::ArchiveBuffer<> buffer(reinterpret_cast<char*>(const_cast<boost::uint8_t*>(bytes)), buffer_size, buffer_size);

        util::archive::LittleEndianBinaryIArchive<> input(buffer);

        input >> p2p_downloader_statistics_info_;

        return input ? (buffer_size - buffer.size()) : -1;
    }

    size_t P2PDownloaderStatisticsData::GetSize()
    {
        return sizeof(p2p_downloader_statistics_info_);
    }

    int LiveDownloadDriverStatisticsData::Serialize(boost::uint8_t bytes[], size_t max_size)
    {
        util::archive::ArchiveBuffer<> buffer(reinterpret_cast<char*>(bytes), max_size);
        util::archive::LittleEndianBinaryOArchive<> output(buffer);

        output << live_download_driver_statistics_info_;

        return output ? buffer.size() : -1;
    }

    int LiveDownloadDriverStatisticsData::Deserialize(const protocol::VERSION_INFO& version, const boost::uint8_t bytes[], size_t buffer_size)
    {
        util::archive::ArchiveBuffer<> buffer(reinterpret_cast<char*>(const_cast<boost::uint8_t*>(bytes)), buffer_size, buffer_size);

        util::archive::LittleEndianBinaryIArchive<> input(buffer);

        input >> live_download_driver_statistics_info_;

        return input ? (buffer_size - buffer.size()) : -1;
    }

    size_t LiveDownloadDriverStatisticsData::GetSize()
    {
        return sizeof(live_download_driver_statistics_info_);
    }
}
