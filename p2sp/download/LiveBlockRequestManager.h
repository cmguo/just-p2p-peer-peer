#ifndef _LIVE_BLOCK_REQUEST_MANAGER_H_
#define _LIVE_BLOCK_REQUEST_MANAGER_H_

namespace p2sp
{
    class LiveDownloader;
    typedef boost::shared_ptr<LiveDownloader> LiveDownloader__p;

    class LiveDownloadDriver;
    typedef boost::shared_ptr<LiveDownloadDriver> LiveDownloadDriver__p;

    class LiveBlockTask;
    typedef boost::shared_ptr<LiveBlockTask> LiveBlockTask__p;

    class LiveBlockRequestManager
#ifdef DUMP_OBJECT
        : public count_object_allocate<LiveBlockRequestManager>
#endif
    {
    public:
        void Start(LiveDownloadDriver__p live_download_driver);
        void Stop();
        bool GetNextBlockForDownload(boost::uint32_t request_block_id, LiveDownloader__p download);
        void AddBlockTask(const protocol::LiveSubPieceInfo & live_block, LiveDownloader__p & download);
        void RemoveBlockTask(boost::uint32_t block_id);
        // 判断LiveBlockInfoEx是否正在下载
        bool IsRequesting(const boost::uint32_t block_id);

        // 判断LiveBlockInfoEx是否超时
        bool IsTimeout(const boost::uint32_t block_id, const LiveDownloader__p & request_download);

    private:
        LiveDownloadDriver__p live_download_driver_;
        std::map<boost::uint32_t, LiveBlockTask__p> live_block_requesting_map_;
    };

    class LiveBlockTask
        : public boost::noncopyable
        , public boost::enable_shared_from_this<LiveBlockTask>
#ifdef DUMP_OBJECT
        , public count_object_allocate<LiveBlockTask>
#endif
    {
    public:
        typedef boost::shared_ptr<LiveBlockTask> p;
        static p Create(const protocol::LiveSubPieceInfo & live_block, LiveDownloader__p & download, boost::uint32_t & timeout)
        {
            return p(new LiveBlockTask(live_block, download, timeout));
        }

        bool IsTimeout(const LiveDownloader__p & request_download);
        void Timeout();

    private:
        protocol::LiveSubPieceInfo live_block_;
        framework::timer::TickCounter tick_count_;
        LiveDownloader__p download_;
        boost::uint32_t timeout_;

        LiveBlockTask(const protocol::LiveSubPieceInfo & live_block, LiveDownloader__p & download, boost::uint32_t & timeout);
    };
}

#endif