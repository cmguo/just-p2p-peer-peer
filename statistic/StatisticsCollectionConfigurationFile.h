//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _STATISTIC_STATISTICS_COLLECTION_CONFIGURATION_LOADER_H_
#define _STATISTIC_STATISTICS_COLLECTION_CONFIGURATION_LOADER_H_

namespace statistic
{
    class StatisticsCollectionConfigurationFile
    {
        string local_config_file_path_;
    public:
        StatisticsCollectionConfigurationFile(string const& config_path);

        bool TryLoad(string& config_xml, std::time_t& file_modified_time);
        void Remove();
        bool Save(const string& config_xml);
    };
}

#endif  // _STATISTIC_STATISTICS_COLLECTION_CONFIGURATION_LOADER_H_
