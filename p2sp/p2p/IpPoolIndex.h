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
        protocol::SocketAddr key_;
        uint32_t last_connect_time_;
        bool is_connecting_;
        bool is_connction_;

        uint32_t tracker_priority_;

        bool CanConnect() const
        {
            return is_connecting_ == false && is_connction_ == false;
        }

        ConnectIndicator(const protocol::SocketAddr& key, uint32_t last_connect_time, bool is_connecting, bool is_connction, boost::uint8_t tracker_priority)
            : key_(key), last_connect_time_(last_connect_time), is_connecting_(is_connecting), is_connction_(is_connction), tracker_priority_(tracker_priority)
        {
        }
    };

    /// 比较两个ConnectIndicator，用于构造std::map
    inline bool operator<(const ConnectIndicator& x, const ConnectIndicator& y)
    {
        if (x.CanConnect() != y.CanConnect())
        {
            if (x.tracker_priority_ != y.tracker_priority_) {
                return CompareIndicator((uint32_t)y.CanConnect(), x.tracker_priority_, (uint32_t)x.CanConnect(), y.tracker_priority_);
            }
            else {
                return CompareIndicator((uint32_t)y.CanConnect(), y.key_, (uint32_t)x.CanConnect(), x.key_);
            }
        }
        else
        {
            if (x.tracker_priority_ != y.tracker_priority_) {
                return CompareIndicator(x.last_connect_time_, x.tracker_priority_, y.last_connect_time_, y.tracker_priority_);
            }
            else {
                return CompareIndicator(x.last_connect_time_, x.key_, y.last_connect_time_, y.key_);
            }
        }

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
        static p create(const protocol::CandidatePeerInfo& peer) { return p(new CandidatePeer(peer)); }
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
        bool is_connction_;

        /// 连接包含时间
        uint32_t connect_protect_time_;
        uint32_t connect_protect_time_count_;
        uint32_t connect_protect_time_index_;

    public:
        // 属性
        protocol::SocketAddr GetKey() const
        {
            uint32_t local_detected_ip = AppModule::Inst()->GetCandidatePeerInfo().DetectIP;
            return this->GetKeySocketAddr(local_detected_ip);
        }
        ExchangeIndicator GetExchangeIndicator() const { return ExchangeIndicator(this->GetKey(), last_exchage_time_, exchange_times_); }
        ConnectIndicator GetConnectIndicator() const { return ConnectIndicator(this->GetKey(), last_connect_time_, is_connecting_, is_connction_, 255-TrackerPriority); }
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

        bool CanConnect() const
        {
            if (is_connction_ || is_connecting_) return false;
            if (last_connect_time_ == 0) return true;
            if (framework::timer::TickCounter::tick_count() - last_connect_time_ < connect_protect_time_)
                return false;
            return true;
        }
        // 操作
        void BeCopiedTo(const CandidatePeer::p peer)
        {
            assert(DetectIP == peer->DetectIP && DetectUdpPort == peer->DetectUdpPort);
            IP = peer->IP;
            PeerVersion = peer->PeerVersion;
            UdpPort = peer->UdpPort;
            StunIP = peer->StunIP;
            StunUdpPort = peer->StunUdpPort;
            last_active_time_ = framework::timer::TickCounter::tick_count();
            connect_protect_time_ = peer->connect_protect_time_;
            connect_protect_time_count_ = peer->connect_protect_time_count_;
            connect_protect_time_index_ = peer->connect_protect_time_index_;

            if (UploadPriority != 1)
            {
                UploadPriority = peer->UploadPriority;
            }
        }
    private:
        CandidatePeer(const protocol::CandidatePeerInfo& peer)
            : protocol::CandidatePeerInfo(peer)
            , last_active_time_(framework::timer::TickCounter::tick_count())
            , last_exchage_time_(0)
            , last_connect_time_(0)
            , exchange_times_(0)
            , is_connecting_(false)
            , is_connction_(false)
            , connect_protect_time_(P2SPConfigs::CONNECT_INITIAL_PROTECT_TIME_IN_MILLISEC)
            , connect_protect_time_count_(0), connect_protect_time_index_(0)
        {
        }
    };
}
#endif  // _P2SP_P2P_IPPOOLINDEX_H_
