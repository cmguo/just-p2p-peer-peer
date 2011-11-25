#include "Common.h"
#include "VodUploadManager.h"
#include "p2sp/AppModule.h"
#include "p2sp/p2p/P2PDownloader.h"
#include "p2sp/proxy/ProxyModule.h"
#include "p2sp/p2p/UploadCacheModule.h"
#include "storage/Storage.h"

namespace p2sp
{
    const size_t VodUploadManager::DesirableUploadSpeedPerPeerInKBps = 8;

    bool VodUploadManager::TryHandlePacket(const protocol::Packet & packet)
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
        case protocol::RequestAnnouncePacket::Action:
            OnRequestAnnouncePacket((const protocol::RequestAnnouncePacket &)packet);
            break;
        case protocol::RequestSubPiecePacket::Action:
            OnRequestSubPiecePacket((const protocol::RequestSubPiecePacket &)packet);
            break;
        case protocol::RIDInfoRequestPacket::Action:
            OnRIDInfoRequestPacket((const protocol::RIDInfoRequestPacket &)packet);
            break;
        case protocol::ReportSpeedPacket::Action:
            OnReportSpeedPacket((const protocol::ReportSpeedPacket &)packet);
            break;
        }
        return true;
    }

    void VodUploadManager::AdjustConnections()
    {
        bool is_watching_live_by_peer = p2sp::ProxyModule::Inst()->IsWatchingLive();

        if (true == is_watching_live_by_peer)
        {
            connections_management_.KickAllConnections();
        }
        else
        {
            const size_t NewPeerProtectionTimeInSeconds = 20;
            connections_management_.KickBadConnections(DesirableUploadSpeedPerPeerInKBps, NewPeerProtectionTimeInSeconds);
        }

        connections_management_.KickTimedOutConnections();
    }

    void VodUploadManager::OnConnectPacket(const protocol::ConnectPacket & packet)
    {
        // 检查 Storage 中是否存在 该 RID 对应的 Instance
        // IIinstace::p = Storage::Inst()->GetInstance(rid)
        // if (!p)
        // {
        //    这种情况就是 不存在 该 RID 对应的 Instance
        //    不存在的 返回一个 Error 报文 ConnectRequestFailed Becuase RID Not Exist
        //    return;
        // }
        // 否则
        //    合成一个 Connect(Response) 然后返回

        // 正在看直播或者是连接已经满了，则回错误报文
        
        if ((true == p2sp::ProxyModule::Inst()->IsWatchingLive()) || 
            (!connections_management_.IsPeerConnected(packet.end_point) && 
             !connections_management_.AcceptsNewConnection(packet.ip_pool_size_)))
        {
            DebugLog("UPLOAD-vod: a new connection is refused as we have accepted enough connections.");
            SendErrorPacket(packet, protocol::ErrorPacket::PPV_CONNECT_CONNECTION_FULL);
            return;
        }

        storage::Instance::p inst = boost::dynamic_pointer_cast<storage::Instance>(
            storage::Storage::Inst()->GetInstanceByRID(packet.resource_id_));

        if (!inst)
        {
            DebugLog("UPLOAD-vod: a new connection is refused as we don't have the requested resource.");
            SendErrorPacket(packet, protocol::ErrorPacket::PPV_CONNECT_NO_RESOURCEID);
        }
        else
        {
            // ReConnect
            protocol::ConnectPacket connect_packet(packet.transaction_id_, inst->GetRID(), 
                AppModule::Inst()->GetPeerGuid(), protocol::PEER_VERSION, 0x01, packet.send_off_time_,
                AppModule::Inst()->GetPeerVersion(), AppModule::Inst()->GetCandidatePeerInfo(),
                protocol::CONNECT_VOD,
                AppModule::Inst()->GetPeerDownloadInfo(),  // global download info
                packet.end_point);

            AppModule::Inst()->DoSendPacket(connect_packet, packet.protocol_version_);
            AppModule::Inst()->DoSendPacket(connect_packet, packet.protocol_version_);

            if (!connections_management_.IsPeerConnected(packet.end_point))
            {
                DebugLog("UPLOAD-vod: accepting a new connection.");
                connections_management_.AddConnection(packet.end_point);
            }

            DebugLog("UPLOAD-vod: updating connection heartbeat.");
            PEER_UPLOAD_INFO& peer_upload_info = connections_management_.GetPeerUploadInfo(packet.end_point);
            peer_upload_info.last_talk_time.reset();
            peer_upload_info.ip_pool_size = packet.ip_pool_size_;
            peer_upload_info.peer_guid = packet.peer_guid_;
            peer_upload_info.is_open_service = inst->IsOpenService();
            peer_upload_info.resource_id = packet.resource_id_;
            peer_upload_info.peer_info = packet.peer_info_;
            // 标识该连接为点播的连接
            peer_upload_info.is_live = false;
            peer_upload_info.connected_time.reset();

            // IncomingPeersCount
            statistic::StatisticModule::Inst()->SubmitIncomingPeer();

            // response announce
            SendAnnouncePacket(packet, inst);

            // response ridinfo
            SendRIDInfoPacket(packet, inst);
        }
    }

    void VodUploadManager::OnRequestAnnouncePacket(const protocol::RequestAnnouncePacket & packet)
    {
        if (!connections_management_.IsPeerConnected(packet.end_point))
        {
            SendErrorPacket(packet, protocol::ErrorPacket::PPV_ANNOUCE_NO_RESOURCEID);
            return;
        }

        connections_management_.UpdateConnectionHeartbeat(packet.end_point);

        storage::Instance::p inst = boost::dynamic_pointer_cast<storage::Instance>(storage::Storage::Inst()->GetInstanceByRID(packet.resource_id_));
        if (!inst)
        {
            SendErrorPacket(packet, protocol::ErrorPacket::PPV_ANNOUCE_NO_RESOURCEID);
        }
        else
        {
            SendAnnouncePacket(packet, inst);
        }
    }

    void VodUploadManager::OnRequestSubPiecePacket(const protocol::RequestSubPiecePacket & packet)
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
                DebugLog("UPLOAD-vod: rejecting a new upload connection.");
                SendErrorPacket(packet, protocol::ErrorPacket::PPV_CONNECT_CONNECTION_FULL);
                return;
            }

            DebugLog("UPLOAD-vod: accepting a new upload connection.");
            connections_management_.AddUploadConnection(packet.end_point);
        }

        connections_management_.UpdateUploadHeartbeat(packet.end_point);

        storage::Instance::p inst = boost::dynamic_pointer_cast<storage::Instance>(
            storage::Storage::Inst()->GetInstanceByRID(packet.resource_id_, false));

        if (!inst)
        {
            SendErrorPacket(packet, protocol::ErrorPacket::PPV_SUBPIECE_NO_RESOURCEID);
            return;
        }

        if (!connections_management_.TryAddUniqueTransactionId(packet.end_point, packet.transaction_id_))
        {
            //no-op. igoring duplicate requests
            return;
        }

        protocol::SubPieceInfo sub_piece_info;
        std::vector<protocol::SubPieceInfo> request_subpieces = packet.subpiece_infos_;
        for (uint32_t i = 0; i < request_subpieces.size(); i ++)
        {
            sub_piece_info = request_subpieces[i];
            if (sub_piece_info.GetPosition(inst->GetBlockSize()) > inst->GetFileLength())
            {
                assert(false);
                continue;
            }

            UploadCacheModule::Inst()->GetSubPieceForUpload(sub_piece_info, packet, inst, shared_from_this());
        }
    }

    void VodUploadManager::OnRIDInfoRequestPacket(const protocol::RIDInfoRequestPacket & packet)
    {
        if (!connections_management_.IsPeerConnected(packet.end_point))
        {
            SendErrorPacket(packet, protocol::ErrorPacket::PPV_RIDINFO_NO_RESOURCEID);
            return;
        }

        connections_management_.UpdateConnectionHeartbeat(packet.end_point);

        storage::Instance::p inst = boost::dynamic_pointer_cast<storage::Instance>(
            storage::Storage::Inst()->GetInstanceByRID(packet.resource_id_));

        if (!inst)
        {
            SendErrorPacket(packet, protocol::ErrorPacket::PPV_RIDINFO_NO_RESOURCEID);
        }
        else
        {
            SendRIDInfoPacket(packet, inst);
        }
    }

    void VodUploadManager::OnReportSpeedPacket(const protocol::ReportSpeedPacket & packet)
    {
        if (!connections_management_.IsPeerConnected(packet.end_point))
        {
            SendErrorPacket(packet, protocol::ErrorPacket::PPV_RIDINFO_NO_RESOURCEID);
            return;
        }

        connections_management_.UpdateConnectionHeartbeat(packet.end_point);

        if (connections_management_.IsPeerAcceptedForUpload(packet.end_point))
        {
            connections_management_.UpdatePeerUploadSpeed(packet.end_point, packet.speed_);
        }
    }

    void VodUploadManager::OnAsyncGetSubPieceSucced(const RID& rid, protocol::SubPieceInfo const& subpiece_info,
        const protocol::Packet & packet, const protocol::SubPieceBuffer & buffer)
    {
        storage::Storage::Inst_Storage()->UploadOneSubPiece(rid);

        protocol::SubPiecePacket subpiece_packet(packet.transaction_id_, rid, AppModule::Inst()->GetPeerGuid(),
            subpiece_info.GetSubPieceInfoStruct(), buffer.Length(), buffer, packet.end_point);

        bool ignoreUploadSpeedLimit = connections_management_.IsPeerFromSameSubnet(packet.end_point);

        upload_speed_limiter_->SendPacket(
            subpiece_packet,
            ignoreUploadSpeedLimit,
            ((const protocol::RequestSubPiecePacket &)packet).priority_,
            ((const protocol::RequestSubPiecePacket &)packet).protocol_version_);

        connections_management_.GetPeerUploadInfo(packet.end_point).speed_info.SubmitUploadedBytes(SUB_PIECE_SIZE);
    }

    void VodUploadManager::OnAsyncGetSubPieceFailed(const RID& rid, protocol::SubPieceInfo const& subpiece_info,
        const protocol::Packet & packet)
    {
        SendErrorPacket((const protocol::RequestSubPiecePacket&)packet, protocol::ErrorPacket::PPV_SUBPIECE_SUBPIECE_NOT_FOUND);
    }

    void VodUploadManager::SendAnnouncePacket(const protocol::CommonPeerPacket & packet, storage::Instance::p inst)
    {
        protocol::AnnouncePacket announce_packet(packet.transaction_id_, inst->GetRID(),
            AppModule::Inst()->GetPeerGuid(), AppModule::Inst()->GetPeerDownloadInfo(inst->GetRID()),
            *(inst->GetBlockMap()), packet.end_point);

        AppModule::Inst()->DoSendPacket(announce_packet, packet.protocol_version_);
    }

    void VodUploadManager::SendRIDInfoPacket(const protocol::CommonPeerPacket & packet, storage::Instance::p inst)
    {
        protocol::RidInfo rid_info;
        inst->GetRidInfo(rid_info);

        protocol::PEER_COUNT_INFO peer_count_info;
        P2PDownloader::p p2p_downloader = P2PModule::Inst()->GetP2PDownloader(rid_info.GetRID());
        if (p2p_downloader)
        {
            peer_count_info = p2p_downloader->GetPeerCountInfo();
        }

        protocol::RIDInfoResponsePacket ridinfo_packet (packet.transaction_id_, AppModule::Inst()->GetPeerGuid(),
            rid_info, peer_count_info, packet.end_point);

        AppModule::Inst()->DoSendPacket(ridinfo_packet, packet.protocol_version_);
    }

    bool VodUploadManager::IsValidPacket(const protocol::Packet & packet)
    {
        if (packet.PacketAction == protocol::RequestAnnouncePacket::Action ||
            packet.PacketAction == protocol::RequestSubPiecePacketOld::Action ||
            packet.PacketAction == protocol::RequestSubPiecePacket::Action ||
            packet.PacketAction == protocol::RIDInfoRequestPacket::Action ||
            packet.PacketAction == protocol::ReportSpeedPacket::Action)
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

            if (connect_packet.protocol_version_ < protocol::PEER_VERSION_V7)
            {
                return true;
            }
            else
            {
                if (protocol::CONNECT_VOD == connect_packet.connect_type_)
                {
                    return true;
                }
                else
                {
                    assert(connect_packet.connect_type_ == protocol::CONNECT_LIVE_PEER ||
                        connect_packet.connect_type_ == protocol::CONNECT_LIVE_UDPSERVER);
                }
            }
        }

        return false;
    }
}