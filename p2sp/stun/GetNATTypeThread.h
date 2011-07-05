//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef PEER_STUN_GETNATTYPETHREAD_H
#define PEER_STUN_GETNATTYPETHREAD_H
// 用于读取TrackerList配置文件 的 线程

#include "p2sp/stun/StunClient.h"

namespace boost
{
    class thread;
}

namespace p2sp
{
    class GetNATTypeThread
        : public boost::noncopyable
        , public boost::enable_shared_from_this<GetNATTypeThread>
    {
    public:
        void Start(
            boost::asio::io_service & io_svc);

        void Stop();

        void GetNATType(const string& config_path);

    public:
        static GetNATTypeThread& Inst() { return *inst_; }
        static boost::asio::io_service& IOS() { return Inst().ios_; }

    private:
        GetNATTypeThread() : work_(NULL), thread_(NULL) {}
        static void Run()
        {
            IOS().run();
        }

    private:
        static GetNATTypeThread* inst_;
    private:
        boost::asio::io_service * ios_main_;
        boost::asio::io_service ios_;
        boost::asio::io_service::work* work_;
        boost::thread* thread_;

        CStunClient stun_client;
    };
}

#endif  // PEER_STUN_GETNATTYPETHREAD_H
