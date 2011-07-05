//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _FILERESOURCEINFO_H_
#define _FILERESOURCEINFO_H_

#include "storage/storage_base.h"

namespace storage
{
    struct FileResourceInfo
#ifdef DUMP_OBJECT
        : public count_object_allocate<FileResourceInfo>
#endif
    {
        // version 1
        protocol::RidInfo rid_info_;  // 资源ID信息
        std::vector<protocol::UrlInfo> url_info_;
        string file_path_;  // 资源绝对路径
        // version 2
        std::list<uint32_t> traffic_list_;    // 上传流量统计队列
        uint32_t last_push_time_;             // 上次队列push时间
        // version 3
        unsigned char down_mode_;           // 资源下载方式
        string web_url_;                    // 页面地址
        uint32_t file_duration_in_sec_;       // 时长
        boost::uint64_t last_write_time_;   // 该资源上一次写磁盘的时间
        uint32_t data_rate_;                  // 文件码流率Bps
        // version 4
        string file_name_;                  // 文件名
        // version 5
        uint32_t is_open_service_;            // 是否为开放服务
        // version 6
        boost::uint8_t is_push_;            // 是否是Push的资源

        explicit FileResourceInfo() :
        rid_info_(), url_info_(), file_path_(), last_push_time_(0), down_mode_(DM_BY_ACCELERATE)
        {
            for (uint32_t i = 0; i < rid_info_.GetBlockCount(); i++)
            {
                rid_info_.block_md5_s_.push_back(MD5());
            }
            // clear
            web_url_.clear();
            file_duration_in_sec_ = 0;
            last_write_time_ = 0;
            data_rate_ = 0;
            file_name_.clear();
            is_open_service_ = 0;
        }

        base::AppBuffer ToBuffer() const;
        bool Parse(const base::AppBuffer buf, uint32_t version);

        // 资源文件是否还处于临时文件状态
        bool IsTempFile() const;

        // 文件大小是否等于输入参数
        bool CheckFileSize(uint32_t size) const;
    };
}

#endif