//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "MainThread.h"

MainThread* MainThread::instance_(NULL);
boost::uint32_t MainThread::id_(0);

MainThread::MainThread()
{
}

void MainThread::Start()
{
    assert(instance_ == NULL);

#ifdef PEER_PC_CLIENT
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

#ifdef PEER_PC_CLIENT

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
    WaitForSingleObject(ios_thread, INFINITE);
    CloseHandle(ios_thread);
}

#else

BoostMainThread::BoostMainThread()
    :ios_thread(NULL)
{
}

void BoostMainThread::StartIoserviceThread()
{
    global_io_svc().run();
}

void BoostMainThread::StartThread()
{
    global_io_svc().reset();
    ios_thread = new boost::thread(boost::bind(&BoostMainThread::StartIoserviceThread, this));
    // TODO(herain):2011-4-8:boost::threadû��ֱ�ӻ�ȡthread id�Ľӿڣ���Ҫ��ȡnatvie handle����natvie API���
    // SetThreadId();
}

void BoostMainThread::StopThread()
{
    ios_thread->join();
    delete ios_thread;
}

#endif

