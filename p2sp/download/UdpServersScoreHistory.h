#ifndef _UDP_SERVERS_SCORE_HISTORY_H_
#define _UDP_SERVERS_SCORE_HISTORY_H_

namespace p2sp
{
    class UdpServerScore
    {
    private:
        boost::uint32_t ip_;
        boost::uint32_t original_score_;
        bool updated_;
        int accumulative_score_change_;

        const static int MaxScoreChange = 10;
        const static int MaxUdpServerScore = 500;
    public:
        const static int DefaultUdpServerScore = 100;

        UdpServerScore();
        UdpServerScore(boost::uint32_t ip, boost::uint32_t score);

        boost::uint32_t GetIp() const;
        boost::uint32_t CurrentScore() const;
        void UpdateServiceScore(int service_score);
        bool operator< (const UdpServerScore& rhs) const;
    };

    class UdpServersScoreHistory
    {
    private:
        std::map<boost::uint32_t, UdpServerScore> udpservers_score_;

    public:
        bool Initialize(const std::vector<boost::uint32_t>& server_ips, const std::vector<boost::uint32_t>& scores);
        void UpdateServiceScore(const boost::asio::ip::udp::endpoint& udp_server, int service_score);
        const std::map<boost::uint32_t, boost::uint32_t> GetServicesScore(size_t max_result) const;
    };
}

#endif //_UDP_SERVERS_SCORE_HISTORY_H_