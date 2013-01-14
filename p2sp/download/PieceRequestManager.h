//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// PieceRequestManager.h

#ifndef _P2SP_DOWNLOAD_PIECE_REQUEST_MANAGER_H_
#define _P2SP_DOWNLOAD_PIECE_REQUEST_MANAGER_H_

namespace p2sp
{
    class VodDownloader;
    typedef boost::shared_ptr<VodDownloader> VodDownloader__p;
    
    class DownloadDriver;
    typedef boost::shared_ptr<DownloadDriver> DownloadDriver__p;

    class PieceTask
        : public boost::noncopyable
#ifdef DUMP_OBJECT
        , public count_object_allocate<PieceTask>
#endif
    {
    public:
        PieceTask(DownloadDriver__p download_driver, VodDownloader__p downloader, u_int timeout)
            : download_driver_(download_driver)
            , downloader_(downloader)
            , timeout_(timeout)
        {

        }

    public:
        DownloadDriver__p download_driver_;
        VodDownloader__p downloader_;
        framework::timer::TickCounter start_time_;
        boost::uint32_t timeout_;
        bool bStartDownloading;
    public:
        bool IsTimeout() const;
        bool IsTimeout(VodDownloader__p downloader, const protocol::PieceInfoEx& piece_info) const;
        void OnPieceRequest()
        {
            if (!bStartDownloading)
            {
                bStartDownloading = true;
                start_time_.reset();
            }
        }
    };

    //////////////////////////////////////////////////////////////////////////
    class PieceRequestManager
        : public boost::noncopyable
        , public boost::enable_shared_from_this<PieceRequestManager>
    {
    public:
        typedef boost::shared_ptr<PieceRequestManager> p;

        static p Create(DownloadDriver__p download_driver) { return p(new PieceRequestManager(download_driver));}
        ~PieceRequestManager();
    public:
        void Start();
        void Stop();
    public:
        bool IsRequesting(const protocol::PieceInfo& piece_info) const;
        bool AddPieceTask(const protocol::PieceInfo& piece_info, VodDownloader__p downloader);
        void ClearTasks();
        bool RemovePieceTask(const protocol::PieceInfo& piece_info, VodDownloader__p downloader);
        void CheckPieceTimeout();
        bool HasNextPieceForDownload(boost::uint32_t playing_possition, protocol::PieceInfoEx &piece_info_ex, VodDownloader__p downloader);
        bool GetNextPieceForDownload(boost::uint32_t playing_possition, protocol::PieceInfoEx &piece_info_ex, VodDownloader__p downloader);
        void NoticePieceTaskTimeOut(const protocol::PieceInfoEx& piece_info_ex, VodDownloader__p downloader);
        void OnPieceRequest(const protocol::PieceInfo & piece)
        {
            std::map<protocol::PieceInfo, PieceTask *>::iterator iter = requesting_map_.find(piece);
            if (iter != requesting_map_.end())
                iter->second->OnPieceRequest();
        }
    public:
        void ClearTaskRangeMap();
        void AddTaskRange(boost::uint32_t start, boost::uint32_t end);
    private:

        volatile bool is_running_;
        std::map<protocol::PieceInfo, PieceTask *> requesting_map_;
        DownloadDriver__p download_driver_;
        std::map<boost::uint32_t, boost::uint32_t> task_range_map_;

    private:
        PieceRequestManager(DownloadDriver__p download_driver);
    };
}

#endif  // _P2SP_DOWNLOAD_PIECE_REQUEST_MANAGER_H_
