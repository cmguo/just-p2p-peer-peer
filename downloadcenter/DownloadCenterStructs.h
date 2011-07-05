//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------


#ifndef DOWNLOAD_CENTER_STRUCTS_H
#define DOWNLOAD_CENTER_STRUCTS_H


//////////////////////////////////////////////////////////////////////////
//
namespace downloadcenter
{
    enum DOWNLOAD_STATUS  // : boost::uint16_t
    {
        NOT_DOWNLOADING     = 0,
        IS_DOWNLOADING      = 1,
        DOWNLOAD_COMPLETE   = 2,
        DOWNLOAD_FAILED     = 3,
        DOWNLOAD_LOCAL      = 4,
    };

    struct DownloadResourceData
    {
        // 资源名称
        string FileName;

        // 页面Url
        string WebUrl;

        // 下载Url
        string DownloadUrl;

        // 下载ReferUrl
        string RefererUrl;

        // 资源保存路径
        string StorePath;

        // 0: 下载未完成
        // 1: 下载已完成
        bool IsFinished;

        // 0: 未开始下载
        // 1: 正在下载
        bool IsDownloading;

        // 下载状态
        // 0: 未下载
        // 1: 正在下载
        // 2: 下载完成
        // 3: 下载失败
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

        DownloadResourceData() {
            FileName.clear();
            WebUrl.clear();
            DownloadUrl.clear();
            RefererUrl.clear();
            StorePath.clear();
            IsFinished = 0;
            IsDownloading = 0;
            DownloadStatus = 0;
            FileLength = 0;
            FileDuration = 0;
            DataRate = 0;
            DownloadedBytes = 0;
            P2PDownloadBytes = 0;
            HttpDownloadBytes = 0;
            DownloadSpeed = 0;
            P2PDownloadSpeed = 0;
            HttpDownloadSpeed = 0;
        }
    };

}

#endif  // DOWNLOAD_CENTER_STRUCTS_H
