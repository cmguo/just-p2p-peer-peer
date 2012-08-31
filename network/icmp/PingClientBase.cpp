#include "Common.h"
#include "PingClientBase.h"
#include "PingClient.h"
#ifdef BOOST_WINDOWS_API
#include "PingClientWithAPI.h"
#endif

namespace network
{
    typedef boost::shared_ptr<PingClientBase> p;
    p PingClientBase::create(boost::asio::io_service & io_svc)
    {
#ifdef BOOST_WINDOWS_API
        //已知在Win7下构造raw socket会抛出异常，或者构造后不能正常收到ICMP回包
        OSVERSIONINFO osi;
        ZeroMemory(&osi, sizeof(OSVERSIONINFO));
        osi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        GetVersionEx((OSVERSIONINFO*) &osi);

        //Win vista及以上操作系统
        if (osi.dwMajorVersion >= 6)
        {
            return PingClientWithAPI::Create();
        }
        else
        {
            return PingClient::Create(io_svc);
        }
#else
        return PingClient::Create(io_svc);
#endif
    }

    void PingClientBase::AddHandler(uint16_t sequence_num, 
        boost::function<void(unsigned char, string, boost::uint32_t)> handler)
    {
        assert(handler_map_.find(sequence_num) == handler_map_.end());
        handler_map_.insert(std::make_pair(sequence_num, handler));
    }

    void PingClientBase::NotifyHandler(uint16_t sequence_num, unsigned char type, const string & ip,
        boost::uint32_t ping_rtt_for_win7)
    {
        if (handler_map_.find(sequence_num) != handler_map_.end())
        {
            handler_map_[sequence_num](type, ip, ping_rtt_for_win7);
            handler_map_.erase(sequence_num);
        }
    }

    void PingClientBase::Cancel(uint16_t sequence_num)
    {
        if (handler_map_.find(sequence_num) != handler_map_.end())
        {
            handler_map_.erase(sequence_num);
        }
    }

    void PingClientBase::CancelAll()
    {
        handler_map_.clear();
    }
}
