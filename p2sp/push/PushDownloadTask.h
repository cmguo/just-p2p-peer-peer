#ifndef P2SP_PUSH_PUSHDOWNLOADTASK_H
#define P2SP_PUSH_PUSHDOWNLOADTASK_H

#include "protocol/PushServerPacketV2.h"
#include "CopyRightChecker.h"

namespace p2sp
{
#ifdef DISK_MODE

class PushDownloadTask 
    : public boost::noncopyable
    , public boost::enable_shared_from_this<PushDownloadTask>
    , public CopyrightCheckerListener
{
public:
    typedef boost::shared_ptr<PushDownloadTask> p;

    static p create(boost::asio::io_service & io_svc, const protocol::PushTaskItem& task_param)
    {
        return p(new PushDownloadTask(io_svc, task_param));
    }

    enum TaskState { TS_IDLE, TS_CHECK_COPYRIGHT, TS_DOWNLOADING, TS_FAILED, TS_COMPLETED };
    PushDownloadTask(boost::asio::io_service & io_svc, const protocol::PushTaskItem& task_param);
    ~PushDownloadTask();

    void Start();
    void Stop();

    TaskState GetTaskState();

    bool IsTaskTerminated() { return GetTaskState() == TS_FAILED || GetTaskState() == TS_COMPLETED; }

    const protocol::PushTaskItem& GetPushTaskParam() { return task_param_; }
    
    void LimitSpeed(boost::uint32_t speed);

private:
    virtual void OnCopyrightCheckResult(bool passed);
    virtual void OnCopyrightCheckFailed();
    void OnDownloadProgress(boost::uint32_t file_size, boost::uint32_t completed_size);

private:
    TaskState state_;
    boost::uint32_t speed_limit_;
    protocol::PushTaskItem task_param_;
    boost::asio::io_service & io_svc_;
    CopyrightChecker::p copyright_checker_ptr_;
};

#endif

}

#endif