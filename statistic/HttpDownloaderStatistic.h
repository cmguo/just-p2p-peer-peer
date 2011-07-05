//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// HttpDownloaderStatistic.h

#ifndef _STATISTIC_HTTP_DOWNLOADER_STATISTIC_H_
#define _STATISTIC_HTTP_DOWNLOADER_STATISTIC_H_

#include "statistic/StatisticStructs.h"
#include "statistic/SpeedInfoStatistic.h"

namespace statistic
{
    class DownloadDriverStatistic;
    typedef boost::shared_ptr<DownloadDriverStatistic> DownloadDriverStatistic__p;

    class HttpDownloaderStatistic
        : public boost::noncopyable
        , public boost::enable_shared_from_this<HttpDownloaderStatistic>
    {
    public:

        typedef boost::shared_ptr<HttpDownloaderStatistic> p;

        static p Create(const string& url, DownloadDriverStatistic__p download_driver_statistic);

    public:

        void Start();

        void Stop();

        void Clear();

        bool IsRunning() const { return is_running_; }

    public:

        //////////////////////////////////////////////////////////////////////////
        // HTTP Downloader Info

        HTTP_DOWNLOADER_INFO GetHttpDownloaderInfo();

        //////////////////////////////////////////////////////////////////////////
        // url

        void SetUrl(const string& url);

        string GetUrl() const;

        void SetReferUrl(const string& refer_url);

        string GetReferUrl() const;

        void SetRedirectUrl(const string& redirect_url);

        string GetRedirectUrl() const;

        //////////////////////////////////////////////////////////////////////////
        // piece info

        void SetDownloadingPieceInfo(const PIECE_INFO_EX & downloading_piece_info);

        void SetStartPieceInfo(const PIECE_INFO_EX& start_piece_info);

        void SetDownloadingPieceInfo(boost::uint16_t BlockIndex, boost::uint16_t PieceIndexInBlock, boost::uint16_t SubPieceIndexInPiece);

        void SetStartPieceInfo(boost::uint16_t BlockIndex, boost::uint16_t PieceIndexInBlock, boost::uint16_t SubPieceIndexInPiece);

        //////////////////////////////////////////////////////////////////////////
        // speed

        SPEED_INFO GetSpeedInfo();

        SPEED_INFO_EX GetSpeedInfoEx();

        void SubmitDownloadedBytes(uint32_t downloaded_bytes);

        void SubmitUploadedBytes(uint32_t uploaded_bytes);

        //////////////////////////////////////////////////////////////////////////
        // Events

        void SubmitHttpConnected();

        void SubmitRequestPiece();

        void SubmitRetry();

        void ClearRetry();

        //////////////////////////////////////////////////////////////////////////
        // status

        void SetSupportRange(bool is_support_range);

        void SetHttpStatusCode(uint32_t http_status_code);

        //////////////////////////////////////////////////////////////////////////
        // death

        void SetIsDeath(bool is_death);

        bool IsDeath() const;

        void SetIsPause(bool is_pause);

    private:

        void UpdateSpeedInfo();

    private:

        DownloadDriverStatistic__p download_driver_statistic_;

    private:

        volatile bool is_running_;

        string url_;

        string refer_url_;

        string redirect_url_;

        HTTP_DOWNLOADER_INFO http_downloader_info_;

        SpeedInfoStatistic speed_info_;

    private:

        HttpDownloaderStatistic() {}  // never call me

        HttpDownloaderStatistic(const string& url, DownloadDriverStatistic__p download_driver_statistic);

    };
}

#endif  // _STATISTIC_HTTP_DOWNLOADER_STATISTIC_H_
