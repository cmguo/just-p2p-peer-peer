#include "Common.h"

#ifdef BOOST_WINDOWS_API

#include "PingClientWithAPI.h"
#include "p2sp/AppModule.h"

#include <winsock2.h>
#include <icmpapi.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

namespace network
{
    uint16_t PingClientWithAPI::sequence_num_ = 0;
    string PingClientWithAPI::ping_body_ = "Hello PPLive";

    PingClientWithAPI::p PingClientWithAPI::Create()
    {
        return PingClientWithAPI::p(new PingClientWithAPI());
    }

    PingClientWithAPI::PingClientWithAPI()
    {
        hIcmpFile_ = IcmpCreateFile();
        reply_buffer_size_ = sizeof (ICMP_ECHO_REPLY) + ping_body_.length() + 8;
        reply_buffer_ = (char *) malloc(reply_buffer_size_);

        memset(&ip_option_, 0, sizeof(ip_option_));
        ip_option_.Ttl = 255;
    }

    PingClientWithAPI::~PingClientWithAPI()
    {
        IcmpCloseHandle(hIcmpFile_);
        free(reply_buffer_);
    }

    uint16_t PingClientWithAPI::AsyncRequest(boost::function<void(unsigned char, string)> handler)
    {
        sequence_num_++;
        assert(ping_body_.length() >= 12);

        if (hIcmpFile_ == INVALID_HANDLE_VALUE)
        {
            return 0;
        }
        
        if (reply_buffer_ == NULL)
        {
            return 0;
        }

        AddHandler(sequence_num_, handler);

        _beginthread(&AsyncRequestThread, 0, this);
       
        return sequence_num_;
    }

    void AsyncRequestThread(LPVOID param)
    {
        PingClientWithAPI * ping_client_with_api_ = (PingClientWithAPI *)param;

        DWORD dwRetVal = 0;

        dwRetVal = IcmpSendEcho2(
            ping_client_with_api_->hIcmpFile_,
            NULL,
            NULL,
            NULL,
            ping_client_with_api_->destination_ip_,
            (LPVOID)(ping_client_with_api_->ping_body_.c_str()),
            ping_client_with_api_->ping_body_.length(),
            &(ping_client_with_api_->ip_option_),
            ping_client_with_api_->reply_buffer_,
            ping_client_with_api_->reply_buffer_size_,
            1000
            );

        if (dwRetVal != 0) 
        {
            assert(dwRetVal == 1);

            PICMP_ECHO_REPLY pEchoReply = (PICMP_ECHO_REPLY) ping_client_with_api_->reply_buffer_;
            struct in_addr ReplyAddr;
            ReplyAddr.S_un.S_addr = pEchoReply->Address;

            if (ping_client_with_api_->ip_option_.Ttl != 255)
            {
                assert(pEchoReply->Status == IP_TTL_EXPIRED_TRANSIT);
            }

            global_io_svc().post(boost::bind(&PingClientWithAPI::NotifyHandler, ping_client_with_api_,
                ping_client_with_api_->sequence_num_,
                ping_client_with_api_->ip_option_.Ttl == 255 ? IP_SUCCESS : icmp_header::time_exceeded,
                string(inet_ntoa(ReplyAddr)))
                );
        }
        else 
        {
            DWORD dwError = 0;
            dwError = GetLastError();
            switch (dwError)
            {
            case IP_BUF_TOO_SMALL:
                assert(false);
                break;
            case IP_REQ_TIMED_OUT:
                global_io_svc().post(boost::bind(&PingClientWithAPI::Cancel, ping_client_with_api_,
                    ping_client_with_api_->sequence_num_));
                break;
            default:
                DebugLog("\tExtended error returned: %ld\n", dwError);
                break;
            }
        }
    }

    bool PingClientWithAPI::Bind(const string & destination_ip)
    {
        destination_ip_ = inet_addr(destination_ip.c_str());

        if (destination_ip_ == INADDR_NONE) 
        {
            return false;
        }

        return true;
    }

    bool PingClientWithAPI::SetTtl(int32_t ttl)
    {
        ip_option_.Ttl = ttl;
        return true;
    }
}

#endif
