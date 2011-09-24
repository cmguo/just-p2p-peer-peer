//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "storage/storage_base.h"
#include "storage/StorageThread.h"

#include <boost/thread/thread.hpp>

namespace storage
{
#ifdef DISK_MODE
    StorageThread::p StorageThread::main_thread_;

    StorageThread::StorageThread()
        : work_(NULL)
        , ios_(NULL)
        , thread_(NULL)
    {
    }

    void StorageThread::Start()
    {
        if (NULL == work_)
        {
            ios_ = new boost::asio::io_service();
            work_ = new boost::asio::io_service::work(*ios_);
            thread_ = new boost::thread(&StorageThread::Run);
        }
    }

    void StorageThread::Stop()
    {
        if (NULL != work_)
        {
            delete work_;
            work_ = NULL;

            ios_->stop();

            thread_->join();
            delete thread_;
            thread_ = NULL;

            delete ios_;
            ios_ = NULL;
        }

        main_thread_.reset();
    }

    void StorageThread::Run()
    {
        IOS().run();
    }

    void StorageThread::Post(boost::function<void()> handler)
    {
        if (main_thread_->work_ != NULL && main_thread_->ios_ != NULL)
        {
            main_thread_->ios_->post(handler);
        }
    }

    void StorageThread::Dispatch(boost::function<void()> handler)
    {
        if (main_thread_->work_ != NULL && main_thread_->ios_ != NULL)
        {
            main_thread_->ios_->dispatch(handler);
        }
    }
#endif  // DISK_MODE

}
