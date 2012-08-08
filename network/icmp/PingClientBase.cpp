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
        try
        {
            return PingClient::Create(io_svc);
        }
        catch(boost::system::system_error & e)
        {
            // 目前已经的问题是在win 7系统，由于权限不够，创建raw socket会异常
            DebugLog("upload create ping client failed ec:%d, %s\n", e.code().value(), e.what());
#ifdef BOOST_WINDOWS_API && !(defined __MINGW32__)
            return PingClientWithAPI::Create();
#else
            return PingClient::p();
#endif
        }
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
