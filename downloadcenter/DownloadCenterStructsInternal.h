//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------


#ifndef DOWNLOAD_CENTER_STRUCTS_INTERNAL_H
#define DOWNLOAD_CENTER_STRUCTS_INTERNAL_H

#ifdef BOOST_WINDOWS_API
#pragma pack(push, 1)
#endif

namespace downloadcenter
{
    namespace internal
    {

        struct STRING
        {
            boost::uint16_t ID;
            boost::uint16_t Length;
            boost::uint8_t Data[];

            STRING() {
                memset(this, 0, sizeof(STRING));
            }
        };

        struct STRING_REF
        {
            boost::uint16_t StringID;

            STRING_REF() {
                memset(this, 0, sizeof(STRING_REF));
            }
        };

        struct DATA_HEADER
        {
            uint32_t Version;
            uint32_t StringStoreOffset;
            uint32_t DownloadResourceStoreOffset;
            uint32_t EndOffset;

            DATA_HEADER() {
                memset(this, 0, sizeof(DATA_HEADER));
            }
        };

        struct STRING_STORE
        {
            boost::uint16_t StringCount;
            boost::uint8_t StringData[];

            STRING_STORE() {
                memset(this, 0, sizeof(STRING_STORE));
            }
        };

        struct DOWNLOAD_RESOURCE_STORE
        {
            boost::uint16_t ResourceCount;
            boost::uint8_t ResourceData[];

            DOWNLOAD_RESOURCE_STORE() {
                memset(this, 0, sizeof(DOWNLOAD_RESOURCE_STORE));
            }
        };

        struct DOWNLOAD_RESOURCE_DATA
        {
            // 资源名称w
            STRING_REF FileName;

            // 页面Url
            STRING_REF WebUrl;

            // 下载Url
            STRING_REF DownloadUrl;

            // 下载ReferUrl
            STRING_REF RefererUrl;

            // 资源保存路径w
            STRING_REF StorePath;

            // 0: 下载未完成
            // 1: 下载已完成
            boost::uint8_t IsFinished;

            // 0: 未开始下载
            // 1: 正在下载
            boost::uint8_t IsDownloading;

            // 下载状态
            // 0: 未下载
            // 1: 正在下载
            // 2: 下载完成
            // 3: 下载失败
            // 4: 本地已有
            boost::uint16_t DownloadStatus;

            // 资源大小(字节)
            boost::uint64_t FileLength;

            // 文件时长(秒)
            uint32_t FileDuration;

            // 码流率(字节/秒)
            uint32_t DataRate;

            // 已经下载的数据量(字节)
            boost::uint64_t DownloadedBytes;

            // P2P下载字节数
            boost::uint64_t P2PDownloadBytes;

            // Http下载字节数
            boost::uint64_t HttpDownloadBytes;

            // 下载速度(字节/秒)
            uint32_t DownloadSpeed;

            // P2P下载速度(字节/秒)
            uint32_t P2PDownloadSpeed;

            // Http下载速度(字节/秒)
            uint32_t HttpDownloadSpeed;

            DOWNLOAD_RESOURCE_DATA() {
                memset(this, 0, sizeof(DOWNLOAD_RESOURCE_DATA));
            }
        };
    }
}

#ifdef BOOST_WINDOWS_API
#pragma pack(pop)
#endif

#endif  // DOWNLOAD_CENTER_STRUCTS_INTERNAL_H
