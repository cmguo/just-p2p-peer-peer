#include "Common.h"
#include "LiveUploadManager.h"
#include "storage/Storage.h"
#include "p2sp/AppModule.h"
#include "statistic/DACStatisticModule.h"
#include "p2sp/p2p/PeerHelper.h"
#include "UploadModule.h"
#include "P2PModule.h"
#include "p2sp/proxy/ProxyModule.h"

namespace p2sp
{
    const size_t LiveUploadManager::DesirableUploadSpeedPerPeerInKBps = 2;

    bool LiveUploadManager::TryHandlePacket(const protocol::Packet & packet)
    {
        if (!IsValidPacket(packet))
        {
            return false;
        }

        switch (packet.PacketAction)
        {
        case protocol::ConnectPacket::Action:
            OnConnectPacket((const protocol::ConnectPacket &)packet);
            break;
        case protocol::LiveRequestAnnouncePacket::Action:
            OnLiveRequestAnnouncePacket((const protocol::LiveRequestAnnouncePacket &)packet);
            break;
        case protocol::LiveRequestSubPiecePacket::Action:
            OnLiveRequestSubPiecePacket((const protocol::LiveRequestSubPiecePacket &)packet);
            break;
        case protocol::PeerInfoPacket::Action:
            OnPeerInfoPacket((const protocol::PeerInfoPacket &)packet);
            break;
        default:
            assert(false);
        }

        return true;
    }

    void LiveUploadManager::AdjustConnections()
    {
        const size_t NewPeerProtectionTimeInSeconds = 20;
        connections_management_.KickBadConnections(DesirableUploadSpeedPerPeerInKBps, NewPeerProtectionTimeInSeconds);
        connections_management_.KickTimedOutConnections();
    }

    void LiveUploadManager::OnConnectPacket(const protocol::ConnectPacket & packet)
    {
        if (!connections_management_.IsPeerConnected(packet.end_point) && 
            !connections_management_.AcceptsNewConnection(packet.ip_pool_size_))
        {
            DebugLog("UPLOAD-live: a new connection is refused as we have accepted enough connections.");
            SendErrorPacket((protocol::CommonPeerPacket const &)packet, protocol::ErrorPacket::PPV_CONNECT_CONNECTION_FULL);
            return;
        }

        storage::LiveInstance::p live_inst =
            boost::dynamic_pointer_cast<storage::LiveInstance>(storage::Storage::Inst()->GetLiveInstanceByRid(packet.resource_id_));

        if (!live_inst)
        {
            DebugLog("UPLOAD-live: a new connection is rejected as we don't have the specified resource.");
            SendErrorPacket((protocol::CommonPeerPacket const &)packet, protocol::ErrorPacket::PPV_CONNECT_NO_RESOURCEID);
            return;
        }
        
        // ReConnect
        protocol::ConnectPacket connect_packet(packet.transaction_id_, live_inst->GetRID(),
            AppModule::Inst()->GetPeerGuid(),  protocol::PEER_VERSION, 0x01, packet.send_off_time_,
            AppModule::Inst()->GetPeerVersion(), AppModule::Inst()->GetCandidatePeerInfo(),
            protocol::CONNECT_LIVE_PEER,
            AppModule::Inst()->GetPeerDownloadInfo(), // global download info
            packet.end_point);

        AppModule::Inst()->DoSendPacket(connect_packet, packet.protocol_version_);
        AppModule::Inst()->DoSendPacket(connect_packet, packet.protocol_version_);

        // accept
        if (!connections_management_.IsPeerConnected(packet.end_point))
        {
            DebugLog("UPLOAD-live: accepting a new connection.");
            connections_management_.AddConnection(packet.end_point);
        }

        PEER_UPLOAD_INFO& peer_upload_info = connections_management_.GetPeerUploadInfo(packet.end_point);
        peer_upload_info.last_talk_time.reset();
        peer_upload_info.ip_pool_size = packet.ip_pool_size_;
        peer_upload_info.peer_guid = packet.peer_guid_;
        peer_upload_info.is_open_service = 0;
        peer_upload_info.resource_id = packet.resource_id_;
        peer_upload_info.peer_info = packet.peer_info_;
        peer_upload_info.is_live = true;  // 标识该连接为直播的连接
        peer_upload_info.connected_time.reset();

        // IncomingPeersCount
        statistic::StatisticModule::Inst()->SubmitIncomingPeer();
    }

    void LiveUploadManager::OnLiveRequestAnnouncePacket(protocol::LiveRequestAnnouncePacket const & packet)
    {
        if (!connections_management_.IsPeerConnected(packet.end_point))
        {
            SendErrorPacket(packet, protocol::ErrorPacket::PPV_ANNOUCE_NO_RESOURCEID);
            return;
        }

        connections_management_.UpdateConnectionHeartbeat(packet.end_point);

        storage::LiveInstance::p live_inst = boost::dynamic_pointer_cast<storage::LiveInstance>(storage::Storage::Inst()->GetLiveInstanceByRid(packet.resource_id_));
        if (!live_inst)
        {
            SendErrorPacket(packet, protocol::ErrorPacket::PPV_ANNOUCE_NO_RESOURCEID);
        }
        else
        {
            protocol::LiveAnnounceMap live_announce_map;

            live_inst->BuildAnnounceMap(packet.request_block_id_, live_announce_map);

            protocol::LiveAnnouncePacket live_announce_packet(protocol::Packet::NewTransactionID(), live_inst->GetRID(),
                live_announce_map, packet.end_point);

            AppModule::Inst()->DoSendPacket(live_announce_packet, packet.protocol_version_);
        }
    }

    void LiveUploadManager::OnLiveRequestSubPiecePacket(protocol::LiveRequestSubPiecePacket const & packet)
    {
        if (!connections_management_.IsPeerConnected(packet.end_point))
        {
            SendErrorPacket(packet, protocol::ErrorPacket::PPV_SUBPIECE_NO_RESOURCEID);
            return;
        }

        connections_management_.UpdateConnectionHeartbeat(packet.end_point);

        if (!connections_management_.IsPeerAcceptedForUpload(packet.end_point))
        {
            if (!connections_management_.AcceptsNewUploadConnection(packet.end_point))
            {
                DebugLog("UPLOAD-live: rejecting a new upload connection.");
                SendErrorPacket(packet, protocol::ErrorPacket::PPV_CONNECT_CONNECTION_FULL);
                return;
            }

            DebugLog("UPLOAD-live: accepting a new upload connection.");
            connections_management_.AddUploadConnection(packet.end_point);
        }

        connections_management_.UpdateUploadHeartbeat(packet.end_point);

        if (upload_speed_limiter_->GetSpeedLimitInKBps() == 0)
        {
            return;
        }

        storage::LiveInstance::p live_inst = boost::dynamic_pointer_cast<storage::LiveInstance>(storage::Storage::Inst()->GetLiveInstanceByRid(packet.resource_id_));
        if (!live_inst)
        {
            SendErrorPacket(packet, protocol::ErrorPacket::PPV_SUBPIECE_NO_RESOURCEID);
            return;
        }

        if (!connections_management_.TryAddUniqueTransactionId(packet.end_point, packet.transaction_id_))
        {
            //no-op. ignoring duplicate requests
            return;
        }

        for (uint32_t i = 0; i < packet.sub_piece_infos_.size(); ++i)
        {
            const protocol::LiveSubPieceInfo &live_sub_piece_info = packet.sub_piece_infos_[i];

            if (live_inst->HasSubPiece(live_sub_piece_info))
            {
                protocol::LiveSubPieceBuffer tmp_buf;

                live_inst->GetSubPiece(live_sub_piece_info, tmp_buf);

                protocol::LiveSubPiecePacket live_subpiece_packet(packet.transaction_id_, packet.resource_id_,
                    live_sub_piece_info, tmp_buf.Length(), tmp_buf, packet.end_point);

                bool ignoreUploadSpeedLimit = connections_management_.IsPeerFromSameSubnet(packet.end_point);

                upload_speed_limiter_->SendPacket(
                    live_subpiece_packet,
                    ignoreUploadSpeedLimit,
                    packet.priority_,
                    packet.protocol_version_);

                statistic::DACStatisticModule::Inst()->SubmitLiveP2PUploadBytes(LIVE_SUB_PIECE_SIZE);

                connections_management_.GetPeerUploadInfo(packet.end_point).speed_info.SubmitUploadedBytes(LIVE_SUB_PIECE_SIZE);
            }
            else
            {
                SendErrorPacket(packet, protocol::ErrorPacket::PPV_SUBPIECE_SUBPIECE_NOT_FOUND);
            }
        }
    }

    bool LiveUploadManager::IsValidPacket(const protocol::Packet & packet)
    {
        if (packet.PacketAction == protocol::LiveRequestAnnouncePacket::Action ||
            packet.PacketAction == protocol::LiveRequestSubPiecePacket::Action ||
            packet.PacketAction == protocol::PeerInfoPacket::Action)
        {
            return true;
        }
        else if (packet.PacketAction == protocol::ConnectPacket::Action)
        {
            const protocol::ConnectPacket & connect_packet = (const protocol::ConnectPacket &)packet;

            if (!connect_packet.IsRequest())
            {
                return false;
            }

            if (connect_packet.protocol_version_ >= protocol::PEER_VERSION_V7)
            {
                if (protocol::CONNECT_LIVE_PEER == connect_packet.connect_type_)
                {
                    return true;
                }
            }
        }

        return false;
    }

    void LiveUploadManager::OnTimerElapsed(framework::timer::Timer * pointer)
    {
        if (pointer == &timer_)
        {
            SendPeerInfo();
        }
    }

    void LiveUploadManager::SendPeerInfo()
    {
        protocol::PeerInfo peer_info(P2PModule::Inst()->GetDownloadConnectedCount(),
            connections_management_.GetAcceptUploadingPeersCount(),
            statistic::UploadStatisticModule::Inst()->GetUploadSpeed(),
            UploadModule::Inst()->GetMaxUploadSpeedIncludeSameSubnet(),
            ProxyModule::Inst()->GetLiveRestPlayableTime(),
            ProxyModule::Inst()->GetLostRate(),
            ProxyModule::Inst()->GetRedundancyRate());

        protocol::PeerInfoPacket peer_info_packet(protocol::Packet::NewTransactionID(), protocol::PEER_VERSION, peer_info);

        std::set<boost::asio::ip::udp::endpoint> accept_uploading_peers;
        connections_management_.GetUploadingPeers(accept_uploading_peers);

        for (std::set<boost::asio::ip::udp::endpoint>::iterator iter = accept_uploading_peers.begin();
            iter != accept_uploading_peers.end(); ++iter)
        {
            peer_info_packet.end_point = *iter;
            AppModule::Inst()->DoSendPacket(peer_info_packet, protocol::PEER_VERSION);
        }
    }

    void LiveUploadManager::OnPeerInfoPacket(protocol::PeerInfoPacket const & packet)
    {
        statistic::PEER_INFO peer_info(packet.peer_info_.download_connected_count_, packet.peer_info_.upload_connected_count_,
            packet.peer_info_.upload_speed_, packet.peer_info_.max_upload_speed_, packet.peer_info_.rest_playable_time_,
            packet.peer_info_.lost_rate_, packet.peer_info_.redundancy_rate_);

        statistic::UploadStatisticModule::Inst()->SubmitUploadPeerInfo(packet.end_point.address(), peer_info);
    }

    void LiveUploadManager::OnConfigUpdated()
    {
        if (timer_->interval() != BootStrapGeneralConfig::Inst()->GetSendPeerInfoPacketIntervalInSecond())
        {
            timer_->interval(BootStrapGeneralConfig::Inst()->GetSendPeerInfoPacketIntervalInSecond());
        }
    }

    void LiveUploadManager::Start()
    {
        BootStrapGeneralConfig::Inst()->AddUpdateListener(shared_from_this());
        timer_->start();
    }

    void LiveUploadManager::Stop()
    {
        timer_->stop();
        BootStrapGeneralConfig::Inst()->RemoveUpdateListener(shared_from_this());
    }

    void LiveUploadManager::GetUploadingPeersExcludeSameSubnet(std::set<boost::asio::ip::address> & uploading_peers) const
    {
        connections_management_.GetUploadingPeersExcludeSameSubnet(uploading_peers);
    }
}
