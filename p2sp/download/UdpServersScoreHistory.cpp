#include "Common.h"
#include "UdpServersScoreHistory.h"

namespace p2sp
{
    UdpServerScore::UdpServerScore()
    {
        ip_ = 0;
        original_score_ = UdpServerScore::DefaultUdpServerScore;
        updated_ = false;
        accumulative_score_change_ = 0;
    }

    UdpServerScore::UdpServerScore(boost::uint32_t ip, boost::uint32_t score)
    {
        ip_ = ip;
        original_score_ = score;
        updated_ = false;
        accumulative_score_change_ = 0;
    }

    boost::uint32_t UdpServerScore::GetIp() const
    {
        return ip_;
    }

    boost::uint32_t UdpServerScore::CurrentScore() const
    {
        int delta = accumulative_score_change_;
        
        if (delta == 0 && !updated_ && original_score_ != UdpServerScore::DefaultUdpServerScore)
        {
            //目的是让那些不再活跃着的udpserver的得分有机会回归到缺省值，并最终从history中消失。
            delta = original_score_ > UdpServerScore::DefaultUdpServerScore ? -1 : 1;
        }

        if (delta > UdpServerScore::MaxScoreChange)
        {
            delta = UdpServerScore::MaxScoreChange;
        }

        if (delta < -1*UdpServerScore::MaxScoreChange)
        {
            delta = -1*UdpServerScore::MaxScoreChange;
        }

        int current_score = delta + original_score_;
        if (current_score > UdpServerScore::MaxUdpServerScore)
        {
            return UdpServerScore::MaxUdpServerScore;
        }

        if (current_score < 0)
        {
            return 0;
        }

        return current_score;
    }

    void UdpServerScore::UpdateServiceScore(int service_score)
    {
        updated_ = true;
        accumulative_score_change_ += service_score;
    }

    bool UdpServerScore::operator< (const UdpServerScore& rhs) const
    {
        if (updated_ != rhs.updated_)
        {
            return updated_;
        }

        if (CurrentScore() != rhs.CurrentScore())
        {
            return CurrentScore() > rhs.CurrentScore();
        }

        return ip_ < rhs.ip_;
    }

    bool UdpServersScoreHistory::Initialize(const std::vector<boost::uint32_t>& server_ips, const std::vector<boost::uint32_t>& scores)
    {
        udpservers_score_.clear();
        if (server_ips.size() != scores.size())
        {
            return false;
        }

        for(size_t i = 0; i < server_ips.size(); ++i)
        {
            udpservers_score_[server_ips[i]] = UdpServerScore(server_ips[i], scores[i]);
        }

        return true;
    }

    void UdpServersScoreHistory::UpdateServiceScore(const boost::asio::ip::udp::endpoint& udp_server, int service_score)
    {
        protocol::SocketAddr sock_addr(udp_server);
        boost::uint32_t server_ip = sock_addr.IP;
        if (udpservers_score_.find(server_ip) == udpservers_score_.end())
        {
            udpservers_score_.insert(
                std::make_pair(
                server_ip, 
                UdpServerScore(
                server_ip, 
                UdpServerScore::DefaultUdpServerScore)));
        }

        udpservers_score_[server_ip].UpdateServiceScore(service_score);
    }

    const std::map<boost::uint32_t, boost::uint32_t> UdpServersScoreHistory::GetServicesScore(size_t max_result) const
    {
        std::vector<UdpServerScore> scores;
        for(std::map<boost::uint32_t, UdpServerScore>::const_iterator iter = udpservers_score_.begin();
            iter != udpservers_score_.end();
            ++iter)
        {
            if (iter->second.CurrentScore() != UdpServerScore::DefaultUdpServerScore)
            {
                scores.push_back(iter->second);
            }
        }

        std::sort(scores.begin(), scores.end());

        std::map<boost::uint32_t, boost::uint32_t> result;
        for(size_t i = 0; i < scores.size(); ++i)
        {
            result[scores[i].GetIp()] = scores[i].CurrentScore();

            if (max_result > 0 && result.size() >= max_result)
            {
                break;
            }
        }

        return result;
    }
}