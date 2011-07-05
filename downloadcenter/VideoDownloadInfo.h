//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef VIDEO_DOWNLOAD_INFO_H
#define VIDEO_DOWNLOAD_INFO_H

namespace downloadcenter
{
    const unsigned int BUFFER_VERSION = 0x00010001;

    class VideoDownloadInfo
#ifdef DUMP_OBJECT
        : public count_object_allocate<VideoDownloadInfo>
#endif
    {
    public:
        string WebUrl;
        string Url;
        string Title;
        string FileExt;
        string RequestHeader;

    public:
        bool WriteBuffer(void* buffer, uint32_t buffer_len, uint32_t* p_result_len);
        bool ReadBuffer(const void* buffer, uint32_t buffer_len, uint32_t* p_result_len);
    };

    class VideoDownloadInfoGroup
    {
    public:
        uint32_t Version;
        string StorePath;
        std::vector<VideoDownloadInfo> VideoInfos;

    public:
        bool WriteBuffer(void* buffer, uint32_t buffer_len, uint32_t * p_result_len);
        bool ReadBuffer(const void* buffer, uint32_t buffer_len, uint32_t * p_result_len);
        void Clear();
        void InsertItem(const VideoDownloadInfo& info);

    public:
        VideoDownloadInfoGroup() : Version(BUFFER_VERSION) {}
    };
}

#endif  // VIDEO_DOWNLOAD_INFO_H
