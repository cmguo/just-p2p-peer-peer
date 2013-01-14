// GateWayFinder.h

#ifndef _P2SP_GATEWAY_FINDER_H_
#define _P2SP_GATEWAY_FINDER_H_

#include "network/icmp/PingClientBase.h"
#include "network/icmp/icmp_header.h"

namespace p2sp
{
    class IGateWayFinderListener
    {
    public:
        virtual void OnGateWayFound(const string & gateway_ip) = 0;
        virtual ~IGateWayFinderListener(){}
    };

    class GateWayFinder
        : public boost::enable_shared_from_this<GateWayFinder>
    {
    public:
        GateWayFinder(boost::asio::io_service& io_service, boost::shared_ptr<IGateWayFinderListener> listener);
        ~GateWayFinder();
        void Start();
        void Stop();

    private:
        void StartSend();
        void HandleTimeOut(const boost::system::error_code& error);
        void HandleReceive(unsigned char type, const string & src_ip,
            boost::uint32_t ping_rtt_for_win7 = 65535);

        void Reset();

    private:
        boost::asio::io_service & io_svc_;
        boost::shared_ptr<IGateWayFinderListener> listener_;
        bool is_running_;
        network::PingClientBase::p ping_client_;
        boost::asio::deadline_timer timer_;
        boost::uint16_t sequence_number_;
        boost::posix_time::ptime time_sent_;
        int ttl_;
        boost::int32_t time_out_num_;

        bool is_bind_success_;
    };
}

#endif