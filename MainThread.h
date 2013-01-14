//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _PEER_MAIN_THREAD_H_
#define _PEER_MAIN_THREAD_H_

#include "Common.h"
#include "p2sp/AppModule.h"

// MainThread类封装peer的主线程。在Windows下使用Windows线程，在其他平台上使用boost线程
// 在Windows下不使用boost线程的原因是如果有未处理异常从主线程抛出，boost会抓住异常
// 并弹出runtime error对话框；而期望的行为是PPAP直接crash，这样该异常可以被报告到crash
// 后台.
//
// MainThread类使用方法：
//
// 调用静态函数MainThread::Start()启动主线程
// 调用静态函数MainThread::Stop()停止主线程。
//
class MainThread
{
    public:
    static void Start();
    static void Stop();
    static boost::uint32_t GetThreadId() {return id_;}
    static void SetThreadId(boost::uint32_t id) {id_ = id;}
    static bool IsRunning() {return instance_ != NULL;}

    protected:
    MainThread();

    virtual void StartThread() = 0;
    virtual void StopThread() = 0;

    static MainThread* instance_;
    static boost::uint32_t id_;
};

#ifdef PEER_PC_CLIENT

#include<windows.h>

class WindowsMainThread
    :public MainThread
{
    public:
    WindowsMainThread();

    protected:
    virtual void StartThread();
    virtual void StopThread();

    private:
    HANDLE ios_thread;
};

#else

#include <boost/thread.hpp>

class BoostMainThread
    :public MainThread
{
    public:
    BoostMainThread();

    protected:
    virtual void StartThread();
    virtual void StopThread();

    private:
    boost::thread* ios_thread;
};

#endif  // PEER_PC_CLIENT

#endif  // _PEER_MAIN_THREAD_H_

