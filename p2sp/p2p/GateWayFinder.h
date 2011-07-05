// GateWayFinder.h

#ifndef _P2SP_GATEWAY_FINDER_H_
#define _P2SP_GATEWAY_FINDER_H_

#include "network/icmp/PingClient.h"

namespace p2sp
{
    class IGateWayFinderListener
    {
    public:
        virtual void OnGateWayFound(const string & gateway_ip) = 0;
        virtual ~IGateWayFinderListener(){}
    };

    class GateWayFinder
    {
    public:
        GateWayFinder(boost::asio::io_service& io_service, IGateWayFinderListener * listener);
        ~GateWayFinder();
        void Start();
        void Stop();

    private:
        void StartSend();
        void HandleTimeOut(const boost::system::error_code& error);
        void HandleReceive(unsigned char type, const string & src_ip);

    private:
        boost::asio::io_service & io_svc_;
        IGateWayFinderListener * listener_;
        network::PingClient::p ping_client_;
        boost::asio::deadline_timer timer_;
        boost::uint16_t sequence_number_;
        boost::posix_time::ptime time_sent_;
        int ttl_;
    };
}

#endif