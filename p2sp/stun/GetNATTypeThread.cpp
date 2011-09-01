//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/stun/GetNATTypeThread.h"
#include "p2sp/stun/StunModule.h"

#include <boost/thread/thread.hpp>

namespace p2sp
{
    GetNATTypeThread* GetNATTypeThread::inst_ = new GetNATTypeThread();

    void GetNATTypeThread::Start(
        boost::asio::io_service & io_svc)
    {
        ios_main_ = &io_svc;
        if (NULL == work_)
        {
            work_ = new boost::asio::io_service::work(ios_);
            thread_ = new boost::thread(&GetNATTypeThread::Run);
        }
    }

    void GetNATTypeThread::Stop()
    {
        if (NULL != work_)
        {
            delete work_;
            work_ = NULL;
            thread_->join();
            delete thread_;
            thread_ = NULL;
        }
    }

    void GetNATTypeThread::GetNATType(const string& config_path)
    {
        protocol::MY_STUN_NAT_TYPE nat_type = stun_client.StartGetNatType(config_path);
        ios_main_->post(boost::bind(&StunModule::OnGetNATType, StunModule::Inst(), nat_type));
        stun_client.Stop();
    }
}
