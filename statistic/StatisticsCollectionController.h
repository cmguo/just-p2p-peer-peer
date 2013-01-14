//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _STATISTIC_STATISTICS_COLLECTION_CONTROLLER_H_
#define _STATISTIC_STATISTICS_COLLECTION_CONTROLLER_H_

#include "p2sp/download/IConfigurationDownloadListener.h"
#include "p2sp/bootstrap/BootStrapGeneralConfig.h"

namespace p2sp
{
    class ConfigurationDownloader;
    class BootStrapGeneralConfig;
}

namespace statistic
{
    class StatisticsCollectionConfigurationFile;
    class StatisticsReporter;
    struct StatisticsConfigurations;

    class StatisticsCollectionController:
        public boost::enable_shared_from_this<StatisticsCollectionController>,
        public p2sp::IConfigurationDownloadListener,
        public p2sp::ConfigUpdateListener
    {
        boost::shared_ptr<p2sp::BootStrapGeneralConfig> bootstrap_config_;
        string config_path_;

        std::vector<string> servers_;
        int selected_server_index_;
        boost::shared_ptr<p2sp::ConfigurationDownloader> config_downloader_;

        boost::shared_ptr<StatisticsReporter> reporter_;

    public:
        StatisticsCollectionController(
            boost::shared_ptr<p2sp::BootStrapGeneralConfig> bootstrap_config, 
            const string& config_path);

        void Start();
        void Stop();

        void OnDownloadFailed(const boost::system::error_code& err);
        void OnDownloadSucceeded(const string& config_content);

        //handler for BS config update
        void OnConfigUpdated();

        boost::shared_ptr<StatisticsReporter> GetStatisticsReporter();

    private:

        void StartAsyncDownload();
        void DoAsyncDownload();

        void StartStatisticsCollection(boost::shared_ptr<StatisticsConfigurations> statistics_configuration);
        void StopStatisticsCollection();

        static bool IsConfigurationExpired(std::time_t file_modified_time, boost::uint32_t expires_in_minutes);

        static string BuildRelativeDownloadUrl();
    };
}

#endif  // _STATISTIC_STATISTICS_COLLECTION_CONTROLLER_H_
