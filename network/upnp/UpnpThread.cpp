#include "Common.h"


#include "network/upnp/UpnpThread.h"
#include <boost/thread/thread.hpp>




namespace p2sp
{
    UpnpThread* UpnpThread::inst_ = new UpnpThread();


    void UpnpThread::Start()
    {
        ios_main_ = new boost::asio::io_service();
        if (NULL == work_)
        {
            work_ = new boost::asio::io_service::work(ios_);
            thread_ = new boost::thread(&UpnpThread::Run);
        }
    }

    void UpnpThread::Stop()
    {
        if (NULL != work_)
        {
            delete work_;
            work_ = NULL;
            thread_->join();
            delete thread_;
            thread_ = NULL;
            delete ios_main_;
            ios_main_ = NULL;
        }
    }

    void UpnpThread::Post(boost::function<void()> handler)
    {
        if (Inst().work_ != NULL )
        {
            Inst().ios_.post(handler);
        }
    }

    void UpnpThread::Dispatch(boost::function<void()> handler)
    {
        if (Inst().work_ != NULL)
        {
            Inst().ios_.dispatch(handler);
        }
    }
}
