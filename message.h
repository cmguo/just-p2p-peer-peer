//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#pragma once

// 当内核启动启动时 发出 启动成功消息
#define UM_STARTUP_SUCCED            WM_USER + 1
// 参数 wParam:  HttpProxy 的端口
// 参数 lParam:  Udp的监听端口  (为了做uPNP新加的)

// 当内核启动失败时 发出 启动失败消息
#define UM_STARTUP_FAILED            WM_USER + 2
// 参数无

// 当内核完美停止时 发出   （强制关闭不发送）
#define UM_CLEARUP_SUCCED            WM_USER + 3
// 参数无

// 当DownloadDriver启动时 发出
#define UM_DONWLOADDRIVER_START        WM_USER + 4
// 参数 wParam: 启动的DownloadDriver ID
// 参数lParam: 结构 DOWNLOADDRIVER_START_DATA 结构体的指针，用来传递启动下载时要提交的数据，结构体具体的定义见下面

// 当DownloadDriver停止时 发出
#define UM_DOWNLOADDRIVER_STOP        WM_USER + 5
// 参数 wParam: 启动的DownloadDriver ID
// 参数 lParam: 结构 DOWNLOADDRIVER_STOP_DATA 结构体的指针，用来传递停止下载时要提交的数据，结构体具体的定义见下面

// 当内核退出的时候，并没有退完，(内核退出有一定的耗时)
#define UM_CORE_STOP                WM_USER + 6
// 参数wParam: 无
// 参数lParam: 结构 CORE_STOP_DATA 结构体的指针，用来传递内核停止时提交的数据

// 当内核得到资源ID的时候，向VA发送消息
#define UM_GOT_RESOURCEID           WM_USER + 7
// 参数wParam: 无
// 参数lParam: 结构 RESOURCEID_DATA 结构体指针，用来传递内核得到的RID数据

// 当内核得到KeywordList之后，向VA发送消息
#define UM_GOT_KEYWORDLIST          WM_USER + 8
// 参数wParam: 无
// 参数lParam: 结构 KEYWORD_DATA 结构体指针，用来传递内核得到的KeywordList

// DAC 统计消息，下载结束的时候
#define UM_DAC_STATISTIC            WM_USER + 20
// 参数wParam: 启动的DownloadDriver ID
// 参数lParam: 结构 DOWNLOADDRIVER_STOP_DAC_DATA 结构指针, 用来传递下载停止时给DAC提交的数据

#define UM_GOT_RESOURCE_DATARATE    WM_USER + 21
// 参数wParam: 启动的DownloadDriver ID
// 参数lParam: 结构 RESOURCE_DATA_RATE_INFO 的指针，用来传递资源码流率信息

#define UM_PERIOD_DAC_STATISTIC     WM_USER + 22
// 参数wParam: 无
// 参数lParam: 结构 PERIOD_DAC_STATISTIC_INFO 的指针，用来传递周期统计数据

#define UM_NOTIFY_PPTV_TASK         WM_USER + 23
// 参数wParam:
// 参数lParam: 结构 NOTIFY_TASK

// herain:2010-1-4:采用了新的DAC日志提交方式，因此重新定义两个DAC日志提交的消息类型
// DAC 统计消息，下载结束的时候
#define UM_DAC_STATISTIC_V1            WM_USER + 24
// 参数wParam: 启动的DownloadDriver ID
// 参数lParam: 结构 DOWNLOADDRIVER_STOP_DAC_DATA_V1 结构指针, 用来传递下载停止时给DAC提交的数据

#define UM_PERIOD_DAC_STATISTIC_V1     WM_USER + 25
// 参数wParam: 无
// 参数lParam: 结构 PERIOD_DAC_STATISTIC_INFO_V1 的指针，用来传递周期统计数据

#define UM_LIVE_DAC_STATISTIC          WM_USER + 26
// 参数wParam: 启动的LiveDownloadDriver ID
// 参数lParam: 结构 DOWNLOADDRIVER_STOP_DAC_DATA_V1 结构指针, 用来传递下载停止时给DAC提交的数据

#define UM_PERIOD_DAC_LIVE_STATISTIC     WM_USER + 27
// 参数wParam: 无
// 参数lParam: 结构 PERIOD_DAC_STATISTIC_INFO_V1 的指针，用来传递周期统计数据

#define UM_INTERNAL_TCP_PORT_SUCCED     WM_USER + 28
// 参数wParam: 无
// 参数lParam: Tcp上传的内部监听端口

#define UM_LIVE_RESTART WM_USER + 29
// 参数wParam: 无
// 参数lParam: 无


typedef struct _NOTIFY_TASK
{
    boost::uint32_t task_id;
    boost::uint32_t task_type;
    boost::uint32_t content_len;
    char   content[1024];
} NOTIFY_TASK, *LPNOTIFY_TASK;

#define UM_TESTSUCCESSMESSAGE    WM_USER + 1006

#ifdef BOOST_WINDOWS_API
#pragma pack(push, 1)
#endif

typedef struct _CORE_STOP_DATA
{
    boost::uint32_t        uSize;                               // 整个结构的大小
    float                  fStoreUtilizationRatio;              // 存储空间利用率
    boost::uint32_t        uCurrLocalResourceCount;             // 当前资源个数
    boost::uint32_t        uCurrUploadResourceCount;            // 上传资源个数
    boost::int32_t         bIndexServerConnectSuccess;          // 是否连上IndexServer
    boost::uint32_t        uPublicIP;                           // 公网IP
    boost::int32_t         bNATPeer;                            // 是否内网节点
    boost::uint16_t        usNATType;                           // NAT节点类型
    boost::uint16_t        usNATKeeplivePeriods;                // NAT节点饱和时间
    boost::uint32_t        ulTotalDownloadBytes;                // 总下载字节数
    boost::uint32_t        ulTotalUploadBytes;                  // 总上传字节数
    boost::uint32_t        ulTotalP2pDownloadBytes;             // 总P2P下载字节数
    boost::uint32_t        ulTotalOtherServerDownloadBytes;     // 总非原生P2S下载字节数
} CORESTOPDATA, *LPCORESTOPDATA;

typedef struct _DOWNLOADDRIVER_START_DATA
{
    boost::uint32_t        uSize;                               // 整个结构的大小
    char         szOriginalUrl[1000];                           // 下载请求的Url
    char        szOriginalReferUrl[1000];                       // 下载请求的ReferUrl
} DOWNLOADDRIVERSTARTDATA, *LPDOWNLOADDRIVERSTARTDATA;

typedef struct _DOWNLOADDRIVER_STOP_DATA
{
    boost::uint32_t        uSize;                               // 整个结构的大小
    char                   szOriginalUrl[1000];                 // 下载请求的Url
    boost::int32_t         bHasRID;                             // 是否获得了 RID
    Guid                   guidRID;                             // RID, 如果没有RID则是全0
    boost::uint32_t        ulResourceSize;                      // 下载的文件的长度
    boost::uint32_t        ulDownloadBytes;                     // 下载的总字节数
    boost::uint32_t        ulP2pDownloadBytes;                  // P2P下载的字节数
    boost::uint32_t        ulOtherServerDownloadBytes;          // 非原生HttpServer下载的字节数
    boost::uint32_t        uPlayTime;                           // 理论播放时长
    boost::uint32_t        uDataRate;                           // 预测码流率
    char        szOriginalReferUrl[1000];                       // 下载请求的ReferUrl
} DOWNLOADDRIVERSTOPDATA, *LPDOWNLOADDRIVERSTOPDATA;

typedef struct _RESOURCEID_DATA
{
    boost::uint32_t       uSize;                                // 整个结构的大小
    char                  szOriginalUrl[1000];                  // 原始URL
    char                  szOriginalReferUrl[1000];             // 原始ReferURL
    Guid                  guidRID;                              // URL对应的ResourceID
    boost::uint32_t       uDuration;                            // 时长，单位s
    boost::uint32_t       uFileLength;                          // 文件大小，按字节为单位
    char                  szFileType[10];                       // 文件的类型，如果读不到就用”none”
    boost::uint8_t        bUploadPic;                           // 是否上传截图， 0/1
    boost::uint16_t       usVAParamLength;                      // 后面VAParam部分的长度
    char                  szVAParam[];                          // VAParam的内容
} RESOURCEID_DATA, *LPRESOURCEID_DATA;

typedef struct _KEYWORD_DATA
{
    boost::uint32_t       uSize;                                // 整个结构大小
    char                  szKeywordList[1024];                  // 关键字列表，用';'分割
} KEYWORD_DATA, *LPKEYWORD_DATA;


typedef struct _DOWNLOADDRIVER_STOP_DAC_DATA
{
    boost::uint32_t       uSize;                                // 整个结构体大小
    boost::uint16_t       uSourceType;                          // 0:pplive, 1:ikan, 2:ppvod, 3:other
    char                  szLog[4096];                          // 具体的日志内容
} DOWNLOADDRIVER_STOP_DAC_DATA, *LPDOWNLOADDRIVER_STOP_DAC_DATA;

typedef struct _RESOURCE_DATA_RATE_INFO
{
    boost::uint32_t       uSize;                                // 结构体大小
    char                  szOriginalUrl[1000];                  // 播放URL
    float                 fDataRate;                            // 码流率, 单位(Bytes/Second)
} RESOURCE_DATA_RATE_INFO, *LPRESOURCE_DATA_RATE_INFO;

typedef struct _PERIOD_DAC_STATISTIC_INFO
{
    boost::uint32_t       uSize;                                       // 整个结构体大小
    char szLog[1024];                                           // 具体的日志内容
} PERIOD_DAC_STATISTIC_INFO, *LPPERIOD_DAC_STATISTIC_INFO;

#ifdef BOOST_WINDOWS_API
#pragma pack(pop)
#endif