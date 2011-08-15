// GateWayFinder.cpp

#include "Common.h"
#include "GateWayFinder.h"

#include "base/util.h"

#include <boost/date_time.hpp>

using boost::asio::ip::icmp;
namespace posix_time = boost::posix_time;

char *cdnips[]={"125.39.129.53","10.13.13.214","124.89.10.138","61.158.254.141","123.126.34.210",
                "221.192.146.138","61.155.162.135","61.158.254.148","119.167.230.151","211.162.47.74",
                "122.193.14.2","58.22.135.55","211.162.47.89","61.158.254.21","118.123.212.32",
                "121.11.252.11","125.89.73.201","121.11.252.16","119.253.169.87","61.155.162.150"};

namespace p2sp
{
    GateWayFinder::GateWayFinder(boost::asio::io_service& io_service, IGateWayFinderListener * listener)
        : io_svc_(io_service), listener_(listener), is_running_(false), timer_(io_service), sequence_number_(0)
    {
    }

    GateWayFinder::~GateWayFinder()
    {
        Stop();
    }

    void GateWayFinder::Start()
    {
        is_running_ = true;
        if (!ping_client_)
        {
            ping_client_ = network::PingClientBase::create(io_svc_);
            
            if (!ping_client_)
            {
                return;
            }

            ping_client_->Bind(cdnips[rand() % (sizeof(cdnips)/sizeof(cdnips[0]))]);
        }

        Reset();
        StartSend();     
    }

    void GateWayFinder::Stop()
    {
        if (ping_client_)
        {
            ping_client_->CancelAll();
        }

        is_running_ = false;
    }

    void GateWayFinder::Reset()
    {
        ttl_ = 1;
        time_out_num_ = 0;
    }

    void GateWayFinder::StartSend()
    {
        if(is_running_)
        {
            if (ping_client_->SetTtl(ttl_))
            {
                time_sent_ = posix_time::microsec_clock::universal_time();
                sequence_number_ = ping_client_->AsyncRequest(boost::bind(&GateWayFinder::HandleReceive,
                    this, _1, _2));

                timer_.expires_at(time_sent_ + posix_time::seconds(10));
                timer_.async_wait(boost::bind(&GateWayFinder::HandleTimeOut, this, _1));
            }
        }
    }

    void GateWayFinder::HandleTimeOut(const boost::system::error_code& error)
    {
        if (error != boost::asio::error::operation_aborted)
        {
            DebugLog("GateWayFinder::HandleTimeOut\n");
            assert(!error);

            ++time_out_num_;
            ping_client_->Cancel(sequence_number_);
            if (time_out_num_ < 5)
            {
                StartSend();
            }
        }
    }

    void GateWayFinder::HandleReceive(unsigned char type, const string & src_ip)
    {
        if (type == icmp_header::time_exceeded)
        {
            DebugLog("received time_exceeded\n");

            time_out_num_ = 0;
            timer_.cancel();

            posix_time::ptime now = posix_time::microsec_clock::universal_time();
            DebugLog("from %s, ttl=%d, time=%dms\n", src_ip.c_str(),
                ttl_, (now - time_sent_).total_milliseconds());

            if (base::util::is_private_address(src_ip.c_str()))
            {
                DebugLog("received my time_exceeded from private_address\n");
                ++ttl_;
                StartSend();
            }
            else
            {
                DebugLog("get public gateway:%s\n", src_ip.c_str());
                listener_->OnGateWayFound(src_ip);
            }
        }
        else
        {
            assert(false);
        }
    }
}
