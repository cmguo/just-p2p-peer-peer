//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// StorageThread.h

#ifndef _STORAGE_STORAGE_THREAD_H_
#define _STORAGE_STORAGE_THREAD_H_

#include "base/CommonThread.h"

namespace storage
{
#ifdef DISK_MODE
    class StorageThread : public boost::noncopyable
    {
    public:
        typedef boost::shared_ptr<StorageThread> p;

    public:
        void Start();
        void Stop();

    private:
        static StorageThread::p main_thread_;
        base::CommonThread storage_thread_;
    public:
        static StorageThread& Inst()
        {
            if (!main_thread_)
            {
                main_thread_.reset(new StorageThread());
            }

            return *main_thread_; 
        }

        void Post(boost::function<void()> handler);
    private:
        StorageThread();
    };
#endif  // DISK_MODE

}

#endif  // _STORAGE_STORAGE_THREAD_H_
