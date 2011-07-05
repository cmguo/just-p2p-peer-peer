//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "VideoDownloadInfo.h"

namespace downloadcenter
{
    //////////////////////////////////////////////////////////////////////////
    // VideoDownloadInfo
    bool VideoDownloadInfo::WriteBuffer(void* buffer, uint32_t buffer_len, uint32_t* p_result_len)
    {
        // calc length
        uint32_t result_len = 0;
        result_len =
            (WebUrl.length() + Url.length() + Title.length() + FileExt.length() + 4)*sizeof(wchar_t) +
            (RequestHeader.length() + 1)*sizeof(char);

        if (p_result_len != NULL) {
            *p_result_len = result_len;
        }
        if (NULL == buffer) {
            return 0 == buffer_len;
        }
        if (buffer_len < result_len) {
            return false;
        }
        // copy
        uint32_t len = 0;
        char *buf_ = static_cast<char*>(buffer);
        char *buf_end = buf_ + buffer_len;

        // web url
        len = (WebUrl.length() + 1)*sizeof(wchar_t);
        base::util::memcpy2((void*)buf_, buf_end - buf_, (const void*)WebUrl.c_str(), len); buf_ += len;
        // url
        len = (Url.length() + 1)*sizeof(wchar_t);
        base::util::memcpy2((void*)buf_, buf_end - buf_, (const void*)Url.c_str(), len); buf_ += len;
        // title
        len = (Title.length() + 1)*sizeof(wchar_t);
        base::util::memcpy2((void*)buf_, buf_end - buf_, (const void*)Title.c_str(), len); buf_ += len;
        // file ext
        len = (FileExt.length() + 1) * sizeof(wchar_t);
        base::util::memcpy2((void*)buf_, buf_end - buf_, (const void*)FileExt.c_str(), len); buf_ += len;
        // request head
        len = (RequestHeader.length() + 1)*sizeof(char);
        base::util::memcpy2((void*)buf_, buf_end - buf_, (const void*)RequestHeader.c_str(), len); buf_ += len;
        // end
        return true;
    }

    bool VideoDownloadInfo::ReadBuffer(const void* buffer, uint32_t buffer_len, uint32_t* p_result_len)
    {
        if (NULL == buffer || buffer_len == 0) {
            return false;
        }
        // uint32_t len = 0;
        const char* buf_ = static_cast<const char*>(buffer);
        const char* end_ = buf_ + buffer_len;
        // weburl
        if (buf_ + (wcslen((const wchar_t*)buf_) + 1)*sizeof(wchar_t) > end_) {
            return false;
        }
        // WebUrl.assign((const wchar_t*)buf_);
        buf_ += (WebUrl.length() + 1) * sizeof(wchar_t);
        // url
        if (buf_ + (wcslen((const wchar_t*)buf_) + 1)*sizeof(wchar_t) > end_) {
            return false;
        }
        // Url.assign((const wchar_t*)buf_);
        buf_ += (Url.length() + 1) * sizeof(wchar_t);
        // title
        if (buf_ + (wcslen((const wchar_t*)buf_) + 1)*sizeof(wchar_t) > end_) {
            return false;
        }
        // Title.assign((const wchar_t*)buf_);
        buf_ += (Title.length() + 1) * sizeof(wchar_t);
        // file ext
        if (buf_ + (wcslen((const wchar_t*)buf_) + 1)*sizeof(wchar_t) > end_) {
            return false;
        }
        // FileExt.assign((const wchar_t*)buf_);
        buf_ += (FileExt.length() + 1) * sizeof(wchar_t);
        // request head
        if (buf_ + (strlen((const char*)buf_) + 1)*sizeof(char) > end_) {
            return false;
        }
        // RequestHeader.assign((const char*)buf_);
        buf_ += (RequestHeader.length() + 1) * sizeof(char);
        if (buf_ > end_) {
            return false;
        }
        // len
        if (p_result_len != NULL) {
            *p_result_len = buf_ - (const char*)buffer;
        }
        return true;
    }

    //////////////////////////////////////////////////////////////////////////
    // VideoDownloadGroup
    bool VideoDownloadInfoGroup::WriteBuffer(void* buffer, uint32_t buffer_len, uint32_t* p_result_len)
    {
        // calc len
        uint32_t result_len = 0;
        result_len = (StorePath.length() + 1)*sizeof(wchar_t) + sizeof(uint32_t) + sizeof(uint32_t);
        for (uint32_t i = 0; i < VideoInfos.size(); ++i) {
            uint32_t len;
            VideoInfos[i].WriteBuffer(NULL, 0, &len);
            result_len += len;
        }
        if (NULL != p_result_len) {
            *p_result_len = result_len;
        }
        if (NULL == buffer) {
            return 0 == buffer_len;
        }
        if (buffer_len < result_len) {
            return false;
        }
        // copy
        uint32_t len = 0;
        char* buf_ = static_cast<char*>(buffer);
        char* end_ = buf_ + buffer_len;
        // version
        len = sizeof(uint32_t);
        base::util::memcpy2((void*)buf_, end_ - buf_, (const void*)(&Version), len); buf_ += len;
        // store path
        len = (StorePath.length() + 1) * sizeof(wchar_t);
        base::util::memcpy2((void*)buf_, end_ - buf_, (const void*)StorePath.c_str(), len); buf_ += len;
        // count
        len = VideoInfos.size();
        base::util::memcpy2((void*)buf_, end_ - buf_, (const void*)(&len), sizeof(uint32_t)); buf_ += sizeof(uint32_t);
        // video infos
        for (uint32_t i = 0; i < VideoInfos.size(); ++i) {
            if (false == VideoInfos[i].WriteBuffer((void*)buf_, end_ - buf_, &len)) {
                return false;
            }
            buf_ += len;
        }
        return true;
    }

    bool VideoDownloadInfoGroup::ReadBuffer(const void* buffer, uint32_t buffer_len, uint32_t* p_result_len)
    {
        if (NULL == buffer || buffer_len == 0) {
            return false;
        }
        uint32_t len = 0;
        const char* buf_ = static_cast<const char*>(buffer);
        const char* end_ = buf_ + buffer_len;
        // version
        if (buf_ + sizeof(uint32_t) > end_) {
            return false;
        }
        Version = *(const uint32_t*)buf_; buf_ += sizeof(uint32_t);
        // store path
        if (buf_ + (wcslen((const wchar_t*)buf_) + 1)*sizeof(wchar_t) > end_) {
            return false;
        }
        // StorePath.assign((const wchar_t*)buf_);
        buf_ += (StorePath.length() + 1) * sizeof(wchar_t);
        // count
        uint32_t count;
        if (buf_ + sizeof(uint32_t) > end_) {
            return false;
        }
        count = *(const uint32_t*)buf_; buf_ += sizeof(uint32_t);
        // video infos
        VideoInfos.resize(count);
        for (uint32_t i = 0; i < count; ++i) {
            if (false == VideoInfos[i].ReadBuffer((const void*)buf_, end_ - buf_, &len)) {
                return false;
            }
            buf_ += len;
        }
        // len
        if (p_result_len != NULL) {
            *p_result_len = buf_ - (const char*)buffer;
        }
        return true;
    }

    void VideoDownloadInfoGroup::Clear()
    {
        VideoInfos.clear();
    }

    void VideoDownloadInfoGroup::InsertItem(const VideoDownloadInfo& info)
    {
        VideoInfos.push_back(info);
    }
}
