//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// DownloadDriver.h

#ifndef _P2SP_DOWNLOAD_DOWNLOADER_DRIVER_H_
#define _P2SP_DOWNLOAD_DOWNLOADER_DRIVER_H_

#include "storage/IStorage.h"
#include "statistic/DownloadDriverStatistic.h"
#include "p2sp/download/SwitchController.h"
#include "network/HttpRequest.h"
#include "network/HttpResponse.h"
#ifdef DUMP_OBJECT
#include "count_object_allocate.h"
#endif

namespace storage
{
    class Instance;
}

namespace statistic
{
    class BufferringMonitor;
}

namespace p2sp
{
    typedef struct _DOWNLOADDRIVER_STOP_DAC_DATA_STRUCT
    {
        boost::uint32_t       uSize;                                // 整个结构体大小
        Guid                  gPeerID;                              // B: 华哥ID
        Guid                  gResourceID;                          // C: ResourceID
        boost::uint8_t        aPeerVersion[4];                      // D: 内核版本：major, minor, micro, extra
        char                  szVideoName[512];                     // E: 视频名称/WCHAR
        char                  szOriginalUrl[1000];                  // F: Url
        char                  szOriginalReferUrl[1000];             // G: Refer Url
        boost::uint32_t       uDiskBytes;                           // H: 磁盘已有字节数
        boost::uint32_t       uVideoBytes;                          // I: 影片大小
        boost::uint32_t       uP2PDownloadBytes;                    // J: P2P下载字节数
        boost::uint32_t       uHttpDownloadBytes;                   // K: HTTP下载字节数
        boost::uint32_t       uAvgDownloadSpeed;                    // L: 平均下载速度 (B/s)
        boost::uint8_t        bIsSaveMode;                          // M: 是否是下载模式完成的此次下载
        boost::uint32_t       MaxHistoryDownloadSpeed;              // N: 最大历史下载速度
        boost::uint32_t       uAvgP2PDownloadSpeed;                 // O: P2P平均下载速度
        boost::uint32_t       uMaxHttpDownloadSpeed;                // P: 最大HTTP下载速度
        boost::uint16_t       uConnectedPeerCount;                  // Q: 连接上的节点数
        boost::uint16_t       uFullPeerCount;                       // R: 资源全满节点数
        boost::uint16_t       uBakHostStatus;                       // S: 备用CDN状态
        boost::uint16_t       uQueriedPeerCount;                    // T: 查询到的节点数
        boost::uint16_t       uSourceType;                          // U: SourceType
        boost::uint32_t       uDataRate;                            // V: 码流率
        boost::uint32_t       uAccelerateHttpSpeed;                 // W: 加速状态机切换之前的速度
        boost::uint32_t       uAccelerateStatus;                    // X: 加速状态机的状态
        boost::uint32_t       reserved1;                            // Y: 备用 (原来是客户端提交，操作系统版本)
        boost::uint32_t       file_length_in_second;                // Z: 文件时常(原来是客户端提交的)
        boost::uint32_t       download_time;                        // A1: 下载所用的时间
        boost::uint32_t       tiny_drag_result;                     // B1: tiny_drag结果
        boost::uint32_t       is_got_rid;                           // C1: 是否获得RID(0未获得;1获得)
        boost::uint32_t       total_downloaded_bytes;               // D1: J+K
        boost::uint32_t       bwtype;                               // E1: bwtype
        boost::uint32_t       http_avg_speed_in_KBps;               // F1: http 平均下载速度
        boost::uint32_t       p2p_avg_speed_in_KBps;                // G1: p2p 平均下载速度
        boost::uint32_t       connect_full_time_in_seconds;         // J1: p2p 连满的时间
        bool                  is_head_only;                         // K1: 是否下载MP4头部
        boost::uint32_t       avg_connect_rtt;                      // L1: 平均RTT
        boost::uint32_t       avg_lost_rate;                        // M1: UDP丢包率
        boost::uint32_t       avg_http_download_byte;               // N1: HTTP平均下载的长度
        boost::uint32_t       retry_rate;                           // O1: 冗余率
        boost::uint32_t       tiny_drag_http_status;                // P1: drag状态码
        boost::uint32_t       total_sn_download_bytes;              // Q1: SN下载字节数
        bool                  is_push;                              // Q2: 是否是push任务

    } DOWNLOADDRIVER_STOP_DAC_DATA_STRUCT, *LPDOWNLOADDRIVER_STOP_DAC_DATA_STRUCT;

    class VodDownloader;
    typedef boost::shared_ptr<VodDownloader> VodDownloader__p;
    class PieceRequestManager;
    typedef boost::shared_ptr<PieceRequestManager> PieceRequestManager__p;
    class ProxyConnection;
    typedef boost::shared_ptr<ProxyConnection> ProxyConnection__p;
    class P2PDownloader;
    typedef boost::shared_ptr<P2PDownloader> P2PDownloader__p;
    class HttpDownloader;
    typedef boost::shared_ptr<HttpDownloader> HttpDownloader__p;
    class HttpDragDownloader;
    typedef boost::shared_ptr<HttpDragDownloader> HttpDragDownloader__p;

    class DownloadDriver
        // : public boost::noncopyable
        : public boost::enable_shared_from_this<DownloadDriver>
        , public storage::IDownloadDriver
        , public IGlobalControlTarget
#ifdef DUMP_OBJECT
        , public count_object_allocate<DownloadDriver>
#endif
    {
        friend struct storage::IInstance;
    public:
        typedef boost::shared_ptr<DownloadDriver> p;
        static p create(
            boost::asio::io_service & io_svc,
            ProxyConnection__p proxy_connetction) {
            return p(new DownloadDriver(io_svc, proxy_connetction));
        }
        ~DownloadDriver();
    public:
        void Start(const network::HttpRequest::p http_request_demo, const protocol::UrlInfo& origanel_url_info, bool is_support_start, boost::int32_t control_mode=-1);
        void Start(const network::HttpRequest::p http_request_demo, const protocol::RidInfo& rid_for_play, boost::int32_t control_mode=-1);
        void Start(const protocol::UrlInfo& url_info, bool is_support_start, bool open_service = false, boost::int32_t control_mode=-1, boost::int32_t force_mode = 0/*FORCE_MODE_NORMAL*/);

        void Stop();

        bool IsRunning() const { return is_running_; }

    public:
        // void OnPlayTimer(uint32_t times);

        boost::shared_ptr<storage::Instance> GetInstance() const { return instance_; }

        void SetDownloaderToDeath(VodDownloader__p downloader);

        void OnNoticePragmaInfo(string server_mod, uint32_t head_length);

        void OnNoticeConnentLength(uint32_t file_length, VodDownloader__p downloader, network::HttpResponse::p http_response);

        void OnNotice304Header(VodDownloader__p downloader, network::HttpResponse::p http_response);

        void OnNotice403Header(VodDownloader__p downloader, network::HttpResponse::p http_response);

        // void OnNoticeDownloadComplete();

        void OnPieceComplete(protocol::PieceInfo piece_info, VodDownloader__p downloader);

        void OnPieceFaild(protocol::PieceInfo piece_info, VodDownloader__p downloader);

        bool HasNextPiece(VodDownloader__p downloader);

        bool RequestNextPiece(VodDownloader__p downloader);

        statistic::DownloadDriverStatistic::p GetStatistic() const { return statistic_; }

        P2PDownloader__p GetP2PDownloader() { return p2p_downloader_; }

        boost::int32_t GetDownloadDriverID() const { return id_; }
        static boost::int32_t GetDownloaderDriverSID() {return s_id_ ++;}

        bool IsHttpDownloaderSupportRange();

        bool IsPlayByRID() const { return is_play_by_rid_; }

        void NoticePieceTaskTimeOut(const protocol::PieceInfoEx& piece_info_ex, VodDownloader__p downloader);

        protocol::UrlInfo GetOriginalUrlInfo() const { return original_url_info_; }

        bool IsHttp403Header() const { return is_http_403_header_; }

        bool NeedBubble() const { return need_bubble_; }
        void SetNeedBubble(bool bubble) { need_bubble_ = bubble; }

        bool IsPush() const { return is_push_; }
        void SetIsPush(bool is_push) { is_push_ = is_push; }

        void SetSpeedLimitInKBps(boost::int32_t speed_limit_in_KBps);
        boost::int32_t GetSpeedLimitInKBps();

        void SetOpenServiceStartPosition(uint32_t start_position);

        void SetOpenServiceHeadLength(uint32_t head_length);

        void SetOpenServiceRange(bool is_openservice_range);
        void SetIsDrag(bool is_drag);

        void OnNoticeFileDownloadComplete();

        bool IsOpenService() const { return is_open_service_; }

        void SetSessionID(const string& session_id) { session_id_ = session_id; }
        string GetSessionID() const { return session_id_; }

        void SetOpenServiceFileName(const string& filename) { openservice_file_name_ = filename; }
        string GetOpenServiceFileName() const { return openservice_file_name_; }

        void SetSourceType(uint32_t source_type) { source_type_ = source_type; }
        uint32_t GetSourceType() const { return source_type_; }

        void SetIsHeadOnly(bool head_only) { is_head_only_ = head_only; }

        uint32_t GetOpenServiceHeadLength() {return openservice_head_length_;}
        void SetBWType(JumpBWType bwtype) {bwtype_ = bwtype;}

        bool IsSaveMode() const;
        bool IsDownloading() const;
        boost::uint32_t GetSecondDownloadSpeed();
        void OnPieceRequest(const protocol::PieceInfo & piece);

        void SetRestPlayTime(boost::uint32_t rest_play_time);
        void SetDownloadMode(boost::int32_t download_mode);
        void GetDragMachineState(boost::int32_t & state);

        void SmartLimitSpeed(boost::uint32_t times);
        void EnableSmartSpeedLimit();
        void DisableSmartSpeedLimit();

        void NoticeP2pSpeedLimited();
        void SetBakHosts(const std::vector<std::string> & bak_hosts) {bak_hosts_ = bak_hosts;}

        void SetRidInfo(const protocol::RidInfo & ridinfo);
        void ReportDragFetchResult(uint32_t drag_fetch_result){ drag_fetch_result_ = drag_fetch_result;}

        bool IsDragLocalPlayForClient();

        void ReportDragHttpStatus(boost::uint32_t tiny_drag_http_status);

    public:
        //////////////////////////////////////////////////////////////////////////
        // IGlobalControlTarget

        virtual uint32_t GetBandWidth();
        virtual uint32_t GetFileLength();
        virtual uint32_t GetDataRate();
        virtual uint32_t GetPlayElapsedTimeInMilliSec();
        virtual uint32_t GetDownloadingPosition();
        virtual uint32_t GetDownloadedBytes();
        virtual uint32_t GetDataDownloadSpeed();
        virtual bool IsStartFromNonZero();
        virtual bool IsDrag();
        virtual bool IsHeadOnly();
        virtual bool HasRID();
        virtual void SetSwitchState(boost::int32_t h, boost::int32_t p, boost::int32_t tu, boost::int32_t t);
        virtual boost::uint32_t GetRestPlayableTime();
        virtual boost::int32_t GetDownloadMode();
        virtual void SetDragMachineState(boost::int32_t state);
        virtual bool IsPPLiveClient();
        virtual bool IsDragLocalPlayForSwitch();
        virtual void SetAcclerateStatus(boost::int32_t status);
        virtual JumpBWType GetBWType() {return bwtype_;}
        virtual void NoticeLeave2300();
        virtual void SetDragHttpStatus(int32_t status);
        virtual std::vector<IHTTPControlTarget::p> GetAllHttpControlTargets();
        virtual void ReportUseBakHost() {bak_host_status_ = BAK_HOST_GREAT;}
        virtual void ReportBakHostFail() {bak_host_status_ = BAK_HOST_USELESS;}

        virtual IHTTPControlTarget::p GetHTTPControlTarget();
        virtual IP2PControlTarget::p GetP2PControlTarget();

        virtual void OnStateMachineType(uint32_t state_machine_type);
        virtual void OnStateMachineState(const string& state_machine_state);

        virtual void OnTimerElapsed(framework::timer::Timer * pointer);

        virtual bool ShouldUseCDNWhenLargeUpload() const { return false; }
        virtual boost::uint32_t GetRestPlayTimeDelim() const { return 0; }
        virtual bool IsUploadSpeedLargeEnough() { return true; }

    private:
        HttpDownloader__p AddHttpDownloader(const protocol::UrlInfo& url_info, bool is_orginal = false);

        HttpDownloader__p AddHttpDownloader(network::HttpRequest::p http_request_demo, const protocol::UrlInfo& url_info, bool is_orginal = false);

        // 原来trunk没有这个函数
        // void OnDownloaderConnected(VodDownloader__p downloader);

        void ChangeToPoolModel();

        void GetSubPieceForPlay();

        // 在Stop的时候向客户端发送DAC统计消息
        void SendDacStopData();

        void AddBakHttpDownloaders(protocol::UrlInfo& original_url_info);

        void DetectBufferring();
        void StartBufferringMonitor();

        bool IsLocalDataEnough(const boost::uint32_t second);

        // SN
        void SNStrategy();

    protected:
        // IDownloadDriver 接口消息
        // 下载完成之后, 生成的RidInfo 消息
        virtual void OnNoticeChangeResource(boost::shared_ptr<storage::Instance> instance_old, boost::shared_ptr<storage::Instance> instance_new);
        // 通知 DownloadDriver  rid 从无到有
        virtual void OnNoticeRIDChange();
        // 通知 DownloadDriver 下载完成
        virtual void OnNoticeDownloadComplete();
        // 通知 DownloadDriver 一个Block检验成功
        virtual void OnNoticeMakeBlockSucced(uint32_t block_info);
        // 通知 DownloadDriver 一个Block检验失败
        virtual void OnNoticeMakeBlockFailed(uint32_t block_info);
        // 通知 DownloadDriver contenthash校验成功
        virtual void OnNoticeContentHashSucced(string url, MD5 content_md5, uint32_t content_bytes, uint32_t file_length);
        // 通知获得文件名
        virtual void OnNoticeGetFileName(const string& file_name);

        virtual void OnNoticeSetWebUrl(const string& web_url);

        virtual void OnRecvSubPiece(uint32_t position, const protocol::SubPieceBuffer& buffer);

        virtual uint32_t GetPlayingPosition() const;

        virtual bool IsHeaderResopnsed();
    private:
        boost::asio::io_service & io_svc_;
        protocol::RidInfo rid_info_;

        P2PDownloader__p p2p_downloader_;
        std::set<VodDownloader__p> downloaders_;
        
        struct UrlHttpDownloaderPair
        {
            UrlHttpDownloaderPair(string url, HttpDownloader__p http_downloader)
                : url_(url)
                , http_downloader_(http_downloader)
            {

            }
            string url_;
            HttpDownloader__p http_downloader_;
        };

        class UrlHttpDownloaderEqual
        {
        public:
            UrlHttpDownloaderEqual(string url)
                : url_(url)
            {

            }
            bool operator() (const UrlHttpDownloaderPair & other)
            {
                if (url_ == other.url_)
                {
                    return true;
                }
                else
                {
                    return false;
                }
            }
        private:
            string url_;
        };

        std::list<UrlHttpDownloaderPair> url_indexer_;
        PieceRequestManager__p piece_request_manager_;
        boost::shared_ptr<storage::Instance> instance_;
        ProxyConnection__p proxy_connection_;
        statistic::DownloadDriverStatistic::p statistic_;
        boost::shared_ptr<statistic::BufferringMonitor> bufferring_monitor_;
        uint32_t max_rest_playable_time_;

        protocol::UrlInfo original_url_info_;
        uint32_t block_check_faild_times_;
        bool is_complete_;
        boost::int32_t id_;
        static boost::int32_t s_id_;
        bool is_support_start_;
        bool is_pausing_;
        bool is_open_service_;
        bool is_drag_;
        bool is_drag_local_play_for_switch_;
        bool is_drag_local_play_for_client_;
        boost::int32_t drag_machine_state_;

        bool is_http_403_header_;
        bool is_http_304_header_;

        bool is_play_by_rid_;

        bool is_pool_mode_;
        bool need_bubble_;
        bool is_push_;

        SwitchController::p switch_controller_;
        SwitchController::ControlModeType switch_control_mode_;

        volatile bool is_running_;

        framework::timer::TickCounter download_time_counter_;
        boost::int32_t speed_limit_in_KBps_;

        // openservice
        uint32_t openservice_start_position_;
        uint32_t openservice_head_length_;
        bool is_pragmainfo_noticed_;
        string openservice_file_name_;
        uint32_t init_local_data_bytes_;
        uint32_t source_type_;
        bool is_openservice_range_;
        string session_id_;
        bool is_head_only_;
        boost::uint32_t rest_play_time_;
        framework::timer::TickCounter rest_play_time_set_counter_;
        boost::int32_t download_mode_;
        bool is_got_accelerate_http_speed;
        boost::int32_t accelerate_http_speed;
        boost::int32_t accelerate_status_;
        JumpBWType bwtype_;

        bool disable_smart_speed_limit_;

        framework::timer::PeriodicTimer second_timer_;
        int32_t avg_download_speed_before_limit_;
        int32_t avg_http_download_speed_in2300_;
        int32_t drag_http_status_;

        HttpDragDownloader__p http_drag_downloader_;
        // drag_fetch_result 位字段定义，第0位为最高位
        // 如果不需要获取drag，约定为0xFFFFFFFF
        // 0：是否获取成功(成功:1,失败:0)
        // 1：获取来源(http:0, udp:1)
        // 2：是否解析成功(成功:1,失败:0)
        // 4-7: 获取次数(无论是否获取成功都有)
        // 8-31：获取时间(ms)(仅仅在获取成功时设置)
        uint32_t drag_fetch_result_;

        boost::uint32_t tiny_drag_http_status_;

        std::vector<std::string> bak_hosts_;
        enum BakHostStatus
        {
            BAK_HOST_NONE = 0,
            BAK_HOST_GREAT,
            BAK_HOST_USELESS
        } bak_host_status_;

        // SN
        bool is_sn_added_;

    private:
        DownloadDriver(
            boost::asio::io_service & io_svc,
            ProxyConnection__p proxy_connetction);
    };
}

#endif  // _P2SP_DOWNLOAD_DOWNLOADER_DRIVER_H_
