//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "storage/FileResourceInfo.h"
#include "base/wsconvert.h"
#include "base/util.h"

namespace storage
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_file_resource = log4cplus::Logger::getInstance("[file_resource_info]");
#endif
    using namespace base;
    using base::util::memcpy2;

#ifdef DISK_MODE

//临时关闭以下ToBuffer函数的优化，以方便我们调试其中的crash问题。
//TODO, ericzheng, 记得以后恢复这里的优化
#ifdef PEER_PC_CLIENT
#pragma optimize("", off)
#endif

    base::AppBuffer FileResourceInfo::ToBuffer() const
    {
        AppBuffer tmp(1024);
        boost::uint32_t total_len = sizeof(boost::uint32_t);  // reserver 4 bytes for total_len
        boost::uint32_t data_len = 0;
        // boost::uint32_t &len = *(boost::uint32_t*) p;

        // file name!
        boost::uint32_t len = file_path_.size();
        data_len = len + 4;
        while (total_len + data_len + 256 > tmp.Length())
        {
            tmp.Extend(tmp.Length());
        }
        memcpy2((tmp.Data() + total_len), tmp.Length() - total_len, &len, sizeof(boost::uint32_t));
        total_len += sizeof(boost::uint32_t);
        memcpy2((tmp.Data() + total_len), tmp.Length() - total_len, (boost::uint8_t*) file_path_.c_str(), len);
        total_len += len;

        // Rid_info
        protocol::RidInfo *rid_info = (protocol::RidInfo*)(tmp.Data() + total_len);
        data_len = sizeof(rid_info->rid_) + sizeof(rid_info->block_count_) + sizeof(rid_info->file_length_) +
            sizeof(rid_info->block_size_);
        while (total_len + data_len + 256 > tmp.Length())
        {
            tmp.Extend(tmp.Length());
        }
        // rid_info->rid_ = rid_info_.GetRID();
        // rid_info->block_count_ = rid_info_.GetBlockCount();
        // rid_info->file_length_ = rid_info_.GetFileLength();
        // rid_info->block_size_ = rid_info_.GetBlockSize();
        memcpy2(&(rid_info->rid_), sizeof(rid_info->rid_), &(rid_info_.rid_), sizeof(rid_info->rid_));
        memcpy2(&(rid_info->block_count_), sizeof(rid_info->block_count_), &(rid_info_.block_count_), sizeof(rid_info->block_count_));
        memcpy2(&(rid_info->file_length_), sizeof(rid_info->file_length_), &(rid_info_.file_length_), sizeof(rid_info->file_length_));
        memcpy2(&(rid_info->block_size_), sizeof(rid_info->block_size_), &(rid_info_.block_size_), sizeof(rid_info->block_size_));
        total_len += ((char*) &(rid_info->block_md5_s_) - (char*) rid_info);

        data_len = rid_info_.GetBlockCount() * sizeof(MD5);
        while (total_len + data_len + 256 > tmp.Length())
        {
            tmp.Extend(tmp.Length());
        }
        boost::uint32_t i = 0;
        for (; i < rid_info_.block_md5_s_.size(); i++)
        {
            // *(MD5*) p = rid_info_.block_md5_s_[i];
            MD5 md5 = rid_info_.block_md5_s_[i];
            memcpy2((void*)(tmp.Data() + total_len), tmp.Length() - total_len, (void*)(&md5), sizeof(MD5));
            total_len += sizeof(MD5);
        }
        for (; i < rid_info_.GetBlockCount() - rid_info_.block_md5_s_.size(); i++)
        {
            // *(MD5*) p = MD5();
            MD5 md5;
            memcpy2((void*)(tmp.Data() + total_len), tmp.Length() - total_len, (void*)(&md5), sizeof(MD5));
            total_len += sizeof(MD5);
        }
        // URL
        data_len = sizeof(boost::uint32_t);
        while (total_len + data_len + 256 > tmp.Length())
        {
            tmp.Extend(tmp.Length());
        }
        boost::uint32_t url_len = url_info_.size();
        // *(boost::uint32_t*) p = url_info_.size();
        memcpy2((void*)(tmp.Data() + total_len), tmp.Length() - total_len, (void*)(&url_len), sizeof(boost::uint32_t));
        total_len += sizeof(boost::uint32_t);
        for (i = 0; i < url_info_.size(); i++)
        {
            data_len = sizeof(boost::uint16_t) + sizeof(boost::uint32_t) + url_info_[i].url_.size() + sizeof(boost::uint32_t) + url_info_[i].refer_url_.size();
            while (total_len + data_len + 256 > tmp.Length())
            {
                tmp.Extend(tmp.Length());
            }
            // *(boost::uint16_t*) p = url_info_[i].type_;
            boost::uint16_t url_type = url_info_[i].type_;
            memcpy2((void*)(tmp.Data() + total_len), tmp.Length() - total_len, (void*)(&url_type), sizeof(boost::uint16_t));
            total_len += sizeof(boost::uint16_t);
            // *(boost::uint32_t*) p = url_info_[i].url_.size();
            url_len = url_info_[i].url_.size();
            memcpy2((void*)(tmp.Data() + total_len), tmp.Length() - total_len, (void*)(&url_len), sizeof(boost::uint32_t));
            total_len += sizeof(boost::uint32_t);
            memcpy2((tmp.Data() + total_len), tmp.Length() - total_len, url_info_[i].url_.c_str(), url_info_[i].url_.size());
            total_len += url_info_[i].url_.size();
            // *(boost::uint32_t*) p = url_info_[i].refer_url_.size();
            url_len = url_info_[i].refer_url_.size();
            memcpy2((void*)(tmp.Data() + total_len), tmp.Length() - total_len, (void*)(&url_len), sizeof(boost::uint32_t));
            total_len += sizeof(boost::uint32_t);
            memcpy2((tmp.Data() + total_len), tmp.Length() - total_len, url_info_[i].refer_url_.c_str(), url_info_[i].refer_url_.size());
            total_len += url_info_[i].refer_url_.size();
        }
        // ----------------------------------------------
        // version2
        // *(boost::uint32_t*) p = last_push_time_;
        data_len = sizeof(boost::uint32_t) + sizeof(boost::uint32_t) + traffic_list_.size()*sizeof(boost::uint32_t);
        while (total_len + data_len + 256 > tmp.Length())
        {
            tmp.Extend(tmp.Length());
        }
        memcpy2((void*)(tmp.Data() + total_len), tmp.Length() - total_len, (void*)(&last_push_time_), sizeof(boost::uint32_t));
        total_len += sizeof(boost::uint32_t);
        // *(boost::uint32_t*) p = traffic_list_.size();
        boost::uint32_t traff_size = traffic_list_.size();
        memcpy2((void*)(tmp.Data() + total_len), tmp.Length() - total_len, (void*)(&traff_size), sizeof(boost::uint32_t));
        total_len += sizeof(boost::uint32_t);
        for (std::list<boost::uint32_t>::const_iterator it = traffic_list_.begin(); it != traffic_list_.end(); ++it)
        {
            // *(boost::uint32_t*) p = *it;
            boost::uint32_t v = *it;
            memcpy2((void*)(tmp.Data() + total_len), tmp.Length() - total_len, (void*)(&v), sizeof(boost::uint32_t));
            total_len += sizeof(boost::uint32_t);
        }
        // ----------------------------------------------
        // version3
        // *(unsigned char*) p = down_mode_;
        data_len = sizeof(unsigned char) + sizeof(boost::uint32_t) + web_url_.length() + sizeof(boost::uint32_t) + sizeof(boost::uint64_t) + sizeof(boost::uint32_t);
        while (total_len + data_len + 256 > tmp.Length())
        {
            tmp.Extend(tmp.Length());
        }
        memcpy2((void*)(tmp.Data() + total_len), tmp.Length() - total_len, (void*)(&down_mode_), sizeof(unsigned char));
        total_len += sizeof(unsigned char);
        // web url
        // *(boost::uint32_t*) p = web_url_.length();
        boost::uint32_t web_url_len = web_url_.length();
        memcpy2((void*)(tmp.Data() + total_len), tmp.Length() - total_len, (void*)(&web_url_len), sizeof(boost::uint32_t));
        total_len += sizeof(boost::uint32_t);
        memcpy2((tmp.Data() + total_len), tmp.Length() - total_len, web_url_.c_str(), web_url_.length());
        total_len += web_url_.length();
        // file duration
        // *(boost::uint32_t*) p = file_duration_in_sec_;
        memcpy2((void*)(tmp.Data() + total_len), tmp.Length() - total_len, (void*)(&file_duration_in_sec_), sizeof(boost::uint32_t));
        total_len += sizeof(boost::uint32_t);
        // last write time
        // *(boost::uint64_t*) p = last_write_time_;
        memcpy2((void*)(tmp.Data() + total_len), tmp.Length() - total_len, (void*)(&last_write_time_), sizeof(boost::uint64_t));
        total_len += sizeof(boost::uint64_t);
        // data rate
        // *(boost::uint32_t*) p = data_rate_;
        memcpy2((void*)(tmp.Data() + total_len), tmp.Length() - total_len, (void*)(&data_rate_), sizeof(boost::uint32_t));
        total_len += sizeof(boost::uint32_t);
        // ----------------------------------------------
        // version4
        // *(boost::uint32_t*) p = file_name_.length();
        data_len = sizeof(boost::uint32_t) + file_name_.length();
        while (total_len + data_len + 256 > tmp.Length())
        {
            tmp.Extend(tmp.Length());
        }
        web_url_len = file_name_.length();
        memcpy2((void*)(tmp.Data() + total_len), tmp.Length() - total_len, (void*)(&web_url_len), sizeof(boost::uint32_t));
        total_len += sizeof(boost::uint32_t);
        memcpy2((tmp.Data() + total_len), tmp.Length() - total_len, (boost::uint8_t*) file_name_.c_str(), file_name_.length());
        total_len += file_name_.length();

        // ----------------------------------------------
        // version 5
        // *(boost::uint32_t*) p = is_open_service_;
        data_len = sizeof(boost::uint32_t);
        while (total_len + data_len + 256 > tmp.Length())
        {
            tmp.Extend(tmp.Length());
        }
        memcpy2((void*)(tmp.Data() + total_len), tmp.Length() - total_len, (void*)(&is_open_service_), sizeof(boost::uint32_t));
        total_len += sizeof(boost::uint32_t);

        // version 6
        // add is_push
        data_len = sizeof(boost::uint8_t);
        while (total_len + data_len + 256 > tmp.Length())
        {
            tmp.Extend(tmp.Length());
        }
        memcpy2((void*)(tmp.Data() + total_len), tmp.Length() - total_len, (void*)(&is_push_), sizeof(boost::uint8_t));
        total_len += sizeof(boost::uint8_t);

        // ----------------------------------------------
        // total_len = p - tmp.Data();
        // *(boost::uint32_t*) tmp.Data() = total_len;
        memcpy2((void*)tmp.Data(), sizeof(boost::uint32_t), (void*)(&total_len), sizeof(boost::uint32_t));
        base::AppBuffer r_buf(tmp.Data(), total_len);
        return r_buf;
    }

#ifdef PEER_PC_CLIENT
#pragma optimize("", on)
#endif

    bool FileResourceInfo::Parse(const AppBuffer buf, boost::uint32_t version)
    {
        if ((buf.Length() >= 2 * 1024 * 1024) || (buf.Length() < 4))
        {
            LOG4CPLUS_ERROR_LOG(logger_file_resource, "FileResourceInfo::Parse SubPieceContent size error! " 
                << buf.Length());
            return false;
        }

        boost::uint8_t *p = buf.Data();
        boost::uint32_t offset = 0;

        if (offset + 4 > buf.Length())
        {
            assert(false);
            return false;
        }
        boost::uint32_t total_len = 0;
        memcpy2(&total_len, sizeof(total_len), p + offset, sizeof(boost::uint32_t));
        if (total_len != buf.Length())
        {
            LOG4CPLUS_ERROR_LOG(logger_file_resource, "FileResourceInfo::Parse length error! buf.Length():" 
                << buf.Length() << " total_len:" << total_len);
            return false;
        }
        offset += sizeof(boost::uint32_t);

        if (offset + 4 > buf.Length())
        {
            assert(false);
            return false;
        }
        boost::int32_t file_path_len = 0;
        memcpy2((void*)(&file_path_len), sizeof(file_path_len), p + offset, sizeof(boost::int32_t));
        offset += sizeof(boost::int32_t);

        if (file_path_len < 0 || offset + file_path_len > buf.Length())
        {
            assert(false);
            return false;
        }
        file_path_.clear();
        if (version < SecVerCtrl::sec_version6)
        {
            wstring wfile_path;
            wfile_path.assign((wchar_t*) (p + offset), file_path_len/sizeof(wchar_t));
            file_path_ = base::ws2s(wfile_path);
        }
        else
        {
            file_path_.assign((char*) (p + offset), file_path_len);
        }
        offset += file_path_len;

        if (offset + sizeof(RID) + 12 > buf.Length())
        {
            assert(false);
            return false;
        }
        protocol::RidInfo *rid_info = (protocol::RidInfo*)(p + offset);
        offset += sizeof(RID);

        memcpy2(&(rid_info_.rid_), sizeof(rid_info_.rid_), &(rid_info->rid_), sizeof(RID));
        memcpy2(&(rid_info_.block_count_), sizeof(rid_info_.block_count_), &(rid_info->block_count_), sizeof(rid_info->block_count_));
        memcpy2(&(rid_info_.file_length_), sizeof(rid_info_.file_length_), &(rid_info->file_length_), sizeof(rid_info->file_length_));
        memcpy2(&(rid_info_.block_size_), sizeof(rid_info_.block_size_), &(rid_info->block_size_), sizeof(rid_info->block_size_));
        offset += 12;

        boost::int32_t block_count = rid_info_.GetBlockCount();

        if (block_count < 0 || offset + block_count * sizeof(MD5) > buf.Length())
        {
            assert(false);
            return false;
        }
        rid_info_.block_md5_s_.clear();
        for (boost::int32_t i = 0; i < block_count; i++)
        {
            MD5 md5;
            memcpy2((void*)(&md5), sizeof(md5), p + offset, sizeof(MD5));
            rid_info_.block_md5_s_.push_back(md5);
            offset += sizeof(MD5);
        }

        // URL
        if (offset + 4 > buf.Length())
        {
            assert(false);
            return false;
        }

        boost::int32_t url_count;
        memcpy2((void*)(&url_count), sizeof(url_count), p + offset, sizeof(boost::int32_t));
        offset += sizeof(boost::int32_t);

        url_info_.clear();
        for (boost::int32_t i = 0; i < url_count; i++)
        {
            protocol::UrlInfo t_url_info;

            if (offset + 2 > buf.Length())
            {
                assert(false);
                return false;
            }

            boost::uint16_t type;
            memcpy2(&type, sizeof(type), p + offset, sizeof(boost::uint16_t));
            t_url_info.type_ = (boost::uint8_t)type;
            offset += sizeof(boost::uint16_t);

            if (offset + 4 > buf.Length())
            {
                assert(false);
                return false;
            }
            boost::int32_t url_len;
            memcpy2((void*)(&url_len), sizeof(url_len), p + offset, sizeof(boost::int32_t));
            offset += sizeof(boost::int32_t);

            if (url_len < 0 || offset + url_len > buf.Length())
            {
                assert(false);
                return false;
            }
            t_url_info.url_.assign((char*)p + offset, url_len);
            offset += url_len;

            if (offset + 4 > buf.Length())
            {
                assert(false);
                return false;
            }

            boost::int32_t ref_len;
            memcpy2((void*)(&ref_len), sizeof(ref_len), p + offset, sizeof(boost::int32_t));
            offset += sizeof(boost::int32_t);

            if (ref_len < 0 || offset + ref_len > buf.Length())
            {
                assert(false);
                return false;
            }
            t_url_info.refer_url_.assign((char*)p + offset, ref_len);
            offset += ref_len;

            url_info_.push_back(t_url_info);
        }

        if (version == SecVerCtrl::sec_version1)
        {
            assert(offset == buf.Length());
            return true;
        }

        // ----------------------------------------------
        //  version2
        if (offset + 8 > buf.Length())
        {
            assert(false);
            return false;
        }
        memcpy2((void*)(&last_push_time_), sizeof(last_push_time_), p + offset, sizeof(boost::uint32_t));
        offset += sizeof(boost::uint32_t);
        boost::int32_t list_size;
        memcpy2((void*)(&list_size), sizeof(list_size), p + offset, sizeof(boost::int32_t));
        offset += sizeof(boost::int32_t);

        if (list_size < 0 || offset + list_size*4 > buf.Length())
        {
            assert(false);
            return false;
        }
        traffic_list_.clear();
        for (boost::int32_t i = 0; i < list_size; i++)
        {
            boost::uint32_t unit_traffic;
            memcpy2((void*)(&unit_traffic), sizeof(unit_traffic), p + offset, sizeof(boost::uint32_t));
            offset += sizeof(boost::uint32_t);
            traffic_list_.push_back(unit_traffic);
        }

        if (version == SecVerCtrl::sec_version2)
        {
            assert(offset == buf.Length());
            return true;
        }

        // ----------------------------------------------
        //  version3

        if (offset + sizeof(unsigned char) > buf.Length())
        {
            assert(false);
            return false;
        }
        memcpy2(&down_mode_, sizeof(down_mode_), p + offset, sizeof(unsigned char));
        offset += sizeof(unsigned char);

        // web url
        if (offset + sizeof(boost::uint32_t) > buf.Length())
        {
            assert(false);
            return false;
        }

        boost::int32_t web_url_len;
        memcpy2((void*)(&web_url_len), sizeof(web_url_len), p + offset, sizeof(boost::int32_t));
        offset += sizeof(boost::int32_t);

        if (web_url_len < 0 || offset + web_url_len > buf.Length())
        {
            assert(false);
            return false;
        }
        web_url_.assign((const char*) (p + offset), web_url_len);
        offset += web_url_len;

        // file duration, last_write_time, data_rate
        if (offset + 16 > buf.Length())
        {
            assert(false);
            return false;
        }
        memcpy2((void*)(&file_duration_in_sec_), sizeof(file_duration_in_sec_), p + offset, sizeof(boost::uint32_t));
        offset += sizeof(boost::uint32_t);
        memcpy2((void*)(&last_write_time_), sizeof(last_write_time_), p + offset, sizeof(boost::uint64_t));
        offset += sizeof(boost::uint64_t);
        memcpy2((void*)(&data_rate_), sizeof(data_rate_), p + offset, sizeof(boost::uint32_t));
        offset += sizeof(boost::uint32_t);
        // version
        if (version == SecVerCtrl::sec_version3)
        {
            assert(offset == buf.Length());
            return true;
        }

        // ----------------------------------------------
        //  version4

        // file_anme_
        if (offset + 4 > buf.Length())
        {
            assert(false);
            return false;
        }

        boost::int32_t file_name_len;
        memcpy2((void*)(&file_name_len), sizeof(file_name_len), p + offset, sizeof(boost::int32_t));
        offset += sizeof(boost::int32_t);

        if (file_name_len < 0 || offset + file_name_len > buf.Length())
        {
            assert(false);
            return false;
        }
        if (version < SecVerCtrl::sec_version6)
        {
            wstring wfile_name;
            wfile_name.assign((wchar_t*) (p + offset), file_name_len/sizeof(wchar_t));
            file_name_ = base::ws2s(wfile_name);
        }
        else
        {
            file_name_.assign((char*) (p + offset), file_name_len);
        }
        offset += file_name_len;

        // version
        if (version == SecVerCtrl::sec_version4)
        {
            assert(offset == buf.Length());
            return true;
        }

        // ----------------------------------------------
        // version5
        if (offset + 4 > buf.Length())
        {
            assert(false);
            return false;
        }
        memcpy2((void*)(&is_open_service_), sizeof(is_open_service_), p + offset, sizeof(boost::uint32_t));
        offset += sizeof(boost::uint32_t);

        // version
        if (version == SecVerCtrl::sec_version5)
        {
            assert(offset == buf.Length());
            return true;
        }

        // ----------------------------------------------
        // version6
        // change tstring to string
        if (version == SecVerCtrl::sec_version6)
        {
            assert(offset == buf.Length());
            return true;
        }

        // ----------------------------------------------
        // version7
        // version6有bug，增加一个版本号屏蔽version6
        if (version == SecVerCtrl::sec_version7)
        {
            assert(offset == buf.Length());
            return true;
        }

        // ----------------------------------------------
        // version8
        // 增加is_push
        if (offset + sizeof(boost::uint8_t) > buf.Length())
        {
            assert(false);
            return false;
        }

        memcpy2((void*)(&is_push_), sizeof(is_push_), p + offset, sizeof(boost::uint8_t));
        offset += sizeof(boost::uint8_t);

        if (version == SecVerCtrl::sec_version8)
        {
            assert(offset == buf.Length());
            return true;
        }

        return false;
    }

    bool FileResourceInfo::IsTempFile() const
    {
        return this->file_path_.rfind(tpp_extname) != string::npos;
    }

    bool FileResourceInfo::CheckFileSize(boost::uint32_t size) const
    {
        return this->rid_info_.GetFileLength() == size;
    }
#endif
}