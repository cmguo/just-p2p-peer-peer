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

//#include "p2sp/stun/StunClient.h"
#include <map>

namespace boost
{
    class thread;
}

namespace p2sp
{
    class UpnpThread
        : public boost::noncopyable
        , public boost::enable_shared_from_this<UpnpThread>
    {
    public:
        void Start();

        void Stop();

    public:
        static UpnpThread& Inst() { return *inst_; }
        static boost::asio::io_service& IOS() { return Inst().ios_; }

        static void Post(boost::function<void()> handler);
        static void Dispatch(boost::function<void()> handler);
        

    private:
        UpnpThread() : work_(NULL), thread_(NULL) {}
        
        static void Run()
        {
            IOS().run();
        }

    private:
        boost::asio::io_service ios_;
        boost::asio::io_service::work* work_;

        static UpnpThread* inst_;

        boost::asio::io_service * ios_main_;
        
        boost::thread* thread_;

        
    };
}

#endif  // UPNP_THREAD_H_CK_20120824
