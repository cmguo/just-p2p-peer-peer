#include "Common.h"
#include "VodUploadManager.h"
#include "p2sp/AppModule.h"
#include "p2sp/p2p/P2PDownloader.h"
#include "p2sp/proxy/ProxyModule.h"
#include "p2sp/p2p/UploadCacheModule.h"
#include "storage/Storage.h"

namespace p2sp
{
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
            KickAllUploadConnections();
        }
        else
        {
            KickUploadConnections();
        }

        KickTimeoutConnection();
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
            (IsConnectionFull(packet.ip_pool_size_) && 
            accept_connecting_peers_.find(packet.end_point) == accept_connecting_peers_.end()))
        {
            SendErrorPacket(packet, protocol::ErrorPacket::PPV_CONNECT_CONNECTION_FULL);
            return;
        }

        storage::Instance::p inst = boost::dynamic_pointer_cast<storage::Instance>(
            storage::Storage::Inst()->GetInstanceByRID(packet.resource_id_));

        if (!inst)
        {
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

            if (accept_connecting_peers_.find(packet.end_point) == accept_connecting_peers_.end())
            {
                accept_connecting_peers_[packet.end_point] = PEER_UPLOAD_INFO();
            }

            PEER_UPLOAD_INFO& peer_upload_info = accept_connecting_peers_[packet.end_point];
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
        if (accept_connecting_peers_.find(packet.end_point) == accept_connecting_peers_.end())
        {
            SendErrorPacket(packet, protocol::ErrorPacket::PPV_ANNOUCE_NO_RESOURCEID);
            return;
        }

        accept_connecting_peers_[packet.end_point].last_talk_time.reset();

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
        if (accept_connecting_peers_.find(packet.end_point) == accept_connecting_peers_.end())
        {
            SendErrorPacket(packet, protocol::ErrorPacket::PPV_SUBPIECE_NO_RESOURCEID);
            return;
        }

        accept_connecting_peers_[packet.end_point].last_talk_time.reset();

        if (accept_uploading_peers_.find(packet.end_point) == accept_uploading_peers_.end())
        {
            
            if (IsUploadConnectionFull(packet.end_point))
            {
                SendErrorPacket(packet, protocol::ErrorPacket::PPV_CONNECT_CONNECTION_FULL);
                return;
            }
            accept_uploading_peers_.insert(packet.end_point);
            uploading_peers_speed_[packet.end_point] = std::make_pair(0, framework::timer::TickCounter());
        }

        accept_connecting_peers_[packet.end_point].last_data_time.reset();

        storage::Instance::p inst = boost::dynamic_pointer_cast<storage::Instance>(
            storage::Storage::Inst()->GetInstanceByRID(packet.resource_id_, false));

        if (!inst)
        {
            SendErrorPacket(packet, protocol::ErrorPacket::PPV_SUBPIECE_NO_RESOURCEID);
        }
        else if (!accept_connecting_peers_[packet.end_point].IsInLastDataTransIDs(packet.transaction_id_))
        {
            accept_connecting_peers_[packet.end_point].UpdateLastDataTransID(packet.transaction_id_);

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
    }

    void VodUploadManager::OnRIDInfoRequestPacket(const protocol::RIDInfoRequestPacket & packet)
    {
        if (accept_connecting_peers_.find(packet.end_point) == accept_connecting_peers_.end())
        {
            SendErrorPacket(packet, protocol::ErrorPacket::PPV_RIDINFO_NO_RESOURCEID);
            return;
        }

        accept_connecting_peers_[packet.end_point].last_talk_time.reset();

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
        if (accept_connecting_peers_.find(packet.end_point) == accept_connecting_peers_.end())
        {
            SendErrorPacket(packet, protocol::ErrorPacket::PPV_RIDINFO_NO_RESOURCEID);
            return;
        }

        accept_connecting_peers_[packet.end_point].last_talk_time.reset();

        if (uploading_peers_speed_.find(packet.end_point) != uploading_peers_speed_.end())
        {
            std::pair<boost::uint32_t, framework::timer::TickCounter> & speed = uploading_peers_speed_[packet.end_point];
            speed.first = packet.speed_;
            speed.second.reset();
        }
    }

    void VodUploadManager::OnAsyncGetSubPieceSucced(const RID& rid, protocol::SubPieceInfo const& subpiece_info,
        const protocol::Packet & packet, const protocol::SubPieceBuffer & buffer)
    {
        storage::Storage::Inst_Storage()->UploadOneSubPiece(rid);

        protocol::SubPiecePacket subpiece_packet(packet.transaction_id_, rid, AppModule::Inst()->GetPeerGuid(),
            subpiece_info.GetSubPieceInfoStruct(), buffer.Length(), buffer, packet.end_point);

        bool ignoreUploadSpeedLimit = IsPeerFromSameSubnet(packet.end_point);

        upload_speed_limiter_->SendPacket(
            subpiece_packet,
            ignoreUploadSpeedLimit,
            ((const protocol::RequestSubPiecePacket &)packet).priority_,
            ((const protocol::RequestSubPiecePacket &)packet).protocol_version_);

        accept_connecting_peers_[packet.end_point].speed_info.SubmitUploadedBytes(SUB_PIECE_SIZE);
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

    bool VodUploadManager::IsConnectionFull(uint32_t ip_pool_size) const
    {
        boost::int32_t max_speed_limit_in_KBps = P2PModule::Inst()->GetUploadSpeedLimitInKBps();
        boost::int32_t max_connect_peers = P2PModule::Inst()->GetMaxConnectLimitSize();

        if (max_connect_peers <= -1)
        {
            return false;
        }
        else if (max_connect_peers == 0)
        {
            return true;
        }

        boost::uint32_t now_upload_data_speed = statistic::StatisticModule::Inst()->GetUploadDataSpeed();
        if ((boost::uint32_t)max_connect_peers > accept_connecting_peers_.size())
        {
            return false;
        }
        else if (ip_pool_size > 0 && ip_pool_size <= 40 &&
            (boost::uint32_t)max_connect_peers + 3 > accept_connecting_peers_.size())
        {
            return false;
        }
        else if ((boost::uint32_t)max_speed_limit_in_KBps * 1024 >= now_upload_data_speed + 5 * 1024)
        {
            return false;
        }
        else
        {
            for (std::set<boost::asio::ip::udp::endpoint>::const_iterator 
                it = accept_uploading_peers_.begin(); it != accept_uploading_peers_.end(); ++it)
            {
                boost::asio::ip::udp::endpoint ep = *it;

                std::map<boost::asio::ip::udp::endpoint, PEER_UPLOAD_INFO>::const_iterator 
                    iter = accept_connecting_peers_.find(*it);

                if (iter != accept_connecting_peers_.end())
                {
                    if (false == iter->second.is_open_service)
                    {
                        return false;
                    }
                }
            }
        }

        return true;
    }

    bool VodUploadManager::IsUploadConnectionFull(boost::asio::ip::udp::endpoint const& end_point)
    {
        boost::int32_t max_speed_limit_in_KBps = P2PModule::Inst()->GetUploadSpeedLimitInKBps();
        boost::int32_t max_upload_peers = P2PModule::Inst()->GetMaxUploadLimitSize();

        if (max_upload_peers <= -1)
        {
            return false;
        }
        else if (max_upload_peers == 0)
        {
            return true;
        }
        else
        {
            if ((boost::uint32_t)max_upload_peers <= accept_uploading_peers_.size())
            {
                boost::uint32_t now_upload_data_speed = statistic::StatisticModule::Inst()->GetUploadDataSpeed();

                for (std::set<boost::asio::ip::udp::endpoint>::iterator 
                    it = accept_uploading_peers_.begin(); it != accept_uploading_peers_.end(); ++it)
                {
                    std::map<boost::asio::ip::udp::endpoint, PEER_UPLOAD_INFO>::iterator 
                        iter = accept_connecting_peers_.find(*it);

                    if (iter != accept_connecting_peers_.end())
                    {
                        const PEER_UPLOAD_INFO & curr_info = accept_connecting_peers_[end_point];

                        if (iter->second.last_data_time.elapsed() >= 3 * 1000)
                        {
                            uploading_peers_speed_.erase(*it);
                            accept_uploading_peers_.erase(it);
                            return false;
                        }
                        else if (true == curr_info.is_open_service && false == iter->second.is_open_service &&
                            (boost::int32_t)now_upload_data_speed + 5 * 1024 > max_speed_limit_in_KBps * 1024)
                        {
                            uploading_peers_speed_.erase(*it);
                            accept_uploading_peers_.erase(it);
                            return false;
                        }
                    }
                    else
                    {
                        assert(false);
                    }
                }

                if (accept_uploading_peers_.size() <= (boost::uint32_t)max_upload_peers + 3 && 
                    now_upload_data_speed + 5 * 1024 < (boost::uint32_t)max_speed_limit_in_KBps * 1024)
                {
                    return false;
                }
                else
                {
                    std::map<boost::asio::ip::udp::endpoint, PEER_UPLOAD_INFO>::const_iterator 
                        it = accept_connecting_peers_.find(end_point);
                    if (it != accept_connecting_peers_.end())
                    {
                        const PEER_UPLOAD_INFO & peer_upload_info = it->second;
                        if (peer_upload_info.ip_pool_size > 0 && peer_upload_info.ip_pool_size <= 30)
                        {
                            if (accept_uploading_peers_.size() <= (boost::uint32_t)max_upload_peers + 3)
                            {
                                return false;
                            }
                        }
                    }
                }
            }
            else
            {
                return false;
            }
        }

        return true;
    }

    void VodUploadManager::KickAllUploadConnections()
    {
        accept_connecting_peers_.clear();
        accept_uploading_peers_.clear();
        uploading_peers_speed_.clear();
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

    void VodUploadManager::KickUploadConnections()
    {
        // 如果连接数超过了最大允许的值，则按照速度由小到大首先踢掉连接
        // 如果刚连接上还没超过20秒，则不踢
        boost::int32_t need_kick_count = 0;  // 需要踢掉的连接数
        std::multimap<boost::uint32_t, boost::asio::ip::udp::endpoint> kick_upload_connection_map;

        boost::int32_t max_connect_peers = UploadModule::Inst()->GetMaxConnectLimitSize();

        if (max_connect_peers < 0)
        {
            return;
        }

        if (accept_connecting_peers_.size() > (boost::uint32_t)max_connect_peers)
        {
            need_kick_count = accept_connecting_peers_.size() - max_connect_peers;
            for (std::map<boost::asio::ip::udp::endpoint, PEER_UPLOAD_INFO>::iterator
                iter = accept_connecting_peers_.begin(); iter != accept_connecting_peers_.end(); ++iter)
            {
                kick_upload_connection_map.insert(std::make_pair(iter->second.speed_info.GetSpeedInfo().NowUploadSpeed, iter->first));
            }
        }

        // 已经踢掉的连接个数
        std::multimap<boost::uint32_t, boost::asio::ip::udp::endpoint>::iterator iter_kick = kick_upload_connection_map.begin();
        for (boost::int32_t kicked_count = 0; kicked_count < need_kick_count;)
        {
            if (iter_kick == kick_upload_connection_map.end())
            {
                break;
            }

            const boost::asio::ip::udp::endpoint & ep = iter_kick->second;
            if (accept_connecting_peers_[ep].connected_time.elapsed() >= 20 * 1000)
            {
                accept_connecting_peers_.erase(ep);
                accept_uploading_peers_.erase(ep);
                uploading_peers_speed_.erase(ep);

                ++kicked_count;
            }

            ++iter_kick;
        }
    }
}