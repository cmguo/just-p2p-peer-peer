//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// IpPoolIndex.h

#ifndef _P2SP_P2P_IPPOOLINDEX_H_
#define _P2SP_P2P_IPPOOLINDEX_H_

#include "p2sp/AppModule.h"
#include "p2sp/p2p/P2SPConfigs.h"

namespace p2sp
{
    class CandidatePeer;

    template <typename T1, typename T2>
    inline bool CompareIndicator(const T1& x, const T2& keyX, const T1& y, const T2& keyY)
    {
        if (x == y) return keyX < keyY;
        return x < y;
    }

    /// 洪泛相关的索引键
    struct ExchangeIndicator
    {
        protocol::SocketAddr key_;
        uint32_t last_exchage_time_;
        uint32_t exchange_times_;

        ExchangeIndicator(const protocol::SocketAddr& key, uint32_t last_exchage_time, uint32_t exchange_times)
            : key_(key), last_exchage_time_(last_exchage_time), exchange_times_(exchange_times)
        {
        }
    };

    inline bool operator<(const ExchangeIndicator& x, const ExchangeIndicator& y)
    {
        return CompareIndicator(x.last_exchage_time_, x.key_, y.last_exchage_time_, y.key_);
    }

    /// 连接相关的索引键
    struct ConnectIndicator
    {
        std::string key_;
        uint32_t next_time_to_connect_;
        bool is_connecting_;
        bool is_connected_;

        uint32_t tracker_priority_;

        uint32_t last_active_time_;

        bool should_use_firstly_;

        size_t peer_score_;

        bool is_udpserver_from_cdn_;

        // 只是检测了是不是正在连接或者是已经连接上了
        bool CanConnect() const
        {
            return is_connecting_ == false && is_connected_ == false;
        }

        ConnectIndicator(const CandidatePeer & candidate_peer);
    };

    /// 比较两个ConnectIndicator，用于构造std::map
    inline bool operator<(const ConnectIndicator& x, const ConnectIndicator& y)
    {
        if (x.CanConnect() != y.CanConnect())
        {
            return x.CanConnect();
        }

        if (x.is_udpserver_from_cdn_ != y.is_udpserver_from_cdn_)
        {
            return x.is_udpserver_from_cdn_;
        }

        if (x.next_time_to_connect_ != y.next_time_to_connect_)
        {
            return x.next_time_to_connect_ < y.next_time_to_connect_;
        }

        if (x.should_use_firstly_ != y.should_use_firstly_)
        {
            return x.should_use_firstly_ > y.should_use_firstly_;
        }

        if (x.tracker_priority_ != y.tracker_priority_)
        {
            return x.tracker_priority_ < y.tracker_priority_;
        }

        if (x.last_active_time_ != y.last_active_time_)
        {
            return x.last_active_time_ > y.last_active_time_;
        }

        if (x.peer_score_ != y.peer_score_)
        {
            return x.peer_score_ > y.peer_score_;
        }

        return x.key_.compare(y.key_) > 0;
    }

    /// 活跃相关的索引键
    struct ActiveIndicator
    {
        protocol::SocketAddr key_;
        uint32_t active_time_;

        ActiveIndicator(const protocol::SocketAddr& key, uint32_t active_time)
            : key_(key), active_time_(active_time)
        {
        }

    };

    inline bool operator<(const ActiveIndicator& x, const ActiveIndicator& y)
    {
        // return x.active_time_ < y.active_time_;
        return CompareIndicator(x.active_time_, x.key_, y.active_time_, y.key_);
    }

    /// peer地址信息项
    class CandidatePeer : public protocol::CandidatePeerInfo
    {
    public:
        typedef boost::shared_ptr<CandidatePeer> p;
        static p create(const protocol::CandidatePeerInfo& peer, bool should_use_firstly, size_t peer_score, bool is_udpserver_from_cdn)
        {
            return p(new CandidatePeer(peer, should_use_firstly, peer_score, is_udpserver_from_cdn));
        }
    public:
        /// 上一次活跃时间
        uint32_t last_active_time_;

        /// 上一次洪范的时间
        uint32_t last_exchage_time_;

        /// 发出洪范的次数
        uint32_t exchange_times_;

        /// 上一次 发出连接 的时间
        boost::uint32_t last_connect_time_;

        /// 是否连接中
        bool is_connecting_;

        /// 是否正在连接中
        bool is_connected_;

        /// 连接包含时间
        uint32_t connect_protect_time_;
        uint32_t connect_protect_time_count_;
        uint32_t connect_protect_time_index_;

        size_t connections_attempted_;

        bool should_use_firstly_;

        size_t peer_score_;
        bool is_udpserver_from_cdn_;

    public:
        // 属性
        protocol::SocketAddr GetKey() const
        {
            uint32_t local_detected_ip = AppModule::Inst()->GetCandidatePeerInfo().DetectIP;
            return this->GetKeySocketAddr(local_detected_ip);
        }
        ExchangeIndicator GetExchangeIndicator() const { return ExchangeIndicator(this->GetKey(), last_exchage_time_, exchange_times_); }
        ConnectIndicator GetConnectIndicator() const { return ConnectIndicator(*this); }
        ActiveIndicator GetActiveIndicator() const { return ActiveIndicator(this->GetKey(), last_active_time_); }

        protocol::CandidatePeerInfo GetCandidatePeerInfo() const
        {
            // protocol::CandidatePeerInfo candidate_peer_info(IP, UdpPort, TcpPort, DetectIP, DetectUdpPort, StunIP, StunUdpPort);
            return (protocol::CandidatePeerInfo)(*this);
        }

        void OnConnectFailed()
        {
            connect_protect_time_ = P2SPConfigs::CONNECT_FINAL_PROTECT_TIME_IN_MILLISEC;
        }

        void OnConnectTimeout()
        {
            if (connect_protect_time_ < P2SPConfigs::CONNECT_FINAL_PROTECT_TIME_IN_MILLISEC)
            {
                if (connect_protect_time_count_ == 0) {
                    connect_protect_time_ *= 2;
                    connect_protect_time_count_ = ++connect_protect_time_index_;
                }
                else {
                    --connect_protect_time_count_;
                }
                // check
                if (connect_protect_time_ >= P2SPConfigs::CONNECT_FINAL_PROTECT_TIME_IN_MILLISEC)
                {
                    connect_protect_time_ = P2SPConfigs::CONNECT_FINAL_PROTECT_TIME_IN_MILLISEC;
                }
            }
        }

        void OnConnectSucceed()
        {
            connect_protect_time_ = P2SPConfigs::CONNECT_INITIAL_PROTECT_TIME_IN_MILLISEC;
        }

        bool CanExchange() const
        {
            if (last_exchage_time_ == 0) return true;
            if (framework::timer::TickCounter::tick_count() - (last_exchage_time_) < P2SPConfigs::EXCHANGE_PROTECT_TIME_IN_MILLISEC)
                return false;
            return true;
        }

        // 不光检测了是不是已经连接上(is_connction_)，是不是正在连(is_connecting_)
        // 还检测了是否处于连接的保护时间内
        bool CanConnect(bool is_udpserver = false) const
        {
            if (is_connected_ || is_connecting_)
            {
                return false;
            }
            if (last_connect_time_ == 0)
            {
                return true;
            }
            if (framework::timer::TickCounter::tick_count() - last_connect_time_ < connect_protect_time_)
            {
                if (is_udpserver)
                {
                    return true;
                }
                return false;
            }
            return true;
        }
        // 操作
        void BeCopiedTo(const CandidatePeer::p peer)
        {
            assert(DetectIP == peer->DetectIP);
            DetectUdpPort = peer->DetectUdpPort;
            IP = peer->IP;
            PeerVersion = peer->PeerVersion;
            UdpPort = peer->UdpPort;
            StunIP = peer->StunIP;
            StunUdpPort = peer->StunUdpPort;
            last_active_time_ = framework::timer::TickCounter::tick_count();
            connect_protect_time_ = peer->connect_protect_time_;
            connect_protect_time_count_ = peer->connect_protect_time_count_;
            connect_protect_time_index_ = peer->connect_protect_time_index_;
            connections_attempted_ = peer->connections_attempted_;
            should_use_firstly_ = peer->should_use_firstly_;
            peer_score_ = peer->peer_score_;

            if (UploadPriority != 1)
            {
                UploadPriority = peer->UploadPriority;
            }
        }

    private:
        CandidatePeer(const protocol::CandidatePeerInfo& peer, bool should_use_firstly, size_t peer_score, bool is_udpserver_from_cdn)
            : protocol::CandidatePeerInfo(peer)
            , last_active_time_(framework::timer::TickCounter::tick_count())
            , last_exchage_time_(0)
            , last_connect_time_(0)
            , exchange_times_(0)
            , is_connecting_(false)
            , is_connected_(false)
            , connect_protect_time_(P2SPConfigs::CONNECT_INITIAL_PROTECT_TIME_IN_MILLISEC)
            , connect_protect_time_count_(0), connect_protect_time_index_(0)
            , connections_attempted_(0)
            , should_use_firstly_(should_use_firstly)
            , peer_score_(peer_score)
            , is_udpserver_from_cdn_(is_udpserver_from_cdn)
        {
        }
    };
}
#endif  // _P2SP_P2P_IPPOOLINDEX_H_
