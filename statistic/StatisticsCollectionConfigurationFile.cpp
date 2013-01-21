//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "statistic/StatisticsCollectionConfigurationFile.h"
#include "p2sp/download/ConfigurationDownloader.h"
#include <util/archive/BinaryOArchive.h>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <fstream>
#include <util/archive/BinaryIArchive.h>

namespace statistic
{
    StatisticsCollectionConfigurationFile::StatisticsCollectionConfigurationFile(string const& config_path)
    {
        local_config_file_path_ = config_path;

        boost::filesystem::path temp_path(local_config_file_path_);
        temp_path /= "pp_statistics_collection_cf";
        local_config_file_path_ = temp_path.file_string();
    }

    bool StatisticsCollectionConfigurationFile::TryLoad(string& config_xml, std::time_t& file_modified_time)
    {
        config_xml = "";

        boost::filesystem::path file_path(local_config_file_path_);
        
        if (false == base::filesystem::exists_nothrow(file_path))
        {
            return false;
        }

        std::ifstream input_file_stream(local_config_file_path_.c_str());
        if (input_file_stream)
        {
            string config_string;
            util::archive::BinaryIArchive<> ar(input_file_stream);
            ar >> config_xml;

            input_file_stream.close();

            boost::system::error_code code;
            base::filesystem::last_write_time_nothrow(file_path, file_modified_time, code);
            if (!code)
            {
                return true;
            }
        }

        return false;
    }

    void StatisticsCollectionConfigurationFile::Remove()
    {
        boost::filesystem::path file_path(local_config_file_path_);
        if (base::filesystem::exists_nothrow(file_path))
        {
            base::filesystem::remove_nothrow(file_path);
        }
    }

    bool StatisticsCollectionConfigurationFile::Save(const string& config_xml)
    {
        Remove();

        std::ofstream output_file_stream(local_config_file_path_.c_str(), std::ios_base::out | std::ios_base::binary);
        if (output_file_stream)
        {
            util::archive::BinaryOArchive<> ar(output_file_stream);
            ar << config_xml;
            return true;
        }

        return false;
    }
}
