#ifndef PING_CLIENT_BASE_H
#define PING_CLIENT_BASE_H

namespace network
{
    class PingClientBase
    {
    public:
        typedef boost::shared_ptr<PingClientBase> p;

        virtual uint16_t AsyncRequest(boost::function<void(unsigned char, string)> handler) = 0;

        virtual void Bind(const string & destination_ip) = 0;

        virtual bool SetTtl(int32_t ttl) = 0;

        virtual ~PingClientBase() {}

    public:
        static p create(boost::asio::io_service & io_svc);

        void AddHandler(uint16_t sequence_num, boost::function<void(unsigned char, string)> handler);
        void NotifyHandler(uint16_t sequence_num, unsigned char type, string & ip);
        void Cancel(uint16_t sequence_num);
        void CancelAll();

    private:
        std::map<uint16_t, boost::function<void (unsigned char, string)> > handler_map_;
    };
}

#endif
