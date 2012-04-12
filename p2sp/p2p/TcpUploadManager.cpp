#include "Common.h"
#include "TcpUploadManager.h"
#include "storage/Storage.h"
#include "p2sp/p2p/UploadCacheModule.h"
#include "storage/Instance.h"
#include "network/tcp/TcpConnection.h"

namespace p2sp
{
    bool TcpUploadManager::TryHandlePacket(const protocol::Packet & packet)
    {
        if (!IsValidPacket(packet))
        {
            return false;
        }

        assert(((const protocol::TcpCommonPacket & )packet).tcp_connection_);

        boost::shared_ptr<network::TcpConnection> tcp_connection = 
            ((protocol::TcpCommonPacket&)packet).tcp_connection_;

        if (tcp_speed_map_.find(tcp_connection) == tcp_speed_map_.end())
        {
            tcp_speed_map_.insert(std::make_pair(tcp_connection, 0));
        }

        switch (packet.PacketAction)
        {
        case protocol::TcpAnnounceRequestPacket::Action:
            OnTcpAnnounceRequestPacket((const protocol::TcpAnnounceRequestPacket &)packet);
            break;
        case protocol::TcpSubPieceRequestPacket::Action:
            OnTcpSubPieceRequestPacket((const protocol::TcpSubPieceRequestPacket &)packet);
            break;
        case protocol::TcpReportSpeedPacket::Action:
            OnTcpReportSpeedPacket((const protocol::TcpReportSpeedPacket &)packet);
            break;
        case protocol::TcpClosePacket::Action:
            OnTcpConnectionClose((const protocol::TcpClosePacket&)packet);
            break;
        default:
            assert(false);
        }

        return true;
    }

    void TcpUploadManager::AdjustConnections()
    {

    }

    boost::uint32_t TcpUploadManager::MeasureCurrentUploadSpeed() const
    {
        boost::uint32_t total_speed = 0;
        for (std::map<boost::shared_ptr<network::TcpConnection>, boost::uint32_t>::const_iterator
            iter = tcp_speed_map_.begin(); iter != tcp_speed_map_.end(); ++iter)
        {
            total_speed += iter->second;
        }
        return total_speed;
    }

    bool TcpUploadManager::IsValidPacket(const protocol::Packet & packet)
    {
        if (packet.PacketAction == protocol::TcpAnnounceRequestPacket::Action ||
            packet.PacketAction == protocol::TcpSubPieceRequestPacket::Action ||
            packet.PacketAction == protocol::TcpReportSpeedPacket::Action ||
            packet.PacketAction == protocol::TcpClosePacket::Action)
        {
            return true;
        }

        return false;
    }

    void TcpUploadManager::GetUploadingPeers(std::set<boost::asio::ip::address> & uploading_peers)
    {
        for (std::map<boost::shared_ptr<network::TcpConnection>, boost::uint32_t>::const_iterator
            iter = tcp_speed_map_.begin(); iter != tcp_speed_map_.end(); ++iter)
        {
            boost::system::error_code ec;
            boost::asio::ip::tcp::endpoint remote_endpoint = iter->first->socket().remote_endpoint(ec);

            if (ec)
            {
                return;
            }

            uploading_peers.insert(remote_endpoint.address());
        }
    }

    void TcpUploadManager::OnTcpAnnounceRequestPacket(const protocol::TcpAnnounceRequestPacket & packet)
    {
        storage::Instance::p inst = boost::static_pointer_cast<storage::Instance>(storage::Storage::Inst()->GetInstanceByRID(packet.resource_id_));
        if (!inst)
        {
            SendErrorPacket(packet, protocol::ErrorPacket::PPV_ANNOUCE_NO_RESOURCEID);
        }
        else
        {
            SendAnnouncePacket(packet, inst);
        }
    }

    void TcpUploadManager::OnTcpSubPieceRequestPacket(const protocol::TcpSubPieceRequestPacket & packet)
    {
        storage::Instance::p inst = boost::static_pointer_cast<storage::Instance>(
            storage::Storage::Inst()->GetInstanceByRID(packet.resource_id_, false));

        if (!inst)
        {
            SendErrorPacket(packet, protocol::ErrorPacket::PPV_SUBPIECE_NO_RESOURCEID);
        }
        else
        {
            const std::vector<protocol::SubPieceInfo> & request_subpieces = packet.subpiece_infos_;
            for (uint32_t i = 0; i < request_subpieces.size(); i ++)
            {
                const protocol::SubPieceInfo & sub_piece_info = request_subpieces[i];
                if (sub_piece_info.GetPosition(inst->GetBlockSize()) > inst->GetFileLength())
                {
                    assert(false);
                    continue;
                }

                UploadCacheModule::Inst()->GetSubPieceForUpload(sub_piece_info, packet, inst, shared_from_this());
            }
        }
    }

    void TcpUploadManager::OnTcpReportSpeedPacket(const protocol::TcpReportSpeedPacket & packet)
    {
        if (tcp_speed_map_.find(packet.tcp_connection_) != tcp_speed_map_.end())
        {
            tcp_speed_map_[packet.tcp_connection_] = packet.speed_;
        }
        else
        {
            assert(false);
        }
    }

    void TcpUploadManager::OnTcpConnectionClose(const protocol::TcpClosePacket & packet)
    {
        if (tcp_speed_map_.find(packet.tcp_connection_) != tcp_speed_map_.end())
        {
            tcp_speed_map_.erase(packet.tcp_connection_);
        }
        else
        {
            assert(false);
        }
    }

    void TcpUploadManager::SendErrorPacket(const protocol::TcpCommonPacket & packet, boost::uint16_t error_code)
    {
        protocol::TcpErrorPacket error_packet(packet.transaction_id_, error_code);

        packet.tcp_connection_->DoSend(error_packet);
    }

    void TcpUploadManager::SendAnnouncePacket(const protocol::TcpCommonPacket & packet, storage::Instance__p inst)
    {
        protocol::TcpAnnounceResponsePacket announce_packet(packet.transaction_id_, inst->GetRID(),
            AppModule::Inst()->GetPeerDownloadInfo(inst->GetRID()), *(inst->GetBlockMap()));

        packet.tcp_connection_->DoSend(announce_packet);
    }

    void TcpUploadManager::OnAsyncGetSubPieceSucced(const RID& rid, protocol::SubPieceInfo const& subpiece_info,
        const protocol::Packet & packet, const protocol::SubPieceBuffer & buffer)
    {
        storage::Storage::Inst_Storage()->UploadOneSubPiece(rid);

        protocol::TcpSubPieceResponsePacket tcp_subpiece_response_packet(packet.transaction_id_, rid, subpiece_info, buffer);
        tcp_subpiece_response_packet.tcp_connection_ = ((const protocol::TcpCommonPacket &)packet).tcp_connection_;

        boost::system::error_code ec;
        boost::asio::ip::tcp::endpoint remote_endpoint = 
            tcp_subpiece_response_packet.tcp_connection_->socket().remote_endpoint(ec);

        statistic::UploadStatisticModule::Inst()->SubmitUploadSpeedInfo(remote_endpoint.address(), packet.length());
        statistic::StatisticModule::Inst()->SubmitUploadDataBytes(tcp_subpiece_response_packet.sub_piece_length_);

        upload_speed_limiter_->SendPacket(tcp_subpiece_response_packet, false, 
            ((const protocol::TcpSubPieceRequestPacket &)packet).priority_,
            ((const protocol::TcpSubPieceRequestPacket &)packet).protocol_version_);
    }

    void TcpUploadManager::OnAsyncGetSubPieceFailed(const RID& rid, protocol::SubPieceInfo const& subpiece_info,
        const protocol::Packet & packet)
    {
        SendErrorPacket((const protocol::TcpCommonPacket&)packet, protocol::ErrorPacket::PPV_SUBPIECE_SUBPIECE_NOT_FOUND);
    }
}
