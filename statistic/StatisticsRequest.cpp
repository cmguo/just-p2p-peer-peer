//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "statistic/StatisticsRequest.h"
#include "statistic/StatisticsData.h"
#include "statistic/StatisticStructs.h"
#include "statistic/GZipCompressor.h"
#include "statistic/ReportCondition.h"
#include "p2sp/AppModule.h"
#include "network/UrlCodec.h"
#include "storage/Storage.h"
#include "storage/Instance.h"
#include <framework/string/Convert.h>

namespace statistic
{
    const int StatisticsRequest::MaxAttempts = 5;

    bool StatisticsRequest::GetCompressedData(std::ostream& data)
    {
        const int max_size = 1000*1024;
        uint8_t* buffer = new uint8_t[max_size];

        util::archive::ArchiveBuffer<> archive_buffer(reinterpret_cast<char*>(buffer), max_size);
        util::archive::LittleEndianBinaryOArchive<> output(archive_buffer);

        PeerStatisticsInfo peer_statistics_info(p2sp::AppModule::Inst()->GetKernelVersionInfo(), statistics_.size());
        output << peer_statistics_info;

        int offset = archive_buffer.size();

        for(size_t i = 0; i < statistics_.size(); ++i)
        {
            int incremental_bytes = statistics_[i]->Serialize(buffer+offset, max_size - offset);

            if (incremental_bytes < 0)
            {
                delete[] buffer;
                return false;
            }

            offset += incremental_bytes;
        }

        GZipCompressor compressor;
        bool compression_succeeded = compressor.Compress(buffer, offset, data);
        delete[] buffer;
        return compression_succeeded;
    }

    string StatisticsRequest::BuildPostRelativeUrl()
    {
        std::ostringstream stream;

        stream << "/ReportStatistics.jsp";

        string statistics_id = condition_->GetConditionId();
        stream << "?statistics_id=" << network::UrlCodec::Encode(statistics_id);

        protocol::VERSION_INFO ver = p2sp::AppModule::GetKernelVersionInfo();
        stream << "&peer_version="<<static_cast<int>(ver.Major)<<'.'<<static_cast<int>(ver.Minor)<<'.'<<static_cast<int>(ver.Micro)<<'.'<<static_cast<int>(ver.Extra);

        Guid peer_id = p2sp::AppModule::Inst()->GetUniqueGuid();
        stream << "&peer_id=" << peer_id.to_string();

        stream << "&rid=" << rid_.to_string();

        storage::Instance::p instance = boost::static_pointer_cast<storage::Instance>(storage::Storage::Inst()->GetInstanceByRID(rid_));
        string resource_name = "<unknown resource>";
        if (instance)
        {
            string ascii_resource_name = instance->GetFileName();

            framework::string::Convert convert("utf8", "acp");
            string utf8_resource_name;
            boost::system::error_code convert_error = convert.convert(ascii_resource_name, utf8_resource_name);

            resource_name = convert_error ? ascii_resource_name : utf8_resource_name;
        }

        stream << "&resourceName=" << network::UrlCodec::Encode(resource_name);

        return stream.str();
    }
}
