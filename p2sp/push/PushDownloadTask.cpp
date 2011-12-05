#include "Common.h"
#include "PushDownloadTask.h"
#include "p2sp/proxy/ProxyModule.h"
#include <boost/bind.hpp>

namespace p2sp
{
#ifdef DISK_MODE

PushDownloadTask::PushDownloadTask(boost::asio::io_service & io_svc, const protocol::PushTaskItem& task_param)
:
io_svc_(io_svc),
task_param_(task_param),
state_(TS_IDLE),
speed_limit_(-1)
{
}

PushDownloadTask::~PushDownloadTask()
{
    Stop();
}

void PushDownloadTask::Start()
{
    BOOST_ASSERT(state_ == TS_IDLE);

    state_ = TS_CHECK_COPYRIGHT;

    copyright_checker_ptr_ = CopyrightChecker::create(io_svc_, shared_from_this());
    copyright_checker_ptr_->Start(task_param_.channel_id_);
}

void PushDownloadTask::Stop()
{   
    if (copyright_checker_ptr_) {
        copyright_checker_ptr_->Stop();
        copyright_checker_ptr_.reset();
    }

    p2sp::ProxyModule::Inst()->StopProxyConnection(task_param_.url_);
}

void PushDownloadTask::OnCopyrightCheckResult(bool passed)
{
    BOOST_ASSERT(state_ == TS_CHECK_COPYRIGHT);

    if (passed) {
        state_ = TS_DOWNLOADING;
        p2sp::ProxyModule::Inst()->StartDownloadFileByRid(task_param_.rid_info_,
            protocol::UrlInfo(task_param_.url_, task_param_.refer_url_),
            protocol::TASK_OPEN_SERVICE, true);
        p2sp::ProxyModule::Inst()->LimitDownloadSpeedInKBps(task_param_.url_, speed_limit_);
    }
    else {
        state_ = TS_FAILED;
    }
}

void PushDownloadTask::OnCopyrightCheckFailed()
{
    BOOST_ASSERT(state_ == TS_CHECK_COPYRIGHT);

    state_ = TS_FAILED;
}

void PushDownloadTask::LimitSpeed(boost::uint32_t speed)
{
    speed_limit_ = speed;

    if (state_ == TS_DOWNLOADING) {
        p2sp::ProxyModule::Inst()->LimitDownloadSpeedInKBps(task_param_.url_, speed_limit_);
    }
}

PushDownloadTask::TaskState PushDownloadTask::GetTaskState()
{
    if (state_ == TS_DOWNLOADING) {
        p2sp::ProxyModule::Inst()->QueryDownloadProgress(task_param_.rid_info_.rid_, 
            boost::bind(&PushDownloadTask::OnDownloadProgress, shared_from_this(), _1, _2));
    }
    return state_;
}

void PushDownloadTask::OnDownloadProgress(boost::uint32_t file_size, boost::uint32_t completed_size)
{
    if (file_size != 0 && file_size == completed_size) {
        state_ = TS_COMPLETED;
    }
}

#endif

}