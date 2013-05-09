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
#include "p2sp/proxy/PlayInfo.h"
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
    struct DOWNLOADDRIVER_STOP_DAC_DATA_STRUCT
    {
        DOWNLOADDRIVER_STOP_DAC_DATA_STRUCT()
        {
            gPeerID = RID();
            gResourceID = RID();
            memset(aPeerVersion, 0, sizeof(aPeerVersion));
            memset(szVideoName, 0, sizeof(szVideoName));
            memset(szOriginalUrl, 0, sizeof(szOriginalUrl));
            memset(szOriginalReferUrl, 0, sizeof(szOriginalReferUrl));
            uDiskBytes = 0;
            uVideoBytes = 0;
            peer_downloadbytes_without_redundance = 0;
            http_downloadbytes_without_redundance = 0;
            uAvgDownloadSpeed = 0;
            bIsSaveMode = 0;
            MaxHistoryDownloadSpeed = 0;
            uAvgP2PDownloadSpeed = 0;
            uMaxHttpDownloadSpeed = 0;
            uConnectedPeerCount = 0;
            uFullPeerCount = 0;
            uBakHostStatus = 0;
            uQueriedPeerCount = 0;
            uSourceType = 0;
            uDataRate = 0;
            avg_http_download_speed_in2300 = 0;
            avg_download_speed_before_limit = 0;
            http_downloadbytes_with_redundance = 0;
            peer_downloadbytes_with_redundance = 0;
            download_time = 0;
            tiny_drag_result = 0;
            is_got_rid = 0;
            sn_downloadbytes_with_redundance = 0;
            bwtype = 0;
            http_avg_speed_in_KBps = 0;
            p2p_avg_speed_in_KBps = 0;
            connect_full_time_in_seconds = 0;
            is_head_only = 0;
            avg_connect_rtt = 0;
            avg_lost_rate = 0;
            avg_http_download_byte = 0;
            retry_rate = 0;
            tiny_drag_http_status = 0;
            sn_downloadbytes_without_redundance = 0;
            is_push = false;
            instance_is_push = false;
            vip = 0;
            total_http_start_download_bytes = 0;
            http_start_download_reason = 0;
            preroll = false;
            p2p_download_max_connect_count = 0;
            p2p_download_min_connect_count = 0;
            uUsedDiskSizeInMB = 0;
            uTotalDiskSizeInMB = 0;
            http_port = 0;
            total_list_request_packet_count = 0;
            total_list_response_packet_count = 0;
            is_fetch_tinydrag_success = 0;
            is_fetch_tinydrag_from_udp = 0;
            is_parse_tinydrag_success = 0;
            fetch_tinydrag_count = 0;
            fetch_tinydrag_time = 0;
            channel_name = "";
            tracker_respons_info = "";
            peer_connect_request_sucess_count = "";
            nat_type = 0;
            nat_check_state = 0;
            more_than_one_proxyconnections = false;
            bak_host_string = "";
        }

        Guid                  gPeerID;                              // B: 华哥ID
        Guid                  gResourceID;                          // C: ResourceID
        boost::uint16_t       aPeerVersion[4];                      // D: 内核版本：major, minor, micro, extra
        char                  szVideoName[512];                     // E: 视频名称/WCHAR
        char                  szOriginalUrl[1000];                  // F: Url
        char                  szOriginalReferUrl[1000];             // G: Refer Url
        boost::uint32_t       uDiskBytes;                           // H: 磁盘已有字节数
        boost::uint32_t       uVideoBytes;                          // I: 影片大小
        boost::uint32_t       peer_downloadbytes_without_redundance;// J: Peer总下载字节数（不带冗余）
        boost::uint32_t       http_downloadbytes_without_redundance;// K: http总下载字节数（不带冗余）
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
        boost::uint32_t       avg_http_download_speed_in2300;       // W: 2300状态的http平均速度
        boost::uint32_t       avg_download_speed_before_limit;      // X: 不限速时的平均下载速度
        boost::uint32_t       http_downloadbytes_with_redundance;   // Y: HTTP总下载字节数（包含冗余）
        boost::uint32_t       peer_downloadbytes_with_redundance;   // Z: Peer总下载字节数（包含冗余）
        boost::uint32_t       download_time;                        // A1: 下载所用的时间
        boost::uint32_t       tiny_drag_result;                     // B1: tiny_drag结果
        boost::uint32_t       is_got_rid;                           // C1: 是否获得RID(0未获得;1获得)
        boost::uint32_t       sn_downloadbytes_with_redundance;     // D1: SN 总下载字节数（包含冗余）
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
        boost::uint32_t       sn_downloadbytes_without_redundance;  // Q1: SN总下载字节数（不带冗余）
        bool                  is_push;                              // R1: 是否是push任务
        bool                  instance_is_push;                     // S1: 是否是push下载的任务，用于判断命中率
        boost::uint32_t       vip;                                  // T1: VIP
        boost::int32_t        total_http_start_download_bytes;      // U1: 由http启动下载字节数
        boost::uint32_t       http_start_download_reason;           // V1: 记录导致http启动下载的原因
        bool                  preroll;                              // W1: 记录是否是客户端跨集预下载
        boost::uint32_t       p2p_download_max_connect_count;       // X1: p2p最大连接数
        boost::uint32_t       p2p_download_min_connect_count;       // Y1: p2p最小连接数
        boost::uint32_t       uUsedDiskSizeInMB;                    // Z1: 缓存目录已用大小
        boost::uint32_t       uTotalDiskSizeInMB;                   // A2: 缓存目录设置大小
        boost::uint16_t       http_port;                            // B2: 本地http server监听端口
        boost::uint32_t       total_list_request_packet_count;      // C2: 发给tracker用于查询peer list包的总数
        boost::uint32_t       total_list_response_packet_count;     // D2: Tracker返回的list包总数
        boost::uint32_t       is_fetch_tinydrag_success;            // E2: 是否获取成功(成功:1,失败:0)
        boost::uint32_t       is_fetch_tinydrag_from_udp;           // F2: 获取来源(http:0, udp:1)
        boost::uint32_t       is_parse_tinydrag_success;            // G2: 是否解析成功(成功:1,失败:0)
        boost::uint32_t       fetch_tinydrag_count;                 // H2: 获取次数(无论是否获取成功都有)
        boost::uint32_t       fetch_tinydrag_time;                  // I2: 获取时间(ms)(仅仅在获取成功时设置)
        string                channel_name;                         // J2: channel name
        string                tracker_respons_info;                 // K2: 分别统计向每组station tracker发送的list包数和响应数
        string                peer_connect_request_sucess_count;    // L2: 每种nat类型连接请求数以及成功数
        boost::uint32_t       nat_type;                             // M2: 获取NAT类型
        int                   nat_check_state;                      // N2: 当前nat检测状态
        string                p2p_location_bytes_with_redundance;   // O2: p2p下载的字节里，来自不同地域源的信息
        string                vvid;                                 // P2: vvid
        bool                  more_than_one_proxyconnections;       // Q2: 是否某一个时段有多个proxyconnections在请求内容
        string                bak_host_string;                      // R2: bak_host
        boost::uint32_t       block_hash_failed_count;              // S2: block校验失败次数
    };

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

    enum VIP_LEVEL
    {
        NO_VIP = 0,
        VIP
    };

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
            ProxyConnection__p proxy_connetction)
        {
            return p(new DownloadDriver(io_svc, proxy_connetction));
        }

    public:
        void Start(const network::HttpRequest::p http_request_demo, const protocol::UrlInfo& origanel_url_info, bool is_support_start, boost::int32_t control_mode=-1);
        void Start(const network::HttpRequest::p http_request_demo, const protocol::RidInfo& rid_for_play, boost::int32_t control_mode=-1);
        void Start(const protocol::UrlInfo& url_info, bool is_support_start, bool open_service = false, boost::int32_t control_mode=-1, boost::int32_t force_mode = 0/*FORCE_MODE_NORMAL*/);

        void Stop();

        bool IsRunning() const { return is_running_; }

    public:
        // void OnPlayTimer(boost::uint32_t times);

        boost::shared_ptr<storage::Instance> GetInstance() const { return instance_; }

        void SetDownloaderToDeath(VodDownloader__p downloader);

        void OnNoticePragmaInfo(string server_mod, boost::uint32_t head_length);

        void OnNoticeConnentLength(boost::uint32_t file_length, VodDownloader__p downloader, network::HttpResponse::p http_response);

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

        bool IsPlayByRID() const { return is_play_by_rid_; }

        void NoticePieceTaskTimeOut(const protocol::PieceInfoEx& piece_info_ex, VodDownloader__p downloader);

        protocol::UrlInfo GetOriginalUrlInfo() const { return original_url_info_; }

        bool IsHttp403Header() const { return is_http_403_header_; }

        bool IsPush() const { return is_push_; }
        void SetIsPush(bool is_push) { is_push_ = is_push; }

        void SetSpeedLimitInKBps(boost::int32_t speed_limit_in_KBps);
        boost::int32_t GetSpeedLimitInKBps();

        void SetOpenServiceStartPosition(boost::uint32_t start_position);

        void SetOpenServiceHeadLength(boost::uint32_t head_length);

        void SetOpenServiceRange(bool is_openservice_range);
        void SetIsDrag(bool is_drag);

        void OnNoticeFileDownloadComplete();

        bool IsOpenService() const { return is_open_service_; }

        void SetSessionID(const string& session_id) { session_id_ = session_id; }
        string GetSessionID() const { return session_id_; }

        void SetOpenServiceFileName(const string& filename) { openservice_file_name_ = filename; }
        string GetOpenServiceFileName() const { return openservice_file_name_; }

        void SetSourceType(boost::uint32_t source_type) { source_type_ = source_type; }

        void SetIsHeadOnly(bool head_only) { is_head_only_ = head_only; }

        boost::uint32_t GetOpenServiceHeadLength() {return openservice_head_length_;}
        void SetBWType(JumpBWType bwtype) {bwtype_ = bwtype;}

        bool IsSaveMode() const;
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

        void SetVipLevel(VIP_LEVEL vip) {vip_level_ = vip;}
        void SetDownloadLevel(PlayInfo::DownloadLevel download_level) {download_level_ = download_level;}

        void SetRidInfo(const protocol::RidInfo & ridinfo);
        void ReportDragFetchResult(boost::uint32_t drag_fetch_result, boost::uint32_t is_fetch_tinydrag_success, 
            boost::uint32_t is_fetch_tinydrag_from_udp, boost::uint32_t is_parse_tinydrag_success, 
            boost::uint32_t fetch_tinydrag_count, boost::uint32_t fetch_tinydrag_time)
        { 
            drag_fetch_result_ = drag_fetch_result;
            is_fetch_tinydrag_success_ = is_fetch_tinydrag_success;
            is_fetch_tinydrag_from_udp_ = is_fetch_tinydrag_from_udp;
            is_parse_tinydrag_success_ = is_parse_tinydrag_success;
            fetch_tinydrag_count_ = fetch_tinydrag_count;
            fetch_tinydrag_time_ = fetch_tinydrag_time;
        }

        bool IsDragLocalPlayForClient();

        void ReportDragHttpStatus(boost::uint32_t tiny_drag_http_status);

        void RestrictSendListLength(boost::uint32_t postion,vector<protocol::SubPieceBuffer>&buffers);
        void SetPreroll(bool is_preroll) {is_preroll_ = is_preroll;}

        boost::uint32_t GetRestTimeNeedLimitSpeed()const {return rest_play_time_need_to_limit_speed_;}

        void ResumeOrPause(bool need_pause);

    public:
        //////////////////////////////////////////////////////////////////////////
        // IGlobalControlTarget

        virtual boost::uint32_t GetBandWidth();
        virtual boost::uint32_t GetFileLength();
        virtual boost::uint32_t GetDataRate();
        virtual boost::uint32_t GetPlayElapsedTimeInMilliSec();
        virtual boost::uint32_t GetDownloadingPosition();
        virtual boost::uint32_t GetDownloadedBytes();
        virtual boost::uint32_t GetDataDownloadSpeed();
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
        virtual JumpBWType GetBWType() {return bwtype_;}
        virtual void NoticeLeave2300();
        virtual void NoticeLeave2000();
        virtual void SetDragHttpStatus(boost::int32_t status);
        virtual std::vector<IHTTPControlTarget::p> GetAllHttpControlTargets();
        virtual void ReportUseBakHost() {bak_host_status_ = BAK_HOST_GREAT;}
        virtual void ReportBakHostFail() {bak_host_status_ = BAK_HOST_USELESS;}

        virtual IHTTPControlTarget::p GetHTTPControlTarget();
        virtual IP2PControlTarget::p GetP2PControlTarget();

        virtual void OnStateMachineType(boost::uint32_t state_machine_type);
        virtual void OnStateMachineState(const string& state_machine_state);

        virtual void OnTimerElapsed(framework::timer::Timer * pointer);
        virtual boost::uint32_t GetVipLevel() {return vip_level_;}
        virtual boost::uint32_t GetDownloadLevel() {return download_level_;}

        boost::uint32_t GetSourceType() const { return source_type_; }
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

        void DoCDNFlowStatistic();

        void LoadSnOnCDN();

        void GetSnListOnCDN(std::list<boost::asio::ip::udp::endpoint> &sn_list);

        string GetBakHostString();

    protected:
        // IDownloadDriver 接口消息
        // 下载完成之后, 生成的RidInfo 消息
        virtual void OnNoticeChangeResource(boost::shared_ptr<storage::Instance> instance_old, boost::shared_ptr<storage::Instance> instance_new);
        // 通知 DownloadDriver  rid 从无到有
        virtual void OnNoticeRIDChange();
        // 通知 DownloadDriver 下载完成
        virtual void OnNoticeDownloadComplete();
        // 通知 DownloadDriver 一个Block检验成功
        virtual void OnNoticeMakeBlockSucced(boost::uint32_t block_info);
        // 通知 DownloadDriver 一个Block检验失败
        virtual void OnNoticeMakeBlockFailed(boost::uint32_t block_info);
        // 通知获得文件名
        virtual void OnNoticeGetFileName(const string& file_name);

        virtual void OnRecvSubPiece(boost::uint32_t position, const protocol::SubPieceBuffer& buffer);

        virtual boost::uint32_t GetPlayingPosition() const;

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
        boost::uint32_t max_rest_playable_time_;

        protocol::UrlInfo original_url_info_;
        boost::uint32_t block_check_faild_times_;
        boost::int32_t id_;
        static boost::int32_t s_id_;
        bool is_support_start_;
        bool is_open_service_;
        bool is_drag_;
        bool is_drag_local_play_for_switch_;
        bool is_drag_local_play_for_client_;
        boost::int32_t drag_machine_state_;

        bool is_http_403_header_;
        bool is_http_304_header_;

        bool is_play_by_rid_;

        bool is_pool_mode_;
        bool is_push_;

        SwitchController::p switch_controller_;
        SwitchController::ControlModeType switch_control_mode_;

        volatile bool is_running_;

        framework::timer::TickCounter download_time_counter_;
        boost::int32_t speed_limit_in_KBps_;

        // openservice
        boost::uint32_t openservice_start_position_;
        boost::uint32_t openservice_head_length_;
        bool is_pragmainfo_noticed_;
        string openservice_file_name_;
        boost::uint32_t init_local_data_bytes_;
        boost::uint32_t source_type_;
        string session_id_;
        bool is_head_only_;
        boost::uint32_t rest_play_time_;
        boost::uint32_t rest_play_time_need_to_limit_speed_;
        framework::timer::TickCounter rest_play_time_set_counter_;
        boost::int32_t download_mode_;
        JumpBWType bwtype_;
        bool is_preroll_;       //标记是否跨集预下载

        bool disable_smart_speed_limit_;

        framework::timer::PeriodicTimer second_timer_;
        boost::int32_t avg_download_speed_before_limit_;
        boost::int32_t avg_http_download_speed_in2300_;
        boost::int32_t drag_http_status_;

        HttpDragDownloader__p http_drag_downloader_;
        // drag_fetch_result 位字段定义，第0位为最高位
        // 如果不需要获取drag，约定为0xFFFFFFFF
        // 0：是否获取成功(成功:1,失败:0)
        // 1：获取来源(http:0, udp:1)
        // 2：是否解析成功(成功:1,失败:0)
        // 4-7: 获取次数(无论是否获取成功都有)
        // 8-31：获取时间(ms)(仅仅在获取成功时设置)
        boost::uint32_t drag_fetch_result_;
        boost::uint32_t       is_fetch_tinydrag_success_;      // 是否获取成功(成功:1,失败:0)
        boost::uint32_t       is_fetch_tinydrag_from_udp_;     // 获取来源(http:0, udp:1)
        boost::uint32_t       is_parse_tinydrag_success_;      // 是否解析成功(成功:1,失败:0)
        boost::uint32_t       fetch_tinydrag_count_;           // 获取次数(无论是否获取成功都有)
        boost::uint32_t       fetch_tinydrag_time_;            // 获取时间(ms)(仅仅在获取成功时设置)

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

        boost::uint32_t vip_level_;
        boost::uint32_t download_level_;

        enum Http_Start_Download_Reason
        {
            NORMAL_LAUNCH = 0,
            PREDOWNLOAD,                                           //拖动后下载当前播放段的下一段定义为预下载
            DRAG,
            NONE_RID,                                              //表示因没有取到RID而导致http下载，不包含BWType是httponly的情况
            INVALID                                                //不是使用OpenServiceVideoMode状态机，不统计导致http启动下载原因
        } http_download_reason_;

        boost::uint32_t total_http_start_downloadbyte_;            //http启动下载引起的CDN数据总量
        boost::uint32_t position_after_drag_;                       //接收起始播放位置，用于判断实际拖动
        boost::int32_t total_download_byte_2000_;                  //http启动下载阶段2000状态下载数据总量
        boost::int32_t total_download_byte_2300_;                  //http启动下载阶段2300状态下载数据总量
        bool has_effected_by_other_proxyconnections_;              // 某一时段有多个proxyconnection存在

    private:
        DownloadDriver(
            boost::asio::io_service & io_svc,
            ProxyConnection__p proxy_connetction);
    };
}

#endif  // _P2SP_DOWNLOAD_DOWNLOADER_DRIVER_H_
