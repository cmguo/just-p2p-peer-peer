// GateWayFinder.cpp

#include "Common.h"
#include "GateWayFinder.h"

#include "base/util.h"

#include <boost/date_time.hpp>

using boost::asio::ip::icmp;
namespace posix_time = boost::posix_time;

namespace p2sp
{
    GateWayFinder::GateWayFinder(boost::asio::io_service& io_service, IGateWayFinderListener * listener)
        : io_svc_(io_service), listener_(listener), timer_(io_service), sequence_number_(0), ttl_(1)
    {
    }

    GateWayFinder::~GateWayFinder()
    {
        Stop();
    }

    void GateWayFinder::Start()
    {
        ping_client_ = network::PingClient::Create(io_svc_);
        if (ping_client_)
        {
            ping_client_->Bind("60.28.216.149");
            StartSend();
        }        
    }

    void GateWayFinder::Stop()
    {
        if (ping_client_)
        {
            ping_client_->Close();
        }
    }

    void GateWayFinder::StartSend()
    {
        ping_client_->SetTtl(ttl_);

        time_sent_ = posix_time::microsec_clock::universal_time();
        sequence_number_ = ping_client_->AsyncRequest(boost::bind(&GateWayFinder::HandleReceive,
            this, _1, _2));

        timer_.expires_at(time_sent_ + posix_time::seconds(1));
        timer_.async_wait(boost::bind(&GateWayFinder::HandleTimeOut, this, _1));
    }

    void GateWayFinder::HandleTimeOut(const boost::system::error_code& error)
    {
        if (error != boost::asio::error::operation_aborted)
        {
            DebugLog("GateWayFinder::HandleTimeOut");
            assert(!error);
            ping_client_->Cancel(sequence_number_);
            StartSend();
        }
    }

    void GateWayFinder::HandleReceive(unsigned char type, const string & src_ip)
    {
        if (type == icmp_header::time_exceeded)
        {
            DebugLog("received time_exceeded");

            timer_.cancel();

            posix_time::ptime now = posix_time::microsec_clock::universal_time();
            DebugLog("from %s, ttl=%d, time=%dms", src_ip.c_str(),
                ttl_, (now - time_sent_).total_milliseconds());

            if (base::util::is_private_address(src_ip.c_str()))
            {
                DebugLog("received my time_exceeded from private_address");
                ++ttl_;
                StartSend();
            }
            else
            {
                DebugLog("get public gateway:%s", src_ip.c_str());
                listener_->OnGateWayFound(src_ip);
            }
        }
    }
}
