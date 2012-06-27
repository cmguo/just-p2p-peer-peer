//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/tracker/TrackerClient.h"
#include "p2sp/tracker/TrackerModule.h"

#include "p2sp/AppModule.h"
#include "p2sp/stun/StunModule.h"
#include "p2sp/p2p/P2PModule.h"

#include "statistic/StatisticModule.h"
#include <framework/network/Endpoint.h>
#include <limits>

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("tracker");

    void TrackerClient::Start()
    {

        last_response_rid_count_ = 0;
        last_transaction_id_ = 0;
        local_resources_.clear();
        is_sync_ = false;
    }

    void TrackerClient::Stop()
    {
        last_response_rid_count_ = 0;
        last_transaction_id_ = 0;
        local_resources_.clear();
        is_sync_ = false;
        // TRACK_DEBUG("TrackerClient::Stop " << tracker_info_);
    }

    void TrackerClient::SetRidCount(uint32_t rid_count)
    {
        // 上次收到的keepalive或者commit中服务器的存储的本机的资源个数
        last_response_rid_count_ = rid_count;
    }

    void TrackerClient::DoList(const RID& rid, bool list_for_live_udpserver)
    {
        if (list_for_live_udpserver != IsTrackerForLiveUdpServer())
        {
            return;
        }



        last_transaction_id_ = protocol::Packet::NewTransactionID();
        protocol::ListPacket list_request_packet(last_transaction_id_,
                protocol::PEER_VERSION, rid, p2sp::AppModule::Inst()->GetPeerGuid(),
                MAX_REQUEST_PEER_COUNT_, end_point_);

        // 从该TrackerClient发送ListRequestPacket
        LOG(__EVENT, "tracker", "TrackerClient::DoList " << rid << " " << end_point_);

        p2sp::AppModule::Inst()->DoSendPacket(list_request_packet);

        // 统计信息
        statistic::StatisticModule::Inst()->SubmitListRequest(tracker_info_, rid);
    }

    void TrackerClient::OnListResponsePacket(protocol::ListPacket const & packet)
    {
        // 讲List到的peer加入ip pool
        // 将packet解析出 vector<PeerInfo::p> peers
        // LOG(__EVENT, "tracker", "TrackerClient::OnListPacket peers.size()=" << peers.size());
        std::vector<protocol::CandidatePeerInfo> peers = packet.response.peer_infos_;
        for (uint32_t i = 0; i < peers.size(); ++i) {
            if (peers[i].UploadPriority < 255 && peers[i].UploadPriority > 0) {
                peers[i].UploadPriority++;
            }
        }

        p2sp::AppModule::Inst()->AddCandidatePeers(packet.response.resource_id_, peers, is_tracker_for_live_udpserver_);

        // 统计信息
        statistic::StatisticModule::Inst()->SubmitListResponse(tracker_info_, packet.response.peer_infos_.size(), packet.response.resource_id_);
    }

    void TrackerClient::OnReportResponsePacket(protocol::ReportPacket const & packet)
    {
        TRACK_DEBUG("Report Response RID Count: " << last_response_rid_count_);

        // 统计信息
        statistic::StatisticModule::Inst()->SubmitCommitResponse(tracker_info_);

        // IP信息
        UpdateIpStatistic(protocol::SocketAddr(packet.response.detected_ip_, packet.response.detected_udp_port_));

        if (packet.transaction_id_ == last_transaction_id_)
        {
            // 更新updates
            last_response_rid_count_ = packet.response.resource_count_;

            // updates
            STL_FOR_EACH_CONST(std::vector<protocol::REPORT_RESOURCE_STRUCT>, last_updates_, iter)
            {
                if (iter->Type == 1)
                {
                    // add
                    local_resources_.insert(iter->ResourceID);
                }
                else if (iter->Type == 0)
                {
                    // del
                    local_resources_.erase(iter->ResourceID);
                }
                else
                {

                    assert(0);
                }
            }
        }
        else
        {
            TRACK_WARN("TrackerClient::OnReportPacket: Unexpected Transaction ID, " << packet.transaction_id_);
        }
    }

    boost::uint32_t TrackerClient::DoSubmit()
    {
        TRACK_INFO("TrackerClient::DoSubmit ModNO:" << (uint32_t)tracker_info_.ModNo
            << ", IP:" << framework::network::Endpoint(tracker_info_.IP, tracker_info_.Port).to_string());

        uint32_t result = 0;

        /*
        // 首先在m_storage中查找本地资源  set<RID> now_resource_ = AppModule::Inst()->GetLocalResource();
        set<RID> now_resources = GetClientResource();
        // 将他的数量与last_response_rid_count比较
        if (now_resources.size() != last_response_rid_count_ ||
             now_resources.size() != local_resources_.size())
        {
            result = DoReport();
        }
        else
        {
            // 与本地资源比较
            if (now_resources != local_resources_)  // set compare
                result = DoReport();
            else
                result = DoKeepAlive();
        }

        // local_resources_ = now_resources;
        */

        // 服务器会在一个Interval内回包，否则视为超时；
        // 因此，如果服务器回包了，同步资源一定是与服务器同步了；
        // 所以这里使用local_resources_.size()

        // 本地实际资源
        std::set<RID> now_resources = GetClientResource();
        // 打印本地资源和同步资源到release log，正式release版中没有

        // 如果last_response_rid_count为0，表明服务器重启，清空服务器同步资源，并report
        if (last_response_rid_count_ == 0)
        {
            local_resources_.clear();
        }

        // 如果本地资源和同步资源不相同，则进行Report
        if (now_resources.size() != local_resources_.size() || now_resources != local_resources_)
        {
            is_sync_ = false;
            result = DoReport();
        }
        // 如果本地资源和同步资源已经相同，但是服务器上保存的个数小于同步资源个数
        else if (last_response_rid_count_ < local_resources_.size())
        {
            is_sync_ = false;
            // 如果服务器资源太少，重新同步
            if (last_response_rid_count_ <= (uint32_t)(local_resources_.size() * 0.7))
            {
                local_resources_.clear();
            }
            result = DoReport();
        }
        // 如果本地资源和同步资源已经相同，且服务器个数和同步资源个数也相同
        else if (last_response_rid_count_ == local_resources_.size())
        {
            is_sync_ = true;
            // 发送KeepAlive(也就是空Report)
            // ! 空Report的发送可优化
            result = DoReport();
        }
        // 如果本地资源和同步资源已经相同，但是服务器上保存的个数大于同步资源个数
        else if (last_response_rid_count_ > local_resources_.size())
        {
            // Nothing to do! 服务器会累积，直到Peer退出
            // 发送空Report
        }

        return result;
        // 如果不同则return DoCommit();
        // 否则
        //    将其与Local_Resource()比较
        //    如果不同则return DoCommit();
        // 否则         return DoKeepALive();
        // local_resource_ = now_resource_;
    }

    extern void LoadLocalIPs(std::vector<uint32_t>& ipArray);

    /**
     * @return Transaction ID.
     */
    uint32_t TrackerClient::DoReport()
    {
        // 统计信息
        TRACK_INFO("TrackerClient::DoReport ");

        statistic::StatisticModule::Inst()->SubmitCommitRequest(tracker_info_);

        // 将m_storage拼出protocol::CommitRequestPacket发送

        // transaction id
        last_transaction_id_ = protocol::Packet::NewTransactionID();

        // local resource
        std::set<RID> local_resource_set = GetClientResource();


        // 发送50个update资源
        std::vector<protocol::REPORT_RESOURCE_STRUCT> update_resource_set;
        std::set<RID>::const_iterator real_iter, server_iter;
        real_iter = local_resource_set.begin();
        server_iter = local_resources_.begin();
        // merge
        while (update_resource_set.size() < MAX_REPORT_RID_COUNT)
        {
            if (real_iter == local_resource_set.end() && server_iter == local_resources_.end())
            {
                // both end
                break;
            }
            else if (server_iter == local_resources_.end() ||
                (real_iter != local_resource_set.end() && *real_iter < *server_iter))
            {
                protocol::REPORT_RESOURCE_STRUCT update;
                update.ResourceID = *real_iter;
                update.Type = 1;  // ADD
                // push back
                update_resource_set.push_back(update);
//                ++last_response_rid_count_;
                ++real_iter;
            }
            else if (real_iter == local_resource_set.end() ||
                (server_iter != local_resources_.end() && *server_iter < *real_iter))
            {
                protocol::REPORT_RESOURCE_STRUCT update;
                update.ResourceID = *server_iter;
                update.Type = 0;  // DEL
                // push back
                update_resource_set.push_back(update);
//                --last_response_rid_count_;
                ++server_iter;
            }
            else  // should be equal and both real_iter, server_iter not ends
            {
                // assert
                assert(real_iter != local_resource_set.end() && server_iter != local_resources_.end());
                assert(*real_iter == *server_iter);
                // next
                ++real_iter;
                ++server_iter;
            }
        }

        // ip
        std::vector<uint32_t> real_ips;

        // stun info
        uint32_t stun_ip;
        boost::uint16_t stun_port;
        StunModule::Inst()->GetStunEndpoint(stun_ip, stun_port);

        boost::uint32_t stun_detect_ip = statistic::StatisticModule::Inst()->GetLocalPeerInfo().DetectIP;
        boost::uint16_t stun_detect_port = statistic::StatisticModule::Inst()->GetLocalPeerInfo().DetectUdpPort;

        LoadLocalIPs(real_ips);

        boost::uint16_t local_udp_port = p2sp::AppModule::Inst()->GetLocalUdpPort();
        boost::uint16_t local_tcp_port = p2sp::AppModule::Inst()->GetLocalTcpPort();

        boost::int32_t upload_speed_limit_kbs = p2sp::P2PModule::Inst()->GetUploadSpeedLimitInKBps();
        if (upload_speed_limit_kbs < 0)
        {
            upload_speed_limit_kbs = std::numeric_limits<boost::int32_t>::max();
        }

        // request
        protocol::ReportPacket report_request(
                last_transaction_id_,
                protocol::PEER_VERSION,
                p2sp::AppModule::Inst()->GetPeerGuid(),
                local_resource_set.size(),
                local_resources_.size(),
                local_udp_port,
                stun_ip,
                stun_port,
                stun_detect_ip,
                stun_detect_port,
                real_ips,
                update_resource_set,
                StunModule::Inst()->GetPeerNatType(),
                p2sp::AppModule::Inst()->GenUploadPriority(),
                p2sp::AppModule::Inst()->GetIdleTimeInMins(),
                (boost::int32_t)p2sp::P2PModule::Inst()->GetUploadBandWidthInKBytes(),
                upload_speed_limit_kbs,
                (boost::int32_t)statistic::StatisticModule::Inst()->GetUploadDataSpeedInKBps(),
                end_point_,
                local_tcp_port,
                p2sp::AppModule::Inst()->GetUpnpPortForTcpUpload()
           );

        LOG(__DEBUG, "tracker", "DoReport, TrackerInfo: " << end_point_);

        // post
        p2sp::AppModule::Inst()->DoSendPacket(report_request);

        last_updates_ = update_resource_set;

        return last_transaction_id_;
    }

    const protocol::TRACKER_INFO& TrackerClient::GetTrackerInfo() const
    {
        return tracker_info_;
    }

    void TrackerClient::SetGroupCount(uint32_t group_count)
    {
        group_count_ = group_count;
    }

    uint32_t TrackerClient::GetGroupCount() const
    {
        return group_count_;
    }

    std::set<RID> TrackerClient::GetClientResource() const
    {
        if (is_vod_)
        {
            return p2sp::AppModule::Inst()->GetVodResource(tracker_info_.ModNo, group_count_);
        }
        else
        {
            return p2sp::AppModule::Inst()->GetLiveResource(tracker_info_.ModNo, group_count_);
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Update Ips
    void TrackerClient::UpdateIpStatistic(const protocol::SocketAddr& detected_addr)
    {
        std::vector<uint32_t> local_ips;

        LoadLocalIPs(local_ips);
        statistic::StatisticModule::Inst()->SetLocalIPs(local_ips);


        if (find(local_ips.begin(), local_ips.end(), detected_addr.IP) != local_ips.end())
        {
            statistic::StatisticModule::Inst()->SetLocalPeerIp(detected_addr.IP);
        }
        else if (local_ips.size() > 0)
        {
            statistic::StatisticModule::Inst()->SetLocalPeerIp(local_ips.front());
        }

        if (statistic::StatisticModule::Inst()->GetLocalPeerInfo().IP == detected_addr.IP)
        {
            // MainThread::Post(boost::bind(&StunModule::SetIsNeededStun, StunModule::Inst(), false));
            StunModule::Inst()->SetIsNeededStun(false);
        }
        // statistic::StatisticModule::Inst()->SetLocalDetectSocketAddress(detected_addr);
    }

    void TrackerClient::PPLeave()
    {
        protocol::LeavePacket leave_packet(
            protocol::Packet::NewTransactionID(),
            protocol::PEER_VERSION,
            AppModule::Inst()->GetPeerGuid(),
            end_point_);
        
        p2sp::AppModule::Inst()->DoSendPacket(leave_packet);
        is_sync_ = false;
    }

    bool TrackerClient::IsTrackerForLiveUdpServer() const
    {
        return is_tracker_for_live_udpserver_;
    }
}
