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
#include "statistic/DACStatisticModule.h"
#include "network/upnp/UpnpModule.h"

namespace p2sp
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_tracker_client = log4cplus::Logger::getInstance("[tracker_client]");
#endif

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
    }

    void TrackerClient::SetRidCount(boost::uint32_t rid_count)
    {
        // 上次收到的keepalive或者commit中服务器的存储的本机的资源个数
        last_response_rid_count_ = rid_count;
    }

    void TrackerClient::HttpTrackerList(string const & server, RID const & rid)
    {
        boost::shared_ptr<util::protocol::HttpClient> http_client(new util::protocol::HttpClient(global_io_svc()));
        boost::system::error_code error;
        http_client->bind_host(end_point_.address().to_string(error), error);
        if (error)
        {
            return;
        }
        std::ostringstream http_request_path;
        http_request_path << "/trackeragcgi?rid=" << rid;
        string http_request_url;
        http_request_url = "http://" + end_point_.address().to_string(error) + http_request_path.str();

        http_client->async_fetch(http_request_url, boost::bind(& TrackerClient::HandleFetchResult, 
            shared_from_this(), http_client, boost::asio::placeholders::error));
    }

    void TrackerClient::HandleFetchResult(boost::shared_ptr<util::protocol::HttpClient> http_client, 
        const boost::system::error_code & err)
    {
        if (err)
        {
            LOG4CPLUS_INFO_LOG(logger_tracker_client, "HandleFetchResult err: "<< err);
            statistic::DACStatisticModule::Inst()->SubmitHttpTrackerListIpCodeString(end_point_, err.value());
            return;
        }
        else
        {
            boost::uint32_t status_code = http_client->get_response_head().err_code;
            statistic::DACStatisticModule::Inst()->SubmitHttpTrackerListIpCodeString(end_point_, status_code);
            if (status_code != 200)
            {
                LOG4CPLUS_INFO_LOG(logger_tracker_client, "HandleFetchResult get status_code: "<< status_code);
                return;
            }
            boost::asio::streambuf & response = http_client->get_response().data();
            std::string response_content((std::istreambuf_iterator<char>(& response)), 
                std::istreambuf_iterator<char>());
            if (!response_content.size() || response_content.size() % 2)
            {
                return;
            }
            boost::uint32_t check_sum = 0;
            boost::uint8_t action = 0;
            std::istringstream iss;;
            iss.str(Hex2Bin(response_content));
            iss.read((char *)&check_sum, sizeof(boost::uint32_t));
            action = iss.get();
            if (action != protocol::ListWithIpPacket::Action)
            {
                return;
            }
            protocol::ListWithIpPacket http_response_list;
            util::archive::LittleEndianBinaryIArchive<> iarchive(iss);
            iarchive >> http_response_list;

            UpdatePeerInfo(http_response_list.response.peer_infos_);
            p2sp::AppModule::Inst()->AddCandidatePeers(http_response_list.response.resource_id_, 
                http_response_list.response.peer_infos_, is_tracker_for_live_udpserver_);
            statistic::StatisticModule::Inst()->SubmitListResponse(tracker_info_, 
                http_response_list.response.peer_infos_.size(), http_response_list.response.resource_id_);
        }
    }

    std::string TrackerClient::Hex2Bin(string const & hex_string)
    {
        std::string s;
        int i = 0;
        while(hex_string[i] != '\0')
        {
            int temp[2];
            for(int j = 0; j <= 1; j ++)
            {
                if (hex_string[i+j] >= '0' && hex_string[i+j] <= '9')
                {
                    temp[j] = hex_string[i+j] - '0';
                }
                else if (hex_string[i+j] >= 'A' && hex_string[i+j] <= 'F')
                {
                    temp[j] = hex_string[i+j] -'A' + 10;
                }
                else if (hex_string[i+j] >= 'a' && hex_string[i+j] <= 'f')
                {
                    temp[j] = hex_string[i+j] - 'a' + 10;
                }
            }
            s += temp[0] * 16 + temp[1];
            i += 2;
        }
        return s;
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
                MAX_REQUEST_PEER_COUNT_, end_point_,StunModule::Inst()->GetPeerNatType());

        // 从该TrackerClient发送ListRequestPacket
        LOG4CPLUS_INFO_LOG(logger_tracker_client, "TrackerClient::DoList " << rid << " " << end_point_);

        p2sp::AppModule::Inst()->DoSendPacket(list_request_packet);

        // 统计信息
        statistic::StatisticModule::Inst()->SubmitListRequest(tracker_info_, rid);
    }

    void TrackerClient::UpdatePeerInfo(std::vector<protocol::CandidatePeerInfo> & peer_infos_)
    {
        for (boost::uint32_t i = 0; i < peer_infos_.size(); ++ i)
        {
            if (peer_infos_[i].UploadPriority < 255 && peer_infos_[i].UploadPriority > 0)
            {
                peer_infos_[i].UploadPriority ++;
            }
        }
    }

    void TrackerClient::OnListResponsePacket(protocol::ListPacket const & packet)
    {
        // 讲List到的peer加入ip pool
        // 将packet解析出 vector<PeerInfo::p> peers
        std::vector<protocol::CandidatePeerInfo> peers = packet.response.peer_infos_;
        UpdatePeerInfo(peers);

        p2sp::AppModule::Inst()->AddCandidatePeers(packet.response.resource_id_, peers, is_tracker_for_live_udpserver_);

        // 统计信息
        statistic::StatisticModule::Inst()->SubmitListResponse(tracker_info_, packet.response.peer_infos_.size(), packet.response.resource_id_);
    }

    void TrackerClient::OnReportResponsePacket(protocol::ReportPacket const & packet)
    {
        LOG4CPLUS_DEBUG_LOG(logger_tracker_client, "Report Response RID Count: " << last_response_rid_count_);

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
            statistic::DACStatisticModule::Inst()->SubmitReportResponse();
        }
        else
        {
            LOG4CPLUS_WARN_LOG(logger_tracker_client, "TrackerClient::OnReportPacket: Unexpected Transaction ID, " 
                << packet.transaction_id_);
        }
    }

    boost::uint32_t TrackerClient::DoSubmit()
    {
        LOG4CPLUS_INFO_LOG(logger_tracker_client, "TrackerClient::DoSubmit ModNO:" << (boost::uint32_t)tracker_info_.ModNo
            << ", IP:" << framework::network::Endpoint(tracker_info_.IP, tracker_info_.Port).to_string());

        boost::uint32_t result = 0;

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
            if (last_response_rid_count_ <= (boost::uint32_t)(local_resources_.size() * 0.7))
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

    extern void LoadLocalIPs(std::vector<boost::uint32_t>& ipArray);

    /**
     * @return Transaction ID.
     */
    boost::uint32_t TrackerClient::DoReport()
    {
        // 统计信息
        LOG4CPLUS_INFO_LOG(logger_tracker_client, "TrackerClient::DoReport ");

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
        std::vector<boost::uint32_t> real_ips;

        // stun info
        boost::uint32_t stun_ip;
        boost::uint16_t stun_port;
        StunModule::Inst()->GetStunEndpoint(stun_ip, stun_port);

        boost::uint32_t stun_detect_ip = statistic::StatisticModule::Inst()->GetLocalPeerInfo().DetectIP;
        boost::uint16_t stun_detect_port = statistic::StatisticModule::Inst()->GetLocalPeerInfo().DetectUdpPort;

        //ck20120920采用的临时方案，用 stun_detect_port 表示upnp映射之后的port,最终的解决方法，是在report报文里加上一个字段表示
        if(0 != StunModule::Inst()->GetUpnpExUdpport())
        {
            stun_detect_port = StunModule::Inst()->GetUpnpExUdpport();
        }

        LoadLocalIPs(real_ips);

        boost::uint16_t local_udp_port = p2sp::AppModule::Inst()->GetLocalUdpPort();
        boost::uint16_t local_tcp_port = p2sp::AppModule::Inst()->GetLocalTcpPort();

        boost::int32_t upload_speed_limit_kbs = p2sp::P2PModule::Inst()->GetUploadSpeedLimitInKBps();
        if (upload_speed_limit_kbs < 0)
        {
            upload_speed_limit_kbs = std::numeric_limits<boost::int32_t>::max();
        }

        //TODO:设置到tracker的路由ip。5个就够了。
        std::vector<boost::uint32_t> traceroute_ips;

        boost::uint16_t upnp_tcp_port = UpnpModule::Inst()->GetUpnpExternalTcpPort(p2sp::AppModule::Inst()->GetLocalTcpPort());

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
                //如果内核没有获取到，那么就看看播放器是否获取到了，然后设置进来
                0 == upnp_tcp_port ? p2sp::AppModule::Inst()->GetUpnpPortForTcpUpload():upnp_tcp_port,
                traceroute_ips
           );

        LOG4CPLUS_DEBUG_LOG(logger_tracker_client, "DoReport, TrackerInfo: " << end_point_);

        // post
        p2sp::AppModule::Inst()->DoSendPacket(report_request);

        statistic::DACStatisticModule::Inst()->SubmitReportRequest();
        last_updates_ = update_resource_set;

        return last_transaction_id_;
    }

    const protocol::TRACKER_INFO& TrackerClient::GetTrackerInfo() const
    {
        return tracker_info_;
    }

    void TrackerClient::SetGroupCount(boost::uint32_t group_count)
    {
        group_count_ = group_count;
    }

    boost::uint32_t TrackerClient::GetGroupCount() const
    {
        return group_count_;
    }

    std::set<RID> TrackerClient::GetClientResource() const
    {
        if (group_count_ == 0)
        {
            return std::set<RID>();
        }

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
        std::vector<boost::uint32_t> local_ips;

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
