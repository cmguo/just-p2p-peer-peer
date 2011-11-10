#include "Common.h"
#include "LiveUploadManager.h"
#include "storage/Storage.h"
#include "p2sp/AppModule.h"
#include "statistic/DACStatisticModule.h"
#include "p2sp/p2p/PeerHelper.h"

namespace p2sp
{
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
        default:
            assert(false);
        }

        return true;
    }

    void LiveUploadManager::AdjustConnections()
    {
        KickTimeoutConnection();
    }

    void LiveUploadManager::OnConnectPacket(const protocol::ConnectPacket & packet)
    {
        storage::LiveInstance::p live_inst =
            boost::dynamic_pointer_cast<storage::LiveInstance>(storage::Storage::Inst()->GetLiveInstanceByRid(packet.resource_id_));

        if (!live_inst)
        {
            SendErrorPacket((protocol::CommonPeerPacket const &)packet, protocol::ErrorPacket::PPV_CONNECT_NO_RESOURCEID);
        }
        else
        {
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
            if (accept_connecting_peers_.find(packet.end_point) == accept_connecting_peers_.end())
            {
                accept_connecting_peers_[packet.end_point] = PEER_UPLOAD_INFO();
            }

            PEER_UPLOAD_INFO& peer_upload_info = accept_connecting_peers_[packet.end_point];
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
    }

    void LiveUploadManager::OnLiveRequestAnnouncePacket(protocol::LiveRequestAnnouncePacket const & packet)
    {
        if (accept_connecting_peers_.find(packet.end_point) == accept_connecting_peers_.end())
        {
            SendErrorPacket(packet, protocol::ErrorPacket::PPV_ANNOUCE_NO_RESOURCEID);
            return;
        }

        accept_connecting_peers_[packet.end_point].last_talk_time.reset();

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
        if (accept_connecting_peers_.find(packet.end_point) == accept_connecting_peers_.end())
        {
            SendErrorPacket(packet, protocol::ErrorPacket::PPV_SUBPIECE_NO_RESOURCEID);
            return;
        }

        accept_connecting_peers_[packet.end_point].last_talk_time.reset();

        if (accept_uploading_peers_.find(packet.end_point) == accept_uploading_peers_.end())
        {
            accept_uploading_peers_.insert(packet.end_point);
            uploading_peers_speed_[packet.end_point] = std::make_pair(0, framework::timer::TickCounter());
        }

        accept_connecting_peers_[packet.end_point].last_data_time.reset();

        if (upload_speed_limiter_->GetSpeedLimitInKBps() == 0)
        {
            return;
        }

        storage::LiveInstance::p live_inst = boost::dynamic_pointer_cast<storage::LiveInstance>(storage::Storage::Inst()->GetLiveInstanceByRid(packet.resource_id_));
        if (!live_inst)
        {
            SendErrorPacket(packet, protocol::ErrorPacket::PPV_SUBPIECE_NO_RESOURCEID);
        }
        else if (!accept_connecting_peers_[packet.end_point].IsInLastDataTransIDs(packet.transaction_id_))
        {
            accept_connecting_peers_[packet.end_point].UpdateLastDataTransID(packet.transaction_id_);

            for (uint32_t i = 0; i < packet.sub_piece_infos_.size(); ++i)
            {
                const protocol::LiveSubPieceInfo &live_sub_piece_info = packet.sub_piece_infos_[i];

                if (live_inst->HasSubPiece(live_sub_piece_info))
                {
                    protocol::LiveSubPieceBuffer tmp_buf;

                    live_inst->GetSubPiece(live_sub_piece_info, tmp_buf);

                    protocol::LiveSubPiecePacket live_subpiece_packet(packet.transaction_id_, packet.resource_id_,
                        live_sub_piece_info, tmp_buf.Length(), tmp_buf, packet.end_point);

                    bool ignoreUploadSpeedLimit = IsPeerFromSameSubnet(packet.end_point);

                    upload_speed_limiter_->SendPacket(
                        live_subpiece_packet,
                        ignoreUploadSpeedLimit,
                        packet.priority_,
                        packet.protocol_version_);

                    statistic::DACStatisticModule::Inst()->SubmitLiveP2PUploadBytes(LIVE_SUB_PIECE_SIZE);

                    accept_connecting_peers_[packet.end_point].speed_info.SubmitUploadedBytes(LIVE_SUB_PIECE_SIZE);
                }
                else
                {
                    SendErrorPacket(packet, protocol::ErrorPacket::PPV_SUBPIECE_SUBPIECE_NOT_FOUND);
                }

            }
        }
    }

    bool LiveUploadManager::IsValidPacket(const protocol::Packet & packet)
    {
        if (packet.PacketAction == protocol::LiveRequestAnnouncePacket::Action ||
            packet.PacketAction == protocol::LiveRequestSubPiecePacket::Action)
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
}
