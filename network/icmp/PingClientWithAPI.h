#ifndef NETWORK_PING_CLIENT_WITH_API_H
#define NETWORK_PING_CLIENT_WITH_API_H

#ifdef BOOST_WINDOWS_API

#include "icmp_header.h"
#include "PingClientBase.h"

#include <iphlpapi.h>

namespace network
{
    class PingClientWithAPI
        : public PingClientBase
        , public boost::enable_shared_from_this<PingClientWithAPI>
    {
    public:
        typedef boost::shared_ptr<PingClientWithAPI> p;
        static p Create();

        virtual void Bind(const string & destination_ip);

        virtual uint16_t AsyncRequest(boost::function<void(unsigned char, string)> handler);

        virtual bool SetTtl(int32_t ttl);

        virtual ~PingClientWithAPI();

    private:
        PingClientWithAPI();
        friend void AsyncRequestThread(LPVOID param);

    public:
        unsigned long destination_ip_;
        IP_OPTION_INFORMATION ip_option_;
        HANDLE hIcmpFile_;

        char * reply_buffer_;
        uint32_t reply_buffer_size_;

        static uint16_t sequence_num_;
        static string ping_body_;
    };
}

void AsyncRequestThread(LPVOID param);
#endif

#endif
