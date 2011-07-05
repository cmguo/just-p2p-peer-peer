//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/AppModule.h"
#include "p2sp/p2p/P2PModule.h"
#include "p2sp/p2p/UploadManager.h"
#include "p2sp/stun/StunClient.h"
#include "p2sp/p2p/P2SPConfigs.h"
#include "p2sp/p2p/P2PDownloader.h"
#include "p2sp/p2p/PeerHelper.h"
#include "p2sp/proxy/ProxyModule.h"
#include "p2sp/p2p/NetworkQualityMonitor.h"
#include "p2sp/bootstrap/BootStrapGeneralConfig.h"

#include "protocol/PeerPacket.h"

#include "storage/IStorage.h"
#include "storage/Storage.h"
#include "statistic/StatisticModule.h"
#include "statistic/UploadStatisticModule.h"
#include "statistic/DACStatisticModule.h"

#ifdef COUNT_CPU_TIME
#include "count_cpu_time.h"
#endif

#define P2P_DEBUG(s) LOG(__DEBUG, "P2P", s)
#define P2P_INFO(s)    LOG(__INFO, "P2P", s)
#define P2P_EVENT(s) LOG(__EVENT, "P2P", s)
#define P2P_WARN(s)    LOG(__WARN, "P2P", s)
#define P2P_ERROR(s) LOG(__ERROR, "P2P", s)

#define UPLOAD_DEBUG(msg) LOGX(__DEBUG, "upload", msg)

static const int32_t MinUploadSpeedLimitInKbs = 20;

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("upload");

    void UploadManager::Start(const string& config_path)
    {
        if (is_running_ == true)
            return;

        is_running_ = true;

        recent_play_series_ = 0;

        is_upload_too_more_ = false;
        upload_param_.SetMaxSpeedInKBps(-1);
        upload_param_.SetMaxConnectPeers(-1);
        upload_param_.SetMaxUploadPeers(-1);

        desktop_type_ = storage::DT_DEFAULT;
        is_locking_ = false;

        max_upload_cache_len_ = P2SPConfigs::UPLOAD_MAX_CACHE_LENGTH;

        // config path
        if (config_path.length() == 0)
        {
            string szPath;
#ifdef DISK_MODE
            if (base::util::GetAppDataPath(szPath))
            {
                ppva_config_path_.assign(szPath);
            }
#endif  // #ifdef DISK_MODE
        }
        else
        {
            ppva_config_path_ = config_path;
        }

        LoadHistoricalMaxUploadSpeed();

        network_quality_monitor_ = boost::shared_ptr<NetworkQualityMonitor>(new NetworkQualityMonitor(global_io_svc()));
        //network_quality_monitor_->Start();
    }

    void UploadManager::Stop()
    {
        if (is_running_ == false)
            return;

        SaveHistoricalMaxUploadSpeed();

        // clear cache
        cache_list_.clear();
        need_resource_map_.clear();

        is_running_ = false;
    }

    bool UploadManager::IsConnectionFull(uint32_t ip_pool_size) const
    {
        if (false == is_running_)
            return false;

        bool is_connection_full;
        assert(upload_param_.GetMaxConnectPeers() >= -1);
        // 不限连接
        if (upload_param_.GetMaxConnectPeers() <= -1)
        {
            is_connection_full = false;
        }
        // 禁止连接
        else if (upload_param_.GetMaxConnectPeers() == 0)
        {
            is_connection_full = true;
        }
        // 限制连接
        else
        {
            uint32_t now_upload_data_speed = statistic::StatisticModule::Inst()->GetUploadDataSpeed();
            if ((uint32_t)upload_param_.GetMaxConnectPeers() > accept_connecting_peers_.size())
            {
                UPLOAD_DEBUG("UploadLimit OK, current_connected_peers = " << accept_connecting_peers_.size()
                    << ", MaxConnectPeers = " << upload_param_.GetMaxConnectPeers());
                is_connection_full = false;
            }
            else if (ip_pool_size > 0 && ip_pool_size <= 40 && (uint32_t)upload_param_.GetMaxConnectPeers() + 3
            > accept_connecting_peers_.size())
            {
                UPLOAD_DEBUG("UploadLimit! current_connected_peers=" << accept_connecting_peers_.size());
                is_connection_full = false;
            }
            else if ((uint32_t)upload_param_.GetMaxSpeedInKBps() * 1024 >= now_upload_data_speed + 5 * 1024)
            {
                UPLOAD_DEBUG("Upload Bandwidth not full, SpeedLimit = "
                    << now_upload_data_speed/1024.0 << "/" << upload_param_.GetMaxSpeedInKBps()
                    << "accept_connection_peers=" << accept_connecting_peers_.size());
                is_connection_full = false;
            }
            else
            {
                is_connection_full = true;
                // check
                std::set<boost::asio::ip::udp::endpoint>::const_iterator it;
                for (it = accept_uploading_peers_.begin(); it != accept_uploading_peers_.end(); ++it)
                {
                    boost::asio::ip::udp::endpoint ep = *it;
                    std::map<boost::asio::ip::udp::endpoint, PEER_UPLOAD_INFO>::const_iterator iter;
                    iter = accept_connecting_peers_.find(ep);
                    if (iter != accept_connecting_peers_.end())
                    {
                        if (false == iter->second.is_open_service)
                        {
                            UPLOAD_DEBUG("Find Non Openservice Connection Uploading! Can Connect");
                            is_connection_full = false;
                            break;
                        }
                    }
                }
                // log
                if (is_connection_full)
                {
                    UPLOAD_DEBUG("Upload Connection Full!");
                }
            }
        }
        return is_connection_full;
    }

    bool UploadManager::IsUploadConnectionFull(boost::asio::ip::udp::endpoint const& end_point)
    {
        if (false == is_running_)
            return false;

        bool is_upload_connection_full;
        // no limitation
        if (upload_param_.GetMaxUploadPeers() <= -1)
        {
            UPLOAD_DEBUG("Upload No limitation!");
            is_upload_connection_full = false;
        }
        // no upload
        else if (upload_param_.GetMaxUploadPeers() == 0)
        {
            UPLOAD_DEBUG("Upload Forbidden!");
            is_upload_connection_full = true;
        }
        else
        {
            std::map<boost::asio::ip::udp::endpoint, PEER_UPLOAD_INFO>::iterator curr_iter;
            curr_iter = accept_connecting_peers_.find(end_point);
            PEER_UPLOAD_INFO curr_info;
            if (curr_iter != accept_connecting_peers_.end())
            {
                curr_info = curr_iter->second;
            }
            else
            {
                UPLOAD_DEBUG("Current Endpoint Not Exists!!");
                return true;
            }

            if (static_cast<uint32_t>(upload_param_.GetMaxUploadPeers()) <= accept_uploading_peers_.size())
            {
                uint32_t now_upload_data_speed = statistic::StatisticModule::Inst()->GetUploadDataSpeed();
                // check time out
                is_upload_connection_full = true;
                UPLOAD_DEBUG("accept_uploading_peers_ ");
                std::set<boost::asio::ip::udp::endpoint>::iterator it;
                for (it = accept_uploading_peers_.begin(); it != accept_uploading_peers_.end();)
                {
                    boost::asio::ip::udp::endpoint ep = *it;
                    std::map<boost::asio::ip::udp::endpoint, PEER_UPLOAD_INFO>::iterator iter =
                        accept_connecting_peers_.find(ep);
                    if (iter != accept_connecting_peers_.end())
                    {
                        if (iter->second.last_data_time.elapsed() >= 3 * 1000)
                        {
                            // kick
                            UPLOAD_DEBUG("Kick Peer, DataTime Too Long, PeerGuid = " << iter->second.peer_guid
                                << ", Peer Endpoint = " << ep
                                << ", ResourceID = " << iter->second.resource_id
                                << ", IsOpenService = " << iter->second.is_open_service
                                << ", Last TalkTime = " << iter->second.last_talk_time.elapsed()
                                << ", Last DataTime = " << iter->second.last_data_time.elapsed());
                            accept_uploading_peers_.erase(it);
                            uploading_peers_speed_.erase(ep);
                            is_upload_connection_full = false;
                            break;
                        }
                        else if (true == curr_info.is_open_service && false == iter->second.is_open_service &&
                            (boost::int32_t)now_upload_data_speed + 5 * 1024 > upload_param_.GetMaxSpeedInKBps() * 1024)
                        {
                            UPLOAD_DEBUG("Kick Peer, NonOpenService, PeerGuid = " << iter->second.peer_guid
                                << ", Peer Endpoint = " << ep
                                << ", ResourceID = " << iter->second.resource_id
                                << ", IsOpenService = " << iter->second.is_open_service
                                << ", Last TalkTime = " << iter->second.last_talk_time.elapsed()
                                << ", Last DataTime = " << iter->second.last_data_time.elapsed());
                            accept_uploading_peers_.erase(it);
                            uploading_peers_speed_.erase(ep);
                            is_upload_connection_full = false;
                            break;
                        }

                        ++it;
                    }
                    else
                    {
                        // kick
                        UPLOAD_DEBUG("Kick Peer, No Such Peer, peer_guid = " << iter->second.peer_guid
                            << ", Peer Endpoint = " << ep
                            << ", Last TalkTime = " << iter->second.last_talk_time.elapsed()
                            << ", Last DataTime = " << iter->second.last_data_time.elapsed());
                        accept_uploading_peers_.erase(it);
                        uploading_peers_speed_.erase(ep);
                        is_upload_connection_full = false;
                        break;
                    }
                }
                //
                if (true == is_upload_connection_full)
                {
                    // extend
                    if (accept_uploading_peers_.size() <= static_cast<uint32_t>(upload_param_.GetMaxUploadPeers() + 3)
                        && now_upload_data_speed + 5 * 1024 < (uint32_t)upload_param_.GetMaxSpeedInKBps() * 1024)
                    {
                        UPLOAD_DEBUG("UploadLimit! False. current_upload_peers=" << accept_uploading_peers_.size());
                        is_upload_connection_full = false;
                    }
                    else
                    {
                        UPLOAD_DEBUG("UploadLimit! True. current_upload_peers=" << accept_uploading_peers_.size());
                        is_upload_connection_full = true;
                        std::map<boost::asio::ip::udp::endpoint, PEER_UPLOAD_INFO>::const_iterator it =
                            accept_connecting_peers_.find(end_point);
                        if (it != accept_connecting_peers_.end())
                        {
                            const PEER_UPLOAD_INFO& peer_upload_info = it->second;
                            if (peer_upload_info.ip_pool_size > 0 && peer_upload_info.ip_pool_size <= 30)
                            {
                                if (accept_uploading_peers_.size() <= (uint32_t)upload_param_.GetMaxUploadPeers() + 3)
                                {
                                    UPLOAD_DEBUG("UploadLimit! False. ip_pool_size=" << peer_upload_info.ip_pool_size << " current_upload_peers=" << accept_uploading_peers_.size());
                                    is_upload_connection_full = false;
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                UPLOAD_DEBUG("Upload Connection OK, " << accept_uploading_peers_.size() << " / " << upload_param_.GetMaxUploadPeers());
                is_upload_connection_full = false;
            }
        }
        return is_upload_connection_full;
    }

    void UploadManager::CheckCacheList()
    {
        if (false == is_running_)
            return;

        while (!cache_list_.empty())
        {
            RidBlock rid_block = cache_list_.back();
            if (rid_block.touch_time_counter_.elapsed() >= P2SPConfigs::UPLOAD_CACHE_BLOCK_EXPIRE_TIME_IN_MILLISEC)
            {
                UPLOAD_DEBUG("Erase No Use RID Cache, rid = " << rid_block.index.rid
                    << ", touch time = " << rid_block.touch_time_counter_.elapsed());
                cache_list_.pop_back();
            }
            else
            {
                break;
            }
        }
    }

    UploadManager::ApplyListPtr UploadManager::CreateApplyList()
    {
        if (false == is_running_)
        {
            return ApplyListPtr();
        }
        return ApplyListPtr(new ApplyList());
    }

    bool UploadManager::IsPeerFromSameSubnet(const boost::asio::ip::udp::endpoint& peer_endpoint) const
    {
        std::map<boost::asio::ip::udp::endpoint, PEER_UPLOAD_INFO>::const_iterator peer_upload_info_iter = 
            accept_connecting_peers_.find(peer_endpoint);
        
        if (peer_upload_info_iter != accept_connecting_peers_.end())
        {
            return PeerHelper::IsPeerFromSameSubnet(peer_upload_info_iter->second.peer_info);
        }

        return false;
    }

    boost::uint32_t UploadManager::MeasureCurrentUploadSpeed() const
    {
        boost::uint32_t current_upload_speed = 0;
        std::map<boost::asio::ip::udp::endpoint, std::pair<boost::uint32_t, framework::timer::TickCounter> >::const_iterator iter = uploading_peers_speed_.begin();
        for (; iter != uploading_peers_speed_.end(); ++iter)
        {
            if (iter->second.second.elapsed() < 5000)
            {
                if (IsPeerFromSameSubnet(iter->first))
                {
                    UPLOAD_DEBUG("Peer from the same subnet is excluded for upload speed measurement. Peer:" << iter->first << ", speed: " <<iter->second.first);
                }
                else
                {
                    UPLOAD_DEBUG(iter->first << " speed is:" << iter->second.first);
                    current_upload_speed += iter->second.first;
                }
            }
        }

        return current_upload_speed;
    }

    void UploadManager::OnP2PTimer(uint32_t times)
    {
#ifdef COUNT_CPU_TIME
        count_cpu_time(__FUNCTION__);
#endif
        if (is_running_ == false) return;

        if (times % 4 == 0)
        {
            boost::uint32_t current_upload_speed = MeasureCurrentUploadSpeed();
            upload_speed_limit_tracker_.ReportUploadSpeed(current_upload_speed);
            DebugLog("upload current_upload_speed:%d", current_upload_speed);
        }

        // upload control
        OnUploadControl(times);
        statistic::UploadStatisticModule::Inst()->SubmitUploadInfo(upload_param_.GetMaxSpeedInKBps(), accept_uploading_peers_);

        // 每250MS检查上传速度是否超过门限

        // 每一秒钟检查是否有超时的accpet peer
        if (times % 4 == 0)
        {
            // bool can_notify = false;
            upload_param_.GetMaxSpeedInKBps();
            std::map<boost::asio::ip::udp::endpoint, PEER_UPLOAD_INFO>::iterator iter;
            for (iter = accept_connecting_peers_.begin(); iter != accept_connecting_peers_.end();)
            {
                if (iter->second.last_talk_time.elapsed() >= 10000)
                {
                    UPLOAD_DEBUG("Connection Timeout " << iter->first);
                    accept_uploading_peers_.erase(iter->first);
                    uploading_peers_speed_.erase(iter->first);
                    accept_connecting_peers_.erase(iter++);
                }
                else
                {
                    iter++;
                }
            }

            KickUploadConnections();

            if (times % 40 == 0)  // 10s
            {
                statistic::StatisticModule::Inst()->SetUploadCacheHit(static_cast<boost::uint32_t>(get_from_cache_));
                statistic::StatisticModule::Inst()->SetUploadCacheRequest(static_cast<boost::uint32_t>(apply_subpiece_num_));

                // 检查CacheList
                CheckCacheList();

                // check local ip address
                if (CStunClient::GetLocalFirstIP() != local_ip_from_ini_)
                {
                    // change
                    upload_speed_limit_tracker_.Reset(P2SPConfigs::UPLOAD_MIN_UPLOAD_BANDWIDTH);

                    SaveHistoricalMaxUploadSpeed();
                    LoadHistoricalMaxUploadSpeed();
                }
            }

            // write
            if (times % (10 * 60 * 4) == 0)
            {
                SaveHistoricalMaxUploadSpeed();
            }
        }
    }

    void UploadManager::OnLiveRequestAnnouncePacket(protocol::LiveRequestAnnouncePacket const & packet)
    {
        if (is_running_ == false)
            return;

        UPLOAD_DEBUG("UploadManager::OnLiveRequestAnnouncePacket " << packet.end_point );
        if (accept_connecting_peers_.find(packet.end_point) == accept_connecting_peers_.end())
            // 如果不是在限定连接之内则回错误报文
        {
            UPLOAD_DEBUG("UploadManager::OnLiveRequestAnnouncePacket No Such EndPoint,  response error packet"
                << packet.end_point );
            SendErrorPacket((protocol::LivePeerPacket const &)packet, protocol::ErrorPacket::PPV_ANNOUCE_NO_RESOURCEID);
            return;
        }
        accept_connecting_peers_[packet.end_point].last_talk_time.reset();

        storage::LiveInstance::p live_inst = boost::dynamic_pointer_cast<storage::LiveInstance>(storage::Storage::Inst()->GetLiveInstanceByRid(packet.resource_id_));
        if (!live_inst)
        {
            // error
            SendErrorPacket((protocol::LivePeerPacket const &)packet, protocol::ErrorPacket::PPV_ANNOUCE_NO_RESOURCEID);
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

    void UploadManager::OnRequestAnnouncePacket(protocol::RequestAnnouncePacket const & packet)
    {
        if (is_running_ == false) return;

        UPLOAD_DEBUG("UploadManager::OnRequestAnnouncePacket " << packet.end_point);

        // 检查 Storage 中是否存在 该 RID 对应的 Instance
        // IIinstace::p = Storage::Inst()->GetInstance(rid)
        // if (!p)
        // {
        //    这种情况就是 不存在 该 RID 对应的 Instance
        //    不存在的 返回一个 Error 报文 RequestAnnounceFailed Becuase RID Not Exist
        //    return;
        // }
        // 否则
        //    把 IIinstace::p->GetBlockMap() 的内容拼成 Announce 报文，然后发送

        if (accept_connecting_peers_.find(packet.end_point) == accept_connecting_peers_.end())
        // 如果不是在限定连接之内则回错误报文
        {
            UPLOAD_DEBUG("UploadManager::OnRequestAnnouncePacket No Such EndPoint,  response error packet" << packet.end_point);
            protocol::ErrorPacket  error_packet((protocol::PeerPacket const &)packet);
            error_packet.peer_guid_ = AppModule::Inst()->GetPeerGuid();
            error_packet.error_code_ =  protocol::ErrorPacket::PPV_ANNOUCE_NO_RESOURCEID;
            AppModule::Inst()->DoSendPacket(error_packet, packet.sequece_id_);
            return;
        }
        accept_connecting_peers_[packet.end_point].last_talk_time.reset();

        storage::Instance::p inst = boost::dynamic_pointer_cast<storage::Instance>(storage::Storage::Inst()->GetInstanceByRID(packet.resource_id_));
        if (!inst)
        {
            // error
            protocol::ErrorPacket error_packet((protocol::PeerPacket const &)packet);
            error_packet.peer_guid_ = AppModule::Inst()->GetPeerGuid();
            error_packet.error_code_ =  protocol::ErrorPacket::PPV_ANNOUCE_NO_RESOURCEID;
            AppModule::Inst()->DoSendPacket(error_packet, packet.sequece_id_);
        }
        else
        {
            protocol::AnnouncePacket announce_packet(packet.transaction_id_, inst->GetRID(),
                AppModule::Inst()->GetPeerGuid(), AppModule::Inst()->GetPeerDownloadInfo(inst->GetRID()),
                *(inst->GetBlockMap()), packet.end_point);

            AppModule::Inst()->DoSendPacket(announce_packet, packet.sequece_id_);
        }
    }

    void UploadManager::OnRIDInfoRequestPacket(protocol::RIDInfoRequestPacket const & packet)
    {
        if (false == is_running_)
            return;

        if (accept_connecting_peers_.find(packet.end_point) == accept_connecting_peers_.end())
        // 如果不是在限定连接之内则回错误报文
        {
            UPLOAD_DEBUG("No such connection.");
            protocol::ErrorPacket error_packet((protocol::PeerPacket const &)packet);
            error_packet.peer_guid_ = AppModule::Inst()->GetPeerGuid();
            error_packet.error_code_ =  protocol::ErrorPacket::PPV_RIDINFO_NO_RESOURCEID;
            AppModule::Inst()->DoSendPacket(error_packet, packet.sequece_id_);
            return;
        }
        accept_connecting_peers_[packet.end_point].last_talk_time.reset();

        storage::Instance::p inst = boost::dynamic_pointer_cast<storage::Instance>(storage::Storage::Inst()->GetInstanceByRID(packet.resource_id_));
        if (!inst)
        {
            UPLOAD_DEBUG("No Such Instance");

            protocol::ErrorPacket  error_packet((protocol::PeerPacket const &)packet);
            error_packet.peer_guid_ = AppModule::Inst()->GetPeerGuid();
            error_packet.error_code_ =  protocol::ErrorPacket::PPV_RIDINFO_NO_RESOURCEID;
            AppModule::Inst()->DoSendPacket(error_packet, packet.sequece_id_);
        }
        else
        {
            UPLOAD_DEBUG("Yes, I have it.");
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

            AppModule::Inst()->DoSendPacket(ridinfo_packet, packet.sequece_id_);
        }
    }

    void UploadManager::OnReportSpeedPacket(protocol::ReportSpeedPacket const & packet)
    {
        if (is_running_ == false) return;
        P2P_EVENT("UploadManager::OnReportSpeedPacket " << packet.end_point << ", speed: " << packet.speed_);

        if (accept_connecting_peers_.find(packet.end_point) == accept_connecting_peers_.end())
            // 如果不是在限定连接之内则回错误报文
        {
            UPLOAD_DEBUG("No such connection.");

            protocol::ErrorPacket  error_packet((protocol::PeerPacket const &)packet);
            error_packet.peer_guid_ = AppModule::Inst()->GetPeerGuid();
            error_packet.error_code_ =  protocol::ErrorPacket::PPV_RIDINFO_NO_RESOURCEID;
            AppModule::Inst()->DoSendPacket(error_packet, packet.sequece_id_);
            return;
        }
        accept_connecting_peers_[packet.end_point].last_talk_time.reset();

        if (uploading_peers_speed_.find(packet.end_point) != uploading_peers_speed_.end())
        {
            std::pair<boost::uint32_t, framework::timer::TickCounter> & speed = uploading_peers_speed_[packet.end_point];
            speed.first = packet.speed_;
            speed.second.reset();
            UPLOAD_DEBUG(packet.end_point << " speed update:" << speed.first);
        }
        else
        {
            UPLOAD_DEBUG("Error: No such uploading connection." << packet.end_point);
        }
    }

    void UploadManager::OnLiveRequestSubPiecePacket(protocol::LiveRequestSubPiecePacket const & packet)
    {
        if (is_running_ == false)
            return;

        // connection
        P2P_EVENT("UploadManager::OnLiveRequestSubPiecePacket " <<packet.end_point );
        if (accept_connecting_peers_.find(packet.end_point) == accept_connecting_peers_.end())
            // 如果不是在限定连接之内则回错误报文
        {
            P2P_EVENT("UploadManager::OnLiveRequestSubPiecePacket No Such EndPoint,  response error packet"
                <<packet.end_point );
            SendErrorPacket((protocol::LivePeerPacket const &)packet, protocol::ErrorPacket::PPV_SUBPIECE_NO_RESOURCEID);
            return;
        }

        P2P_EVENT("UploadManager::OnLiveRequestSubPiecePacket" << packet.end_point );

        // talked
        accept_connecting_peers_[packet.end_point].last_talk_time.reset();

        // upload
        if (accept_uploading_peers_.find(packet.end_point) == accept_uploading_peers_.end())
        {
            UPLOAD_DEBUG("AcceptNewUploadingPeer="<< packet.end_point << " uploading_peers=" << accept_uploading_peers_.size());
            accept_uploading_peers_.insert(packet.end_point);
            uploading_peers_speed_[packet.end_point] = std::make_pair(0, framework::timer::TickCounter());
        }

        accept_connecting_peers_[packet.end_point].last_data_time.reset();

        // speed
        if (upload_limiter_.GetSpeedLimitInKBps() == 0)
        {
            return;
        }

        storage::LiveInstance::p live_inst = boost::dynamic_pointer_cast<storage::LiveInstance>(storage::Storage::Inst()->GetLiveInstanceByRid(packet.resource_id_));
        if (!live_inst)
        {
            // error
            SendErrorPacket((protocol::LivePeerPacket const &)packet, protocol::ErrorPacket::PPV_SUBPIECE_NO_RESOURCEID);
        }
        else if (true == accept_connecting_peers_[packet.end_point].IsInLastDataTransIDs(packet.transaction_id_))
        {
            // directly ignore
            LOGX(__DEBUG, "upload", "true == accept_connecting_peers_[" << packet.end_point << "].IsInLastDataTransIDs(" << packet.transaction_id_ << ")");
        }
        else
        {
            // update data trans id
            accept_connecting_peers_[packet.end_point].UpdateLastDataTransID(packet.transaction_id_);
            LOGX(__DEBUG, "upload", "accept_connecting_peers_[" << packet.end_point << "].UpdateLastDataTransID(" << packet.transaction_id_ << "), All: [ "
                << accept_connecting_peers_[packet.end_point].last_data_trans_ids[0] << ", "
                << accept_connecting_peers_[packet.end_point].last_data_trans_ids[1] << ", "
                << accept_connecting_peers_[packet.end_point].last_data_trans_ids[2] << ", "
                << accept_connecting_peers_[packet.end_point].last_data_trans_ids[3] << ", "
                << accept_connecting_peers_[packet.end_point].last_data_trans_ids[4] << " ]");
            // fetch data
            for (uint32_t i = 0; i < packet.sub_piece_infos_.size(); ++i)
            {
                const protocol::LiveSubPieceInfo &live_sub_piece_info = packet.sub_piece_infos_[i];
                P2P_EVENT("UploadManager::OnLiveRequestSubPiecePacket endpoint: " << packet.end_point << live_sub_piece_info);

                if (live_inst->HasSubPiece(live_sub_piece_info))
                {
                    protocol::LiveSubPieceBuffer tmp_buf;

                    live_inst->GetSubPiece(live_sub_piece_info, tmp_buf);

                    protocol::LiveSubPiecePacket live_subpiece_packet(packet.transaction_id_, packet.resource_id_,
                        live_sub_piece_info, tmp_buf.Length(), tmp_buf, packet.end_point);

                    bool ignoreUploadSpeedLimit = IsPeerFromSameSubnet(packet.end_point);

                    upload_limiter_.SendPacket(
                        live_subpiece_packet,
                        ignoreUploadSpeedLimit,
                        packet.priority_,
                        packet.protocol_version_);

                    statistic::DACStatisticModule::Inst()->SubmitLiveP2PUploadBytes(LIVE_SUB_PIECE_SIZE);

                    accept_connecting_peers_[packet.end_point].speed_info.SubmitUploadedBytes(LIVE_SUB_PIECE_SIZE);
                }
                else
                {
                    SendErrorPacket((protocol::LivePeerPacket const &)packet, protocol::ErrorPacket::PPV_SUBPIECE_SUBPIECE_NOT_FOUND);
                }

            } // for
        } // else
    }

    void UploadManager::OnRequestSubPiecePacket(protocol::RequestSubPiecePacket const & packet)
    {
        if (is_running_ == false) return;

        // 判断此时是否允许上传
        if (is_disable_upload_)
        {
            return;
        }

        // 检查 Storage 中是否存在 该 RID 对应的 Instance
        // IIinstace::p = Storage::Inst()->GetInstance(rid)
        // if (!p)
        // {
        //    这种情况就是 不存在 该 RID 对应的 Instance
        //    不存在的 返回一个 Error 报文 RequestSubPieecFailed Becuase RID Not Exist
        //    return;
        // }
        // 否则
        //    异步调用 IInstance->AsyncGetSubPiece

        // connection
        P2P_EVENT("UploadManager::OnRequestSubPiecePacket " << packet.end_point);
        if (accept_connecting_peers_.find(packet.end_point) == accept_connecting_peers_.end())
        // 如果不是在限定连接之内则回错误报文
        {
            P2P_EVENT("UploadManager::OnRequestSubPiecePacket No Such EndPoint,  response error packet" << packet.end_point);

            protocol::ErrorPacket error_packet((protocol::PeerPacket const &)packet);
            error_packet.peer_guid_ = AppModule::Inst()->GetPeerGuid();
            error_packet.error_code_ =  protocol::ErrorPacket::PPV_SUBPIECE_NO_RESOURCEID;
            AppModule::Inst()->DoSendPacket(error_packet, packet.sequece_id_);
            return;
        }

        P2P_EVENT("UploadManager::OnRequestSubPiecePacket 11111" << packet.end_point);

        // talked
        accept_connecting_peers_[packet.end_point].last_talk_time.reset();

        // upload
        if (accept_uploading_peers_.find(packet.end_point) == accept_uploading_peers_.end())
        {
            if (IsUploadConnectionFull(packet.end_point))
            {
                protocol::ErrorPacket error_packet((protocol::PeerPacket const &)packet);
                error_packet.peer_guid_ = AppModule::Inst()->GetPeerGuid();
                error_packet.error_code_ =  protocol::ErrorPacket::PPV_CONNECT_CONNECTION_FULL;
                AppModule::Inst()->DoSendPacket(error_packet, packet.sequece_id_);
                return;
            }
            UPLOAD_DEBUG("AcceptNewUploadingPeer=" << packet.end_point << " uploading_peers=" << accept_uploading_peers_.size());
            accept_uploading_peers_.insert(packet.end_point);
            uploading_peers_speed_[packet.end_point] = std::make_pair(0, framework::timer::TickCounter());
        }

        accept_connecting_peers_[packet.end_point].last_data_time.reset();

        // speed
        if (upload_limiter_.GetSpeedLimitInKBps() == 0)
        {
            return;
        }

        storage::Instance::p inst = boost::dynamic_pointer_cast<storage::Instance>(storage::Storage::Inst()->GetInstanceByRID(packet.resource_id_, false));
        if (!inst)
        {
            // error
            protocol::ErrorPacket error_packet((protocol::PeerPacket const &)packet);
            error_packet.peer_guid_ = AppModule::Inst()->GetPeerGuid();
            error_packet.error_code_ =  protocol::ErrorPacket::PPV_SUBPIECE_NO_RESOURCEID;
            AppModule::Inst()->DoSendPacket(error_packet, packet.sequece_id_);
        }
        else if (true == accept_connecting_peers_[packet.end_point].IsInLastDataTransIDs(packet.transaction_id_))
        {
            // directly ignore
            LOGX(__DEBUG, "upload", "true == accept_connecting_peers_[" << packet.end_point << "].IsInLastDataTransIDs(" << packet.transaction_id_ << ")");
        }
        else
        {
            // update data trans id
            accept_connecting_peers_[packet.end_point].UpdateLastDataTransID(packet.transaction_id_);
            LOGX(__DEBUG, "upload", "accept_connecting_peers_[" << packet.end_point << "].UpdateLastDataTransID(" << packet.transaction_id_ << "), All: [ "
                << accept_connecting_peers_[packet.end_point].last_data_trans_ids[0] << ", "
                << accept_connecting_peers_[packet.end_point].last_data_trans_ids[1] << ", "
                << accept_connecting_peers_[packet.end_point].last_data_trans_ids[2] << ", "
                << accept_connecting_peers_[packet.end_point].last_data_trans_ids[3] << ", "
                << accept_connecting_peers_[packet.end_point].last_data_trans_ids[4] << " ]");
            // fetch data
            protocol::SubPieceInfo sub_piece_info;
            std::vector<protocol::SubPieceInfo> request_subpieces = packet.subpiece_infos_;
            for (uint32_t i = 0; i < request_subpieces.size(); i ++)
            {
                sub_piece_info = request_subpieces[i];
                // 发起的请求越界
                P2P_EVENT("UploadManager::OnRequestSubPiecePacket endpoint: " << packet.end_point << sub_piece_info);
                if (sub_piece_info.GetPosition(inst->GetBlockSize()) > inst->GetFileLength())
                    continue;

                protocol::SubPieceBuffer tmp_buf;
                // 尝试从cache中读取subpiece
                ++apply_subpiece_num_;
                if (GetSubPieceFromCache(sub_piece_info, packet.resource_id_, tmp_buf))
                {
                    ++get_from_cache_;
                    OnAsyncGetSubPieceSucced(packet.resource_id_, sub_piece_info, packet.end_point,
                        tmp_buf, packet, packet.priority_, packet.sequece_id_);
                }
                else  // cache中没有，加入需要资源的队列中
                {
                    // apply队列中以前没有该subpiece所在的block，向instance请求block
                    if (false == AddApplySubPiece(sub_piece_info, packet.resource_id_, packet.end_point, packet))
                    {
                        inst->AsyncGetBlock(sub_piece_info.block_index_, shared_from_this());
                    }
                }  // else
            }  // for
        }  // else
    }


    void UploadManager::OnRequestSubPiecePacketOld(protocol::RequestSubPiecePacketOld const & packet)
    {
        if (is_running_ == false) return;

        // 判断此时是否允许上传
        if (is_disable_upload_)
        {
            return;
        }

        // 检查 Storage 中是否存在 该 RID 对应的 Instance
        // IIinstace::p = Storage::Inst()->GetInstance(rid)
        // if (!p)
        // {
        //    这种情况就是 不存在 该 RID 对应的 Instance
        //    不存在的 返回一个 Error 报文 RequestSubPieecFailed Becuase RID Not Exist
        //    return;
        // }
        // 否则
        //    异步调用 IInstance->AsyncGetSubPiece

        // connection
        P2P_EVENT("UploadManager::OnRequestSubPiecePacketOld " << packet.end_point);
        if (accept_connecting_peers_.find(packet.end_point) == accept_connecting_peers_.end())
            // 如果不是在限定连接之内则回错误报文
        {
            P2P_EVENT("UploadManager::OnRequestSubPiecePacketOld No Such EndPoint,  response error packet" << packet.end_point);

            protocol::ErrorPacket error_packet((protocol::PeerPacket const &)packet);
            error_packet.peer_guid_ = AppModule::Inst()->GetPeerGuid();
            error_packet.error_code_ =  protocol::ErrorPacket::PPV_SUBPIECE_NO_RESOURCEID;
            AppModule::Inst()->DoSendPacket(error_packet, packet.sequece_id_);
            return;
        }

        P2P_EVENT("UploadManager::OnRequestSubPiecePacketOld 11111" << packet.end_point);

        // talked
        accept_connecting_peers_[packet.end_point].last_talk_time.reset();

        // upload
        if (accept_uploading_peers_.find(packet.end_point) == accept_uploading_peers_.end())
        {
            if (IsUploadConnectionFull(packet.end_point))
            {
                protocol::ErrorPacket error_packet((protocol::PeerPacket const &)packet);
                error_packet.peer_guid_ = AppModule::Inst()->GetPeerGuid();
                error_packet.error_code_ =  protocol::ErrorPacket::PPV_CONNECT_CONNECTION_FULL;
                AppModule::Inst()->DoSendPacket(error_packet, packet.sequece_id_);
                return;
            }
            UPLOAD_DEBUG("AcceptNewUploadingPeer=" << packet.end_point << " uploading_peers=" << accept_uploading_peers_.size());
            accept_uploading_peers_.insert(packet.end_point);
            uploading_peers_speed_[packet.end_point] = std::make_pair(0, framework::timer::TickCounter());
        }

        accept_connecting_peers_[packet.end_point].last_data_time.reset();

        // speed
        // speed
        if (upload_limiter_.GetSpeedLimitInKBps() == 0)
        {
            LOGX(__DEBUG, "upload", "IsUploadTooMore");
            return;
        }

        storage::Instance::p inst = boost::dynamic_pointer_cast<storage::Instance>(storage::Storage::Inst()->GetInstanceByRID(packet.resource_id_, false));
        if (!inst)
        {
            // error
            protocol::ErrorPacket error_packet((protocol::PeerPacket const &)packet);
            error_packet.peer_guid_ = AppModule::Inst()->GetPeerGuid();
            error_packet.error_code_ =  protocol::ErrorPacket::PPV_SUBPIECE_NO_RESOURCEID;
            AppModule::Inst()->DoSendPacket(error_packet, packet.sequece_id_);
        }
        else if (true == accept_connecting_peers_[packet.end_point].IsInLastDataTransIDs(packet.transaction_id_))
        {
            // directly ignore
            LOGX(__DEBUG, "upload", "true == accept_connecting_peers_[" << packet.end_point << "].IsInLastDataTransIDs(" << packet.transaction_id_ << ")");
        }
        else
        {
            // update data trans id
            accept_connecting_peers_[packet.end_point].UpdateLastDataTransID(packet.transaction_id_);
            LOGX(__DEBUG, "upload", "accept_connecting_peers_[" << packet.end_point << "].UpdateLastDataTransID(" << packet.transaction_id_ << "), All: [ "
                << accept_connecting_peers_[packet.end_point].last_data_trans_ids[0] << ", "
                << accept_connecting_peers_[packet.end_point].last_data_trans_ids[1] << ", "
                << accept_connecting_peers_[packet.end_point].last_data_trans_ids[2] << ", "
                << accept_connecting_peers_[packet.end_point].last_data_trans_ids[3] << ", "
                << accept_connecting_peers_[packet.end_point].last_data_trans_ids[4] << " ]");
            // fetch data
            protocol::SubPieceInfo sub_piece_info;
            std::vector<protocol::SubPieceInfo> request_subpieces = packet.subpiece_infos_;
            for (uint32_t i = 0; i < request_subpieces.size(); i ++)
            {
                sub_piece_info = request_subpieces[i];
                // 发起的请求越界
                P2P_EVENT("UploadManager::OnRequestSubPiecePacketOld endpoint: " << packet.end_point << sub_piece_info);
                if (sub_piece_info.GetPosition(inst->GetBlockSize()) > inst->GetFileLength())
                    continue;

                protocol::SubPieceBuffer tmp_buf;
                // 尝试从cache中读取subpiece
                ++apply_subpiece_num_;
                if (GetSubPieceFromCache(sub_piece_info, packet.resource_id_, tmp_buf))
                {
                    ++get_from_cache_;
                    OnAsyncGetSubPieceSucced(packet.resource_id_, sub_piece_info, packet.end_point,
                        tmp_buf, packet, protocol::RequestSubPiecePacket::DEFAULT_PRIORITY, packet.sequece_id_);
                }
                else  // cache中没有，加入需要资源的队列中
                {
                    // apply队列中以前没有该subpiece所在的block，向instance请求block
                    if (false == AddApplySubPiece(sub_piece_info, packet.resource_id_, packet.end_point, packet))
                    {
                        inst->AsyncGetBlock(sub_piece_info.block_index_, shared_from_this());
                    }
                }  // else
            }  // for
        }  // else
    }


    void UploadManager::OnConnectPacket(protocol::ConnectPacket const & packet)
    {
        if (is_running_ == false)
            return;

        // 判断此时是否允许上传
        if (is_disable_upload_)
        {
            return;
        }

        if (protocol::LIVE_PACKET_TYPE == packet.packet_type_)  // live
        {
            OnLiveConnectPacket(packet);
        }
        else  // vod
        {
            OnVodConnectPacket(packet);
        }
    }

    void UploadManager::OnLiveConnectPacket(protocol::ConnectPacket const & packet)
    {
        // 收到直播的connect packet之后，总是允许连
        UPLOAD_DEBUG("UploadManager::OnConnectPacket endpoint: " << packet.end_point);

        storage::LiveInstance::p live_inst =
            boost::dynamic_pointer_cast<storage::LiveInstance>(storage::Storage::Inst()->GetLiveInstanceByRid(packet.resource_id_));

        if (!live_inst) // no such instance
        {
            // error
            protocol::ErrorPacket error_packet(packet.transaction_id_, packet.resource_id_, AppModule::Inst()->GetPeerGuid(),
                protocol::ErrorPacket::PPV_CONNECT_NO_RESOURCEID, 0, "", packet.end_point);
            AppModule::Inst()->DoSendPacket(error_packet, packet.protocol_version_);
            UPLOAD_DEBUG("No Such Instance: " << packet.resource_id_);
        }
        else
        {
            // ReConnect
            protocol::ConnectPacket connect_packet(packet.transaction_id_, protocol::LIVE_PACKET_TYPE, live_inst->GetRID(),
                AppModule::Inst()->GetPeerGuid(),  protocol::PEER_VERSION, 0x01, packet.send_off_time_,
                AppModule::Inst()->GetPeerVersion(), AppModule::Inst()->GetCandidatePeerInfo(),
                AppModule::Inst()->GetPeerDownloadInfo(), // global download info
                packet.end_point);

            AppModule::Inst()->DoSendPacket(connect_packet, packet.sequece_id_);
            AppModule::Inst()->DoSendPacket(connect_packet, packet.sequece_id_);

            UPLOAD_DEBUG("AcceptPeer: RID = " << packet.resource_id_
                << ", Endpoint = " << packet.end_point << ", TransID = " << packet.transaction_id_);

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

    void UploadManager::OnVodConnectPacket(protocol::ConnectPacket const & packet)
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

        UPLOAD_DEBUG("UploadManager::OnConnectPacket endpoint: " << packet.end_point);

        // 正在看直播或者是连接已经满了，则回错误报文
        if ((true == is_watching_live_)
            || (IsConnectionFull(packet.ip_pool_size_)
            && accept_connecting_peers_.find(packet.end_point) == accept_connecting_peers_.end()))
        {
            UPLOAD_DEBUG("IsConnectionFull Reject endpoint: " << packet.end_point);

            protocol::ErrorPacket error_packet((protocol::PeerPacket const &)packet);
            error_packet.peer_guid_ = AppModule::Inst()->GetPeerGuid();
            error_packet.error_code_ =  protocol::ErrorPacket::PPV_CONNECT_CONNECTION_FULL;
            AppModule::Inst()->DoSendPacket(error_packet, packet.sequece_id_);

            return;
        }

        storage::Instance::p inst = boost::dynamic_pointer_cast<storage::Instance>(storage::Storage::Inst()->GetInstanceByRID(packet.resource_id_));
        if (!inst)  // no such instance
        {
            // error
            protocol::ErrorPacket error_packet((protocol::PeerPacket const &)packet);
            error_packet.peer_guid_ = AppModule::Inst()->GetPeerGuid();
            error_packet.error_code_ =  protocol::ErrorPacket::PPV_CONNECT_NO_RESOURCEID;
            AppModule::Inst()->DoSendPacket(error_packet, packet.sequece_id_);
            UPLOAD_DEBUG("No Such Instance: " << packet.resource_id_);
        }
        else
        {
            // ReConnect
            protocol::ConnectPacket connect_packet(packet.transaction_id_, protocol::VOD_PACKET_TYPE, inst->GetRID(), 
                AppModule::Inst()->GetPeerGuid(), protocol::PEER_VERSION, 0x01, packet.send_off_time_,
                AppModule::Inst()->GetPeerVersion(), AppModule::Inst()->GetCandidatePeerInfo(),
                AppModule::Inst()->GetPeerDownloadInfo(),  // global download info
                packet.end_point);

            AppModule::Inst()->DoSendPacket(connect_packet, packet.sequece_id_);
            AppModule::Inst()->DoSendPacket(connect_packet, packet.sequece_id_);

            UPLOAD_DEBUG("AcceptPeer: RID = " << packet.resource_id_
                << ", IsOpenService = " << inst->IsOpenService() << ", Endpoint = " << packet.end_point
                << ", TransID = " << packet.transaction_id_);

            // accept
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
            protocol::AnnouncePacket announce_packet(protocol::Packet::NewTransactionID(), inst->GetRID(),
                AppModule::Inst()->GetPeerGuid(), AppModule::Inst()->GetPeerDownloadInfo(inst->GetRID()),
                *(inst->GetBlockMap()), packet.end_point);

            AppModule::Inst()->DoSendPacket(announce_packet, packet.sequece_id_);

            // response ridinfo
            protocol::RidInfo rid_info;
            inst->GetRidInfo(rid_info);

            protocol::PEER_COUNT_INFO peer_count_info;
            P2PDownloader::p p2p_downloader = P2PModule::Inst()->GetP2PDownloader(rid_info.GetRID());
            if (p2p_downloader)
            {
                peer_count_info = p2p_downloader->GetPeerCountInfo();
            }

            protocol::RIDInfoResponsePacket ridinfo_packet (protocol::Packet::NewTransactionID(), AppModule::Inst()->GetPeerGuid(),
                rid_info, peer_count_info, packet.end_point);

            AppModule::Inst()->DoSendPacket(ridinfo_packet, packet.sequece_id_);
        }

    }

    boost::uint16_t UploadManager::GetPeerVersionFromPacket(const protocol::Packet& packet) const
    {
        if (packet.PacketAction == protocol::RequestSubPiecePacket::Action)
        {
            return ((protocol::RequestSubPiecePacket const &)packet).sequece_id_;
        }
        
        assert(packet.PacketAction == protocol::RequestSubPiecePacketOld::Action);
        return ((protocol::RequestSubPiecePacketOld const &)packet).sequece_id_;
    }

    void UploadManager::OnAsyncGetSubPieceSucced(const RID& rid, protocol::SubPieceInfo const& subpiece_info,
        boost::asio::ip::udp::endpoint const& end_point, protocol::SubPieceBuffer buffer,
        protocol::Packet const & packet, uint32_t priority, uint16_t request_peer_version)
    {
        if (is_running_ == false) return;

        P2P_EVENT("UploadManager::OnAsyncGetSubPieceSucced endpoint: " << end_point << " SendPacketSuccess: " << subpiece_info);

        // 异步GetSubPiece 成功
        // 将 buffer 内容合成一个 SubPiece 报文，然后向该 end_point 发送

        storage::Instance::p inst = boost::dynamic_pointer_cast<storage::Instance>(storage::Storage::Inst()->GetInstanceByRID(rid, false));
        if (inst)
        {
            inst->WeUploadSubPiece(1);
        }

        protocol::SubPiecePacket subpiece_packet(packet.transaction_id_, rid, AppModule::Inst()->GetPeerGuid(),
            subpiece_info.GetSubPieceInfoStruct(), buffer.Length(), buffer, packet.end_point);

        bool ignoreUploadSpeedLimit = IsPeerFromSameSubnet(end_point);

        upload_limiter_.SendPacket(
                            subpiece_packet,
                            ignoreUploadSpeedLimit,
                            priority,
                            request_peer_version);

        accept_connecting_peers_[end_point].speed_info.SubmitUploadedBytes(SUB_PIECE_SIZE);
    }

    void UploadManager::OnAsyncGetSubPieceFailed(const RID& rid, protocol::SubPieceInfo const& subpiece_info,
        boost::asio::ip::udp::endpoint const& end_point, int failed_code, protocol::Packet const & packet)
    {
        if (is_running_ == false) return;
        // 向对方返回一个Error报文

        P2P_EVENT("UploadManager::OnAsyncGetSubPieceFailed endpoint: " << end_point << " SendPacketFaild: " << subpiece_info);

        protocol::SubPieceBuffer buffer;
        protocol::ErrorPacket error_packet((protocol::PeerPacket const &)packet);
        error_packet.peer_guid_ = AppModule::Inst()->GetPeerGuid();
        error_packet.error_code_ =  protocol::ErrorPacket::PPV_SUBPIECE_SUBPIECE_NOT_FOUND;

        if (packet.PacketAction == protocol::RequestSubPiecePacket::Action)
        {
            AppModule::Inst()->DoSendPacket(error_packet, 
                ((protocol::RequestSubPiecePacket const &)packet).sequece_id_);
        }
        else
        {
            assert(packet.PacketAction == protocol::RequestSubPiecePacketOld::Action);
            AppModule::Inst()->DoSendPacket(error_packet, 
                ((protocol::RequestSubPiecePacketOld const &)packet).sequece_id_);
        }        
    }

    // 收到Storage上传的block，检查需要资源的subpiece，合成报文并发送。然后对cache执行淘汰策略
    void UploadManager::OnAsyncGetBlockSucced(const RID& rid, uint32_t block_index, base::AppBuffer const & buffer)
    {
        if (false == is_running_) { return; }
        P2P_EVENT("UploadManager::OnAsyncGetBlockSucced RID: " << rid << " SendPacketSuccess: " << block_index);

        // 遍历RBIndex所对应的需要获取的subpiece，发送SubPiece报文
        RBIndex rb_index;
        rb_index.block_index = block_index;
        rb_index.rid = rid;
        NeedResourceMapIterator it = need_resource_map_.find(rb_index);
        if (it != need_resource_map_.end())
        {
            ApplyListPtr apply_list = it->second;
            for (ApplyList::iterator itl = apply_list->begin(); itl != apply_list->end(); ++itl)
            {
                // 获取subpiece对应的buf，合成subpiece报文并发送
                ApplySubPiece apply_subpiece = *itl;
                protocol::SubPieceBuffer subpiece_buf = GetSubPieceFromBlock(apply_subpiece.subpiece_info, rid, buffer);

                if (subpiece_buf)
                {
                    OnAsyncGetSubPieceSucced(rb_index.rid, apply_subpiece.subpiece_info, apply_subpiece.end_point,
                        subpiece_buf, apply_subpiece.packet, apply_subpiece.priority, apply_subpiece.request_peer_version_);
                }
            }
            need_resource_map_.erase(it);
        }

        // 执行淘汰策略
        RidBlock rid_block;
        rid_block.index = rb_index;
        rid_block.buf = buffer;
        rid_block.touch_time_counter_.reset();
        std::list<RidBlock>::iterator i = cache_list_.begin();
        for (;i != cache_list_.end(); ++i)
        {
            if ((*i).index == rb_index)
            {
                break;
            }
        }
        if (i != cache_list_.end())
        {
            cache_list_.erase(i);
        }
        cache_list_.push_front(rid_block);
        ShrinkCacheListIfNeeded(max_upload_cache_len_);
    }

    // 获取block失败
    void UploadManager::OnAsyncGetBlockFailed(const RID& rid, boost::uint32_t block_index, int failed_code)
    {
        if (is_running_ == false) return;
        // 向对方返回一个Error报文

        P2P_EVENT("UploadManager::OnAsyncGetBlockFailed RID: " << rid << " SendPacketFaild: " << block_index);
        // 遍历RBIndex所对应的需要获取的subpiece，发送SubPiece报文
        RBIndex rb_index;
        rb_index.block_index = block_index;
        rb_index.rid = rid;
        NeedResourceMapIterator it = need_resource_map_.find(rb_index);
        if (it != need_resource_map_.end())
        {
            ApplyListPtr apply_list = it->second;
            if (apply_list)
            {
                for (ApplyList::iterator itl = apply_list->begin(); itl != apply_list->end(); ++itl)
                {
                    ApplySubPiece apply_subpiece = *itl;
                    OnAsyncGetSubPieceFailed(rb_index.rid, apply_subpiece.subpiece_info, apply_subpiece.end_point,
                        failed_code, apply_subpiece.packet);
                }
            }
            need_resource_map_.erase(it);
        }
    }

    // 加入需要资源的队列，返回值为false时，需要向instance申请block
    bool UploadManager::AddApplySubPiece(const protocol::SubPieceInfo& subpiece_info, const RID& rid, const EndPoint& end_point,
        protocol::Packet const & packet)
    {
        if (false == is_running_)
            return false;

        RBIndex rb_index;
        rb_index.block_index = subpiece_info.block_index_;
        rb_index.rid = rid;
        ApplySubPiece apply_subpiece;
        apply_subpiece.subpiece_info = subpiece_info;
        apply_subpiece.end_point = end_point;
        apply_subpiece.packet = packet;

        if (packet.PacketAction == 0x55)
        {
            // RequestSubPiecePacketOld协议没有priority字段，使用默认优先级
            apply_subpiece.priority = protocol::RequestSubPiecePacket::DEFAULT_PRIORITY;
            const protocol::RequestSubPiecePacketOld & rsp = 
                dynamic_cast<const protocol::RequestSubPiecePacketOld&>(packet);
            apply_subpiece.request_peer_version_ = rsp.sequece_id_;
        }
        else if (packet.PacketAction == 0x5B)
        {
            const protocol::RequestSubPiecePacket & rsp = dynamic_cast<const protocol::RequestSubPiecePacket&>(packet);
            apply_subpiece.priority = rsp.priority_;
            apply_subpiece.request_peer_version_ = rsp.sequece_id_;
        }
        else
        {
            assert(false);
        }

        NeedResourceMapIterator it = need_resource_map_.find(rb_index);
        if (it == need_resource_map_.end())
        {
            ApplyListPtr as_list = CreateApplyList();
            as_list->push_back(apply_subpiece);
            need_resource_map_.insert(std::make_pair(rb_index, as_list));
            return false;
        }
        else
        {
            ApplyListPtr apply_list = it->second;
            ApplyList::nth_index<1>::type& set_index = apply_list->get<1>();
//            std::list<ApplySubPiece>::iterator il = std::find(it->second.begin(), it->second.end(), apply_subpiece);
//            if (il == it->second.end())
            if (set_index.find(apply_subpiece) == set_index.end())
            {
                apply_list->push_back(apply_subpiece);
            }
            return true;
        }
    }

    // 从block的buffer中提取出某个subpiece所需要的资源
    protocol::SubPieceBuffer UploadManager::GetSubPieceFromBlock(const protocol::SubPieceInfo& subpiece_info, const RID& rid, const base::AppBuffer& block_buf)
    {
        if (false == is_running_)
            return protocol::SubPieceBuffer();

        RBIndex rb_index;
        rb_index.block_index = subpiece_info.block_index_;
        rb_index.rid = rid;

        uint32_t block_offset, block_len;
        storage::Instance::p inst = boost::dynamic_pointer_cast<storage::Instance>(storage::Storage::Inst()->GetInstanceByRID(rb_index.rid, false));
        assert(inst);
        if (inst)
        {
            inst->GetBlockPosition(rb_index.block_index, block_offset, block_len);
            uint32_t sub_offset, sub_len;
            inst->GetSubPiecePosition(subpiece_info, sub_offset, sub_len);
            STORAGE_TEST_DEBUG("block index:" << rb_index.block_index << "--block offset, block len:<"
                << block_offset << "," << block_len << ">--sub offset, sub len:<" << sub_offset << "," << sub_len << ">"
                << "--buf len:" << block_buf.Length());
            assert(sub_offset >= block_offset);
            assert(sub_len <= block_len);
            assert(block_len == block_buf.Length());
            uint32_t start = sub_offset - block_offset;

            if (start + sub_len > block_buf.Length() || sub_offset < block_offset || sub_len > block_len || block_len
                != block_buf.Length())
            {
                return protocol::SubPieceBuffer();
            }
            else
            {
                protocol::SubPieceBuffer buf(new protocol::SubPieceContent, sub_len);
                if (buf) {
                    base::util::memcpy2(buf.Data(), buf.Length(), block_buf.Data() + start, sub_len);
                }
                return buf;
            }
        }
        else
        {
            return protocol::SubPieceBuffer();
        }
    }

    // 尝试从cache中获取subpiece，获取成功，则执行淘汰策略
    bool UploadManager::GetSubPieceFromCache(const protocol::SubPieceInfo& subpiece_info, const RID& rid, protocol::SubPieceBuffer& buf)
    {
        if (false == is_running_)
            return false;

        RBIndex rb_index;
        rb_index.block_index = subpiece_info.block_index_;
        rb_index.rid = rid;
        bool if_find = false;
        std::list<RidBlock>::iterator it = cache_list_.begin();
        for (; it != cache_list_.end(); ++it)
        {
            if (it->index == rb_index)
            {
                if_find = true;
                buf = GetSubPieceFromBlock(subpiece_info, rid, it->buf);
                break;
            }
        }
        // 执行淘汰策略，push到前面去
        if (if_find)
        {
            RidBlock rid_block;
            // rid_block.buf = it->buf.Clone();
            rid_block.buf = it->buf;
            rid_block.index = rb_index;
            rid_block.touch_time_counter_.reset();

            cache_list_.erase(it);
            cache_list_.push_front(rid_block);
            return true;
        }
        return false;
    }

    void UploadManager::OnUploadControl(uint32_t times)
    {
        if (false == is_running_)
            return;

        // 根据历史最大上传速度做限制
        if (times % 4 == 0)
        {
            UPLOAD_DEBUG("ConnectionLimit=" << accept_connecting_peers_.size() << "/" << upload_param_.GetMaxConnectPeers()
                << " UploadConnectionLimit=" << accept_uploading_peers_.size() << "/" << upload_param_.GetMaxUploadPeers()
                << " UploadSpeedLimit=" << statistic::StatisticModule::Inst()->GetUploadDataSpeed()/1024.0 << "/" << upload_param_.GetMaxSpeedInKBps());

            if (P2SPConfigs::UPLOAD_BOOL_CONTROL_MODE)
            {
                UPLOAD_DEBUG("P2SPConfigs::UPLOAD_SPEED_LIMIT working");
                UpdateSpeedLimit(P2SPConfigs::UPLOAD_SPEED_LIMIT);
                return;
            }

            if (times % (4*60) == 0)
            {
                recent_play_series_ <<= 1;
            }

            if (times % (4*5) == 0)
            {
                if (p2sp::ProxyModule::Inst()->IsWatchingMovie())
                {
                    recent_play_series_ |= 1;
                }
            }

            bool is_main_state = ((AppModule::Inst()->GetPeerState() & 0xFFFF0000) == PEERSTATE_MAIN_STATE);
            bool is_watching_live = ((AppModule::Inst()->GetPeerState() & 0x0000ffff) == PEERSTATE_LIVE_WORKING);
            if (BootStrapGeneralConfig::Inst()->GetUploadPolicy() == BootStrapGeneralConfig::policy_ping
                && is_main_state && !is_watching_live && network_quality_monitor_->IsWorking())
            {
                UploadControlOnPingPolicy();
                return;
            }

            // status
            bool is_download_with_slowmode = p2sp::ProxyModule::Inst()->IsDownloadWithSlowMode();
            bool is_downloading_movie = p2sp::ProxyModule::Inst()->IsDownloadingMovie();
            bool is_http_downloading = p2sp::ProxyModule::Inst()->IsHttpDownloading();
            bool is_p2p_downloading = p2sp::ProxyModule::Inst()->IsP2PDownloading();
            bool is_watching_movie = p2sp::ProxyModule::Inst()->IsWatchingMovie();            
            uint32_t idle_time_in_seconds = storage::Performance::Inst()->GetIdleInSeconds();
            uint32_t upload_bandwidth = GetMaxUploadSpeedForControl();
            boost::uint32_t revised_up_speedlimit = (std::max)((boost::int32_t)upload_bandwidth - 262144, boost::int32_t(0));     // 超过256KBps的上传带宽

            if (false == is_watching_live_ && p2sp::ProxyModule::Inst()->IsWatchingLive() == true)
            {
                KickVodUploadConnections();
            }

            is_watching_live_ = p2sp::ProxyModule::Inst()->IsWatchingLive();

            desktop_type_ = storage::Performance::Inst()->GetCurrDesktopType();
            if (false == is_locking_)
            {
                if (storage::DT_WINLOGON == desktop_type_ || storage::DT_SCREEN_SAVER == desktop_type_)
                {
                    UPLOAD_DEBUG("WINLOGON or SCREEN_SAVER");
                    if (idle_time_in_seconds >= 5) {
                        UPLOAD_DEBUG("WINLOGON or SCREEN_SAVER, Idle Seconds >= 5, Locking!");
                        is_locking_ = true;
                    }
                }
            }
            else
            {
                if (storage::DT_DEFAULT == desktop_type_)
                {
                    UPLOAD_DEBUG("From Locking to DEFAULT deskt_type");
                    is_locking_ = false;
                }
            }

            // info
            UPLOAD_DEBUG("----- Upload Control -----");
            UPLOAD_DEBUG(" Idle Time in Seconds: " << idle_time_in_seconds);
            UPLOAD_DEBUG(" Desk Type: " << desktop_type_);
            UPLOAD_DEBUG(" Upload Bandwidth (KBps): " << (upload_bandwidth/1024.0));

            // locking
            if (true == is_locking_)
            {
                UPLOAD_DEBUG("User Locking");
                UpdateSpeedLimit(-1);
            }
            // live
            else if (true == is_watching_live_)
            {
                UPLOAD_DEBUG("User Watching Live Video");
                boost::int32_t speed_limit = revised_up_speedlimit > 0 ? revised_up_speedlimit : upload_bandwidth * 9 / 10;
                UpdateSpeedLimit(speed_limit);
            }
            // slow down mode
            else if (is_download_with_slowmode)
            {
                UPLOAD_DEBUG("SLOW MODE");
                boost::int32_t speed_limit = revised_up_speedlimit > 0 ? revised_up_speedlimit : std::min(static_cast<uint32_t>(upload_bandwidth * 0.3 + 0.5), (uint32_t)15 * 1024);
                UpdateSpeedLimit(speed_limit);
            }
            // downloading
            else if (true == is_downloading_movie && is_http_downloading)
            {
                if (is_p2p_downloading)
                {
                    UPLOAD_DEBUG("User Downloading Movie with http & p2p");
                    // boost::int32_t speed_limit = static_cast<boost::int32_t>(upload_bandwidth * 0.5 + 0.5);
                    boost::int32_t speed_limit = revised_up_speedlimit > 0 ? revised_up_speedlimit : 20 * 1024;
                    UpdateSpeedLimit(speed_limit);
                }
                else
                {
                    UPLOAD_DEBUG("User Downloading Movie with http");
                    // boost::int32_t speed_limit = static_cast<boost::int32_t>(upload_bandwidth * 0.5 + 0.5);
                    boost::int32_t speed_limit = revised_up_speedlimit > 0 ? revised_up_speedlimit : 32 * 1024;
                    UpdateSpeedLimit(speed_limit);
                }
            }
            else if (true == is_downloading_movie && !is_http_downloading)
            {
                UPLOAD_DEBUG("User Downloading Movie without http");
                // boost::int32_t speed_limit = static_cast<boost::int32_t>(upload_bandwidth * 0.5 + 0.5);
                boost::int32_t speed_limit = revised_up_speedlimit > 0 ? revised_up_speedlimit : static_cast<boost::int32_t>(upload_bandwidth * 0.5 + 0.5);
                UpdateSpeedLimit(speed_limit);
            }
            // watching or 5min after watching
            else if (true == is_watching_movie)
            {
                UPLOAD_DEBUG("User Watching Movie");
                boost::int32_t speed_limit = -1;
                UpdateSpeedLimit(speed_limit);
            }
            else if (is_main_state && (recent_play_series_ & 0x1Fu) > 0)
            {
                UPLOAD_DEBUG("User In Main State, Watched Movie in last 5 minutes");
                // boost::int32_t speed_limit = static_cast<boost::int32_t>(upload_bandwidth * 0.9 + 0.5);
                boost::int32_t speed_limit = revised_up_speedlimit > 0 ? (revised_up_speedlimit + 235930) : static_cast<boost::int32_t>(upload_bandwidth * 0.9 + 0.5);  // 235930 = 256*1024*0.9
                UpdateSpeedLimit(speed_limit);
            }
            // [0, 1)
            else if (idle_time_in_seconds >= 0 && idle_time_in_seconds < 1 * 60)
            {
                UPLOAD_DEBUG("User Idle [0, 1)");
                // boost::int32_t speed_limit = static_cast<boost::int32_t>(upload_bandwidth * 0.3 + 0.5);
                boost::int32_t speed_limit = revised_up_speedlimit > 0 ? revised_up_speedlimit : std::min(static_cast<uint32_t>(upload_bandwidth * 0.3 + 0.5), (uint32_t)32 * 1024);
                // speed_limit = min(speed_limit, 32 * 1024);  // 32KBps
                UpdateSpeedLimit(speed_limit);
            }
            // [1, 5)
            else if (idle_time_in_seconds >= 1 && idle_time_in_seconds < 5 * 60)
            {
                UPLOAD_DEBUG("User Idle [1, 5)");
                // boost::int32_t speed_limit = static_cast<boost::int32_t>(upload_bandwidth * 0.3 + 0.5);
                // speed_limit = min(speed_limit, 32 * 1024);  // 32KBps
                // 78643 = 256*1024*0.3
                boost::int32_t speed_limit = revised_up_speedlimit > 0 ? (revised_up_speedlimit  + 78643) : std::min(static_cast<uint32_t>(upload_bandwidth * 0.5 + 0.5), (uint32_t)32 * 1024);
                UpdateSpeedLimit(speed_limit);
            }
            // [5, 20)
            else if (idle_time_in_seconds >= 5 * 60 && idle_time_in_seconds < 20 * 60)
            {
                UPLOAD_DEBUG("User Idle [5, 20)");
                // boost::int32_t speed_limit = static_cast<boost::int32_t>(upload_bandwidth * 0.7 + 0.5);
                // speed_limit = min(speed_limit, 128 * 1024);  // 128KBps
                // 183501 = 256*1024*0.7
                boost::int32_t speed_limit = revised_up_speedlimit > 0 ? (revised_up_speedlimit  + 183501) : std::min(static_cast<uint32_t>(upload_bandwidth * 0.8 + 0.5), (uint32_t)128 * 1024);
                UpdateSpeedLimit(speed_limit);
            }
            // [20, + Inf)
            else if (idle_time_in_seconds >= 20 * 60)
            {
                UPLOAD_DEBUG("User Idle [20, +Inf)");
                boost::int32_t speed_limit = -1;
                UpdateSpeedLimit(speed_limit);
            }
            else
            {
                UPLOAD_DEBUG("User ????");
            }
        }  // 每秒
    }

    void UploadManager::UpdateSpeedLimit(boost::int32_t speed_limit)
    {
        if (false == is_running_)
            return;
        UPLOAD_DEBUG("SpeedLimit = " << speed_limit);

        if (speed_limit < 0)
        {
            upload_speed_limit_tracker_.SetUploadWithoutLimit(true);
            SetUploadSpeedLimitInKBps(speed_limit);
            SetUploadMaxConnectionCount(P2SPConfigs::UPLOAD_MAX_CONNECT_PEER_COUNT);
            SetUploadMaxUploadConnectionCount(P2SPConfigs::UPLOAD_MAX_UPLOAD_PEER_COUNT);
        }
        else
        {
            upload_speed_limit_tracker_.SetUploadWithoutLimit(false);
            boost::uint32_t speed_limit_in_KBps = static_cast<boost::uint32_t>(speed_limit / 1024.0 + 0.5);
            boost::uint32_t conn_count = static_cast<boost::uint32_t>(speed_limit_in_KBps / 8);
            LIMIT_MIN_MAX(conn_count, 1, P2SPConfigs::UPLOAD_MAX_UPLOAD_PEER_COUNT);
            SetUploadMaxConnectionCount(conn_count + 5);
            SetUploadMaxUploadConnectionCount(conn_count);
            SetUploadSpeedLimitInKBps(speed_limit_in_KBps);
        }

        UPLOAD_DEBUG("UploadMaxConnectionCount=" << upload_param_.GetMaxConnectPeers());
        UPLOAD_DEBUG("UploadMaxUploadConnectionCount=" << upload_param_.GetMaxUploadPeers());
        UPLOAD_DEBUG("UploadSpeedLimitInKBps=" << upload_param_.GetMaxSpeedInKBps());

    }

    void UploadManager::UpdateParameters(boost::int32_t miniutes, boost::int32_t type)
    {
        if (false == is_running_)
            return;

        UPLOAD_DEBUG("minutes = " << miniutes << ", type = " << type);

        uint32_t bandwidth = statistic::StatisticModule::Inst()->GetBandWidth();
        uint32_t bandwidth_limit = 256 * 1024;

        uint32_t speed_limit_in_KBps;

        uint32_t modified_minutes = 0;
        if (miniutes < 20)
            modified_minutes = miniutes / 2;
        else if (miniutes < 40)
            modified_minutes = 10 + (miniutes - 20);
        else if (miniutes < 60)
            modified_minutes = 30 + 2 * (miniutes - 40);
        else
            modified_minutes = 70;

        UPLOAD_DEBUG("bandwidth = " << bandwidth << ", bandwidth_limit = " << bandwidth_limit);
        if (bandwidth < bandwidth_limit)
        {
            // 小于1M带宽, 上传7KB/S
            speed_limit_in_KBps = (5 + modified_minutes);
            UPLOAD_DEBUG("speed_limit_in_KBps (5 + modified_minutes) = " << speed_limit_in_KBps);

            if (bandwidth >= 128 * 1024)
            {
                // 在1M - 2M带宽，上传15KB/S
                speed_limit_in_KBps = (10 + modified_minutes);
                UPLOAD_DEBUG("speed_limit_in_KBps (10 + modified_minutes) = " << speed_limit_in_KBps);
            }
        }
        else
        {
            // 2M带宽以上
            if (GetMaxUploadSpeedForControl() > 64*1024)
            {
                speed_limit_in_KBps = (15 + modified_minutes);
                UPLOAD_DEBUG("speed_limit_in_KBps (15+modified_minutes) = " << speed_limit_in_KBps);
            }
            else
            {
                speed_limit_in_KBps = (15 + modified_minutes);
                UPLOAD_DEBUG("speed_limit_in_KBps (15+modified_minutes) = " << speed_limit_in_KBps);
            }
        }

        // not watching movie
        if (type == 0)
        {
            SetUploadMaxConnectionCount(speed_limit_in_KBps / 5 + 6);
            SetUploadMaxUploadConnectionCount(speed_limit_in_KBps / 5);
            SetUploadSpeedLimitInKBps(speed_limit_in_KBps);
            UPLOAD_DEBUG("type == 0 UploadMaxConnectionCount=" << upload_param_.GetMaxConnectPeers());
            UPLOAD_DEBUG("type == 0 UploadMaxUploadConnectionCount=" << upload_param_.GetMaxUploadPeers());
            UPLOAD_DEBUG("type == 0 UploadSpeedLimitInKBps=" << upload_param_.GetMaxSpeedInKBps());
        }
        // waching movie
        else if (type == 1)
        {
            SetUploadMaxConnectionCount((std::max)(uint32_t(10), speed_limit_in_KBps / 5 + 6));
            SetUploadMaxUploadConnectionCount((std::max)(uint32_t(5), speed_limit_in_KBps / 5));
            SetUploadSpeedLimitInKBps(speed_limit_in_KBps + 5);
            UPLOAD_DEBUG("type == 1 UploadMaxConnectionCount=" << upload_param_.GetMaxConnectPeers());
            UPLOAD_DEBUG("type == 1 UploadMaxUploadConnectionCount=" << upload_param_.GetMaxUploadPeers());
            UPLOAD_DEBUG("type == 1 UploadSpeedLimitInKBps=" << upload_param_.GetMaxSpeedInKBps());
        }

        upload_speed_limit_tracker_.SetUploadWithoutLimit(false);
    }

    void UploadManager::LoadHistoricalMaxUploadSpeed()
    {
        if (false == is_running_)
            return;

        if (ppva_config_path_.length() > 0)
        {
#ifdef BOOST_WINDOWS_API
            
            boost::filesystem::path configpath(ppva_config_path_);
            configpath /= TEXT("ppvaconfig.ini");
            string filename = configpath.file_string();

            uint32_t upload_velocity = 64*1024;

            try
            {
                framework::configure::Config conf(filename);
                framework::configure::ConfigModule & ppva_um_conf =
                    conf.register_module("PPVA_UM_NEW");

                // IP check
                ppva_um_conf(CONFIG_PARAM_NAME_RDONLY("I", local_ip_from_ini_));
                uint32_t ip_local = CStunClient::GetLocalFirstIP();

                if (local_ip_from_ini_ == ip_local)
                {
                    ppva_um_conf(CONFIG_PARAM_NAME_RDONLY("V", upload_velocity));
                }
            }
            catch(...)
            {
                base::filesystem::remove_nothrow(filename);
            }

            upload_speed_limit_tracker_.Reset(upload_velocity);
#endif
        }
    }

    void UploadManager::SaveHistoricalMaxUploadSpeed()
    {
        if (false == is_running_)
            return;

        if (ppva_config_path_.length() > 0)
        {
#ifdef BOOST_WINDOWS_API
            
            boost::filesystem::path configpath(ppva_config_path_);
            configpath /= TEXT("ppvaconfig.ini");
            string filename = configpath.file_string();

            try
            {
                framework::configure::Config conf(filename);
                framework::configure::ConfigModule & ppva_um_conf =
                    conf.register_module("PPVA_UM_NEW");

                // IP
                uint32_t ip_local;
                // velocity
                uint32_t v;
                ppva_um_conf(CONFIG_PARAM_NAME_RDONLY("I", ip_local));
                ppva_um_conf(CONFIG_PARAM_NAME_RDONLY("V", v));

                ip_local = CStunClient::GetLocalFirstIP();
                v = upload_speed_limit_tracker_.GetMaxUnlimitedUploadSpeedInRecord();

                // store
                conf.sync();
            }
            catch(...)
            {
                base::filesystem::remove_nothrow(filename);
            }
#endif
        }
    }

    void UploadManager::SetUploadUserSpeedLimitInKBps(boost::int32_t user_speed_in_KBps)
    {
        if (false == is_running_)
            return;
        assert(user_speed_in_KBps >= -1);
        upload_param_.SetUserSpeedInKBps(user_speed_in_KBps);
        upload_limiter_.SetSpeedLimitInKBps(upload_param_.GetMaxSpeedInKBps());
    }
    void UploadManager::SetUploadSpeedLimitInKBps(boost::int32_t upload_speed_limit_in_KBps)
    {
        if (false == is_running_)
            return;
        assert(upload_speed_limit_in_KBps >= -1);

        // 提交上传限速的变化，不限速时提交最大历史上传速度
        if (upload_param_.GetMaxSpeedInKBps() != upload_speed_limit_in_KBps)
        {
            if (upload_speed_limit_in_KBps != -1)
            {
                statistic::DACStatisticModule::Inst()->SubmitP2PUploadSpeedLimitInKBps(upload_speed_limit_in_KBps);
            }
            else
            {
                statistic::DACStatisticModule::Inst()->SubmitP2PUploadSpeedLimitInKBps(
                    static_cast<boost::int32_t>(GetMaxUploadSpeedForControl()/1024.0));
            }
        }

        upload_param_.SetMaxSpeedInKBps(upload_speed_limit_in_KBps);
        upload_limiter_.SetSpeedLimitInKBps(upload_param_.GetMaxSpeedInKBps());
    }
    boost::int32_t UploadManager::GetUploadSpeedLimitInKBps() const
    {
        if (false == is_running_) return 0;
        return upload_param_.GetMaxSpeedInKBps();
    }
    void UploadManager::SetUploadMaxConnectionCount(boost::int32_t max_connection_count)
    {
        if (false == is_running_)
            return;
        assert(max_connection_count >= -1);
        upload_param_.SetMaxConnectPeers(max_connection_count);
    }
    void UploadManager::SetUploadMaxUploadConnectionCount(boost::int32_t max_upload_connection_count)
    {
        if (false == is_running_)
            return;
        assert(max_upload_connection_count >= -1);
        upload_param_.SetMaxUploadPeers(max_upload_connection_count);
    }
    void UploadManager::SetUploadControlParam(UploadControlParam param)
    {
        if (false == is_running_)
            return;
        upload_param_ = param;
        upload_limiter_.SetSpeedLimitInKBps(upload_param_.GetMaxSpeedInKBps());
    }

    boost::uint32_t UploadManager::GetMaxUploadSpeedForControl() const
    {
        if (false == is_running_)
            return 0;

        return upload_speed_limit_tracker_.GetMaxUploadSpeedForControl();
    }

    // 设置上传开关，用于控制是否启用上传
    void UploadManager::SetUploadSwitch(bool is_disable_upload)
    {
        is_disable_upload_ = is_disable_upload;
    }

    void UploadManager::SendErrorPacket(protocol::LivePeerPacket const &packet, boost::uint16_t error_code)
    {
        protocol::ErrorPacket error_packet(packet);
        error_packet.peer_guid_ = AppModule::Inst()->GetPeerGuid();
        error_packet.error_code_ =  error_code;
        AppModule::Inst()->DoSendPacket(error_packet, packet.protocol_version_);
    }

    void UploadManager::KickUploadConnections()
    {
        // 如果连接数超过了最大允许的值，则按照速度由小到大首先踢掉连接
        // 如果刚连接上还没超过20秒，则不踢

        boost::int32_t need_kick_count = 0;  // 需要踢掉的连接数
        std::multimap<boost::uint32_t, boost::asio::ip::udp::endpoint> kick_upload_connection_map;

        if (accept_connecting_peers_.size() > upload_param_.GetMaxConnectPeers())
        {
            need_kick_count = accept_connecting_peers_.size() - upload_param_.GetMaxConnectPeers();
            std::map<boost::asio::ip::udp::endpoint, PEER_UPLOAD_INFO>::iterator iter;
            for (iter = accept_connecting_peers_.begin(); iter != accept_connecting_peers_.end(); ++iter)
            {
                kick_upload_connection_map.insert(std::make_pair(iter->second.speed_info.GetSpeedInfo().NowUploadSpeed, iter->first));
            }
        }

        // 已经踢掉的连接个数
        boost::int32_t kicked_count = 0;
        std::multimap<boost::uint32_t, boost::asio::ip::udp::endpoint>::iterator iter_kick = kick_upload_connection_map.begin();
        for (boost::int32_t kicked_count = 0; kicked_count < need_kick_count;)
        {
            if (iter_kick == kick_upload_connection_map.end())
            {
                break;
            }

            boost::asio::ip::udp::endpoint ep = iter_kick->second;
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

    size_t UploadManager::GetCurrentCacheSize() const
    {
        if (!is_running_)
        {
            return 0;
        }

        if (max_upload_cache_len_ == 0)
        {
            return cache_list_.size();
        }

        return std::min<size_t>(max_upload_cache_len_, cache_list_.size());
    }

    void UploadManager::SetCurrentCacheSize(size_t cache_size)
    {
        size_t max_cache_size = cache_size;

        if (max_upload_cache_len_ > 0 && max_upload_cache_len_ < max_cache_size)
        {
            max_cache_size = max_upload_cache_len_;
        }

        ShrinkCacheListIfNeeded(max_cache_size);
    }

    void UploadManager::ShrinkCacheListIfNeeded(size_t cache_size)
    {
        if (cache_size <= 0)
        {
            UPLOAD_DEBUG("Cache size is too small and is ignored.");
            return;
        }

        UPLOAD_DEBUG("CacheListSize = " << cache_list_.size() << ", Max Cache Size = " << cache_size);
        size_t max_cache_size_allowed = std::min<size_t>(accept_uploading_peers_.size() * 2, cache_size);
        while (cache_list_.size() > max_cache_size_allowed)
        {
            cache_list_.pop_back();
        }
    }

    void UploadManager::KickVodUploadConnections()
    {
        std::map<boost::asio::ip::udp::endpoint, PEER_UPLOAD_INFO> accept_connecting_peers_;

        for (std::map<boost::asio::ip::udp::endpoint, PEER_UPLOAD_INFO>::iterator iter = accept_connecting_peers_.begin();
            iter != accept_connecting_peers_.end();)
        {
            if (false == iter->second.is_live)
            {
                accept_uploading_peers_.erase(iter->first);
                uploading_peers_speed_.erase(iter->first);
                accept_connecting_peers_.erase(iter++);
            }
            else
            {
                ++iter;
            }
        }
    }
    
    void UploadManager::UploadControlOnPingPolicy()
    {
        uint32_t upload_speed_kbs = statistic::StatisticModule::Inst()->GetUploadDataSpeedInKBps();
        DebugLog("upload lost_rate:%d\%, avg_delay:%d ms, upload_speed:%d, upload_bd:%d", network_quality_monitor_->GetPingLostRate(),
            network_quality_monitor_->GetAveragePingDelay(), upload_speed_kbs, GetMaxUploadSpeedForControl() / 1024);

        bool is_network_good;
        if (GetMaxUploadSpeedForControl() > 100 * 1024)
        {
            is_network_good = network_quality_monitor_->GetAveragePingDelay() < 20 && 
                network_quality_monitor_->GetPingLostRate() == 0;
        }
        else
        {
            is_network_good = network_quality_monitor_->GetAveragePingDelay() < 80 &&
                network_quality_monitor_->GetPingLostRate() == 0;
        }

        if (network_quality_monitor_->GetPingLostRate() != 0)
        {
            network_quality_monitor_->ClearPingLostRate();
        }

        UpdateSpeedLimit(CalcUploadSpeedLimitOnPingPolicy(is_network_good));
    }

    int32_t UploadManager::CalcUploadSpeedLimitOnPingPolicy(bool is_network_good)
    {
        int32_t upload_speed_limit_kbs = GetUploadSpeedLimitInKBps();
        if (upload_speed_limit_kbs < 0)
        {
            if (is_network_good)
            {
                return upload_speed_limit_kbs;
            }
            else
            {
                return MinUploadSpeedLimitInKbs * 1024;
            }
        }
        else
        {
            if (!is_network_good)
            {
                upload_speed_limit_kbs /= 2;
                LIMIT_MIN(upload_speed_limit_kbs, MinUploadSpeedLimitInKbs);
                DebugLog("upload UploadControlOnPingPolicy dec to %dkb", upload_speed_limit_kbs);
            }
            else
            {
                uint32_t upload_speed_kbs = statistic::StatisticModule::Inst()->GetUploadDataSpeedInKBps();
                if (upload_speed_limit_kbs < upload_speed_kbs + 120 ||
                    upload_speed_limit_kbs < upload_speed_kbs * 12 / 10)
                {
                    upload_speed_limit_kbs *= 1.2;
                    DebugLog("upload UploadControlOnPingPolicy inc to %dkb", upload_speed_limit_kbs);
                }
                else
                {
                    DebugLog("upload UploadControlOnPingPolicy equal to %dkb", upload_speed_limit_kbs);
                }
            }

            return upload_speed_limit_kbs * 1024;
        }
    }
}
