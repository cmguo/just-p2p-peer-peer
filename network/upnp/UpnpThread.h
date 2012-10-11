/******************************************************************************
*
* Copyright (c) 2012 PPLive Inc.  All rights reserved.
*
* UpnpThread.h
* 
* Description: 端口映射的线程
*             
* 
* --------------------
* 2012-08-24,  kelvinchen create
* --------------------
******************************************************************************/

#ifndef UPNP_THREAD_H_CK_20120824
#define UPNP_THREAD_H_CK_20120824

#include <map>
#include "base/CommonThread.h"

namespace p2sp
{
    class UpnpThread
        : public boost::noncopyable
    {
    public:
        void Start();

        void Stop();

        static UpnpThread& Inst() 
        {
            if (!inst_)
            {
                inst_ = new UpnpThread();
            }
            return *inst_;
        }

        void Post(boost::function<void()> handler);
        
    private:
        UpnpThread();

    private:
        static UpnpThread* inst_;
        
        base::CommonThread upnp_thread_;
    };
}

#endif  // UPNP_THREAD_H_CK_20120824
