//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// StorageThread.h

#ifndef _STORAGE_STORAGE_THREAD_H_
#define _STORAGE_STORAGE_THREAD_H_

namespace boost
{
    class thread;
}

#include <boost/function.hpp>

namespace storage
{
#ifdef DISK_MODE
    class StorageThread : public boost::noncopyable
    {
    public:
        typedef boost::shared_ptr<StorageThread> p;
        boost::asio::io_service* ios_;
    private:
        boost::asio::io_service::work* work_;
        boost::thread* thread_;

    public:
        StorageThread();
        void Start();
        void Stop();

    private:
        static StorageThread::p main_thread_;
    public:
        static StorageThread& Inst() { return *main_thread_; };
        static boost::asio::io_service& IOS() { return *main_thread_->ios_; };
        static void Post(boost::function<void()> handler);
        static void Dispatch(boost::function<void()> handler);
    protected:
        static void Run();
    };
#endif  // DISK_MODE

}

#endif  // _STORAGE_STORAGE_THREAD_H_
