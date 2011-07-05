//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "statistic/StatisticsCollectionController.h"
#include "statistic/StatisticsCollectionConfigurationFile.h"
#include "statistic/StatisticsConfiguration.h"
#include "statistic/StatisticsConfigurationsParser.h"
#include "statistic/StatisticsReportingConfiguration.h"
#include "statistic/StatisticsCollectorSettings.h"
#include "statistic/StatisticsCollector.h"
#include "statistic/StatisticsReporter.h"
#include "p2sp/AppModule.h"
#include "p2sp/bootstrap/BootStrapGeneralConfig.h"
#include "p2sp/download/ConfigurationDownloader.h"
#include <boost/date_time/posix_time/conversion.hpp>

namespace statistic
{
    StatisticsCollectionController::StatisticsCollectionController(
        boost::shared_ptr<p2sp::BootStrapGeneralConfig> bootstrap_config, 
        const string& config_path)
        : bootstrap_config_(bootstrap_config), config_path_(config_path), selected_server_index_(-1)
    {
    }

    void StatisticsCollectionController::Start()
    {
        bootstrap_config_->AddUpdateListener(shared_from_this());

        if (bootstrap_config_->IsDataCollectionOn())
        {
            StatisticsCollectionConfigurationFile config_file(config_path_);

            bool needs_to_download_config_from_server = true;

            string config_xml;
            std::time_t file_modified_time;
            if (config_file.TryLoad(config_xml, file_modified_time))
            {
                StatisticsConfigurationsParser parser;
                boost::shared_ptr<StatisticsConfigurations> config = parser.Parse(config_xml);

                if (config)
                {
                    if (false == IsConfigurationExpired(file_modified_time, config->expires_in_minutes_))
                    {
                        needs_to_download_config_from_server = false;
                        StartStatisticsCollection(config);
                    }
                }
            }
            
            if (needs_to_download_config_from_server)
            {
                config_file.Remove();

                StartAsyncDownload();
            }
        }
    }

    void StatisticsCollectionController::Stop()
    {
        bootstrap_config_->RemoveUpdateListener(shared_from_this());
        StopStatisticsCollection();
    }

    bool StatisticsCollectionController::IsConfigurationExpired(std::time_t file_modified_time, uint32_t expires_in_minutes)
    {
        std::time_t now;
        std::time(&now);

        boost::posix_time::time_duration duration_since_file_last_modified = 
            boost::posix_time::from_time_t(now) - boost::posix_time::from_time_t(file_modified_time);

        return static_cast<uint32_t>(duration_since_file_last_modified.total_seconds()) > expires_in_minutes*60;
    }

    void StatisticsCollectionController::StartStatisticsCollection(boost::shared_ptr<StatisticsConfigurations> statistics_configurations)
    {
        statistics_configurations->RemoveInactiveStatisticsConfiguration();

        if (statistics_configurations->statistics_configurations_.size() ==0)
        {
            return;
        }

        StatisticsCollectorSettings collector_settings(*statistics_configurations);
        boost::shared_ptr<StatisticsCollector> statistics_collector(new StatisticsCollector(collector_settings));

        StatisticsReportingConfiguration reporting_config(*statistics_configurations);
        reporter_.reset(new StatisticsReporter(reporting_config, statistics_collector, bootstrap_config_->GetDataCollectionServers()));

        reporter_->Start();
    }

    void StatisticsCollectionController::StopStatisticsCollection()
    {
        if (reporter_)
        {
            reporter_->Stop();
            reporter_.reset();
        }
    }

    //依次尝试从servers_异步下载日志收集配置文件，直到一个成功或全部失败为止
    void StatisticsCollectionController::StartAsyncDownload()
    {
        servers_ = bootstrap_config_->GetDataCollectionServers();

        if (servers_.size() > 0)
        {
            selected_server_index_ = 0;
            DoAsyncDownload();
        }
    }

    //处理BS Configuration更新的情况
    //这里的重点是，如果新的BS里日志收集开关与之前cached的状态不一致，以新的设置为准
    void StatisticsCollectionController::OnConfigUpdated()
    {
        if (bootstrap_config_->IsDataCollectionOn())
        {
            //如果已经在收集，或正在下载日志收集相关配置，
            //就忽略这次BS config udpate。换言之，如果这次BS update仅仅引起data collection server的变化，
            //其变化可能不会在这次PPAP进程周期生效?
            if (reporter_ || config_downloader_)
            {
                return;
            }
        }

        Stop();

        if (bootstrap_config_->IsDataCollectionOn())
        {
            Start();
        }
    }

    void StatisticsCollectionController::OnDownloadFailed(const boost::system::error_code& err)
    {
        assert(selected_server_index_ >= static_cast<int>(0) && selected_server_index_ < static_cast<int>(servers_.size()));
        if (selected_server_index_ < static_cast<int>(servers_.size()) - 1)
        {
            ++selected_server_index_;
            DoAsyncDownload();
        }
        else
        {
            //所有servers_都尝试过了，全部失败
        }
    }

    void StatisticsCollectionController::OnDownloadSucceeded(const string& config_content)
    {
        assert(config_downloader_);
        config_downloader_.reset();

        StatisticsCollectionConfigurationFile config_file(config_path_);
        config_file.Save(config_content);

        StatisticsConfigurationsParser parser;
        boost::shared_ptr<StatisticsConfigurations> config = parser.Parse(config_content);
        StartStatisticsCollection(config);
    }

    string StatisticsCollectionController::BuildRelativeDownloadUrl()
    {
        std::ostringstream stream;

        stream<<"/P2PAnalyzerServer/GetConfiguration.jsp";

        protocol::VERSION_INFO ver = p2sp::AppModule::GetKernelVersionInfo();
        stream<<"?peer_version="<<static_cast<int>(ver.Major)<<'.'<<static_cast<int>(ver.Minor)<<'.'<<static_cast<int>(ver.Micro)<<'.'<<static_cast<int>(ver.Extra);
        stream<<"&peer_id="<<p2sp::AppModule::Inst()->GetUniqueGuid().to_string();
        
        return stream.str();
    }

    void StatisticsCollectionController::DoAsyncDownload()
    {
        assert(selected_server_index_ >= 0 && selected_server_index_ < static_cast<int>(servers_.size()));

        config_downloader_.reset(
            new p2sp::ConfigurationDownloader(
            global_io_svc(), 
            servers_[selected_server_index_],
            BuildRelativeDownloadUrl(),
            shared_from_this()));

        config_downloader_->AsyncDownload();
    }

    boost::shared_ptr<StatisticsReporter> StatisticsCollectionController::GetStatisticsReporter()
    {
        return reporter_;
    }
}
