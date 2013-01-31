//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef PEER_H
#define PEER_H

#ifdef PEER_PC_CLIENT
#  include <windows.h>
#  ifndef __MINGW32__
#    define PEER_API WINAPI
#  else
#    define PEER_API
#  endif
#else
#define PEER_API
#endif  // PEER_PC_CLIENT

#ifdef BOOST_HAS_DECLSPEC

#ifdef PEER_SOURCE
#  define PEER_DECL __declspec(dllexport)
#else
#  define PEER_DECL __declspec(dllimport)
#endif  // PEER_SOURCE

#else
#  define PEER_DECL __attribute__ ((visibility("default")))
#endif

// 内核启动参数结构体
#ifdef PEER_PC_CLIENT
#pragma pack(push, 1)
#endif

// 以下定义是为了让SOP编译通过
#ifndef BOOST_CSTDINT_HPP
namespace boost
{
    typedef signed char         int8_t;
    typedef unsigned char       uint8_t;
    typedef short               int16_t;
    typedef unsigned short      uint16_t;
    typedef long                int32_t;
    typedef unsigned long       uint32_t;
    typedef __int64             int64_t;
    typedef unsigned __int64    uint64_t;
};
#endif

typedef void (*LPSUBMITSTOPLOG)(std::string dac);

typedef struct _INDEXSTARTPARAM
{
    boost::uint8_t            cIndexServerType;
    char            szIndexDomain[32];        // IndexServer domain
    boost::uint16_t            usIndexPort;            // IndexServer UDP port
} INDEXSTRATPARAM, *LPINDEXSTRATPARAM;

typedef struct _WSTRATPARAM{
    boost::uint32_t            uSize;                    // 整个结构的大小
    boost::uint32_t            hWnd;                     // 通讯用Window句柄，界面此处填写接收消息的窗口句柄
    boost::uint16_t            usUdpPort;                // 要建立监听的UDP端口
    boost::uint16_t            usTcpPort;                // 要建立监听的TCP端口
    boost::uint16_t            usHttpProxyPort;          // 要建立的本地代理的端口
    INDEXSTRATPARAM            aIndexServer[16];         // aIndexServer组，目前只适用 IndexServer[0]
    boost::int32_t             bUseDisk;                 // 是否使用磁盘, 如果是TRUE, 则使用磁盘; 如果是FALSE, 则纯内存
    boost::uint64_t            ullDiskLimit;             // 使用磁盘上限
    wchar_t                    wszDiskPath[256];         // 磁盘使用路径
    boost::int32_t             reserve1;
    char                       reserve2[64];
    char                       szPeerGuid[32];           // Peer的GUID, 机器唯一 (16进制形式)
    // extend
    wchar_t                    wszConfigPath[256];       // 配置文件路径, "" 表示使用默认路径
    boost::int32_t             reserve3;
    boost::int32_t             bUsePush;                 // 是否进行push，默认为TRUE
    boost::int32_t             bReadOnly;                // 磁盘只读，默认为FALSE
    boost::int32_t             bHttpProxyEnabled;        // 是否进行本地代理监听，默认为TRUE
    boost::uint8_t             reserve4;
} WSTARTPARAM, *LPWSTARTPARAM;

typedef struct _STRATPARAM{
    boost::uint32_t            uSize;                    // 整个结构的大小
    boost::uint32_t            hWnd;                     // 通讯用Window句柄，界面此处填写接收消息的窗口句柄
    boost::uint16_t            usUdpPort;                // 要建立监听的UDP端口
    boost::uint16_t            usTcpPort;                // 要建立监听的TCP端口
    boost::uint16_t            usHttpProxyPort;          // 要建立的本地代理的端口
    INDEXSTRATPARAM            aIndexServer[16];         // aIndexServer组，目前只适用 IndexServer[0]
    boost::int32_t             bUseDisk;                 // 是否使用磁盘, 如果是TRUE, 则使用磁盘; 如果是FALSE, 则纯内存
    boost::uint64_t            ullDiskLimit;             // 使用磁盘上限
    char                       szDiskPath[512];          // 磁盘使用路径
    LPSUBMITSTOPLOG            submit_stop_log;
    char                       reserve2[64];
    char                       szPeerGuid[32];           // Peer的GUID, 机器唯一 (16进制形式)
    // extend
    char                       szConfigPath[512];        // 配置文件路径, "" 表示使用默认路径
    boost::int32_t             reserve3;
    boost::int32_t             bUsePush;                 // 是否进行push，默认为TRUE
    boost::int32_t             bReadOnly;                // 磁盘只读，默认为FALSE
    boost::int32_t             bHttpProxyEnabled;        // 是否进行本地代理监听，默认为TRUE
    boost::uint8_t             memory_pool_size_in_MB;
} STARTPARAM, *LPSTARTPARAM;

typedef struct _PEERSTATEMACHINE
{
    boost::int32_t state_machine_;
    boost::int32_t http_speed_;
    boost::int32_t p2p_speed_;
}PEERSTATEMACHINE, *LPSTATEMACHINE;


// 初始化系统，并且处理
// 参数说明: pErrMsg  返回错误信息; nBufSize    pErrMsg对应的SIZE
#ifdef PEER_PC_CLIENT
void        PEER_API    Startup(LPWSTARTPARAM lpParam);
typedef
void        (PEER_API    *LPSTARTUP)(LPWSTARTPARAM lpParam);
#else
// PPBOX兼容Startup接口
boost::uint32_t        PEER_API    Startup(LPSTARTPARAM lpParam);
typedef
boost::uint32_t        (PEER_API    *LPSTARTUP)(LPSTARTPARAM lpParam);
#endif

// 系统清除
void        PEER_API    Clearup();
typedef
void        (PEER_API    * LPCLEARUP)();

// 系统即将销毁
void        PEER_API    WillCleanup();
typedef
void        (PEER_API * LPWILLCLEANUP)();

// 删除内核因为发送消息而临时申请的内存
void        PEER_API    FreeBuffer(char * lpBuffer);
// 参数 lpBuffer, SubPieceBuffer 的首地址
typedef
void        (PEER_API    * LPFEERBUFFER)(char * lpBuffer);

// 用于上传控制的函数
void        PEER_API    SetMaxUploadSpeedInKBps(boost::int32_t MaxUploadP2PSpeedInKBps);
typedef
void        (PEER_API    * LPSETMAXUPLOADSPEEDINKBPS)(boost::int32_t MaxUploadP2PSpeedInKBps);

// 用于uPnP的开启
void        PEER_API    OpenUPNPSucced(boost::uint32_t ip, boost::uint16_t udp_port, boost::uint16_t tcp_port);
typedef
void        (PEER_API    * LPOPENUPNPSUCCED)(boost::uint32_t ip, boost::uint16_t udp_port, boost::uint16_t tcp_port);

// 设置Url对应的文件名
#ifdef PEER_PC_CLIENT
void        PEER_API    SetUrlFileName(char const * lpszUrl, boost::uint32_t nUrlLength, wchar_t const * lptszFileName, boost::uint32_t nFileNameLength);
typedef
void        (PEER_API    * LPSETURLFILENAME)(char const * lpszUrl, boost::uint32_t nUrlLength, wchar_t const * lptszFileName, boost::uint32_t nFileNameLength);
#else
// PPBOX兼容SetUrlFileName接口
void PEER_API SetUrlFileName(char const * lpszUrl, boost::uint32_t nUrlLength, char const * lpszFileName, boost::uint32_t nFileNameLength);
typedef
void (PEER_API * LPSETURLFILENAME)(char const * lpszUrl, boost::uint32_t nUrlLength, char const * lpszFileName, boost::uint32_t nFileNameLength);
#endif

// 向Peer上传特定操作的次数
void        PEER_API  UploadAction(boost::uint32_t uAction, boost::uint32_t uTimes);
typedef
void        (PEER_API * LPUPOADACTION)(boost::uint32_t uAction, boost::uint32_t uTimes);

//////////////////////////////////////////////////////////////////////////
/**
 * 通知下载某个url
 *
 * @param Url 下载地址
 * @param ReferUrl 下载Refer地址
 * @param WebUrl 下载html页面的地址
 * @param FileName 文件保存的完整路径
 */
#ifdef PEER_PC_CLIENT
void        PEER_API  StartDownload(char const * lpszUrl, boost::uint32_t nUrlLength,
                                  char const * lpszReferUrl, boost::uint32_t nReferUrlLength,
                                  char const * lpszWebUrl, boost::uint32_t nWebUrlLength,
                                  wchar_t const * lpszFileName, boost::uint32_t nFileNameLength);
typedef
void        (PEER_API * LPSTARTDOWNLOAD)(char const * lpszUrl, boost::uint32_t nUrlLength,
                                  char const * lpszReferUrl, boost::uint32_t nReferUrlLength,
                                  char const * lpszWebUrl, boost::uint32_t nWebUrlLength,
                                  wchar_t const * lpszFileName, boost::uint32_t nFileNameLength);
#else
// PPBOX兼容StartDownload接口
void PEER_API StartDownload(char const * lpszUrl, boost::uint32_t nUrlLength,
                            char const * lpszReferUrl, boost::uint32_t nReferUrlLength,
                            char const * lpszWebUrl, boost::uint32_t nWebUrlLength,
                            char const * lpszFileName, boost::uint32_t nFileNameLength);
typedef
void (PEER_API * LPSTARTDOWNLOAD)(char const * lpszUrl, boost::uint32_t nUrlLength,
                            char const * lpszReferUrl, boost::uint32_t nReferUrlLength,
                            char const * lpszWebUrl, boost::uint32_t nWebUrlLength,
                            char const * lpszFileName, boost::uint32_t nFileNameLength);
#endif
/**
 * 开始下载某个请求
 *
 * @param Url 下载地址
 * @param WebUrl 页面地址
 * @param RequestHeader 请求头部
 * @param FileName 文件名(包含文件扩展名, 不包含路径)
 */
#ifdef PEER_PC_CLIENT
void        PEER_API StartDownloadEx(char const * lpszUrl, boost::uint32_t nUrlLength,
                                   char const * lpszWebUrl, boost::uint32_t nWebUrlLength,
                                   char const * lpszRequestHeader, boost::uint32_t nRequestHeaderLength,
                                   wchar_t const * lpszFileName, boost::uint32_t nFileNameLength);
typedef
void        (PEER_API * LPSTARTDOWNLOADEX)(char const * lpszUrl, boost::uint32_t nUrlLength,
                                         char const * lpszWebUrl, boost::uint32_t nWebUrlLength,
                                         char const * lpszRequestHeader, boost::uint32_t nRequestHeaderLength,
                                         wchar_t const * lpszFileName, boost::uint32_t nFileNameLength);
#else
// PPBOX兼容StartDownloadEx接口
void PEER_API StartDownloadEx(char const * lpszUrl, boost::uint32_t nUrlLength,
                              char const * lpszWebUrl, boost::uint32_t nWebUrlLength,
                              char const * lpszRequestHeader, boost::uint32_t nRequestHeaderLength,
                              char const * lpszFileName, boost::uint32_t nFileNameLength);
typedef
void (PEER_API * LPSTARTDOWNLOADEX)(char const * lpszUrl, boost::uint32_t nUrlLength,
                                    char const * lpszWebUrl, boost::uint32_t nWebUrlLength,
                                    char const * lpszRequestHeader, boost::uint32_t nRequestHeaderLength,
                                    char const * lpszFileName, boost::uint32_t nFileNameLength);
#endif

/**
 * 通知停止下载某个url
 *
 * @param Url 下载地址
 */
void        PEER_API StopDownload(char const * lpszUrl, boost::uint32_t nUrlLength);
typedef
void        (PEER_API * LPSTOPDOWNLOAD)(char const * lpszUrl, boost::uint32_t nUrlLength);

/**
 * 下载完成清零
 */
void        PEER_API ResetCompleteCount();
typedef
void        (PEER_API * LPRESETCOMPLETECOUNT)();

/**
 * 停止并从缓存中删除文件
 */
void        PEER_API RemoveDownloadFile(char const * lpszUrl, boost::uint32_t nUrlLength);
typedef
void        (PEER_API * LPREMOVEDOWNLOADFILE)(char const * lpszUrl, boost::uint32_t nUrlLength);

/**
 * 停止并从共享内存中删除文件信息
 */
void        PEER_API RemoveDownloadFileEx(char const * lpszUrl, boost::uint32_t nUrlLength);
typedef
void        (PEER_API * LPREMOVEDOWNLOADFILEEX)(char const * lpszUrl, boost::uint32_t nUrlLength);


/**
 * 下载所有
 */
void        PEER_API StartDownloadAll(void * lpBuffer, boost::uint32_t nLength);
typedef
void        (PEER_API * LPSTARTDOWNLOADALL)(void * lpBuffer, boost::uint32_t nLength);

/**
 * <0 不限速
 * = 0 不下载
 * >0 限速
 */
void        PEER_API LimitDownloadSpeedInKBpsByUrl(char const * lpszUrl, boost::uint32_t nUrlLength, boost::int32_t speed_in_KBps);
typedef
void        (PEER_API * LPLIMITDOWNLOADSPEEDINKBPSBYURL)(char const * lpszUrl, boost::uint32_t nUrlLength, boost::int32_t speed_in_KBps);

/**
 * = 0 使用默认值
 * >0
 */
void        PEER_API SetMaxUploadCacheSizeInMB(boost::uint32_t nMaxUploadCacheSizeInMB);
typedef
void        (PEER_API * LPSETMAXUPLOADCACHESIZEINMB)(boost::uint32_t nMaxUploadCacheSizeInMB);

/**
 * lpwszRID：对应资源的RID，UTF16编码
 * pTotalSize：对应资源的总字节数，如果为空，可以不用返回。
 * 返回值：当前下载总字节数
 */
#ifdef PEER_PC_CLIENT
boost::int32_t         PEER_API QueryDownloadProgress(wchar_t const * lpszRID, boost::uint32_t nRIDLength, boost::int32_t *pTotalSize);
typedef
boost::int32_t        (PEER_API * LPQUERYDOWNLOADPROGRESS)(wchar_t const * lpszRID, boost::uint32_t nRIDLength, boost::int32_t *pTotalSize);
#else
// PPBox兼容QueryDownloadProgress接口
boost::int32_t PEER_API QueryDownloadProgress(char const * lpszRID, boost::uint32_t nRIDLength, boost::int32_t *pTotalSize);
typedef
boost::int32_t (PEER_API * LPQUERYDOWNLOADPROGRESS)(char const * lpszRID, boost::uint32_t nRIDLength, boost::int32_t *pTotalSize);
#endif

/**
 * lpwszRID：对应资源的RID，UTF16编码
 * 返回值：当前下载速度（以字节为单位）
 */
#ifdef PEER_PC_CLIENT
boost::int32_t         PEER_API QueryDownloadSpeed(wchar_t const * lpszRID, boost::uint32_t nRIDLength);
typedef
boost::int32_t         (PEER_API * LPQUERYDOWNLOADSPEED)(wchar_t const * lpszRID, boost::uint32_t nRIDLength);
#else
// PPBox兼容QueryDownloadSpeed接口
boost::int32_t PEER_API QueryDownloadSpeed(char const * lpszRID, boost::uint32_t nRIDLength);
typedef
boost::int32_t (PEER_API * LPQUERYDOWNLOADSPEED)(char const * lpszRID, boost::uint32_t nRIDLength);
#endif

/**
 * lpwszURL：对应资源的URL，UTF16编码
 * 返回值：当前下载进度（以字节为单位）
 */
#ifdef PEER_PC_CLIENT
boost::int32_t         PEER_API QueryDownloadProgressByUrl(wchar_t const * lpszUrl, boost::uint32_t nUrlLength, boost::int32_t *pTotalSize);
typedef
boost::int32_t         (PEER_API * LPQUERYDOWNLOADPROGRESSBYURL)(wchar_t const * lpszUrl, boost::uint32_t nUrlLength, boost::int32_t *pTotalSize);
#else
// PPBox兼容QueryDownloadProgressByUrl接口
boost::int32_t PEER_API QueryDownloadProgressByUrl(char const * lpszUrl, boost::uint32_t nUrlLength, boost::int32_t *pTotalSize);
typedef
boost::int32_t (PEER_API * LPQUERYDOWNLOADPROGRESSBYURL)(char const * lpszUrl, boost::uint32_t nUrlLength, boost::int32_t *pTotalSize);
#endif

#ifdef PEER_PC_CLIENT
boost::int32_t         PEER_API QueryDownloadSpeedByUrl(wchar_t const * lpszUrl, boost::uint32_t nUrlLength);
typedef
boost::int32_t         (PEER_API * LPQUERYDOWNLOADSPEEDBYURL)(wchar_t const * lpszUrl, boost::uint32_t nUrlLength);
#else
// PPBox兼容QueryDownloadSpeedByUrl接口
boost::int32_t PEER_API QueryDownloadSpeedByUrl(char const * lpszUrl, boost::uint32_t nUrlLength);
typedef
boost::int32_t (PEER_API * LPQUERYDOWNLOADSPEEDBYURL)(char const * lpszUrl, boost::uint32_t nUrlLength);
#endif

void         PEER_API SetWebUrl(const char * url, boost::uint32_t url_len, const char * web_url, boost::uint32_t weburl_len);
typedef
void         (PEER_API * LPSETWEBURL)(const char * url, boost::uint32_t url_len, const char * web_url, boost::uint32_t weburl_len);

#ifdef PEER_PC_CLIENT
PEERSTATEMACHINE PEER_API QueryPeerStateMachine(const wchar_t * lpwszRID, boost::uint32_t nRIDLength);
typedef
PEERSTATEMACHINE (PEER_API * LPQUERYPEERSTATEMACHINE)(const wchar_t * lpwszRID, boost::uint32_t nRIDLength);
#else
// PPBox兼容QueryPeerStateMachine接口
PEERSTATEMACHINE PEER_API QueryPeerStateMachine(const char * lpszRID, boost::uint32_t nRIDLength);
typedef
PEERSTATEMACHINE (PEER_API * LPQUERYPEERSTATEMACHINE)(const char * lpszRID, boost::uint32_t nRIDLength);
#endif

#ifdef PEER_PC_CLIENT
void PEER_API QueryDragPeerState(const wchar_t * lpwszRID, boost::uint32_t nRIDLength, boost::int32_t * state);
typedef
void (PEER_API * LPQUERYDRAGPEERSTATE)(const wchar_t * lpwszRID, boost::uint32_t nRIDLength, boost::int32_t * state);
#else
// PPBox兼容QueryDragPeerState接口
void PEER_API QueryDragPeerState(const char * lpwszRID, boost::uint32_t nRIDLength, boost::int32_t * state);
typedef
void (PEER_API * LPQUERYDRAGPEERSTATE)(const char * lpwszRID, boost::uint32_t nRIDLength, boost::int32_t * state);
#endif

void         PEER_API NotifyTaskStatusChange(boost::uint32_t task_id, boost::uint32_t task_status);
typedef
void         (PEER_API * LPNOTIFYTASKSTATUSCHANGE)(boost::uint32_t task_id, boost::uint32_t task_status);

void         PEER_API NotifyJoinLeave(boost::uint32_t join_or_leave);
typedef
void         (PEER_API * LPNOTIFYJOINLEAVE)(boost::uint32_t join_or_leave);

#ifdef PEER_PC_CLIENT
void         PEER_API SetRestPlayTime(const wchar_t * lpwszRID, boost::uint32_t nRIDLength, boost::uint32_t rest_play_time);
typedef
void         (PEER_API * LPSETRESTPLAYTIME)(const wchar_t * lpwszRID, boost::uint32_t nRIDLength, boost::uint32_t rest_play_time);
#else
// PPBox兼容SetRestPlayTime接口
void PEER_API SetRestPlayTime(const char * lpszRID, boost::uint32_t nRIDLength, boost::uint32_t rest_play_time);
typedef
void (PEER_API * LPSETRESTPLAYTIME)(const char * lpszRID, boost::uint32_t nRIDLength, boost::uint32_t rest_play_time);
#endif

/**
 *  告诉内核当前状态
 *  nPeerState: 当前状态
 *  0x0001 0000: 主力形态
 *  0x0002 0000: 驻留形态
 *  0x0000 0001: 无工作
 *  0x0000 0002: 直播内核工作
 */
void        PEER_API SetPeerState(boost::uint32_t nPeerState);
typedef
void        (PEER_API *LPSETPEERSTATE)(boost::uint32_t nPeerState);


/**
* 获取peer基本信息
*/
#ifdef PEER_PC_CLIENT
boost::int32_t PEER_API GetBasicPeerInfo(
                            boost::int32_t *tcp_port,
                            boost::int32_t *udp_port,
                            wchar_t * bs_ip,
                            boost::int32_t bs_ip_len,
                            boost::int32_t *tracker_count,
                            boost::int32_t *stun_count,
                            boost::int32_t *upload_speed
                           );

typedef
boost::int32_t (PEER_API *LPGETBASICPEERINFO)(
                                 boost::int32_t *tcp_port,
                                 boost::int32_t *udp_port,
                                 wchar_t * bs_ip,
                                 boost::int32_t bs_ip_len,
                                 boost::int32_t *tracker_count,
                                 boost::int32_t *stun_count,
                                 boost::int32_t *upload_speed
                                );
#else
// PPBox兼容GetBasicPeerInfo接口
boost::int32_t PEER_API GetBasicPeerInfo(
                                boost::int32_t *tcp_port,
                                boost::int32_t *udp_port,
                                char * bs_ip,
                                boost::int32_t bs_ip_len,
                                boost::int32_t *tracker_count,
                                boost::int32_t *stun_count,
                                boost::int32_t *upload_speed
                               );
typedef
boost::int32_t (PEER_API *LPGETBASICPEERINFO)(
    boost::int32_t *tcp_port,
    boost::int32_t *udp_port,
    char * bs_ip,
    boost::int32_t bs_ip_len,
    boost::int32_t *tracker_count,
    boost::int32_t *stun_count,
    boost::int32_t *upload_speed
   );
#endif
/**
* 通过绿色通道下载串查询p2p下载信息
* start = 1表示开始下载
* start = 0表示查询
*/
#ifdef PEER_PC_CLIENT
boost::int32_t PEER_API GetPeerInfo(boost::int32_t start, boost::int32_t *ilistCount, boost::int32_t *iConnectCount, boost::int32_t *iAverSpeed, const wchar_t * strURL);
typedef
boost::int32_t (PEER_API *LPGETPEERINFO)(boost::int32_t start, boost::int32_t *ilistCount, boost::int32_t *iConnectCount, boost::int32_t *iAverSpeed, const wchar_t * strURL);
#else
// PPBox兼容GetPeerInfo接口
boost::int32_t PEER_API GetPeerInfo(boost::int32_t start, boost::int32_t *ilistCount, boost::int32_t *iConnectCount, boost::int32_t *iAverSpeed, const char * strURL);
typedef
boost::int32_t (PEER_API *LPGETPEERINFO)(boost::int32_t start, boost::int32_t *ilistCount, boost::int32_t *iConnectCount, boost::int32_t *iAverSpeed, const char * strURL);
#endif

/*
* download_mode:
* 0 智能下载模式
* 1 高速下载模式
* 2 柔和下载模式
*/
#ifdef PEER_PC_CLIENT
void         PEER_API SetDownloadMode(const wchar_t * lpwszRID, boost::uint32_t nRIDLength, boost::uint32_t download_mode);
typedef
void         (PEER_API * LPSETDOWNLOADMODE)(const wchar_t * lpwszRID, boost::uint32_t nRIDLength, boost::uint32_t download_mode);
#else
// PPBox兼容接口
void PEER_API SetDownloadMode(const char * lpwszRID, boost::uint32_t nRIDLength, boost::uint32_t download_mode);
typedef
void (PEER_API * LPSETDOWNLOADMODE)(const char * lpwszRID, boost::uint32_t nRIDLength, boost::uint32_t download_mode);
#endif

/*
 * 开始对所有下载视频的连接限速
 * 用于客户端获取Play, DT, Drag
 */
void         PEER_API StartLimitDownloadConnection();
typedef
void         (PEER_API * LPSTARTLIMITDOWNLOADCONNECTION)();

/*
 * 开始对所有下载视频的连接限速
 * 用于客户端获取Play, DT, Drag之后，恢复原来的下载策略
 */
void         PEER_API StopLimitDownloadConnection();
typedef
void         (PEER_API * LPSTOPLIMITDOWNLOADCONNECTION)();

// 获得Peer统计的用户带宽值
int        PEER_API GetPeerBandWidthInKB();
typedef
int        (PEER_API * LPGETPEERBANDWIDTHINKB)();

// 获得已下载完整的文件的存储路径，如果所查询的文件不存在或未下载完成，则返回失败
// 参数说明：
//      lpwszRID:待查询文件的RID字符串指针
//      nRIDLength:待查询文件的RID字符串的长度
//      lpwszPath:输出存储路径的缓冲区地址
//      nPathLength:输出存储路径的缓冲区长度
// 返回值：
//      ture表示获取成功，lpwszPath指向的缓冲区为所查询的存储路径，是以字符0结尾的字符串
//      false表示获取失败
bool        PEER_API GetCompeletedFilePath(
    const wchar_t* lpwszRID,
    boost::uint32_t nRIDLength,
    wchar_t* lpwszPath,
    boost::uint32_t nPathLength
   );
typedef
bool        (PEER_API * LPGetCompeletedFilePath)(
    const wchar_t* lpwszRID,
    boost::uint32_t nRIDLength,
    wchar_t* lpwszPath,
    boost::uint32_t nPathLength
   );


// 设置内核推送数据的速度，适用于通过9000端口连接的请求，
// 调用者通过url指定一个连接来设置这个连接上的数据推送速度，默认的推送速度为512KB/s
// 参数说明：
//      url:请求url的字符串指针
//      url_len:请求url字符串的长度
//      send_speed_limit:设置的数据推送速度，单位为KB/s
void PEER_API SetSendSpeedLimitByUrl(const char * url, boost::int32_t url_len, boost::int32_t send_speed_limit);
typedef
void (PEER_API * LPSETSENDSPEEDLIMITBYURL)(const char * url, boost::int32_t url_len, boost::int32_t send_speed_limit);

// 禁止/恢复内核上传
// 参数说明
//     is_disable = true  禁止上传
//     is_disable = false 恢复上传
void PEER_API DisableUpload(bool is_disable);
typedef
void (PEER_API * LPDISABLEUPLOAD)(bool is_disable);

// 查询某个rid下载数据校验失败的次数
boost::int32_t PEER_API QueryBlockHashFailed(const char * str_rid, boost::uint32_t rid_len);
typedef
boost::int32_t (PEER_API * LPQUERYBLOCKHASHFAILED)(const char * str_rid, boost::uint32_t rid_len);

// 查询内核正在下载的个数
boost::int32_t PEER_API QueryConnectionCount();
typedef
boost::int32_t (PEER_API * LPQUERYCONNECTIONCOUNT)();

// 查询内核状态机状态
PEERSTATEMACHINE PEER_API QueryPeerStateMachineByUrl(const char * url);
typedef
PEERSTATEMACHINE (PEER_API * LPQUERYPEERSTATEMACHINEBYURL)(const char * url);

// 设置剩余缓冲时间
void PEER_API SetRestPlayTimeByUrl(const char * url, boost::uint32_t rest_play_time);
typedef
void (PEER_API * LPSETRESTPLAYTIMEBYURL)(const char * url, boost::uint32_t rest_play_time);

//设置VIP_LEVEL
void PEER_API SetVipLevelByUrl(const char * url, boost::uint32_t url_len, boost::uint32_t vip_level);
typedef
void (PEER_API * LPSETVIPLEVELBYURL)(const char * url, boost::uint32_t url_len, boost::uint32_t vip_level);
void PEER_API QueryDragPeerStateByUrl(const char * url, boost::int32_t * state);
typedef
void (PEER_API * LPQUERYDRAGPEERSTATEBYURL)(const char * url, boost::int32_t * state);

void  PEER_API SetDownloadModeByUrl(const char * url, boost::uint32_t download_mode);
typedef
void  (PEER_API * LPSETDOWNLOADMODEBYURL)(const char * url, boost::uint32_t download_mode);

bool PEER_API GetCompeletedFilePathByUrl(const char * url, wchar_t* lpwszPath, boost::uint32_t nPathLength);
typedef bool (PEER_API * LPGetCompeletedFilePathByUrl)(const char * url, wchar_t* lpwszPath, boost::uint32_t nPathLength);

boost::int32_t PEER_API QueryBlockHashFailedByUrl(const char * url);
typedef
boost::int32_t (PEER_API * LPQUERYBLOCKHASHFAILEDBYURL)(const char * url);

void PEER_API SetUpnpPortForTcpUpload(boost::uint16_t upnp_port);
typedef
void (PEER_API * LPSETUPNPPROTFORTCPUPLOAD)(boost::uint16_t upnp_port);

void PEER_API QueryDownloadProgressByUrlNew(char const * lpszUrl, boost::uint32_t nUrlLength, boost::int32_t *file_length,
                                            boost::int32_t *downloaded_bytes, boost::int32_t * position);
typedef
void (PEER_API * LPQUERYDOWNLOADPROGRESSBYURLNEW)(char const * lpszUrl, boost::uint32_t nUrlLength, boost::int32_t *total_size,
                                                  boost::int32_t *downloaded_bytes, boost::int32_t * position);

void PEER_API QueryProgressBitmap(const char * url, boost::uint32_t url_len,
                                  char * bitmap, boost::uint32_t * bitmap_size);
typedef
void (PEER_API * LPQUERYPROGRESSBITMAP)(const char * url, boost::uint32_t url_len,
                                        char * bitmap, boost::uint32_t * bitmap_size);

boost::uint32_t PEER_API GetDumpBuffer(char * buffer, boost::uint32_t buffer_length);
typedef
boost::uint32_t (PEER_API * LPGETDUMPBUFFER)(char * buffer, boost::uint32_t buffer_length);
/**
* 函数接口
*/
typedef struct _NETINTERFACE{
    boost::uint32_t                dwVersion;
    // version 0, 1
    LPSTARTUP                Startup;
    LPCLEARUP                Cleanup;
    boost::uint32_t          Reserved1;
    LPFEERBUFFER            CoreFreeBuffer;
    LPWILLCLEANUP            WillCleanup;
    LPSETMAXUPLOADSPEEDINKBPS   SetMaxUploadSpeedInKBps;
    LPOPENUPNPSUCCED        OpenUPNPSucced;
    LPSETURLFILENAME        SetUrlFileName;
    LPUPOADACTION           UploadAction;
    // version 0, 2
    LPSTOPDOWNLOAD          StopDownload;
    LPSTARTDOWNLOAD         StartDownload;
    // version 0, 3
    LPRESETCOMPLETECOUNT    ResetCompleteCount;
    LPREMOVEDOWNLOADFILE    RemoveDownloadFile;
    // version 0, 4
    LPSTARTDOWNLOADALL      StartDownloadAll;
    // version 0, 5
    LPSTARTDOWNLOADEX       StartDownloadEx;
    LPREMOVEDOWNLOADFILEEX  RemoveDownloadFileEx;
    // version 0, 6
    LPLIMITDOWNLOADSPEEDINKBPSBYURL LimitDownloadSpeedInKBpsByUrl;
    // version 0, 7
    boost::uint32_t          Reserved3;
    // version 0, 8
    LPQUERYDOWNLOADPROGRESS QueryDownloadProgress;
    LPQUERYDOWNLOADSPEED    QueryDownloadSpeed;
    // version 0, 9
    LPSETPEERSTATE          SetPeerState;
    // version 0, 10
    LPQUERYDOWNLOADPROGRESSBYURL QueryDownloadProgressByUrl;
    LPQUERYDOWNLOADSPEEDBYURL    QueryDownloadSpeedByUrl;
    // version 0, 11
    LPSETWEBURL SetWebUrl;
    // version 0, 12
    LPQUERYPEERSTATEMACHINE QueryPeerStateMachine;
    // version 0, 13
    LPNOTIFYTASKSTATUSCHANGE NotifyTaskStatusChange;
    LPNOTIFYJOINLEAVE NotifyJoinLeave;
    // version 0, 14
    LPSETRESTPLAYTIME SetRestPlayTime;
    // version 0, 15
    LPQUERYDRAGPEERSTATE QueryDragPeerState;
    // version 0, 16
    LPGETBASICPEERINFO GetBasicPeerInfo;
    LPGETPEERINFO GetPeerInfo;
    // version 0, 17
    LPSETDOWNLOADMODE SetDownloadMode;
    // version 0, 18
    LPSTARTLIMITDOWNLOADCONNECTION StartLimitDownloadConnection;
    LPSTOPLIMITDOWNLOADCONNECTION StopLimitDownloadConnection;
    // version 0, 19
    LPGETPEERBANDWIDTHINKB GetPeerBandWidthInKB;
    LPGetCompeletedFilePath GetCompeletedFilePath;
    LPSETSENDSPEEDLIMITBYURL SetSendSpeedLimitByUrl;
    // version 0, 20
    LPDISABLEUPLOAD DisableUpload;
    LPQUERYBLOCKHASHFAILED QueryBlockHashFailed;
    // version 0, 21
    LPQUERYCONNECTIONCOUNT QueryConnectionCount;
    // version 0, 22
    LPQUERYPEERSTATEMACHINEBYURL QueryPeerStateMachineByUrl;
    LPSETRESTPLAYTIMEBYURL SetRestPlayTimeByUrl;
    LPQUERYDRAGPEERSTATEBYURL QueryDragPeerStateByUrl;
    LPSETDOWNLOADMODEBYURL SetDownloadModeByUrl;
    LPGetCompeletedFilePathByUrl GetCompeletedFilePathByUrl;
    LPQUERYBLOCKHASHFAILEDBYURL QueryBlockHashFailedByUrl;
    // version 0, 23
    LPSETUPNPPROTFORTCPUPLOAD SetUpnpPortForTcpUpload;
    LPQUERYDOWNLOADPROGRESSBYURLNEW QueryDownloadProgressByUrlNew;
    // version 0, 24
    LPQUERYPROGRESSBITMAP QueryProgressBitmap;
    boost::uint32_t                Reserved4[37];
} NETINTERFACE, *LPNETINTERFACE;
#ifdef PEER_PC_CLIENT
#pragma pack(pop)
#endif

#ifdef __cplusplus
extern "C" {
#endif

void PEER_DECL PEER_API TS_XXXX(LPNETINTERFACE lpNetInterface);

#ifdef __cplusplus
}
#endif
#endif  // PEER_H
