//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/p2p/Exchanger.h"
#include "p2sp/p2p/IpPool.h"
#include "p2sp/p2p/P2SPConfigs.h"

namespace p2sp
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_exchanger = log4cplus::Logger::getInstance("[exchanger]");
#endif

    void Exchanger::Start()
    {
        if (is_running_ == true) return;

        LOG4CPLUS_INFO_LOG(logger_exchanger, "Exchanger Start");

        is_running_ = true;
    }

    void Exchanger::Stop()
    {
        if (is_running_ == false) return;

        LOG4CPLUS_INFO_LOG(logger_exchanger, "Exchanger Stop");

        if (p2p_downloader_) { p2p_downloader_.reset(); }
        if (ip_pool_) { ip_pool_.reset(); }

        is_running_ = false;
    }

    void Exchanger::OnP2PTimer(boost::uint32_t times)
    {
        if (is_running_ == false)
        {
            return;
        }

        // 30秒进行一次Exchange
        if (times % 120 == 0)
        {
            if (ip_pool_->GetPeerCount() < 300)
            {
                protocol::CandidatePeerInfo candidate_peer_info;
                if (ip_pool_->GetForExchange(candidate_peer_info))
                {
                    DoPeerExchange(candidate_peer_info);
                }
            }
        }
    }

    void Exchanger::DoPeerExchange(protocol::CandidatePeerInfo candidate_peerinfo) const
    {
        if (is_running_ == false) return;

        LOG4CPLUS_INFO_LOG(logger_exchanger, "Exchanger::DoPeerExchange " << candidate_peerinfo);

        std::vector<protocol::CandidatePeerInfo> candidate_peers;
        p2p_downloader_->GetCandidatePeerInfos(candidate_peers);
        // 对该 endpoint 发出 PeerExchange 报文
        boost::uint32_t local_detected_ip = AppModule::Inst()->GetCandidatePeerInfo().DetectIP;
        protocol::PeerExchangePacket packet(protocol::Packet::NewTransactionID(),
            p2p_downloader_->GetRid(), AppModule::Inst()->GetPeerGuid(), 0, candidate_peers,
            candidate_peerinfo.GetConnectEndPoint(local_detected_ip));

        AppModule::Inst()->DoSendPacket(packet, candidate_peerinfo.PeerVersion);
    }

    void Exchanger::OnPeerExchangePacket(const protocol::PeerExchangePacket & packet)
    {
        ip_pool_->AddCandidatePeers(packet.peer_info_, is_live_ && !packet.IsRequest(), false);

        if (packet.IsRequest())
        {
            std::vector<protocol::CandidatePeerInfo> candidate_peers;
            p2p_downloader_->GetCandidatePeerInfos(candidate_peers);

            protocol::PeerExchangePacket peer_exchange_packet_(
                packet.transaction_id_,
                p2p_downloader_->GetRid(),
                AppModule::Inst()->GetPeerGuid(),
                1,
                candidate_peers,
                packet.end_point);

            AppModule::Inst()->DoSendPacket(peer_exchange_packet_, packet.protocol_version_);
        }
    }
}