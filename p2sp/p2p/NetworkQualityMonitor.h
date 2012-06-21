//NetworkQualityMonitor.h

#ifndef _P2SP_NETWORK_QUALITY_MONITOR_
#define _P2SP_NETWORK_QUALITY_MONITOR_

#include "GateWayFinder.h"
#include "measure/CycleBuffer.h"

namespace p2sp
{
    class NetworkQualityMonitor
        : public IGateWayFinderListener
        , public boost::enable_shared_from_this<NetworkQualityMonitor>
    {
    public:
        NetworkQualityMonitor(boost::asio::io_service & io_svc);
        ~NetworkQualityMonitor();

        void Start();
        void Stop();

        bool IsRunning() const {return is_running_;}
        bool HasGateWay() const {return !gateway_ip_.empty() && ping_counter_.running();}

        void OnGateWayFound(const string & gateway_ip);
        uint32_t GetAveragePingDelay() {return ping_delay_buffer_.Average();}
        uint32_t GetPingLostRate();
        void ClearPingLostRate() {ping_lost_buffer_.Clear();}

    private:
        void OnPingResponse(unsigned char type, const string & src_ip, boost::uint32_t ping_rtt_for_win7 = 65535);
        void OnPingTimerElapsed(framework::timer::Timer * timer);

    private:
        boost::asio::io_service & io_svc_;
        bool is_running_;
        boost::shared_ptr<GateWayFinder> gateway_finder_;
        string gateway_ip_;
        network::PingClientBase::p ping_client_;
        framework::timer::PeriodicTimer ping_timer_;
        framework::timer::TickCounter ping_counter_;
        bool is_ping_replied_;
        uint16_t sequence_num_;

        measure::CycleBuffer ping_delay_buffer_;
        // 丢包插入1，不丢包插入0，这样ping_lost_buffer_的平均值就是丢包率
        measure::CycleBuffer ping_lost_buffer_;
    };
}

#endif