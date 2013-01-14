#ifndef PING_CLIENT_BASE_H
#define PING_CLIENT_BASE_H

namespace network
{
    class PingClientBase
    {
    public:
        typedef boost::shared_ptr<PingClientBase> p;

        virtual boost::uint16_t AsyncRequest(boost::function<void(unsigned char, string, boost::uint32_t)> handler) = 0;

        virtual bool Bind(const string & destination_ip) = 0;

        virtual bool SetTtl(boost::int32_t ttl) = 0;

        virtual ~PingClientBase() {}

    public:
        static p create(boost::asio::io_service & io_svc);

        void AddHandler(boost::uint16_t sequence_num, boost::function<void(unsigned char, string, boost::uint32_t)> handler);

        void NotifyHandler(boost::uint16_t sequence_num, unsigned char type, const string & ip,
            boost::uint32_t ping_rtt_for_win7 = 65535);

        void Cancel(boost::uint16_t sequence_num);
        void CancelAll();

    private:
        std::map<boost::uint16_t, boost::function<void (unsigned char, string, boost::uint32_t)> > handler_map_;
    };
}

#endif
