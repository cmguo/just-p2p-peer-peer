//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _STATISTIC_STATISTICS_H_
#define _STATISTIC_STATISTICS_H_

#include "statistic/StatisticStructs.h"

namespace statistic
{
    class IStatisticsData
    {
    public:
        virtual int Serialize(boost::uint8_t bytes[], size_t max_size) = 0; 
        virtual int Deserialize(const protocol::VERSION_INFO& version, const boost::uint8_t bytes[], size_t buffer_size) = 0; 
        virtual size_t GetSize() = 0;
        virtual ~IStatisticsData(){}
    };

    class VodDownloadDriverStatisticsData;
    class LiveDownloadDriverStatisticsData;
    class P2PDownloaderStatisticsData;

    class StatisticsData
        : public IStatisticsData
    {
    private:
        STASTISTIC_INFO statistics_info_;

        std::vector<boost::shared_ptr<VodDownloadDriverStatisticsData> > vod_download_driver_statistics_;
        std::vector<boost::shared_ptr<LiveDownloadDriverStatisticsData> > live_download_driver_statistics_;
        std::vector<boost::shared_ptr<P2PDownloaderStatisticsData> > p2p_downloader_statistics_;
        
    public:
        StatisticsData();

        StatisticsData(const STASTISTIC_INFO& statistics_info);

        void AddVodDownloadDriverStatistics(boost::shared_ptr<VodDownloadDriverStatisticsData> download_driver_statistics)
        {
            vod_download_driver_statistics_.push_back(download_driver_statistics);
        }

        void AddLiveDownloadDriverStatistics(boost::shared_ptr<LiveDownloadDriverStatisticsData> download_driver_statistics)
        {
            live_download_driver_statistics_.push_back(download_driver_statistics);
        }

        void AddP2PDownloaderStatistics(boost::shared_ptr<P2PDownloaderStatisticsData> p2p_downloader_statistics)
        {
            p2p_downloader_statistics_.push_back(p2p_downloader_statistics);
        }

        STASTISTIC_INFO GetStatisticsInfo() const
        {
            return statistics_info_;
        }

        std::vector<boost::shared_ptr<VodDownloadDriverStatisticsData> > GetVodDownloadDriverStatistics() const
        {
            return vod_download_driver_statistics_;
        }

        std::vector<boost::shared_ptr<LiveDownloadDriverStatisticsData> > GetLiveDownloadDriverStatistics() const
        {
            return live_download_driver_statistics_;
        }

        std::vector<boost::shared_ptr<P2PDownloaderStatisticsData> > GetP2PDownloaderStatistics() const
        {
            return p2p_downloader_statistics_;
        }

        int Serialize(boost::uint8_t bytes[], size_t max_size);

        int Deserialize(const protocol::VERSION_INFO& version, const boost::uint8_t bytes[], size_t buffer_size);

        size_t GetSize();
    };

    class VodDownloadDriverStatisticsData 
        : public IStatisticsData
    {
        DOWNLOADDRIVER_STATISTIC_INFO download_driver_statistics_info_;
    public:
        VodDownloadDriverStatisticsData()
        {
        }

        VodDownloadDriverStatisticsData(const DOWNLOADDRIVER_STATISTIC_INFO& download_driver_statistics_info)
            :download_driver_statistics_info_(download_driver_statistics_info)
        {
        }

        DOWNLOADDRIVER_STATISTIC_INFO GetDownloadDriverStatisticsInfo() const
        {
            return download_driver_statistics_info_;
        }

        int Serialize(boost::uint8_t bytes[], size_t max_size);
        int Deserialize(const protocol::VERSION_INFO& version, const boost::uint8_t bytes[], size_t buffer_size);
        size_t GetSize();
    };

    class P2PDownloaderStatisticsData
        : public IStatisticsData
    {
        P2PDOWNLOADER_STATISTIC_INFO p2p_downloader_statistics_info_;
    public:
        P2PDownloaderStatisticsData()
        {
        }

        P2PDownloaderStatisticsData(const P2PDOWNLOADER_STATISTIC_INFO& p2p_downloader_statistics_info)
            :p2p_downloader_statistics_info_(p2p_downloader_statistics_info)
        {
        }

        int Serialize(boost::uint8_t bytes[], size_t max_size);
        int Deserialize(const protocol::VERSION_INFO& version, const boost::uint8_t bytes[], size_t buffer_size);
        size_t GetSize();

        P2PDOWNLOADER_STATISTIC_INFO GetDownloaderStatisticsInfo() const
        {
            return p2p_downloader_statistics_info_;
        }
    };

    class LiveDownloadDriverStatisticsData
        : public IStatisticsData
    {
        LIVE_DOWNLOADDRIVER_STATISTIC_INFO live_download_driver_statistics_info_;
    public:
        LiveDownloadDriverStatisticsData()
        {
        }

        LiveDownloadDriverStatisticsData(const LIVE_DOWNLOADDRIVER_STATISTIC_INFO& live_download_driver_statistics_info)
            :live_download_driver_statistics_info_(live_download_driver_statistics_info)
        {
        }

        int Serialize(boost::uint8_t bytes[], size_t max_size);
        int Deserialize(const protocol::VERSION_INFO& version, const boost::uint8_t bytes[], size_t buffer_size);
        size_t GetSize();

        LIVE_DOWNLOADDRIVER_STATISTIC_INFO GetDownloadDriverStatisticsInfo() const
        {
            return live_download_driver_statistics_info_;
        }
    };
}

#endif  // _STATISTIC_STATISTICS_H_
