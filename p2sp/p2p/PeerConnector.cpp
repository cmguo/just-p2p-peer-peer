//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/AppModule.h"
#include "p2sp/p2p/PeerConnector.h"
#include "p2sp/p2p/P2PDownloader.h"
#include "p2sp/p2p/LiveP2PDownloader.h"
#include "p2sp/p2p/PeerConnection.h"
#include "p2sp/p2p/IpPool.h"

#include <protocol/StunServerPacket.h>

namespace p2sp
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_peer_connector = log4cplus::Logger::getInstance("[peer_connector]");
#endif
    void PeerConnector::Start(boost::shared_ptr<IConnectTimeoutHandler> connect_timeout_handler)
    {
        if (is_running_ == true) return;
        LOG4CPLUS_INFO_LOG(logger_peer_connector, "PeerConnector::Start");

        is_running_ = true;

        connect_timeout_handler_ = connect_timeout_handler;

        assert(connecting_peers_.size() == 0);
    }

    void PeerConnector::Stop()
    {
        LOG4CPLUS_INFO_LOG(logger_peer_connector, "PeerConnector::STOP");
        if (is_running_ == false) return;

        // 清空 connecting_peers_
        connecting_peers_.clear();

        p2p_downloader_.reset();
        ippool_.reset();

        connect_timeout_handler_.reset();

        is_running_ = false;
    }

    void PeerConnector::OnP2PTimer(boost::uint32_t times)
    {
        if (is_running_ == false) return;

        if (times % 4 == 0)
        {    // 每秒检查一次连接超时
            CheckConnectTimeout();
        }
    }

    void PeerConnector::Connect(const protocol::CandidatePeerInfo& candidate_peer_info)
    {
        if (is_running_ == false) return;

        LOG4CPLUS_INFO_LOG(logger_peer_connector, "PeerConnector::Connect: UP = " << 
            (boost::uint32_t)candidate_peer_info.UploadPriority << ", " << candidate_peer_info);
        // 从 candidate_peer_info 中获得 EndPoint
        // b oost::asio::ip::udp::endpoint ep = candidate_peer_info.GetWanEndPoint();
        // 看 connecting_peers_ 里面是否已经存在这个Endpoint 了
        //     如果存在 先Log 再返回
        //
        // 看 是否已经存在这个连接了 p2p_downlaoder_->IsConnected(ep)
        //       如果存在 先Log 再返回
        //
        // 向这个  ep 发出 Connect 报文
        //
        // 用 protocol::CandidatePeerInfo 构造出 ConnectingPeer::p
        // 将 (ep, ConnectingPeer::p)  添加到 connecting_peers_ 中

        boost::uint32_t local_detected_ip = AppModule::Inst()->GetCandidatePeerInfo().DetectIP;
        boost::asio::ip::udp::endpoint end_point = candidate_peer_info.GetConnectEndPoint(local_detected_ip);
        boost::asio::ip::udp::endpoint end_point_detected = candidate_peer_info.GetConnectEndPoint(0);
    
        //这里对一个ip上有多个endpoint的，只会选择一个来连接，这是合理的。
        if (!FindConnectingPeerEndPointByIp(end_point))
        {
            boost::uint8_t connect_type;
            if (p2p_downloader_->IsLive())
            {
                connect_type = protocol::CONNECT_LIVE_PEER;
            }
            else
            {
                connect_type = protocol::CONNECT_VOD;
            }
            protocol::ConnectPacket packet(
                protocol::Packet::NewTransactionID(),
                p2p_downloader_->GetRid(),
                AppModule::Inst()->GetPeerGuid(),
                protocol::PEER_VERSION,
                0x00,
                framework::timer::TickCounter::tick_count(),
                AppModule::Inst()->GetPeerVersion(),
                AppModule::Inst()->GetCandidatePeerInfo(),
                connect_type,
                AppModule::Inst()->GetPeerDownloadInfo(p2p_downloader_->GetRid()),
                end_point,
                ippool_->GetPeerCount());

            AppModule::Inst()->DoSendPacket(packet, candidate_peer_info.PeerVersion);
            AppModule::Inst()->DoSendPacket(packet, candidate_peer_info.PeerVersion);

            LOG4CPLUS_DEBUG_LOG(logger_peer_connector, "P2PDownloader: " << p2p_downloader_ << ", Endpoint: " << 
                end_point << ", PeerInfo: " << candidate_peer_info);
            ConnectingPeer::p connecting_peer = ConnectingPeer::create(candidate_peer_info);
            connecting_peers_.insert(std::make_pair(end_point, connecting_peer));

            // StunInvoke
            if (candidate_peer_info.NeedStunInvoke(local_detected_ip))
            {
                boost::uint8_t connect_type;
                if (p2p_downloader_->IsLive())
                {
                    connect_type = protocol::CONNECT_LIVE_PEER;
                }
                else
                {
                    connect_type = protocol::CONNECT_VOD;
                }
                LOG4CPLUS_DEBUG_LOG(logger_peer_connector, "StunInvoke");
                protocol::StunInvokePacket stun_invoke_packet(
                    protocol::Packet::NewTransactionID(), 
                    p2p_downloader_->GetRid(), 
                    AppModule::Inst()->GetPeerGuid(),
                    framework::timer::TickCounter::tick_count(),
                    AppModule::Inst()->GetCandidatePeerInfo(),
                    connect_type,
                    candidate_peer_info,
                    AppModule::Inst()->GetPeerDownloadInfo(p2p_downloader_->GetRid()),
                    ippool_->GetPeerCount(),
                    framework::network::Endpoint(candidate_peer_info.StunIP, candidate_peer_info.StunUdpPort));

                AppModule::Inst()->DoSendPacket(stun_invoke_packet);
            }

            ippool_->OnConnect(end_point);

            //点播中对连接成功率进行统计
            if (!p2p_downloader_->IsLive())
            {
                 P2PDownloader__p p2p_downloader = boost::static_pointer_cast<P2PDownloader>(p2p_downloader_);
                 if (p2p_downloader && p2p_downloader->GetStatistic())
                 {
                     p2p_downloader->GetStatistic()->SubmitPeerConnectRequestCount(candidate_peer_info.PeerNatType);
                 }
            }
        }
        else
        {
            LOG4CPLUS_INFO_LOG(logger_peer_connector, "FindConnectingPeerEndPointByIp(end_point) exist " << 
                connecting_peers_.size());
        }
    }

    void PeerConnector::CheckConnectTimeout()
    {
        if (is_running_ == false) return;

        // 遍历 connecting_peers_ std::map
        // {
        //        如果 iter->ConnectingPeer::p 超时，则erase 这个iter,  将来可能还会把信息反馈给 IpPool
        //        // （将来如果有 穿越的，这个函数还要继续）
        // }

        std::map<boost::asio::ip::udp::endpoint, ConnectingPeer::p>::iterator iter;
        for (iter = connecting_peers_.begin(); iter != connecting_peers_.end();)
        {
            ConnectingPeer::p peer = iter->second;
            if (peer->IsTimeOut())
            {
                LOG4CPLUS_DEBUG_LOG(logger_peer_connector, "Connect Timeout, P2PDownloader: " << shared_from_this() 
                    << ", Endpoint: " << iter->first);
                ippool_->OnConnectTimeout(iter->first);
                if (connect_timeout_handler_)
                {
                    connect_timeout_handler_->OnConnectTimeout(iter->first);
                }

                connecting_peers_.erase(iter++);
            }
            else
            {
                iter++;
            }
        }
    }

    void PeerConnector::OnReConectPacket(protocol::ConnectPacket const & packet)
    {
        if (is_running_ == false) return;
        LOG4CPLUS_DEBUG_LOG(logger_peer_connector, "Endpoint = " << packet.end_point << ", AvgUpload: " 
            << packet.peer_download_info_.AvgUpload << ", NowUpload:" << packet.peer_download_info_.NowUpload);

        // 连接成功报文
        // assert(连接失败报文);
        //
        // 如果在 connecting_peers_ 里面如果找不到这个 end_point
        //    日志，不管，返回
        //
        // 日志，根据 ConnectPacket::p 构造出 PeerConnection::p 然后 p2p_downlaoder->AddPeer(PeerConnection::p)
        // 将来还会把信息反馈给 IpPool

        if (p2p_downloader_->GetConnectedPeersCount() > p2p_downloader_->GetMaxConnectCount())
        {
            LOG4CPLUS_DEBUG_LOG(logger_peer_connector, "Endpoint = " << packet.end_point << ", ConnectedCount = " 
                << p2p_downloader_->GetConnectedPeersCount() << " > " << p2p_downloader_->GetMaxConnectCount());
            return;
        }

        if (FindConnectingPeerEndPointByIp(packet.end_point) &&
            packet.peer_guid_ != AppModule::Inst()->GetPeerGuid())
        {
            if (!p2p_downloader_->IsLive())
            {
                // 点播
                // ! 屏蔽旧版Peer
                LOG4CPLUS_DEBUG_LOG(logger_peer_connector, " PeerEndpoint: " << packet.end_point << " Version: " 
                    << packet.peer_guid_);
                if (packet.peer_version_ <= 0x00000006)
                {
                    LOG4CPLUS_DEBUG_LOG(logger_peer_connector, "PeerVersion tooooooold: " << packet.peer_guid_);
                    return;
                }

                P2PDownloader__p p2p_downloader = boost::static_pointer_cast<P2PDownloader>(p2p_downloader_);

                if (p2p_downloader->GetStatistic()->GetConnectionStatisticSize() >=
                    p2p_downloader->GetStatistic()->GetMaxP2PConnectionCount())
                {
                    return;
                }

                if (p2p_downloader->HasPeer(packet.end_point))
                {
                    return;
                }

                // 创建PeerConnection

                boost::intrusive_ptr<PeerConnection> connect_peer = new PeerConnection(p2p_downloader, packet.end_point);

                ConnectingPeer::p connecting_peer = GetConnectingPeerByIp(packet.end_point);

                protocol::CandidatePeerInfo info = (protocol::CandidatePeerInfo)packet.peer_info_;
                if (connecting_peer)
                {
                    //对于symnat和ipport_restrict反连过来的情况，port可能变化了，所以按照socket里看到的为准。
                    connecting_peer->candidate_peer_info_.DetectUdpPort = packet.end_point.port();
                    info = connecting_peer->candidate_peer_info_;
                }
                else
                {   
                    info.DetectUdpPort = packet.end_point.port();
                }
               

                connect_peer->Start(packet, packet.end_point, info);
                p2p_downloader->AddPeer(connect_peer);

                if (p2p_downloader->GetStatistic())
                {
                    p2p_downloader->GetStatistic()->SubmitPeerConnectSuccessCount(info.PeerNatType);
                }
            }
            else
            {
                // 直播
                LiveP2PDownloader__p p2p_downloader = boost::static_pointer_cast<LiveP2PDownloader>(p2p_downloader_);

                if (p2p_downloader->HasPeer(packet.end_point))
                {
                    return;
                }

                // 创建LivePeerConnection
                LivePeerConnection::p connect_peer = LivePeerConnection::create(p2p_downloader, packet.connect_type_);

                ConnectingPeer::p connecting_peer = GetConnectingPeerByIp(packet.end_point);
                protocol::CandidatePeerInfo info = (protocol::CandidatePeerInfo)packet.peer_info_;
                if (connecting_peer)
                {
                    //对于symnat和ipport_restrict反连过来的情况，port可能变化了，所以按照socket里看到的为准。
                    connecting_peer->candidate_peer_info_.DetectUdpPort = packet.end_point.port();
                    info = connecting_peer->candidate_peer_info_;
                }
                else
                {   
                    info.DetectUdpPort = packet.end_point.port();
                }

                connect_peer->Start(packet, packet.end_point, info);

                p2p_downloader->AddPeer(connect_peer);
            }

            ippool_->OnConnectSucced(packet.end_point);
            EraseConnectingPeerByIp(packet.end_point);

            LOG4CPLUS_DEBUG_LOG(logger_peer_connector, "Peer Connected. P2PDownloader = " << p2p_downloader_ << 
                ", Endpoint = " << packet.end_point << ", PeerGuid = " << packet.peer_guid_);
        }
    }

   

    //查找是否有ip匹配的
    bool PeerConnector::FindConnectingPeerEndPointByIp(const boost::asio::ip::udp::endpoint& end_point)
    {
        if (is_running_ == false)
        {
            return false;
        }

        //传入一个port为0
        boost::asio::ip::udp::endpoint end_point2 =end_point;
        end_point2.port(0);

        //endpoint的比较是基于ip和port的，如果ip相同，那么port小的排在前面。所以lower_bound能找到和指定ip有相同ip的那个endpoint
        std::map<boost::asio::ip::udp::endpoint, ConnectingPeer::p>::const_iterator iter 
            = connecting_peers_.lower_bound(end_point2);

        if ( (iter!= connecting_peers_.end()) && (iter->first.address() == end_point2.address()))
        {
            if(iter->first.port() != end_point.port())
            {
                //走到这里，可能是因为对方是symnat或者ipport_retrict_nat
                LOG4CPLUS_INFO_LOG(logger_peer_connector, "FindConnectingPeerEndPointByIp, connecting peer info: " 
                    << iter->second->candidate_peer_info_<<" judge endpoint2:"<<end_point);
            }
            return true;
        }
        return false;
    }

    bool PeerConnector::EraseConnectingPeerByIp(const boost::asio::ip::udp::endpoint& end_point)
    {
        if (is_running_ == false) return false;

        boost::asio::ip::udp::endpoint end_point2 =end_point;
        end_point2.port(0);

        std::map<boost::asio::ip::udp::endpoint, ConnectingPeer::p>::iterator iter 
            = connecting_peers_.lower_bound(end_point2);

        if ( (iter!= connecting_peers_.end()) && (iter->first.address() == end_point2.address()))
        {
            //走到这里，说明ip是相同的，但是port不一定相同
            connecting_peers_.erase(iter);
            return true;
        }
        return false;

    }

    ConnectingPeer::p PeerConnector::GetConnectingPeerByIp(const boost::asio::ip::udp::endpoint& end_point)
    {
        if (false == is_running_) {
            return ConnectingPeer::p();
        }

        boost::asio::ip::udp::endpoint end_point2 =end_point;
        end_point2.port(0);

        std::map<boost::asio::ip::udp::endpoint, ConnectingPeer::p>::iterator iter 
            = connecting_peers_.lower_bound(end_point2);
        if ( (iter!= connecting_peers_.end()) && (iter->first.address() == end_point2.address()))
        {
            return iter->second;
        }
        return ConnectingPeer::p();
    }

    void PeerConnector::OnErrorPacket(protocol::ErrorPacket const & packet)
    {
        if (is_running_ == false) return;

        LOG4CPLUS_INFO_LOG(logger_peer_connector, "PeerConnector::OnErrorPacket " << packet.end_point);
        // 连接失败报文
        // assert(连接失败报文);
        //
        // 如果在 connecting_peers_ 里面如果找不到这个 end_point
        //    日志，不管，返回
        //
        // 日志，erase, 返回
        // 将来还会把信息反馈给 IpPool
        if (!FindConnectingPeerEndPointByIp(packet.end_point))
        {
            LOG4CPLUS_DEBUG_LOG(logger_peer_connector, "PeerNotInConnectingSet, P2PDownloader = " << p2p_downloader_ 
                << ", EndPoint = " << packet.end_point);
        }
        else
        {
            if (packet.error_code_ == protocol::ErrorPacket::PPV_CONNECT_NO_RESOURCEID)
            {
                LOG4CPLUS_DEBUG_LOG(logger_peer_connector, "ippool_->OnConnectFailed(), P2PDownloader = " 
                    << p2p_downloader_ << ", EndPoint = " << packet.end_point);
                ippool_->OnConnectFailed(packet.end_point);
            }
            else
            {
                LOG4CPLUS_DEBUG_LOG(logger_peer_connector, "ippool_->OnConnectTimeout(), P2PDownloader = " 
                    << p2p_downloader_ << ", EndPoint = " << packet.end_point);
                ippool_->OnConnectTimeout(packet.end_point);
            }
            EraseConnectingPeerByIp(packet.end_point);
        }
    }
}
