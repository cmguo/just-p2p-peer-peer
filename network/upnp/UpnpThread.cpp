#include "Common.h"
#include "network/upnp/UpnpThread.h"

namespace p2sp
{
    UpnpThread* UpnpThread::inst_;

    UpnpThread::UpnpThread()
    {
    }

    void UpnpThread::Start()
    {
        upnp_thread_.Start();
    }

    void UpnpThread::Stop()
    {
        upnp_thread_.Stop();
    }

    void UpnpThread::Post(boost::function<void()> handler)
    {
        upnp_thread_.Post(handler);
    }
}
