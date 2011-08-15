//NetworkQualityMonitor.cpp

#include "Common.h"
#include "NetworkQualityMonitor.h"

namespace p2sp
{
    NetworkQualityMonitor::NetworkQualityMonitor(boost::asio::io_service & io_svc)
        : io_svc_(io_svc), is_running_(false), gateway_finder_(io_svc, this)
        , ping_timer_(global_second_timer(), 1000, boost::bind(&NetworkQualityMonitor::OnPingTimerElapsed, this, &ping_timer_))
        , ping_counter_(false), is_ping_replied_(false)
        , ping_delay_buffer_(5), ping_lost_buffer_(20)
    {
    }

    NetworkQualityMonitor::~NetworkQualityMonitor()
    {
        Stop();
    }

    void NetworkQualityMonitor::Start()
    {
        is_running_ = true;
        if (gateway_ip_.empty())
        {
            gateway_finder_.Start();
        }
        
        ping_delay_buffer_.Clear();
        ping_lost_buffer_.Clear();
        ping_timer_.start();
    }

    void NetworkQualityMonitor::Stop()
    {
        gateway_finder_.Stop();
        ping_timer_.stop();
        ping_counter_.stop();
        if (ping_client_)
        {
            ping_client_->CancelAll();
        }

        is_running_ = false;
    }

    void NetworkQualityMonitor::OnGateWayFound(const string & gateway_ip)
    {
        gateway_ip_ = gateway_ip;

        ping_client_ = network::PingClientBase::create(io_svc_);

        if (ping_client_)
        {
            bool is_bind_success = ping_client_->Bind(gateway_ip_);
            assert(is_bind_success);
        }        
    }

    void NetworkQualityMonitor::OnPingTimerElapsed(framework::timer::Timer * timer)
    {
        assert(timer == &ping_timer_);
        if (!gateway_ip_.empty() && ping_client_)
        {
            if (ping_counter_.running())
            {
                if (!is_ping_replied_)
                {
                    // timeout, ping lost
                    ping_client_->Cancel(sequence_num_);
                    ping_lost_buffer_.Push(1);
                }
                else
                {
                    ping_lost_buffer_.Push(0);
                }
            }

            sequence_num_ = ping_client_->AsyncRequest(boost::bind(&NetworkQualityMonitor::OnPingResponse, this, _1, _2));
            ping_counter_.start();
            is_ping_replied_ = false;
        }
    }

    void NetworkQualityMonitor::OnPingResponse(unsigned char type, const string & src_ip)
    {
        if (type == icmp_header::echo_reply)
        {
            assert(src_ip == gateway_ip_);
            ping_delay_buffer_.Push(ping_counter_.elapsed());
            is_ping_replied_ = true;
        }
        else
        {
            assert(false);
        }
    }

    uint32_t NetworkQualityMonitor::GetPingLostRate()
    {
        if (ping_lost_buffer_.Count() == 0)
        {
            return 0;
        }
        else
        {
            return ping_lost_buffer_.Sum() * 100 / ping_lost_buffer_.Count();
        }
    }
}