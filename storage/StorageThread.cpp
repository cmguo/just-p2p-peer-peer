//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "storage/StorageThread.h"

namespace storage
{
#ifdef DISK_MODE
    StorageThread::p StorageThread::main_thread_;

    StorageThread::StorageThread()
    {
    }

    void StorageThread::Start()
    {
        storage_thread_.Start();
    }

    void StorageThread::Stop()
    {
         storage_thread_.Stop();
    }

    void StorageThread::Post(boost::function<void()> handler)
    {
        storage_thread_.Post(handler);
    }
#endif  // DISK_MODE

}
