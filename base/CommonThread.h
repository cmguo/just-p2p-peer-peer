/******************************************************************************
*
* Copyright (c) 2012 PPLive Inc.  All rights reserved.
*
******************************************************************************/

#ifndef _BASE_COMMON_THREAD_H_
#define _BASE_COMMON_THREAD_H_

namespace boost
{
    class thread;
}

#include <boost/function.hpp>

namespace base
{
    class CommonThread 
        : public boost::noncopyable
    {
    private:
        void Run();

    public:
        CommonThread();
        void Start();
        void Stop();

        void Post(boost::function<void()> handler);

    private:
        boost::asio::io_service * ios_;
        boost::asio::io_service::work * work_;
        boost::thread * thread_;
    };
}
#endif // _BASE_COMMON_THREAD_H_