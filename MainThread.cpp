//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "MainThread.h"

MainThread* MainThread::instance_(NULL);
uint32_t MainThread::id_(0);

MainThread::MainThread()
{
}

void MainThread::Start()
{
    assert(instance_ == NULL);

#ifdef BOOST_WINDOWS_API
    instance_ = new WindowsMainThread();
#else
    instance_ = new BoostMainThread();
#endif

    instance_->StartThread();
}

void MainThread::Stop()
{
    if (instance_ != NULL)
    {
        instance_->StopThread();
        delete instance_;
        instance_ = NULL;
    }
}

#ifdef BOOST_WINDOWS_API

DWORD WINAPI MainThreadRunner(LPVOID)
{
    MainThread::SetThreadId(::GetCurrentThreadId());
    global_io_svc().reset();
    global_io_svc().run();
    return 0;
}

WindowsMainThread::WindowsMainThread()
    :ios_thread(NULL)
{
}

void WindowsMainThread::StartThread()
{
    ios_thread = ::CreateThread(NULL, 0, MainThreadRunner, NULL, 0, NULL);
}

void WindowsMainThread::StopThread()
{
    global_io_svc().stop();

    WaitForSingleObject(ios_thread, 2*1000);
    TerminateThread(ios_thread, 0);
    CloseHandle(ios_thread);
}

#else

BoostMainThread::BoostMainThread()
    :ios_thread(NULL)
{
}

void BoostMainThread::StartThread()
{
    global_io_svc().reset();
    ios_thread = new boost::thread(boost::bind(&boost::asio::io_service::run, boost::ref(global_io_svc())));
    // TODO(herain):2011-4-8:boost::thread没有直接获取thread id的接口，需要获取natvie handle后用natvie API获得
    // SetThreadId();
}

void BoostMainThread::StopThread()
{
    ios_thread->join();
    delete ios_thread;
}

#endif

