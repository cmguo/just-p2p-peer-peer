//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/p2p/Exchanger.h"
#include "p2sp/p2p/IpPool.h"
#include "p2sp/p2p/P2SPConfigs.h"

#define P2P_DEBUG(s) LOG(__DEBUG, "P2P", s)
#define P2P_INFO(s)    LOG(__INFO, "P2P", s)
#define P2P_EVENT(s) LOG(__EVENT, "P2P", s)
#define P2P_WARN(s)    LOG(__WARN, "P2P", s)
#define P2P_ERROR(s) LOG(__ERROR, "P2P", s)

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("p2p");

    void Exchanger::Start()
    {
        if (is_running_ == true) return;

        P2P_EVENT("Exchanger Start");

        is_running_ = true;
    }

    void Exchanger::Stop()
    {
        if (is_running_ == false) return;

        P2P_EVENT("Exchanger Stop");

        if (p2p_downloader_) { p2p_downloader_.reset(); }
        if (ip_pool_) { ip_pool_.reset(); }

        is_running_ = false;
    }

    void Exchanger::OnP2PTimer(uint32_t times)
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

        P2P_EVENT("Exchanger::DoPeerExchange " << candidate_peerinfo);

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
        ip_pool_->AddCandidatePeers(packet.peer_info_, is_live_ && !packet.IsRequest());

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