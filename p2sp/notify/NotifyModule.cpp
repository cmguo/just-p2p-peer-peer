//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/AppModule.h"
#include "p2sp/notify/NotifyModule.h"
#include "p2sp/proxy/MessageBufferManager.h"
#include <protocol/StunServerPacket.h>
#include <struct/Structs.h>
#include "statistic/StatisticModule.h"
#include "message.h"
#include "math.h"

#include <framework/network/Endpoint.h>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include "boost/algorithm/string.hpp"
#include "boost/lexical_cast.hpp"

#ifdef BOOST_WINDOWS_API
#include "WindowsMessage.h"
#endif

// 对数函数的底数，数字越小，分布越靠前
const uint32_t PARAM = 15;

namespace p2sp
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_notify = log4cplus::Logger::getInstance("[notify]");
#endif
    NotifyModule::p NotifyModule::inst_;

    void NotifyModule::Start()
    {
        LOG4CPLUS_DEBUG_LOG(logger_notify, "peer notify 启动 PeerGuid = " << AppModule::Inst()->GetPeerGuid());
        if (is_running_)
        {
            return;
        }

        // 定时器启动
        notify_timer_.start();

        is_running_ = true;

        max_peer_node = 16;
        max_peer_return = 8;
        start_time = 30;

        peer_node_retun_number_ = 0;

        is_have_server_endpoint = false;

#if defined(NEED_LOG)
        LoadConfig();
#endif

        total_peer_online_ = 1;
    }

    void NotifyModule::Stop()
    {
        if (!is_running_)
        {
            return;
        }

        is_running_ = false;
    }

#if defined(NEED_LOG)
    void NotifyModule::LoadConfig()
    {
        if (!is_running_)
        {
            return;
        }

        namespace fs = boost::filesystem;
        fs::path config_path("notify_config.txt");
        fs::ifstream fin(config_path);
        if (fin)
        {
            string line;
            while (std::getline(fin, line))
            {
                boost::algorithm::trim(line);
                if (line.empty() || line[0] == '#')
                    continue;
                uint32_t index = line.find_first_of('=');
                if (index != string::npos)
                {
                    string key = boost::algorithm::trim_copy(line.substr(0, index));
                    string value = boost::algorithm::trim_copy(line.substr(index + 1));

                    if (key == "MAX_PEER_NODE")
                    {
                        max_peer_node = boost::lexical_cast<uint32_t>(value);
                        LOG4CPLUS_DEBUG_LOG(logger_notify, "max_peer_node = " << max_peer_node);
                    }
                    else if (key == "MAX_PEER_RETURN")
                    {
                        max_peer_return = boost::lexical_cast<uint32_t>(value);
                        LOG4CPLUS_DEBUG_LOG(logger_notify, "max_peer_return = " << max_peer_return);
                    }
                    else if (key == "START_TIME")
                    {
                        start_time = boost::lexical_cast<uint32_t>(value);
                        LOG4CPLUS_DEBUG_LOG(logger_notify, "start_time = " << start_time);
                    }
                }
            }
        }
    }
#endif

    void NotifyModule::OnUdpRecv(protocol::Packet const & packet)
    {
        if (!is_running_)
        {
            return;
        }

        if (!is_need_join_)
        {
            // 是否加入Notify网络
            return;
        }

        switch (packet.PacketAction)
        {
        case protocol::ConnectPacket::Action:
            {
                // 穿越成功
                protocol::ConnectPacket const & connect_packet = (protocol::ConnectPacket const &)packet;
                if (connect_packet.basic_info_ %2 == 0)  // connect_packet.IsRequest
                {
                    LOG4CPLUS_DEBUG_LOG(logger_notify, "Receive ConnectPacket, send Reconnect packet");
                    // 回发ConnectPacket
                    RID spec_rid("00000000000000000000000000000001");
                    // const string str_rid = "00000000000000000000000000000001";
                    // spec_rid.Parse(str_rid);

                    protocol::ConnectPacket re_connect_packet(protocol::Packet::NewTransactionID(), spec_rid,
                        AppModule::Inst()->GetPeerGuid(), protocol::PEER_VERSION_V4,
                        0x01, framework::timer::TickCounter::tick_count(), AppModule::Inst()->GetPeerVersion(),
                        AppModule::Inst()->GetCandidatePeerInfo(), protocol::CONNECT_NOTIFY,
                        AppModule::Inst()->GetPeerDownloadInfo(spec_rid), connect_packet.end_point,
                        0);

                    // re_connect_packet.end_point = end_point;

                    AppModule::Inst()->DoSendPacket(re_connect_packet, protocol::PEER_VERSION_V4);
                }
                else
                {
                    LOG4CPLUS_DEBUG_LOG(logger_notify, "收到上层发回的Reconnect报文表示穿越成功，发送JoinRequest报文");
                    // 发JoinRequestPacket报文
                    DoSendJoinRequestPacket(connect_packet.end_point);
                }
                break;
            }
        case protocol::JoinRequestPacket::Action:
            {
                LOG4CPLUS_DEBUG_LOG(logger_notify, "收到JoinRequestPacket报文，此时子节点个数为 " << 
                    peer_node_map_.size());
                // 0xA0 请求加入
                // 作为上层节点，判断是否超过最大连接数。如果没有超过，加入成功；否则，加入失败。
                protocol::JoinRequestPacket const & join_request_packet = (protocol::JoinRequestPacket const &)packet;

                std::vector<protocol::NodeInfo> node_vec;

                if (peer_node_map_.size() < max_peer_node)
                {
                    // 子节点个数没有超过上限，加入成功。
                    protocol::JoinResponsePacket join_response_packet(packet.transaction_id_,
                        AppModule::Inst()->GetPeerGuid(), 0, node_vec, join_request_packet.end_point);
                    // join_response_packet.end_point = end_point;

                    AppModule::Inst()->DoSendPacket(join_response_packet, protocol::PEER_VERSION_V4);

                    if (peer_node_map_.find(join_request_packet.peer_guid_) == peer_node_map_.end())
                    {
                        PEER_NODE_STATUS peer_node_status;
                        peer_node_status.end_point = join_request_packet.end_point;


                        u_long ip;
                        u_short port;
                        framework::network::Endpoint ep(join_request_packet.end_point);
                        ip = ep.ip_v4();
                        port = ep.port();

                        LOG4CPLUS_DEBUG_LOG(logger_notify, "!!!!加入成功，IP = " << ip << "Port = " << port);

                        peer_node_status.intern_ip_ = join_request_packet.internal_ip_;
                        peer_node_status.intern_port_ = join_request_packet.internal_port_;
                        peer_node_status.detect_ip_ = join_request_packet.detect_ip_;
                        peer_node_status.detect_port_ = join_request_packet.detect_port_;
                        peer_node_status.stun_ip_ = join_request_packet.stun_ip_;
                        peer_node_status.stun_port_ = join_request_packet.stun_port_;
                        peer_node_status.dead_time_ = 0;
                        peer_node_status.peer_online_ = 0;

                        peer_node_map_.insert(std::make_pair(join_request_packet.peer_guid_, peer_node_status));
                        LOG4CPLUS_DEBUG_LOG(logger_notify, "加入成功，子节点列表维护成功");
                    }
                    else
                    {
                        LOG4CPLUS_DEBUG_LOG(logger_notify, "已经加入");
                    }
                }
                else
                {
                    // 子节点个数超过上限，加入失败。
                    if (peer_node_map_.size() == 0)
                    {
                        LOG4CPLUS_DEBUG_LOG(logger_notify, "子节点个数超过上限，加入失败，但是子节点个数为0");
                        return;
                    }

                    // 随机返回部分节点
                    uint32_t max_return = (std::min)((uint32_t)max_peer_return, (uint32_t)peer_node_map_.size());
                    uint32_t pos = peer_node_retun_number_ % max_return;

                    std::map<Guid, PEER_NODE_STATUS>::const_iterator peer_map_iter_ = peer_node_map_.begin();
                    while (pos != 0)
                    {
                        ++peer_map_iter_;
                        --pos;
                    }

                    // 从pos开始，连续返回max_return个节点
                    for (uint32_t i = 0; i<max_return; i++)
                    {
                        if (peer_map_iter_ == peer_node_map_.end())
                        {
                            peer_map_iter_ = peer_node_map_.begin();
                        }

                        protocol::NodeInfo node;
                        node.PeerGuid = peer_map_iter_->first;
                        node.InternalIP = peer_map_iter_->second.intern_ip_;
                        node.InternalPort = peer_map_iter_->second.intern_port_;
                        node.DetectIP = peer_map_iter_->second.detect_ip_;
                        node.DetectPort = peer_map_iter_->second.detect_port_;
                        node.StunIP = peer_map_iter_->second.stun_ip_;
                        node.StunPort = peer_map_iter_->second.stun_port_;

                        ++peer_map_iter_;
                        ++peer_node_retun_number_;

                        node_vec.push_back(node);
                    }

                    protocol::JoinResponsePacket join_response_packet(packet.transaction_id_, AppModule::Inst()->GetPeerGuid(), 1, node_vec, join_request_packet.end_point);

                    AppModule::Inst()->DoSendPacket(join_response_packet, protocol::PEER_VERSION_V4);
                }
            }
            break;
        case protocol::JoinResponsePacket::Action:
            {
                LOG4CPLUS_DEBUG_LOG(logger_notify, "JoinResponsePacket");
                // 0xA1 请求加入回包
                // 作为下层节点，判断是否加入成功。如成功，则启动定时器，发送心跳包；否则，继续Join。
                protocol::JoinResponsePacket const & join_response_packet = (protocol::JoinResponsePacket const &)packet;

                if (join_response_packet.ret_ == 0)
                {
                    LOG4CPLUS_DEBUG_LOG(logger_notify, "JoinResponsePacket: Join Success");
                    // 加入成功
                    is_join_success_ = true;

                    // 记录上层节点
                    god_endpoint_ = join_response_packet.end_point;
                    god_guid_ = join_response_packet.peer_guid_;
                    god_node_time = 0;

                    peer_to_connect_.clear();

                    hops_ = 1;
                }
                else
                {
                    LOG4CPLUS_DEBUG_LOG(logger_notify, "JoinResponsePacket: Join failed");
                    // 解出节点
                    boost::uint32_t node_count = join_response_packet.node_vec_.size();

                    protocol::NodeInfo nodes[256];
                    for (boost::uint32_t i = 0; i<node_count; i++)
                    {
                        nodes[i] = join_response_packet.node_vec_[i];
                    }
                    // memcpy(nodes, join_response_packet.NodeVec.size(), node_count * sizeof(protocol::NodeInfo));

                    // 加入待加入节点列表
                    for (boost::uint32_t i = 0; i<node_count; i++)
                    {
                        protocol::CandidatePeerInfo candidate_peer;

                        candidate_peer.IP = nodes[i].InternalIP;
                        candidate_peer.UdpPort = nodes[i].InternalPort;
                        candidate_peer.DetectIP = nodes[i].DetectIP;
                        candidate_peer.DetectUdpPort = nodes[i].DetectPort;
                        candidate_peer.StunIP = nodes[i].StunIP;
                        candidate_peer.StunUdpPort = nodes[i].StunPort;

                        peer_to_connect_.push_back(candidate_peer);
                    }
                }
            }
            break;
        case protocol::NotifyKeepAliveRequestPacket::Action:
            {
                LOG4CPLUS_DEBUG_LOG(logger_notify, "收到心跳包NotifyKeepAliveRequestPacket");
                // 0xA2 心跳包
                // 作为上层节点，收到下层节点的心跳包。回包 & 统计
                protocol::NotifyKeepAliveRequestPacket const & keepalive_request_packet = (protocol::NotifyKeepAliveRequestPacket const &)packet;

                if (peer_node_map_.find(keepalive_request_packet.peer_guid_) != peer_node_map_.end())
                {
                    // 子节点信息维护
                    // 更新端口
                    peer_node_map_[keepalive_request_packet.peer_guid_].end_point = keepalive_request_packet.end_point;

                    // 更新在线人数
                    peer_node_map_[keepalive_request_packet.peer_guid_].peer_online_ = keepalive_request_packet.peer_online_;
                    peer_node_map_[keepalive_request_packet.peer_guid_].dead_time_ = 0;

                    // 更新任务完成数
                    boost::uint32_t task_num = keepalive_request_packet.task_info_.size();
                    protocol::TASK_INFO task_status[255];
                    for (boost::uint32_t i = 0; i<task_num; i++)
                    {
                        task_status[i] = keepalive_request_packet.task_info_[i];
                    }
                    // memcpy(task_status, keepalive_request_packet->GetPeerTaskInfo(), task_num * sizeof(protocol::TASK_INFO));

                    for (boost::uint32_t i = 0; i<task_num; i++)
                    {
                        peer_node_map_[keepalive_request_packet.peer_guid_].peer_task_map_[task_status[i].TaskID].finish_num_ = task_status[i].CompleteCount;
                    }

                    // 和当前任务对比，如果比当前任务少，则通知
                    for (std::map<boost::uint32_t, TASK_RECORD>::iterator iter = task_map_.begin();
                        iter != task_map_.end(); ++iter)
                    {
                        bool flag = true;
                        for (boost::uint32_t i = 0; i<task_num; i++)
                        {
                            if (task_status[i].TaskID == iter->first)
                            {
                                flag = false;
                                break;
                            }
                        }

                        if (flag)
                        {
                            // 子节点比当前节点任务少，则通知
                            NotifyPeer(keepalive_request_packet.peer_guid_, iter->first);
                        }
                    }

                    // 心跳包回包
                    protocol::NotifyKeepAliveResponsePacket keepalive_response_packet(keepalive_request_packet.transaction_id_, keepalive_request_packet.end_point);

                    // keepalive_response_packet.end_point = end_point;

                    AppModule::Inst()->DoSendPacket(keepalive_response_packet, protocol::PEER_VERSION_V4);
                }
                else
                {
                    // 找不到该子节点
                    LOG4CPLUS_DEBUG_LOG(logger_notify, "cannot find subnode");
                }
            }
            break;
        case protocol::NotifyKeepAliveResponsePacket::Action:
            {
                LOG4CPLUS_DEBUG_LOG(logger_notify, "收到心跳回报NotifyKeepAliveResponsePacket");
                // 0xA3 心跳回包
                // 作为下层节点，收到上层节点的心跳包回包。认为上层断线的定时器重置。
                god_node_time = 0;
            }
            break;
        case protocol::NotifyRequestPacket::Action:
            {
                LOG4CPLUS_DEBUG_LOG(logger_notify, "收到通知包NotifyRequestPacket");
                // 0xA4 通知包
                // 作为下层节点，收到上层节点的通知包。
                protocol::NotifyRequestPacket const & notify_request_packet = (protocol::NotifyRequestPacket const &) packet;

                // 发送通知回包
                protocol::NotifyResponsePacket notify_response_packet(packet.transaction_id_,
                    AppModule::Inst()->GetPeerGuid(), notify_request_packet.task_id_, notify_request_packet.end_point);

                AppModule::Inst()->DoSendPacket(notify_response_packet, protocol::PEER_VERSION_V4);

                // 收到包的任务信息长度不能超过1024字节
                if (notify_request_packet.buffer_.length() > MAX_CONTENT_LENGTH)
                {
                    return;
                }

                // notify 过滤垃圾任务，即使收到，本地也不处理
                if (task_map_.find(notify_request_packet.task_id_) == task_map_.end() && 
                    notify_request_packet.rest_time_ > 0 &&
                    notify_request_packet.rest_time_ < 65535 &&
                    notify_request_packet.buffer_.length() > 0)
                {
                    // 本地没有该任务
                    TASK_RECORD task_info;
                    task_info.rest_time_ = notify_request_packet.rest_time_;
                    task_info.finish_num_ = 0;
                    task_info.duration_ = notify_request_packet.duration_;
                    task_info.task_type_ = (NOTIFY_TASK_TYPE)notify_request_packet.task_type_;
                    task_info.buffer_len_ = notify_request_packet.buffer_.length();
                    task_info.task_delay_time_ = 0;
                    task_info.is_my_finish_ = false;

                    base::util::memcpy2(task_info.buf, sizeof(task_info.buf), notify_request_packet.buffer_.c_str(), task_info.buffer_len_);

                    task_map_.insert(std::make_pair(notify_request_packet.task_id_, task_info));

                    assert(notify_request_packet.task_type_ < INVALID_TYPE);

                    if (notify_request_packet.task_type_ == TEXT ||
                        notify_request_packet.task_type_ == LIVE ||
                        notify_request_packet.task_type_ == VIP_MESSAGE ||
                        notify_request_packet.task_type_ == EXE ||
                        notify_request_packet.task_type_ == SUBSCRIPTION_MESSAGE)
                    {
                        // 不用下载资源的任务(文本,直播,小喇叭,EXE,订阅通知等等)
                        // 并发送给PPTV

                        NOTIFY_TASK* notify_task = MessageBufferManager::Inst()->NewStruct<NOTIFY_TASK>();

                        notify_task->task_id = notify_request_packet.task_id_;
                        notify_task->task_type = notify_request_packet.task_type_;
                        notify_task->content_len = notify_request_packet.buffer_.size();
                        base::util::memcpy2(
                            notify_task->content, 
                            sizeof(notify_task->content), 
                            notify_request_packet.buffer_.c_str(), 
                            notify_request_packet.buffer_.size());
#ifdef NEED_TO_POST_MESSAGE
                        LOG4CPLUS_DEBUG_LOG(logger_notify, "PostWindowsMessage UM_NOTIFY_PPTV_TASK Task id = " << notify_task->task_id);
                        WindowsMessage::Inst().PostWindowsMessage(UM_NOTIFY_PPTV_TASK, NULL, (LPARAM)notify_task);
#endif
                    }
                    else
                    {
                        // 需要下载资源的任务（图文 or 点播 etc）
                        if (god_endpoint_ == server_endpoint_)
                        {
                            // 第一层，仅仅下载
                            NOTIFY_TASK* notify_task = MessageBufferManager::Inst()->NewStruct<NOTIFY_TASK>();

                            notify_task->task_id = notify_request_packet.task_id_;
                            notify_task->task_type = notify_request_packet.task_type_;
                            notify_task->content_len = notify_request_packet.buffer_.size();
                            base::util::memcpy2(notify_task->content, sizeof(notify_task->content), notify_request_packet.buffer_.c_str(), notify_request_packet.buffer_.size());
#ifdef NEED_TO_POST_MESSAGE
                            LOG4CPLUS_DEBUG_LOG(logger_notify, "PostWindowsMessage UM_NOTIFY_PPTV_TASK TaskID = " << notify_task->task_id);
                            WindowsMessage::Inst().PostWindowsMessage(UM_NOTIFY_PPTV_TASK, NULL, (LPARAM)notify_task);
#endif
                        }
                        else
                        {
                            // 不是第一层，通知延迟下载
                            boost::uint32_t n = (boost::uint32_t)(notify_request_packet.duration_ * 0.95);
                            double  x = (double)(rand() % 1000) / 1000;
                            boost::uint32_t delay_time_ = (boost::uint32_t)((double)mylog(PARAM, 1 + x * (PARAM-1)) * n);

                            delay_time_ += (boost::uint32_t)(notify_request_packet.duration_ * 0.05 + 1);

                            task_map_[notify_request_packet.task_id_].task_delay_time_ = delay_time_;

                            LOG4CPLUS_DEBUG_LOG(logger_notify, "delay_time = " << delay_time_);
                        }
                    }
                }
                else
                {
                    // 本地存在该任务
                   LOG4CPLUS_DEBUG_LOG(logger_notify, "该任务已经存在 ");
                }

                // 依次通知每个Peer
                for (std::map<Guid, PEER_NODE_STATUS>::iterator iter = peer_node_map_.begin();
                    iter != peer_node_map_.end(); ++iter)
                {
                    NotifyPeer(iter->first, notify_request_packet.task_id_);
                }
            }
            break;
        case protocol::NotifyResponsePacket::Action:
            {
                LOG4CPLUS_DEBUG_LOG(logger_notify, "收到通知的回包NotifyResponsePacket");
                // 0xA5 通知回包
                // 作为上层节点，收到字节点的通知回包，说明子节点已经收到通知。
                protocol::NotifyResponsePacket const & notify_response_packet = (protocol::NotifyResponsePacket const &) packet;

                if (peer_node_map_.find(notify_response_packet.peer_guid_) != peer_node_map_.end())
                {
                    if (peer_node_map_[notify_response_packet.peer_guid_].peer_task_map_.find(notify_response_packet.task_id_)
                        != peer_node_map_[notify_response_packet.peer_guid_].peer_task_map_.end())
                    {
                        peer_node_map_[notify_response_packet.peer_guid_].peer_task_map_[notify_response_packet.task_id_].is_notify_ = true;
                        peer_node_map_[notify_response_packet.peer_guid_].peer_task_map_[notify_response_packet.task_id_].is_notify_response_ = true;
                    }
                    else
                    {
                        LOG4CPLUS_DEBUG_LOG(logger_notify, "不存在该任务");
                    }
                }
                else
                {LOG4CPLUS_DEBUG_LOG(logger_notify, "不存在该子节点 ");

                }
            }
            break;
        case protocol::PeerLeavePacket::Action:
            {
                LOG4CPLUS_DEBUG_LOG(logger_notify, "节点离线PeerLeavePacket");
                // 0xA6 离线包
                // 判断是上层节点发的包，还是下层节点。
                // 如果是上层节点，重新加入；如果是下层节点，则删除。否则，忽略。
                protocol::PeerLeavePacket const & peer_leave_packet = (protocol::PeerLeavePacket const &) packet;

                if (god_guid_ == peer_leave_packet.peer_guid_)
                {
                    // 上层节点离线，立刻重新加入
                    is_join_success_ = false;
                    LOG4CPLUS_DEBUG_LOG(logger_notify, "上层节点离线驱动加入网络");
                    JoinNotifyNetwork();
                }
                else
                {
                    // 不是上层节点
                    std::map<Guid, PEER_NODE_STATUS>::iterator iter = peer_node_map_.find(peer_leave_packet.peer_guid_);

                    if (iter != peer_node_map_.end())
                    {
                        peer_node_map_.erase(iter);
                    }
                }
            }
            break;
        }
    }

    void NotifyModule::JoinNotifyNetwork()
    {
        LOG4CPLUS_DEBUG_LOG(logger_notify, "JoinNotifyNetwork");
        if (peer_to_connect_.size() == 0)
        {
            if (is_have_server_endpoint)
            {
                // 从服务器加入
                LOG4CPLUS_DEBUG_LOG(logger_notify, "加入网络JoinNotifyNetwork, join server");
                server_connect_hops_++;
                server_endpoint_ = framework::network::Endpoint(notify_server_s_[server_connect_hops_ % notify_server_s_.size()].IP,
                    notify_server_s_[server_connect_hops_ % notify_server_s_.size()].Port);
                DoSendJoinRequestPacket(server_endpoint_);
            }
        }
        else
        {
            LOG4CPLUS_DEBUG_LOG(logger_notify, "JoinNotifyNetwork, join peer");
            // 从节点加入, 先发送Connect报文
            // 构造特殊的RID
            RID spec_rid;
            const string str_rid = "00000000000000000000000000000001";
            spec_rid.from_string(str_rid);

            // 取出待发送的Peer
            boost::uint32_t index = hops_ % peer_to_connect_.size();
            protocol::CandidatePeerInfo candidate_peer_info = peer_to_connect_[index];
            peer_to_connect_.erase(peer_to_connect_.begin() + index);
            hops_ *= 3;

            LOG4CPLUS_DEBUG_LOG(logger_notify, "取出待发送的Peer IP = " <<  candidate_peer_info.IP);
            LOG4CPLUS_DEBUG_LOG(logger_notify, "取出待发送的Peer PORT = " << candidate_peer_info.UdpPort);
            LOG4CPLUS_DEBUG_LOG(logger_notify, "取出待发送的Peer detect ip = " << candidate_peer_info.DetectIP);
            LOG4CPLUS_DEBUG_LOG(logger_notify, "取出待发送的Peer detect PORT = " << candidate_peer_info.DetectUdpPort);

            boost::uint32_t local_detected_ip = AppModule::Inst()->GetCandidatePeerInfo().DetectIP;
            LOG4CPLUS_DEBUG_LOG(logger_notify, "本机的detect ip = " << local_detected_ip);

            boost::asio::ip::udp::endpoint end_point = candidate_peer_info.GetConnectEndPoint(local_detected_ip);

            // 构造Connect报文
            protocol::ConnectPacket packet(protocol::Packet::NewTransactionID(), spec_rid,
                AppModule::Inst()->GetPeerGuid(), protocol::PEER_VERSION_V4,
                0x00, framework::timer::TickCounter::tick_count(),
                protocol::PEER_VERSION, AppModule::Inst()->GetCandidatePeerInfo(), protocol::CONNECT_NOTIFY,
                AppModule::Inst()->GetPeerDownloadInfo(spec_rid), end_point,
                0);

            u_long ip_;
            u_short port_;
            framework::network::Endpoint ep(end_point);
            ip_ = ep.ip_v4();
            port_ = ep.port();

            // packet.end_point = end_point;

            LOG4CPLUS_DEBUG_LOG(logger_notify, "DoSendPacket ip = " << ip_ << " port = " << port_);

            AppModule::Inst()->DoSendPacket(packet, protocol::PEER_VERSION_V4);

            // 穿越 
            if (candidate_peer_info.NeedStunInvoke(local_detected_ip))
            {
                framework::network::Endpoint ep(candidate_peer_info.StunIP, candidate_peer_info.StunUdpPort);
                boost::asio::ip::udp::endpoint stun_ep_ = ep;

                protocol::StunInvokePacket stun_invoke_packet(protocol::Packet::NewTransactionID()
                    , spec_rid, AppModule::Inst()->GetPeerGuid(), framework::timer::TickCounter::tick_count()
                    , AppModule::Inst()->GetCandidatePeerInfo()
                    , protocol::CONNECT_NOTIFY
                    , candidate_peer_info
                    , AppModule::Inst()->GetPeerDownloadInfo(spec_rid)
                    , 0
                    , stun_ep_);

                // stun_invoke_packet.end_point = stun_ep_;
                AppModule::Inst()->DoSendPacket(stun_invoke_packet);
                LOG4CPLUS_DEBUG_LOG(logger_notify, "穿越进行中 ");
            }
        }
    }

    void NotifyModule::NotifyPeer(Guid peer_guid, boost::uint32_t task_id)
    {
        // 构造通知包
        if (peer_node_map_.find(peer_guid) != peer_node_map_.end())
        {
            if (task_map_.find(task_id) != task_map_.end() && task_map_[task_id].rest_time_ != 0)
            {
                string task_buf;
                task_buf.append(task_map_[task_id].buf, task_map_[task_id].buffer_len_);
                protocol::NotifyRequestPacket notify_request_packet(protocol::Packet::NewTransactionID(),
                    task_id, task_map_[task_id].duration_, task_map_[task_id].rest_time_,
                    task_map_[task_id].task_type_, task_buf, peer_node_map_[peer_guid].end_point);

                framework::network::Endpoint ep(peer_node_map_[peer_guid].end_point);

                boost::uint32_t ip = ep.ip_v4();
                boost::uint16_t port = ep.port();

                LOG4CPLUS_DEBUG_LOG(logger_notify, "!!!!发送通知，IP = " << ip << "Port = " << port);

                // notify_request_packet.end_point = peer_node_map_[peer_guid].end_point;

                AppModule::Inst()->DoSendPacket(notify_request_packet, protocol::PEER_VERSION_V4);

                // 状态更新
                peer_node_map_[peer_guid].peer_task_map_[task_id].is_notify_ = true;
                peer_node_map_[peer_guid].peer_task_map_[task_id].is_notify_response_ = false;
            }
        }
        else
        {
            LOG4CPLUS_DEBUG_LOG(logger_notify, "NotifyPeer: 不存在该Peer");
        }
    }

    void NotifyModule::OnTimerElapsed(framework::timer::Timer * pointer)
    {
        if (!is_running_)
        {
            return;
        }

        if (pointer == &join_timer_ && is_need_join_)
        {
            LOG4CPLUS_DEBUG_LOG(logger_notify, "join in " << start_time << "second");
            JoinNotifyNetwork();
        }
        else if (pointer == &notify_timer_)
        {
            // 每秒任务 rest_time 更新，不管状态
            for (std::map<boost::uint32_t, TASK_RECORD>::iterator iter = task_map_.begin();
                iter != task_map_.end();)
            {
                if (iter->second.rest_time_ == 0)
                {
                    task_map_.erase(iter++);
                }
                else
                {
                    iter->second.rest_time_ -= 1;
                    LOG4CPLUS_DEBUG_LOG(logger_notify, "任务 task_id = " << iter->first << " RestTime = " << 
                        iter->second.rest_time_);

                    if (is_join_success_)
                    {
                        if (iter->second.rest_time_ < 5)
                        {
                            // 统计在线人数
                            CalPeerOnline();
                            // 统计在线任务数
                            CalTaskComplete();
                            // 发送KeepAlive包
                            DoSendKeepAlive();
                        }
                        else if (iter->second.rest_time_ <= 15 && iter->second.rest_time_ % 3 == 0)
                        {
                            // 统计在线人数
                            CalPeerOnline();
                            // 统计在线任务数
                            CalTaskComplete();
                            // 发送KeepAlive包
                            DoSendKeepAlive();
                        }
                    }
                    ++iter;
                }
            }

            // 收到的任务维护
            for (std::map<boost::uint32_t, TASK_RECORD>::iterator iter = task_map_.begin();
                iter != task_map_.end(); ++iter)
            {
                LOG4CPLUS_DEBUG_LOG(logger_notify, "TaskID = " << iter->first << " task_delay_time = " << 
                    iter->second.task_delay_time_);
                if (iter->second.task_delay_time_ > 1)
                {
                    iter->second.task_delay_time_--;
                }
                else if (iter->second.task_delay_time_ == 1)
                {
                    if (is_join_success_)
                    {
                        // 时间到，通知客户端去下载
                        NOTIFY_TASK* notify_task = MessageBufferManager::Inst()->NewStruct<NOTIFY_TASK>();

                        notify_task->task_id = iter->first;
                        notify_task->task_type = iter->second.task_type_;
                        notify_task->content_len = iter->second.buffer_len_;
                        base::util::memcpy2(notify_task->content, sizeof(notify_task->content), iter->second.buf, iter->second.buffer_len_);
#ifdef NEED_TO_POST_MESSAGE
                        LOG4CPLUS_DEBUG_LOG(logger_notify, "已经加入网络，时间到PostWindowsMessage UM_NOTIFY_PPTV_TASK TaskID = " << notify_task->task_id);
                        WindowsMessage::Inst().PostWindowsMessage(UM_NOTIFY_PPTV_TASK, NULL, (LPARAM)notify_task);
#endif

                        iter->second.task_delay_time_ = 0;
                    }
                    else
                    {
                        iter->second.task_delay_time_ = -1;
                        LOG4CPLUS_DEBUG_LOG(logger_notify, "退出网络，但时间到");
                    }
                }
                else if (iter->second.task_delay_time_ == -1)
                {
                    // 时间已经过了
                    if (is_join_success_)
                    {
                        // 立刻执行
                        NOTIFY_TASK* notify_task = MessageBufferManager::Inst()->NewStruct<NOTIFY_TASK>();

                        notify_task->task_id = iter->first;
                        notify_task->task_type = iter->second.task_type_;
                        notify_task->content_len = iter->second.buffer_len_;
                        base::util::memcpy2(notify_task->content, sizeof(notify_task->content), iter->second.buf, iter->second.buffer_len_);
#ifdef NEED_TO_POST_MESSAGE
                        LOG4CPLUS_DEBUG_LOG(logger_notify, "已经加入网络，时间过PostWindowsMessage UM_NOTIFY_PPTV_TASK Taskid = " << notify_task->task_id);
                        WindowsMessage::Inst().PostWindowsMessage(UM_NOTIFY_PPTV_TASK, NULL, (LPARAM)notify_task);
#endif

                        iter->second.task_delay_time_ = 0;
                    }
                }
            }

            if (is_join_success_)
            {
                // 加入成功

                // 上层节点计时器++
                god_node_time++;

                if (god_node_time > 3 * KEEPALIVE_INTERVAL)
                {
                    // 超过3倍的KEEPALIVE_INTERVAL，认为上层节点离线
                    LOG4CPLUS_DEBUG_LOG(logger_notify, "超过3倍的KEEPALIVE_INTERVAL，认为上层节点离线 ");
                    is_join_success_ = false;
                }

                // 下层节点计时器++
                for (std::map<Guid, PEER_NODE_STATUS>::iterator iter = peer_node_map_.begin();
                    iter != peer_node_map_.end();)
                {
                    iter->second.dead_time_++;

                    if (iter->second.dead_time_ > 3 * KEEPALIVE_INTERVAL)
                    {
                        peer_node_map_.erase(iter++);
                    }
                    else
                    {
                        ++iter;
                    }
                }

                // 每5秒，通知重发
                if (pointer->times() % 5 == 0)
                {
                    // 通知重发机制
                    NotifyReSend();
                }

                // 每30秒
                if (pointer->times() % KEEPALIVE_INTERVAL == 0)
                {
                    // 统计在线人数
                    CalPeerOnline();
                }

                if (pointer->times() % KEEPALIVE_INTERVAL == 10)
                {
                    // 统计在线任务数
                    CalTaskComplete();
                }

                if (pointer->times() % KEEPALIVE_INTERVAL == 20)
                {
                    // 发送KeepAlive包
                    DoSendKeepAlive();
                }
            }
            else
            {
                // 加入不成功
                if (is_need_join_ && time_count_.elapsed() > (start_time + 3) * 1000 && pointer->times() % 3 == 0)
                {
                    LOG4CPLUS_DEBUG_LOG(logger_notify, "定时器驱动加入网络 ");
                    JoinNotifyNetwork();
                }
            }
        }
    }

    void NotifyModule::OnNotifyTaskStatusChange(boost::uint32_t task_id, boost::uint32_t task_status)
    {
        if (task_status == 1)
        {
            LOG4CPLUS_DEBUG_LOG(logger_notify, "PPAP调用OnNotifyTaskStatusChange接口");

            if (task_map_.find(task_id) != task_map_.end() && !task_map_[task_id].is_my_finish_)
            {
                task_map_[task_id].is_my_finish_ = true;
                LOG4CPLUS_DEBUG_LOG(logger_notify, "任务完成 task_id = " << task_id);

                // 统计在线人数
                CalPeerOnline();
                // 统计在线任务数
                CalTaskComplete();
                // 发送KeepAlive包
                if (is_join_success_)
                {
                    DoSendKeepAlive();
                }
            }
            else
            {
                LOG4CPLUS_DEBUG_LOG(logger_notify, "任务不存在 或者 任务已经完成过了，不再重复向上汇报 task_id = " 
                    << task_id);
            }
        }
    }

    void NotifyModule::OnNotifyJoinLeave(boost::uint32_t join_or_leave)
    {
        if (join_or_leave == 1 && !is_need_join_)
        {
            LOG4CPLUS_DEBUG_LOG(logger_notify, "PPAP调用OnNotifyJoinLeave接口，加入通知网络");
            // Join
            is_need_join_ = true;

            join_timer_.start();

            time_count_.reset();
        }
        else if (join_or_leave == 0 && is_need_join_)
        {
            LOG4CPLUS_DEBUG_LOG(logger_notify, "PPAP调用OnNotifyJoinLeave接口，离开通知网络");
            // Leave
            is_need_join_ = false;

            if (is_join_success_)
            {
                // 构造 并 发送 Leave 包
                protocol::PeerLeavePacket peer_leave_packet(protocol::Packet::NewTransactionID(), AppModule::Inst()->GetPeerGuid(), god_endpoint_);
                // peer_leave_packet.end_point = god_endpoint_;

                // 发送给上层节点
                AppModule::Inst()->DoSendPacket(peer_leave_packet, protocol::PEER_VERSION_V4);

                // 发送给下层节点
                for (std::map<Guid, PEER_NODE_STATUS>::iterator iter = peer_node_map_.begin();
                    iter != peer_node_map_.end(); ++iter)
                {
                    peer_leave_packet.end_point = iter->second.end_point;
                    AppModule::Inst()->DoSendPacket(peer_leave_packet, protocol::PEER_VERSION_V4);
                }
            }

            peer_to_connect_.clear();
            peer_node_map_.clear();
            is_join_success_ = false;
        }
    }

    void NotifyModule::DoSendJoinRequestPacket(boost::asio::ip::udp::endpoint end_point)
    {
        boost::uint32_t DetectIP = AppModule::Inst()->GetCandidatePeerInfo().DetectIP;
        boost::uint16_t DetectUdpPort = AppModule::Inst()->GetCandidatePeerInfo().DetectUdpPort;

        if (DetectIP == 0 && DetectUdpPort == 0)
        {
            DetectIP = AppModule::Inst()->GetCandidatePeerInfo().IP;
            DetectUdpPort = AppModule::Inst()->GetCandidatePeerInfo().UdpPort;
            LOG4CPLUS_DEBUG_LOG(logger_notify, "DetectIP为0，替换为内网IP = " << 
                AppModule::Inst()->GetCandidatePeerInfo().IP << " port = " << 
                AppModule::Inst()->GetCandidatePeerInfo().UdpPort);
        }

        // 加入包的构造
        protocol::JoinRequestPacket join_request_packet(
            protocol::Packet::NewTransactionID(), AppModule::Inst()->GetPeerGuid(),
            AppModule::Inst()->GetCandidatePeerInfo().IP,
            AppModule::Inst()->GetCandidatePeerInfo().UdpPort,
            DetectIP,
            DetectUdpPort,
            AppModule::Inst()->GetCandidatePeerInfo().StunIP,
            AppModule::Inst()->GetCandidatePeerInfo().StunUdpPort,
            AppModule::Inst()->GetCandidatePeerInfo().PeerNatType,
            end_point);

        LOG4CPLUS_DEBUG_LOG(logger_notify, "加入ip = " << AppModule::Inst()->GetCandidatePeerInfo().IP << 
            " port = " << AppModule::Inst()->GetCandidatePeerInfo().UdpPort << " detect ip = " << DetectIP << 
            " detect port = " << DetectUdpPort);

        // join_request_packet.end_point = end_point;

        // 发给服务器
        AppModule::Inst()->DoSendPacket(join_request_packet, protocol::PEER_VERSION_V4);
    }

    void NotifyModule::NotifyReSend()
    {
        // 如果 is_notify_ == 1 && is_notify_response == 0
        // 则重新通知
        for (std::map<Guid, PEER_NODE_STATUS>::iterator iter = peer_node_map_.begin();
            iter != peer_node_map_.end(); ++iter)
        {
            for (std::map<boost::uint32_t, PEER_TASK_STATUS>::iterator i = iter->second.peer_task_map_.begin();
                i != iter->second.peer_task_map_.end(); ++i)
            {
                if (i->second.is_notify_ && !i->second.is_notify_response_)
                {
                    // 通知重发
                    NotifyPeer(iter->first, i->first);
                }
            }
        }
    }

    void NotifyModule::CalPeerOnline()
    {
        // 在线人数统计
        total_peer_online_ = 1;
        for (std::map<Guid, PEER_NODE_STATUS>::iterator iter = peer_node_map_.begin();
            iter != peer_node_map_.end(); ++iter)
        {
            // 在线人数统计
            total_peer_online_ += iter->second.peer_online_;
        }
        LOG4CPLUS_DEBUG_LOG(logger_notify, "在线人数 = " << total_peer_online_);
    }

    void NotifyModule::CalTaskComplete()
    {
        LOG4CPLUS_DEBUG_LOG(logger_notify, "统计任务完成数 ");
        // 任务完成数清零
        for (std::map<boost::uint32_t, TASK_RECORD>::iterator iter = task_map_.begin();
            iter != task_map_.end(); ++iter)
        {
            iter->second.finish_num_ = 0;
        }

        // 统计任务完成数
        for (std::map<Guid, PEER_NODE_STATUS>::iterator iter = peer_node_map_.begin();
            iter != peer_node_map_.end(); ++iter)
        {
            for (std::map<boost::uint32_t, PEER_TASK_STATUS>::iterator i = iter->second.peer_task_map_.begin();
                i != iter->second.peer_task_map_.end(); ++i)
            {
                task_map_[i->first].finish_num_ += i->second.finish_num_;
                LOG4CPLUS_DEBUG_LOG(logger_notify, "Peer统计任务完成数  task_map[" << i->first << "].finish_num = " << 
                    task_map_[i->first].finish_num_);
            }
        }

        // 统计自己是否完成
        for (std::map<boost::uint32_t, TASK_RECORD>::iterator iter = task_map_.begin();
            iter != task_map_.end(); ++iter)
        {
            if (iter->second.is_my_finish_)
            {
                LOG4CPLUS_DEBUG_LOG(logger_notify, "任务task_id = " << iter->first << " 完成了 ");
                iter->second.finish_num_ += 1;
            }
        }
    }

    // 发送KeepAlive包
    void NotifyModule::DoSendKeepAlive()
    {
        LOG4CPLUS_DEBUG_LOG(logger_notify, "向上级发送KeepAliveRequest包 ");
        // 向上级发送KeepAliveRequest包
        std::vector<protocol::TASK_INFO> my_task_;
        for (std::map<boost::uint32_t, TASK_RECORD>::iterator iter = task_map_.begin();
            iter != task_map_.end(); ++iter)
        {
            if (iter->second.rest_time_ > 0)
            {
                protocol::TASK_INFO task;
                task.TaskID = iter->first;
                task.CompleteCount = iter->second.finish_num_;

                my_task_.push_back(task);
            }
        }

        // 心跳包的构造
        protocol::NotifyKeepAliveRequestPacket keepalive_request_packet(protocol::Packet::NewTransactionID(),
            AppModule::Inst()->GetPeerGuid(), total_peer_online_, my_task_, god_endpoint_);

        // keepalive_request_packet.end_point = god_endpoint_;

        // 发送心跳包
        AppModule::Inst()->DoSendPacket(keepalive_request_packet, protocol::PEER_VERSION_V4);
    }

    void NotifyModule::OnGetNotifyServerList(protocol::QueryNotifyListPacket const & query_notify_response_packet)
    {
        notify_server_s_ = query_notify_response_packet.response.notify_server_info_;

        if (notify_server_s_.size() == 0)
        {
            return;
        }

        server_connect_hops_ = boost::hash_value(AppModule::Inst()->GetPeerGuid()) % notify_server_s_.size();
        for (boost::uint32_t i = 0; i<notify_server_s_.size(); i++)
        {
            LOG4CPLUS_DEBUG_LOG(logger_notify, "IP = " << notify_server_s_[i].IP << " Port = " << 
                notify_server_s_[i].Port);
        }

        is_have_server_endpoint = true;
    }

    double NotifyModule::mylog(double a, double b)
    {
        return log(b) / log(a);
    }
}