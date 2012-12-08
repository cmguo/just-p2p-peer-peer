//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// NET.cpp : 定义 DLL 应用程序的入口点。
//
#include "Common.h"

#define PEER_SOURCE

#include "peer.h"
#include "p2sp/AppModule.h"
#include "p2sp/proxy/ProxyModule.h"
#include "p2sp/proxy/MessageBufferManager.h"
#include "p2sp/p2p/P2PModule.h"
#ifdef NOTIFY_ON
#include "p2sp/notify/NotifyModule.h"
#endif
#include "p2sp/tracker/TrackerModule.h"
#include "storage/Storage.h"
#include "statistic/StatisticModule.h"
#include "statistic/StatisticStructs.h"
#include "message.h"
#include "p2sp/proxy/PlayInfo.h"
#include "base/wsconvert.h"
#include "struct/UdpBuffer.h"
#include "storage/Storage.h"
#include "storage/Instance.h"


#include <framework/memory/MemoryPoolObject.h>
#include <framework/timer/AsioTimerManager.h>

#include <boost/algorithm/string.hpp>
#include <boost/thread.hpp>
#include "MainThread.h"

using p2sp::ProxyModule;

#ifdef NEED_TO_POST_MESSAGE
#include "WindowsMessage.h"
#endif

#ifdef LOG_ENABLE
static log4cplus::Logger logger_peer = log4cplus::Logger::getInstance("[peer]");
#endif

#ifdef BOOST_WINDOWS_API

#pragma managed(push, off)

BOOL APIENTRY DllMain(HMODULE hModule,
    uint32_t ul_reason_for_call,
    void * lpReserved
)
{
    switch (ul_reason_for_call)
    {
        case DLL_PROCESS_ATTACH:
        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
        case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

#pragma managed(pop)

#endif  // BOOST_WINDOWS_API

static boost::mutex peer_mu_;

//////////////////////////////////////////////////////////////////////////
//
//////////////////////////////////////////////////////////////////////////

// helper
struct Event
{
    typedef boost::shared_ptr<Event> p;
public:
    static p Create()
    {
        return p(new Event());
    }
    virtual ~Event()
    {
        Notify();
    }
    void Wait()
    {
        boost::unique_lock<boost::mutex> lock(mutex_);

        while (!data_ready_)
        {
            cond_.wait(lock);
        }
    }
    void Notify()
    {
        {
            boost::lock_guard<boost::mutex> lock(mutex_);
            data_ready_ = true;
        }
        cond_.notify_one();
    }
private:
    Event()
    {
        data_ready_ = false;
    }

    boost::condition_variable cond_;
    boost::mutex mutex_;
    bool data_ready_;
};

struct SimpleResult
{
    Event::p event_;

    SimpleResult(Event::p e) : event_(e) {}
    void result_handler()
    {
        event_->Notify();
    }
};

bool IsProxyModuleStarted()
{
    return ProxyModule::Inst() &&
        ProxyModule::Inst()->IsRunning();
}

#ifdef PEER_PC_CLIENT
void PEER_API Startup(LPWSTARTPARAM lpParam)
#else
boost::uint32_t PEER_API Startup(LPSTARTPARAM lpParam)
#endif
{
    boost::unique_lock<boost::mutex> ul(peer_mu_);
 
#ifdef LOG_ENABLE
    log4cplus::Logger root=log4cplus::Logger::getRoot();
#ifdef PEER_PC_CLIENT
    PropertyConfigurator::doConfigure("C:\\Program Files\\Common Files\\PPLiveNetwork\\peer.config");
#else
    boost::filesystem::path temp_path = framework::filesystem::temp_path();
    string peer_config_path = (temp_path / "peer.config").file_string();

    PropertyConfigurator::doConfigure(peer_config_path);
#endif
#endif
    LOG4CPLUS_DEBUG_LOG(logger_peer, "StartKernel");

#ifdef NEED_TO_POST_MESSAGE
    WindowsMessage::Inst().SetHWND((HWND)lpParam->hWnd);
#endif

    p2sp::AppModuleStartInterface::p appmodule_start_interface = p2sp::AppModuleStartInterface::create(
        lpParam->usUdpPort,
        lpParam->usHttpProxyPort,
        string(lpParam->aIndexServer[0].szIndexDomain),
        lpParam->aIndexServer[0].usIndexPort,
        lpParam->bUseDisk != 0,
        lpParam->ullDiskLimit,
#ifdef PEER_PC_CLIENT
        string (base::ws2s(lpParam->wszDiskPath)),
#else
        string(lpParam->szDiskPath),
#endif
        string(lpParam->szPeerGuid, 32),
#ifdef PEER_PC_CLIENT
        string(base::ws2s(lpParam->wszConfigPath)),
#else

        string(lpParam->szConfigPath),
#endif
        lpParam->bUsePush != 0,
        lpParam->bReadOnly != 0,
        lpParam->bHttpProxyEnabled != 0,
#ifndef PEER_PC_CLIENT
        lpParam->submit_stop_log,
#endif
        lpParam->memory_pool_size_in_MB
        );

    Event::p event_wait = Event::Create();
    boost::shared_ptr<SimpleResult> result(new SimpleResult(event_wait));
    boost::function<void()> fun = boost::bind(&SimpleResult::result_handler, result);
    boost::uint16_t local_http_port;

    global_io_svc().post(boost::bind(&p2sp::AppModule::Start, p2sp::AppModule::Inst(), 
        boost::ref(global_io_svc()), appmodule_start_interface, fun, & local_http_port));

    MainThread::Start();

    event_wait->Wait();
#ifndef PEER_PC_CLIENT
    return local_http_port;
#endif
}

void PEER_API Clearup()
{
    boost::unique_lock<boost::mutex> ul(peer_mu_);
    global_io_svc().post(boost::bind(&p2sp::AppModule::Stop, p2sp::AppModule::Inst()));

    MainThread::Stop();

#ifdef NEED_TO_POST_MESSAGE
    WindowsMessage::Inst().PostWindowsMessage(UM_CLEARUP_SUCCED, NULL, NULL);
#endif
}

void PEER_API WillCleanup()
{
    Clearup();
}

void PEER_API FreeBuffer(char* lpBuffer)
{
    if (!IsProxyModuleStarted())
    {
        return;
    }

    global_io_svc().post(boost::bind(&p2sp::MessageBufferManager::DeleteBuffer, p2sp::MessageBufferManager::Inst(),
        (boost::uint8_t*) lpBuffer));
}

void PEER_API SetMaxUploadSpeedInKBps(boost::int32_t MaxUploadP2PSpeed)
{
    if (!IsProxyModuleStarted())
    {
        return;
    }

    global_io_svc().post(boost::bind(&p2sp::P2PModule::SetMaxUploadSpeedInKBps, p2sp::P2PModule::Inst(),
        MaxUploadP2PSpeed));
}

void PEER_API OpenUPNPSucced(uint32_t ip, boost::uint16_t udp_port, boost::uint16_t tcp_port)
{
    // Modified by jeffrey 2011/6/8
    // 在原来的TrackerManger::OpenUPNPSucced, 直接return
    // 所以在这里直接return,省去一个post, 且Tracker里面的OpenUPNPSucced代码可以删除
    return;
}

void PEER_API SetUrlFileName(char const * lpszUrl, boost::uint32_t nUrlLength, wchar_t const * lptszFileName, boost::uint32_t nFileNameLength)
{
#ifdef PEER_PC_CLIENT
    if (!IsProxyModuleStarted())
    {
        return;
    }

    if (lpszUrl == NULL || lptszFileName == NULL || nUrlLength == 0 || nFileNameLength == 0)
        return;

    string url(lpszUrl, nUrlLength);
    string file_name(base::ws2s(std::wstring(lptszFileName, nFileNameLength)));
    LOG4CPLUS_INFO_LOG(logger_peer, "SetUrlFileName url:" << url << " name:" << file_name);

    if (!storage::Storage::Inst())
        return;

    boost::algorithm::replace_all(file_name, ("."), ("_"));

    global_io_svc().post(boost::bind(&storage::IStorage::AttachFilenameByUrl, storage::Storage::Inst(), url,
        file_name));
#endif
}

// 向Peer上传特定操作的次数
void PEER_API UploadAction(boost::uint32_t uAction, boost::uint32_t uTimes)
{
    return;
}

void PEER_API StartDownload(char const * lpszUrl, boost::uint32_t nUrlLength, char const * lpszReferUrl, boost::uint32_t nReferUrlLength,
    char const * lpszUserAgent, boost::uint32_t nUserAgentLength, wchar_t const * lpszFileName, boost::uint32_t nFileNameLength)
{
#ifdef PEER_PC_CLIENT
    if (NULL == lpszUrl || 0 == nUrlLength)
    {
        return;
    }

    if (!IsProxyModuleStarted())
    {
        return;
    }

    if (NULL == lpszReferUrl || 0 == nReferUrlLength)
    {
        lpszReferUrl = "";
        nReferUrlLength = 0;
    }
    if (NULL == lpszUserAgent || 0 == nUserAgentLength)
    {
        lpszUserAgent = "";
        nUserAgentLength = 0;
    }
    if (NULL == lpszFileName || 0 == nFileNameLength)
    {
        lpszFileName = L"";
        nFileNameLength = 0;
    }

    string url(lpszUrl, nUrlLength);
    string refer_url(lpszReferUrl, nReferUrlLength);
    string user_agent(lpszUserAgent, nUserAgentLength);
    string qualified_file_name(base::ws2s(std::wstring(lpszFileName, nFileNameLength)));

    LOG4CPLUS_DEBUG_LOG(logger_peer, "\n\tUrl = " << url << "\n\tReferer = " << refer_url);

    // check name
    string ext = (".tpp");
    if (boost::algorithm::iends_with(qualified_file_name, ext))
    {
        qualified_file_name = qualified_file_name.substr(0, qualified_file_name.length() - ext.length());
    }

    global_io_svc().post(boost::bind(&p2sp::ProxyModule::StartDownloadFile, p2sp::ProxyModule::Inst(), url,
        refer_url, user_agent, qualified_file_name));
    LOG4CPLUS_DEBUG_LOG(logger_peer, "global_io_svc().post");
#endif
}

void PEER_API StopDownload(char const * lpszUrl, uint32_t nUrlLength)
{
    if (NULL == lpszUrl || 0 == nUrlLength)
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, "NULL == lpszUrl || 0 == nUrlLength");
        return;
    }

    if (!IsProxyModuleStarted())
    {
        return;
    }

    string url(lpszUrl, nUrlLength);
    LOG4CPLUS_DEBUG_LOG(logger_peer, "Url = " << url);

    global_io_svc().post(boost::bind(&p2sp::ProxyModule::StopProxyConnection, p2sp::ProxyModule::Inst(), url));
    LOG4CPLUS_DEBUG_LOG(logger_peer, "global_io_svc().post");
}

void PEER_API ResetCompleteCount()
{
    return;
}

void PEER_API RemoveDownloadFile(char const * lpszUrl, uint32_t nUrlLength)
{
    return;
}

void PEER_API StartDownloadAll(void * lpBuffer, uint32_t nLength)
{
    return;
}

/**
 * 开始下载
 */
void PEER_API StartDownloadEx(char const * lpszUrl, boost::uint32_t nUrlLength, char const * lpszWebUrl, boost::uint32_t nWebUrlLength,
    char const * lpszRequestHeader, boost::uint32_t nRequestHeaderLength, wchar_t const * lpszFileName, boost::uint32_t nFileNameLength)
{
    return;
}

/**
* 停止并从共享内存中删除文件信息
*/
void PEER_API RemoveDownloadFileEx(char const * lpszUrl, boost::uint32_t nUrlLength)
{
    return;
}

/**
* <0 不限速
* = 0 不下载
* >0 限速
*/
void PEER_API LimitDownloadSpeedInKBpsByUrl(char const * lpszUrl, boost::uint32_t nUrlLength, boost::int32_t speed_in_KBps)
{
    LOG4CPLUS_DEBUG_LOG(logger_peer, "Url = " << lpszUrl);
    if (NULL == lpszUrl || 0 == nUrlLength)
    {
        return;
    }

    if (!IsProxyModuleStarted())
    {
        return;
    }

    string url(lpszUrl, nUrlLength);

    // limit speed
    global_io_svc().post(boost::bind(&p2sp::ProxyModule::LimitDownloadSpeedInKBps, p2sp::ProxyModule::Inst(),
        url, speed_in_KBps));
    LOG4CPLUS_DEBUG_LOG(logger_peer, "global_io_svc().post");
}

boost::int32_t PEER_API QueryDownloadProgress(wchar_t const * lpszRID, boost::uint32_t nRIDLength, boost::int32_t * pTotalSize)
{
#ifdef PEER_PC_CLIENT
    if (NULL == lpszRID || 0 == nRIDLength)
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, " lpwszRID = NULL || nRIDLength == 0");
        return -1;
    }

    string rid_str(base::ws2s(std::wstring(lpszRID, nRIDLength)));

    RID rid;
    if (rid.from_string(rid_str))
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, " rid Parse Failed!");
        return -2;
    }

    if (!IsProxyModuleStarted())
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, "ProxyModule is not running!");
        return -3;
    }

    Event::p event_wait = Event::Create();
    LOG4CPLUS_DEBUG_LOG(logger_peer, "CreateEvent: " << event_wait);

    boost::shared_ptr<SimpleResult> result(new SimpleResult(event_wait));
    LOG4CPLUS_DEBUG_LOG(logger_peer,"ResultHolder: " << result);

    boost::function<void()> fun = boost::bind(&SimpleResult::result_handler, result);

    boost::int32_t download_progress;
    global_io_svc().post(boost::bind(&p2sp::ProxyModule::QueryDownloadProgress, p2sp::ProxyModule::Inst(), rid, fun, pTotalSize, &download_progress));

    event_wait->Wait();

    LOG4CPLUS_DEBUG_LOG(logger_peer, "event_wait->Wait() Succeed: download_progress = " << download_progress << 
        ", total_size = " << pTotalSize);
    return download_progress;
#endif
    return 0;
}

/**
* lpwszRID：对应资源的RID，UTF16编码
* 返回值：当前下载速度（以字节为单位）
*/
boost::int32_t PEER_API QueryDownloadSpeed(wchar_t const * lpszRID, boost::uint32_t nRIDLength)
{
#ifdef PEER_PC_CLIENT
    if (NULL == lpszRID || 0 == nRIDLength)
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, " lpwszRID = NULL || nRIDLength == 0");
        return -1;
    }

    string rid_str(base::ws2s(std::wstring(lpszRID, nRIDLength)));


    LOG4CPLUS_DEBUG_LOG(logger_peer, " rid_str = " << rid_str);
    RID rid;
    if (rid.from_string(rid_str))
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, " rid Parse Failed!");
        return -2;
    }

    if (!IsProxyModuleStarted())
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, "ProxyModule is not running!");
        return -3;
    }

    Event::p event_wait = Event::Create();
    LOG4CPLUS_DEBUG_LOG(logger_peer, "CreateEvent: " << event_wait);
    boost::shared_ptr<SimpleResult> result(new SimpleResult(event_wait));
    LOG4CPLUS_DEBUG_LOG(logger_peer, "ResultHolder: " << result);

    boost::function<void()> fun = boost::bind(&SimpleResult::result_handler, result);
    boost::int32_t download_speed;
    global_io_svc().post(
        boost::bind(&p2sp::ProxyModule::QueryDownloadSpeed, p2sp::ProxyModule::Inst(), rid, fun, &download_speed)
       );

    event_wait->Wait();
    LOG4CPLUS_DEBUG_LOG(logger_peer, "event_wait->Wait() Succeed: download_speed = " << download_speed);


    return download_speed;
#endif
    return 0;
}

boost::int32_t PEER_API QueryDownloadProgressByUrl(wchar_t const * lpszURL, boost::uint32_t nURLLength, boost::int32_t *pTotalSize)
{ 
#ifdef PEER_PC_CLIENT
    if (NULL == lpszURL || 0 == nURLLength)
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, " lpwszURL = NULL || nURLLength == 0");
        return -1;
    }

    if (!IsProxyModuleStarted())
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, "ProxyModule is not running!");
        return -3;
    }

    string url_str(base::ws2s(std::wstring(lpszURL, nURLLength)));

    Event::p event_wait = Event::Create();
    boost::shared_ptr<SimpleResult> result(new SimpleResult(event_wait));

    boost::function<void()> fun = boost::bind(&SimpleResult::result_handler, result);

    boost::int32_t download_progress = 0;
    boost::int32_t position = 0;

    global_io_svc().post(
        boost::bind(&p2sp::ProxyModule::QueryDownloadProgressByUrl, p2sp::ProxyModule::Inst(),
        url_str, pTotalSize, &download_progress, &position, fun));

    event_wait->Wait();

    return download_progress;
#endif
    return 0;
}

boost::int32_t PEER_API QueryDownloadSpeedByUrl(wchar_t const * lpszURL, boost::uint32_t nURLLength)
{
#ifdef PEER_PC_CLIENT
    if (NULL == lpszURL || 0 == nURLLength)
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, " lpwszURL = NULL || nURLLength == 0");
        return -1;
    }

    if (!IsProxyModuleStarted())
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, "ProxyModule is not running!");
        return -3;
    }

    string url_str(base::ws2s(std::wstring(lpszURL, nURLLength)));


    LOG4CPLUS_DEBUG_LOG(logger_peer, " url_str = " << url_str);

    Event::p event_wait = Event::Create();
    LOG4CPLUS_DEBUG_LOG(logger_peer, "CreateEvent: " << event_wait);
    boost::shared_ptr<SimpleResult> result(new SimpleResult(event_wait));
    LOG4CPLUS_DEBUG_LOG(logger_peer, "ResultHolder: " << result);

    boost::function<void()> fun = boost::bind(&SimpleResult::result_handler, result);
    boost::int32_t download_speed;
    global_io_svc().post(
        boost::bind(&p2sp::ProxyModule::QueryDownloadSpeedByUrl, p2sp::ProxyModule::Inst(), url_str, fun, &download_speed)
       );

    event_wait->Wait();
    LOG4CPLUS_DEBUG_LOG(logger_peer, "event_wait->Wait() Succeed: download_speed = " << download_speed);

    return download_speed;
#endif
    return 0;
}

void PEER_API SetWebUrl(const char * url, boost::uint32_t url_len, const char * web_url, boost::uint32_t weburl_len)
{
    return;
}

PEERSTATEMACHINE PEER_API QueryPeerStateMachine(wchar_t const * lpwszRID, boost::uint32_t nRIDLength)
{
    PEERSTATEMACHINE peer_state;
    peer_state.state_machine_ = -1;
    peer_state.http_speed_ = 0;
    peer_state.p2p_speed_ = 0;

#ifdef PEER_PC_CLIENT
    if (!IsProxyModuleStarted())
    {
        return peer_state;
    }

    if (NULL == lpwszRID || 0 == nRIDLength)
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, " lpwszRID = NULL || nRIDLength == 0");
        return peer_state;
    }

    string rid_str(base::ws2s(std::wstring(lpwszRID, nRIDLength)));

    LOG4CPLUS_DEBUG_LOG(logger_peer, " RID = " << rid_str);


    RID rid;
    if (rid.from_string(rid_str))
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, " rid Parse Failed!");
        return peer_state;
    }

    Event::p event_wait = Event::Create();
    LOG4CPLUS_DEBUG_LOG(logger_peer, "CreateEvent: " << event_wait);
    boost::shared_ptr<SimpleResult> result(new SimpleResult(event_wait));
    LOG4CPLUS_DEBUG_LOG(logger_peer, "ResultHolder: " << result);

    boost::function<void()> fun = boost::bind(&SimpleResult::result_handler, result);
    global_io_svc().post(
        boost::bind(&p2sp::ProxyModule::QueryPeerStateMachine, p2sp::ProxyModule::Inst(), rid, fun, &peer_state)
       );

    event_wait->Wait();
    LOG4CPLUS_DEBUG_LOG(logger_peer, "event_wait->Wait() Succeed: state = " << peer_state.state_machine_ << 
        " http_speed = " << peer_state.http_speed_ << " p2p_speed = " << peer_state.p2p_speed_);
    return peer_state;
#endif
    return peer_state;
}

void PEER_API NotifyTaskStatusChange(boost::uint32_t task_id, boost::uint32_t task_status)
{
    if (!IsProxyModuleStarted())
    {
        return;
    }
#ifdef NOTIFY_ON
    global_io_svc().post(
        boost::bind(
        &p2sp::NotifyModule::OnNotifyTaskStatusChange, p2sp::NotifyModule::Inst(),
        task_id, task_status));
#endif
}

void PEER_API NotifyJoinLeave(boost::uint32_t join_or_leave)
{
    if (!IsProxyModuleStarted())
    {
        return;
    }
#ifdef NOTIFY_ON
    global_io_svc().post(
        boost::bind(
        &p2sp::NotifyModule::OnNotifyJoinLeave, p2sp::NotifyModule::Inst(),
        join_or_leave));
#endif
}

#ifdef PEER_PC_CLIENT
void PEER_API SetRestPlayTime(wchar_t const * lpwszRID, boost::uint32_t nRIDLength, boost::uint32_t rest_play_time)
#else 
// PPBox兼容SetRestPlayTime接口
void PEER_API SetRestPlayTime(const char * lpszRID, boost::uint32_t nRIDLength, boost::uint32_t rest_play_time)
#endif
{
#ifdef PEER_PC_CLIENT
    string rid_str(base::ws2s(std::wstring(lpwszRID, nRIDLength)));
#else
    string rid_str(lpszRID, nRIDLength);
#endif

    LOG4CPLUS_DEBUG_LOG(logger_peer, " RID = " << rid_str);

    RID rid;
    if (rid.from_string(rid_str))
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, " rid Parse Failed!");
        return;
    }

    if (!IsProxyModuleStarted())
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, "Proxy Module is not started!");
        return;
    }

    global_io_svc().post(
        boost::bind(
        &p2sp::ProxyModule::SetRestPlayTime, p2sp::ProxyModule::Inst(),
        rid, rest_play_time));
}

void PEER_API SetDownloadMode(const wchar_t * lpwszRID, boost::uint32_t nRIDLength, boost::uint32_t download_mode)
{
#ifdef PEER_PC_CLIENT
    string rid_str(base::ws2s(std::wstring(lpwszRID, nRIDLength)));

    LOG4CPLUS_DEBUG_LOG(logger_peer, " RID = " << rid_str);

    RID rid;
    if (rid.from_string(rid_str))
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, " rid Parse Failed!");
        return;
    }

    if (!IsProxyModuleStarted())
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, "Proxy Module is not started!");
        return;
    }

    global_io_svc().post(
        boost::bind(
        &p2sp::ProxyModule::SetDownloadMode, p2sp::ProxyModule::Inst(),
        rid, download_mode));
#endif
}

/**
 *  告诉内核当前状态
 *  nPeerState: 当前状态
 */
void PEER_API SetPeerState(uint32_t nPeerState)
{
    if (!IsProxyModuleStarted())
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, "Proxy Module is not started!");
        return;
    }

    LOG4CPLUS_DEBUG_LOG(logger_peer, "nPeerState " << nPeerState);
    boost::uint32_t peer_state = 0;
    switch (nPeerState & 0xffff0000)
    {
    case PEERSTATE_MAIN_STATE:
        peer_state |= PEERSTATE_MAIN_STATE;
        LOG4CPLUS_DEBUG_LOG(logger_peer, "nPeerState | PEERSTATE_MAIN_STATE");
        break;
    case PEERSTATE_RESIDE_STATE:
        peer_state |= PEERSTATE_RESIDE_STATE;
        LOG4CPLUS_DEBUG_LOG(logger_peer, "nPeerState | PEERSTATE_RESIDE_STATE");
        break;
    default:
        peer_state |= PEERSTATE_MAIN_STATE;
        LOG4CPLUS_DEBUG_LOG(logger_peer, "nPeerState | PEERSTATE_MAIN_STATE (default)");
    }
    switch (nPeerState & 0x0000ffff)
    {
    case PEERSTATE_LIVE_NONE:
        peer_state |= PEERSTATE_LIVE_NONE;
        LOG4CPLUS_DEBUG_LOG(logger_peer, "nPeerState | PEERSTATE_LIVE_NONE");
        break;
    case PEERSTATE_LIVE_WORKING:
        peer_state |= PEERSTATE_LIVE_WORKING;
        LOG4CPLUS_DEBUG_LOG(logger_peer, "nPeerState | PEERSTATE_LIVE_WORKING");
        break;
    default:
        peer_state |= PEERSTATE_LIVE_NONE;
        LOG4CPLUS_DEBUG_LOG(logger_peer, "nPeerState | PEERSTATE_LIVE_NONE (default)");
    }

    global_io_svc().post(boost::bind(&p2sp::AppModule::SetPeerState, p2sp::AppModule::Inst(), peer_state));
    LOG4CPLUS_DEBUG_LOG(logger_peer, "global_io_svc().post");
}

// 拖动时查询内核状态
void PEER_API QueryDragPeerState(const wchar_t * lpwszRID, boost::uint32_t nRIDLength, boost::int32_t * state)
{
#ifdef PEER_PC_CLIENT
    if (!IsProxyModuleStarted())
    {
        return;
    }

    if (NULL == lpwszRID || 0 == nRIDLength)
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, " lpwszRID = NULL || nRIDLength == 0");
        *state = 0;
        return;
    }

    string rid_str(base::ws2s(std::wstring(lpwszRID, nRIDLength)));

    LOG4CPLUS_DEBUG_LOG(logger_peer, " RID = " << rid_str);

    RID rid;
    if (rid.from_string(rid_str))
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, " rid Parse Failed!");
        *state = 0;
        return;
    }

    Event::p event_wait = Event::Create();
    boost::shared_ptr<SimpleResult> result(new SimpleResult(event_wait));

    boost::function<void()> fun = boost::bind(&SimpleResult::result_handler, result);
    global_io_svc().post(boost::bind(&p2sp::ProxyModule::QueryDragState,
        p2sp::ProxyModule::Inst(), rid, state, fun));

    event_wait->Wait();

    LOG4CPLUS_DEBUG_LOG(logger_peer, "QUERYDRAGPEERSTATE Succeed: " << *state);
#endif
}

boost::int32_t PEER_API GetBasicPeerInfo(
                            boost::int32_t *tcp_port,
                            boost::int32_t *udp_port,
                            wchar_t * bs_ip,
                            boost::int32_t bs_ip_len,
                            boost::int32_t *tracker_count,
                            boost::int32_t *stun_count,
                            boost::int32_t *upload_speed
                           )
{
#ifdef PEER_PC_CLIENT
    if (!IsProxyModuleStarted())
    {
        return 0;
    }

    if (tcp_port == NULL || udp_port == NULL || bs_ip == NULL
        || tracker_count == NULL || stun_count == NULL || upload_speed == NULL)
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, " param NULL");
        return 1;
    }

    LOG4CPLUS_DEBUG_LOG(logger_peer, " GetBasicPeerInfo");

    Event::p event_wait = Event::Create();
    LOG4CPLUS_DEBUG_LOG(logger_peer, "CreateEvent: " << event_wait);
    boost::shared_ptr<SimpleResult> result(new SimpleResult(event_wait));
    LOG4CPLUS_DEBUG_LOG(logger_peer, "ResultHolder: " << result);

    // 向StatisticModule发请求
    boost::function<void()> fun = boost::bind(&SimpleResult::result_handler, result);
    statistic::BASICPEERINFO bpi;

    global_io_svc().post(
        boost::bind(&statistic::StatisticModule::QueryBasicPeerInfo, statistic::StatisticModule::Inst(), fun, &bpi)
       );

    event_wait->Wait();

    *tcp_port = bpi.tcp_port;
    *udp_port = bpi.udp_port;
    *tracker_count = bpi.tracker_count;
    *stun_count = bpi.stun_count;
    *upload_speed = bpi.upload_speed;

    string bs_ip_s;
    boost::asio::ip::address_v4 addr(bpi.bs_ip);
    boost::system::error_code ec;
    bs_ip_s = addr.to_string(ec);

    if (!ec && bs_ip_len >= bs_ip_s.size() * sizeof(char))
    {
        std::wstring str_bs_ip(base::s2ws(bs_ip_s));
        wcscpy(bs_ip, str_bs_ip.c_str());

        LOG4CPLUS_DEBUG_LOG(logger_peer, "-----------BasicPeerInfo-----------");
        LOG4CPLUS_DEBUG_LOG(logger_peer, "tcp_port: " << *tcp_port);
        LOG4CPLUS_DEBUG_LOG(logger_peer, "udp_port: " << *udp_port);
        LOG4CPLUS_DEBUG_LOG(logger_peer, "tracker_count: " << *tracker_count);
        LOG4CPLUS_DEBUG_LOG(logger_peer, "stun_count: " << *stun_count);
        LOG4CPLUS_DEBUG_LOG(logger_peer, "upload_speed: " << *upload_speed);
        LOG4CPLUS_DEBUG_LOG(logger_peer, "ips: " << bs_ip_s);

        return 0;
    }

    return 2;
#endif
    return 0;
}

// start = 1表示开始下载
// start = 0表示查询
boost::int32_t PEER_API GetPeerInfo(boost::int32_t start, boost::int32_t *ilistCount, boost::int32_t *iConnectCount, boost::int32_t *iAverSpeed, wchar_t const * strURL)
{
#ifdef PEER_PC_CLIENT
    if (!IsProxyModuleStarted())
    {
        return 0;
    }

    if (ilistCount == NULL || iConnectCount == NULL || iAverSpeed == NULL
        || strURL == NULL)
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, " param NULL");
        return 1;
    }

    string url(base::ws2s(strURL));
    p2sp::PlayInfo::p play_info = p2sp::PlayInfo::Parse(url);
    if (play_info && play_info->HasRidInfo() && play_info->HasUrlInfo())
    {

    }
    else
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, " play info failed, url: " << url);
        return 2;
    }

    LOG4CPLUS_DEBUG_LOG(logger_peer, " start: " << start << ", url: " << url);

    // 先解析出url和rid

    switch (start)
    {
    case 0:  // 查询
        {
            Event::p event_wait = Event::Create();
            LOG4CPLUS_DEBUG_LOG(logger_peer, "CreateEvent: " << event_wait);
            boost::shared_ptr<SimpleResult> result(new SimpleResult(event_wait));
            LOG4CPLUS_DEBUG_LOG(logger_peer, "ResultHolder: " << result);

            boost::function<void()> fun = boost::bind(&SimpleResult::result_handler, result);
            global_io_svc().post(
                boost::bind(&statistic::StatisticModule::QueryPeerInfoByRid,
                statistic::StatisticModule::Inst(), play_info->GetRidInfo().GetRID(), fun, ilistCount, iConnectCount, iAverSpeed)
               );

            event_wait->Wait();


            LOG4CPLUS_DEBUG_LOG(logger_peer, "listcount: " << *ilistCount << ", speed: " << *iAverSpeed);
        }
        break;
    case 1:  // 下载
        {
            global_io_svc().post(
                boost::bind(&p2sp::ProxyModule::StartDownloadFileByRidWithTimeout,
                p2sp::ProxyModule::Inst(), play_info->GetRidInfo(), play_info->GetUrlInfo(), 15));
        }
        break;
    default:
        return 3;
    }

    return 0;
#endif
    return 0;
}

/*
 * 开始对所有下载视频的连接限速
 * 用于客户端获取Play, DT, Drag
 * 接口已经废除
 */
void PEER_API StartLimitDownloadConnection()
{
}

/*
 * 开始对所有下载视频的连接限速
 * 用于客户端获取Play, DT, Drag之后，恢复原来的下载策略
 * 接口已经废除
 */
void PEER_API StopLimitDownloadConnection()
{
}

// 获得Peer统计的用户带宽值
int PEER_API GetPeerBandWidthInKB()
{
    LOG4CPLUS_DEBUG_LOG(logger_peer, "GetPeerBandWidthInKB");

    if (!IsProxyModuleStarted())
    {
        return 0;
    }

    if (NULL == statistic::StatisticModule::Inst())
    {
        return -1;
    }
    else
    {
        return statistic::StatisticModule::Inst()->GetBandWidthInKBps();
    }
}

bool    PEER_API GetCompeletedFilePath(
    const wchar_t* lpwszRID,
    boost::uint32_t nRIDLength,
    wchar_t* lpwszPath,
    boost::uint32_t nPathLength
   )
{
#ifdef PEER_PC_CLIENT
    LOG4CPLUS_DEBUG_LOG(logger_peer, "GetCompeletedFilePath");

    if (NULL == lpwszRID || NULL == lpwszRID)
    {
        return false;
    }

    string rid_str(base::ws2s(std::wstring(lpwszRID, nRIDLength)));
    RID rid;

    if (rid.from_string(rid_str))
    {
        return false;
    }

    if (!IsProxyModuleStarted())
    {
        return false;
    }

    string resource_path;

    Event::p event_wait = Event::Create();
    boost::shared_ptr<SimpleResult> result(new SimpleResult(event_wait));

    boost::function<void()> fun = boost::bind(&SimpleResult::result_handler, result);
    global_io_svc().post(boost::bind(&p2sp::ProxyModule::GetCompeletedFilePath, 
        p2sp::ProxyModule::Inst(), rid, boost::ref(resource_path), fun));

    event_wait->Wait();

    if (resource_path.empty())
    {
        return false;
    }

    wstring w_resource_path = base::s2ws(resource_path);
    if (w_resource_path.length() > nPathLength - 1)
    {
        return false;
    }

    wcscpy(lpwszPath, w_resource_path.c_str());

    return true;
#endif
    return true;
}

/*
 * 设置内核推送数据的速度
 * 用于客户端发送9000端口连接的推送数据
 */
void PEER_API SetSendSpeedLimitByUrl(const char * url, boost::int32_t url_len, boost::int32_t send_speed_limit)
{
    LOG4CPLUS_DEBUG_LOG(logger_peer, "SetSendSpeedLimitByUrl");

    string url_str(url, url_len);

    if (!IsProxyModuleStarted())
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, "Proxy Module is not running");
        return;
    }

    global_io_svc().post(boost::bind(&p2sp::ProxyModule::SetSendSpeedLimitByUrl,
        p2sp::ProxyModule::Inst(), url_str, send_speed_limit));
}

// 禁止/恢复内核上传
// 参数说明
//     is_disable = true  禁止上传
//     is_disable = false 恢复上传
void PEER_API DisableUpload(bool is_enable_or_disable)
{
    if (!IsProxyModuleStarted())
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, "Proxy Module is not running");
        return;
    }

    global_io_svc().post(boost::bind(&p2sp::P2PModule::SetUploadSwitch,
        p2sp::P2PModule::Inst(), is_enable_or_disable));
}

// 查询某个rid下载数据校验失败的次数
boost::int32_t PEER_API QueryBlockHashFailed(const char * str_rid, boost::uint32_t rid_len)
{
    string rid_str(std::string(str_rid, rid_len));
    RID rid;

    if (rid.from_string(rid_str))
    {
        return 0;
    }

    if (!IsProxyModuleStarted())
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, "Proxy Module is not running");
        return 0;
    }

#ifdef DISK_MODE
    boost::int32_t failed_num = 0;

    Event::p event_wait = Event::Create();
    boost::shared_ptr<SimpleResult> result(new SimpleResult(event_wait));

    boost::function<void()> fun = boost::bind(&SimpleResult::result_handler, result);
    global_io_svc().post(boost::bind(&p2sp::ProxyModule::GetBlockHashFailed, 
        p2sp::ProxyModule::Inst(), rid, &failed_num, fun));

    event_wait->Wait();
    return failed_num;
#else
    return 0;
#endif
}

// 查询内核正在下载的个数
boost::int32_t PEER_API QueryConnectionCount()
{
    if (!IsProxyModuleStarted())
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, "Proxy Module is not running");
        return 0;
    }

    return ProxyModule::Inst()->GetProxyConnectionSize();
}

PEERSTATEMACHINE PEER_API QueryPeerStateMachineByUrl(const char * url)
{
    //DebugLog("QueryPeerStateMachineByUrl url:%s", url);

    PEERSTATEMACHINE peer_state;
    peer_state.state_machine_ = -1;
    peer_state.http_speed_ = 0;
    peer_state.p2p_speed_ = 0;

    if (!IsProxyModuleStarted())
    {
        return peer_state;
    }

    LOG4CPLUS_DEBUG_LOG(logger_peer, " url = " << url);

    Event::p event_wait = Event::Create();
    LOG4CPLUS_DEBUG_LOG(logger_peer, "CreateEvent: " << event_wait);
    boost::shared_ptr<SimpleResult> result(new SimpleResult(event_wait));
    LOG4CPLUS_DEBUG_LOG(logger_peer, "ResultHolder: " << result);

    boost::function<void()> fun = boost::bind(&SimpleResult::result_handler, result);
    global_io_svc().post(
        boost::bind(&p2sp::ProxyModule::QueryPeerStateMachineByUrl, p2sp::ProxyModule::Inst(), url, fun, &peer_state)
        );

    event_wait->Wait();
    LOG4CPLUS_DEBUG_LOG(logger_peer, "event_wait->Wait() Succeed: state = " << peer_state.state_machine_ << 
        " http_speed = " << peer_state.http_speed_ << " p2p_speed = " << peer_state.p2p_speed_);

    return peer_state;
}

void PEER_API SetRestPlayTimeByUrl(const char * url, boost::uint32_t rest_play_time)
{
    //DebugLog("SetRestPlayTimeByUrl url:%s, rest_play_time=%d", url, rest_play_time);
    LOG4CPLUS_DEBUG_LOG(logger_peer, " url = " << url);

    if (!IsProxyModuleStarted())
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, "Proxy Module is not started!");
        return;
    }

    global_io_svc().post(
        boost::bind(
        &p2sp::ProxyModule::SetRestPlayTimeByUrl, p2sp::ProxyModule::Inst(),
        string(url), rest_play_time));
}

void PEER_API SetVipLevelByUrl(const char * url, boost::uint32_t url_len, boost::uint32_t vip_level)
{
    string url_string(url, url_len);
    if (!IsProxyModuleStarted())
    {
        return;
    }
    global_io_svc().post(
        boost::bind(
        &p2sp::ProxyModule::SetVipLevelByUrl, p2sp::ProxyModule::Inst(), url_string, vip_level));
}

void PEER_API QueryDragPeerStateByUrl(const char * url, boost::int32_t * state)
{
    if (!IsProxyModuleStarted())
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, "Proxy Module is not started!");
        return;
    }

    LOG4CPLUS_DEBUG_LOG(logger_peer, " url = " << url);

    Event::p event_wait = Event::Create();
    boost::shared_ptr<SimpleResult> result(new SimpleResult(event_wait));

    boost::function<void()> fun = boost::bind(&SimpleResult::result_handler, result);
    global_io_svc().post(boost::bind(&p2sp::ProxyModule::QueryDragStateByUrl,
        p2sp::ProxyModule::Inst(), url, state, fun));

    event_wait->Wait();

    LOG4CPLUS_DEBUG_LOG(logger_peer, "QueryDragPeerStateByUrl Succeed: " << *state);

    return;
}

void PEER_API SetDownloadModeByUrl(const char * url, boost::uint32_t download_mode)
{
    DebugLog("SetDownloadModeByUrl url:%s", url);
    LOG4CPLUS_DEBUG_LOG(logger_peer, " url = " << url);

    if (!IsProxyModuleStarted())
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, "Proxy Module is not started!");
        return;
    }

    global_io_svc().post(
        boost::bind(
        &p2sp::ProxyModule::SetDownloadModeByUrl, p2sp::ProxyModule::Inst(),
        string(url), download_mode));
}

bool PEER_API GetCompeletedFilePathByUrl(const char * url, wchar_t* lpwszPath, boost::uint32_t nPathLength)
{
    DebugLog("GetCompeletedFilePathByUrl url:%s", url);
#ifdef PEER_PC_CLIENT
    LOG4CPLUS_DEBUG_LOG(logger_peer, "GetCompeletedFilePath");

    if (!IsProxyModuleStarted())
    {
        return false;
    }

    string resource_path;

    Event::p event_wait = Event::Create();
    boost::shared_ptr<SimpleResult> result(new SimpleResult(event_wait));

    boost::function<void()> fun = boost::bind(&SimpleResult::result_handler, result);
    global_io_svc().post(boost::bind(&p2sp::ProxyModule::GetCompeletedFilePathByUrl, 
        p2sp::ProxyModule::Inst(), url, boost::ref(resource_path), fun));

    event_wait->Wait();

    if (resource_path.empty())
    {
        return false;
    }

    wstring w_resource_path = base::s2ws(resource_path);
    if (w_resource_path.length() > nPathLength - 1)
    {
        return false;
    }

    wcscpy(lpwszPath, w_resource_path.c_str());
#endif
    return true;
}

// 查询某个url下载数据校验失败的次数
boost::int32_t PEER_API QueryBlockHashFailedByUrl(const char * url)
{
    DebugLog("QueryBlockHashFailedByUrl url:%s", url);
    if (!IsProxyModuleStarted())
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, "Proxy Module is not running");
        return 0;
    }

#ifdef DISK_MODE
    boost::int32_t failed_num = 0;

    Event::p event_wait = Event::Create();
    boost::shared_ptr<SimpleResult> result(new SimpleResult(event_wait));

    boost::function<void()> fun = boost::bind(&SimpleResult::result_handler, result);
    global_io_svc().post(boost::bind(&p2sp::ProxyModule::GetBlockHashFailedByUrl, 
        p2sp::ProxyModule::Inst(), url, &failed_num, fun));

    event_wait->Wait();
    return failed_num;
#else
    return 0;
#endif
}

void PEER_API SetUpnpPortForTcpUpload(boost::uint16_t upnp_port)
{
    p2sp::AppModule::Inst()->SetUpnpPortForTcpUpload(upnp_port);
}

void PEER_API QueryDownloadProgressByUrlNew(char const * lpszUrl, boost::uint32_t nUrlLength, boost::int32_t * file_length,
                                            boost::int32_t * downloaded_bytes, boost::int32_t * position)
{
    if (NULL == lpszUrl || 0 == nUrlLength)
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, " lpwszURL = NULL || nURLLength == 0");
        return;
    }

    if (!IsProxyModuleStarted())
    {
        LOG4CPLUS_DEBUG_LOG(logger_peer, "ProxyModule is not running!");
        return;
    }

    string url_str(lpszUrl, nUrlLength);

    Event::p event_wait = Event::Create();
    boost::shared_ptr<SimpleResult> result(new SimpleResult(event_wait));

    boost::function<void()> fun = boost::bind(&SimpleResult::result_handler, result);

    global_io_svc().post(
        boost::bind(&p2sp::ProxyModule::QueryDownloadProgressByUrl, p2sp::ProxyModule::Inst(),
        url_str, file_length, downloaded_bytes, position, fun));

    event_wait->Wait();
}

void PEER_API QueryDownloadProgress2(const char * url, boost::uint32_t url_len,
                                     boost::uint32_t start_pos, boost::uint32_t * last_pos)
{
    if (NULL == url || 0 == url_len)
    {
        return;
    }

    if (!IsProxyModuleStarted())
    {
        return;
    }

    string url_str(url, url_len);

    Event::p event_wait = Event::Create();
    boost::shared_ptr<SimpleResult> result(new SimpleResult(event_wait));

    boost::function<void()> fun = boost::bind(&SimpleResult::result_handler, result);

    global_io_svc().post(
        boost::bind(&p2sp::ProxyModule::QueryDownloadProgress2, p2sp::ProxyModule::Inst(),
        url_str, start_pos, last_pos, fun));

    event_wait->Wait();
}

//////////////////////////////////////////////////////////////////////////
// 接口分配函数
//////////////////////////////////////////////////////////////////////////


void PEER_DECL PEER_API TS_XXXX(LPNETINTERFACE lpNetInterface)
{
    assert(lpNetInterface != NULL);

    memset(lpNetInterface, 0, sizeof(NETINTERFACE));
    lpNetInterface->dwVersion = (9u << 16);
    // version 0, 1
    lpNetInterface->Startup = Startup;
    lpNetInterface->Cleanup = Clearup;
    lpNetInterface->CoreFreeBuffer = FreeBuffer;
    lpNetInterface->WillCleanup = WillCleanup;
    lpNetInterface->SetMaxUploadSpeedInKBps = SetMaxUploadSpeedInKBps;
    lpNetInterface->OpenUPNPSucced = OpenUPNPSucced;
    lpNetInterface->SetUrlFileName = SetUrlFileName;
    lpNetInterface->UploadAction = UploadAction;
    // version 0, 2
    lpNetInterface->StopDownload = StopDownload;
    lpNetInterface->StartDownload = StartDownload;
    // version 0, 3
    lpNetInterface->ResetCompleteCount = ResetCompleteCount;
    lpNetInterface->RemoveDownloadFile = RemoveDownloadFile;
    // version 0, 4
    lpNetInterface->StartDownloadAll = StartDownloadAll;
    // version 0, 5
    lpNetInterface->StartDownloadEx = StartDownloadEx;
    lpNetInterface->RemoveDownloadFileEx = RemoveDownloadFileEx;
    // version 0, 6
    lpNetInterface->LimitDownloadSpeedInKBpsByUrl = LimitDownloadSpeedInKBpsByUrl;
    // version 0, 7

    // version 0, 8
    lpNetInterface->QueryDownloadProgress = QueryDownloadProgress;
    lpNetInterface->QueryDownloadSpeed = QueryDownloadSpeed;
    // version 0, 9
    lpNetInterface->SetPeerState = SetPeerState;

    lpNetInterface->QueryDownloadProgressByUrl = QueryDownloadProgressByUrl;
    lpNetInterface->QueryDownloadSpeedByUrl = QueryDownloadSpeedByUrl;
    // version 0, 11
    lpNetInterface->SetWebUrl = SetWebUrl;
    // version 0, 12
    lpNetInterface->QueryPeerStateMachine = QueryPeerStateMachine;
    // version 0, 13
    lpNetInterface->NotifyTaskStatusChange = NotifyTaskStatusChange;
    lpNetInterface->NotifyJoinLeave = NotifyJoinLeave;
    // version 0, 14
    lpNetInterface->SetRestPlayTime = SetRestPlayTime;
    // version 0, 15
    lpNetInterface->QueryDragPeerState = QueryDragPeerState;
    // version 0, 16
    lpNetInterface->GetBasicPeerInfo = GetBasicPeerInfo;
    lpNetInterface->GetPeerInfo = GetPeerInfo;
    // version 0, 17
    lpNetInterface->SetDownloadMode = SetDownloadMode;
    // version 0, 18
    lpNetInterface->StartLimitDownloadConnection = StartLimitDownloadConnection;
    lpNetInterface->StopLimitDownloadConnection = StopLimitDownloadConnection;
    // version 0, 19
    lpNetInterface->GetPeerBandWidthInKB = GetPeerBandWidthInKB;
    lpNetInterface->GetCompeletedFilePath = GetCompeletedFilePath;
    lpNetInterface->SetSendSpeedLimitByUrl = SetSendSpeedLimitByUrl;
    // version 0, 20
    lpNetInterface->DisableUpload = DisableUpload;
    lpNetInterface->QueryBlockHashFailed = QueryBlockHashFailed;
    // version 0, 21
    lpNetInterface->QueryConnectionCount = QueryConnectionCount;
    // version 0, 22
    lpNetInterface->QueryPeerStateMachineByUrl = QueryPeerStateMachineByUrl;
    lpNetInterface->SetRestPlayTimeByUrl = SetRestPlayTimeByUrl;
    lpNetInterface->QueryDragPeerStateByUrl = QueryDragPeerStateByUrl;
    lpNetInterface->SetDownloadModeByUrl = SetDownloadModeByUrl;
    lpNetInterface->GetCompeletedFilePathByUrl = GetCompeletedFilePathByUrl;
    lpNetInterface->QueryBlockHashFailedByUrl = QueryBlockHashFailedByUrl;
    // verison 0, 23
    lpNetInterface->SetUpnpPortForTcpUpload = SetUpnpPortForTcpUpload;
    lpNetInterface->QueryDownloadProgressByUrlNew = QueryDownloadProgressByUrlNew;
    lpNetInterface->QueryDownloadProgress2 = QueryDownloadProgress2;
    lpNetInterface->SetVipLevelByUrl = SetVipLevelByUrl;
}
