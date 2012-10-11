#include "Common.h"
#include "base/CommonThread.h"
#include <boost/thread/thread.hpp>

namespace base
{
    CommonThread::CommonThread ()
        : work_(NULL)
        , ios_(NULL)
        , thread_(NULL)
    {
    }

    void CommonThread::Start()
    {
        if (NULL == work_)
        {
            ios_ = new boost::asio::io_service();
            work_ = new boost::asio::io_service::work(* ios_);
            thread_ = new boost::thread(boost::bind(&CommonThread::Run, this));
        }
    }

    void CommonThread::Stop()
    {
        if (NULL != work_)
        {
            delete work_;
            work_ = NULL;
            ios_ -> stop();
            thread_ -> join();
            delete thread_;
            thread_ = NULL;
            delete ios_;
            ios_ = NULL;
        }
    }

    void CommonThread::Run()
    {
        ios_ -> run();
    }

    void CommonThread::Post(boost::function<void()> handler)
    {
        if (work_ != NULL && ios_ != NULL)
        {
            ios_ -> post(handler);
        }
    }
}