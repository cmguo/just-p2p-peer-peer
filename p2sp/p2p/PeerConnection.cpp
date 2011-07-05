//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/AppModule.h"
#include "p2sp/p2p/PeerConnection.h"
#include "p2sp/p2p/P2PDownloader.h"
#include "p2sp/p2p/Assigner.h"
#include "p2sp/p2p/SubPieceRequestManager.h"
#include "p2sp/p2p/IpPool.h"
#include "p2sp/p2p/Exchanger.h"
#include "p2sp/p2p/P2PModule.h"
#include "p2sp/download/DownloadDriver.h"
#include "p2sp/p2p/SessionPeerCache.h"

#include "statistic/P2PDownloaderStatistic.h"
#include "storage/Instance.h"

#define P2P_DEBUG(s) LOG(__DEBUG, "P2P", s)
#define P2P_INFO(s)    LOG(__INFO, "P2P", s)
#define P2P_EVENT(s) LOG(__EVENT, "P2P", s)
#define P2P_WARN(s)    LOG(__WARN, "P2P", s)
#define P2P_ERROR(s) LOG(__ERROR, "P2P", s)

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("p2p");

    void PeerConnection::Start(protocol::ConnectPacket const & reconnect_packet,
        const boost::asio::ip::udp::endpoint& end_point, const protocol::CandidatePeerInfo& peer_info)
    {
        if (is_running_ == true)
            return;

        is_running_ = true;


        assert(p2p_downloader_->GetStatistic());
        statistic_ = p2p_downloader_->GetStatistic()->AttachPeerConnectionStatistic(reconnect_packet.peer_guid_);
        assert(statistic_);

        statistic_->SetPeerVersion(reconnect_packet.peer_version_);
        statistic_->SetPeerDownloadInfo(reconnect_packet.peer_download_info_);
        statistic_->SetCandidatePeerInfo(peer_info);

        assert(statistic_);

        // 根据Packet  rtt_; peer_version_; CandidatePeerinfo; peer_download_info_; end_point_;赋值
        // assert(task_queue_.size() == 0);
        // window_size_ = 2;
        // requesting_count_ = 0;
        // TimeCounter last_receive_time_.Sync;;
        // avg_delt_time_ = 3000;
        //
        // peer_is_downloading_
        // peer_guid;
        // recent_avg_delt_times_ = measure::CycleBuffer::Create(30);

        // 从SessionCachePeer

        connect_rtt_ = rtt_ = framework::timer::TickCounter::tick_count() - reconnect_packet.send_off_time_;
        // rtt_ += P2SPConfigs::PEERCONNECTION_RTT_MODIFICATION_TIME_IN_MILLISEC;
        //
        rtt_ = (uint32_t)(sqrt(rtt_ + 0.0) * 10 + 0.5) + P2SPConfigs::PEERCONNECTION_RTT_MODIFICATION_TIME_IN_MILLISEC;
        P2P_EVENT(__FUNCTION__ << " " << p2p_downloader_ << " " << shared_from_this() << " EndPoint = " << end_point << " RTT = " << rtt_);
        longest_rtt_ = rtt_ + 500;

        peer_version_ = reconnect_packet.peer_version_;
        candidate_peer_info_ = reconnect_packet.peer_info_;
        if (candidate_peer_info_.IP == peer_info.IP && candidate_peer_info_.UdpPort == peer_info.UdpPort)
        {
            P2P_EVENT("SET CANDIDATE_PEER_INFO: " << peer_info);
            candidate_peer_info_ = peer_info;
        }
        peer_download_info_ = reconnect_packet.peer_download_info_;
        end_point_ = end_point;

        requesting_count_ = 0;
        last_receive_time_.reset();
        avg_delt_time_ = rtt_ + 1;

        window_size_ = P2SPConfigs::PEERCONNECTION_MIN_WINDOW_SIZE;  // connect_rtt_ >= 400 ?  P2SPConfigs::PEERCONNECTION_MIN_WINDOW_SIZE : ((P2SPConfigs::PEERCONNECTION_MAX_WINDOW_SIZE - P2SPConfigs::PEERCONNECTION_MIN_WINDOW_SIZE) * (400 - (float)connect_rtt_) / 400 + P2SPConfigs::PEERCONNECTION_MIN_WINDOW_SIZE);
        window_size_init_ = 0;
        avg_delt_time_init_ = 0;

        requested_size_ = 0;
        curr_delta_size_ = P2SPConfigs::PEERCONNECTION_MIN_DELTA_SIZE;
        // window_size_ = 6 * 1000 / avg_delt_time_;
        // LIMIT_MIN_MAX(window_size_, P2SPConfigs::PEERCONNECTION_MIN_WINDOW_SIZE, max(P2SPConfigs::PEERCONNECTION_MIN_WINDOW_SIZE, P2SPConfigs::PEERCONNECTION_MAX_WINDOW_SIZE/2));

        std::set<DownloadDriver__p>::iterator i;
        std::set<DownloadDriver__p> ddset = p2p_downloader_->GetDownloadDrivers();
        for (i = ddset.begin(); i != ddset.end(); i++)
        {
            if (P2PModule::Inst()->GetSessionPeerCache()->IsHit((*i)->GetSessionID(), end_point))
            {
                //
                longest_rtt_ = P2PModule::Inst()->GetSessionPeerCache()->GetSessionPeerInfo((*i)->GetSessionID(), end_point).m_rtt;
                // window_size_ = P2PModule::Inst()->GetSessionPeerCache()->GetSessionPeerInfo((*i)->GetSessionID(), end_point).m_window_size;
                window_size_init_ = P2PModule::Inst()->GetSessionPeerCache()->GetSessionPeerInfo((*i)->GetSessionID(), end_point).m_window_size;
                avg_delt_time_ = P2PModule::Inst()->GetSessionPeerCache()->GetSessionPeerInfo((*i)->GetSessionID(), end_point).m_avg_delt_time;
                avg_delt_time_init_ = P2PModule::Inst()->GetSessionPeerCache()->GetSessionPeerInfo((*i)->GetSessionID(), end_point).m_avg_delt_time;
            }
        }

        last_live_response_time_.reset();
        last_request_time_.reset();
        connected_time_.reset();

        sent_count_ = 0;
        received_count_ = 0;

        assert(task_queue_.size() == 0);

        peer_guid_ = reconnect_packet.peer_guid_;

        statistic_->SetWindowSize(window_size_);
        // statistic_->SetWindowSize(request_size_);
        statistic_->SubmitRTT(rtt_);

        statistic_->SetIsRidInfoValid(IsRidInfoValid());

        DoAnnounce();

        if (p2p_downloader_->GetIpPool()->GetPeerCount() < 500)
        {
            p2p_downloader_->GetExchanger()->DoPeerExchange(candidate_peer_info_);
        }
    }

    void PeerConnection::Stop()
    {
        if (is_running_ == false) return;

        P2P_EVENT("PeerConnection::Stop" << end_point_);

        // task_queue.clear();
        P2PModule::Inst()->RemoveRequestCount(requesting_count_);
        task_queue_.clear();

        //
        IpPool::p ip_pool = p2p_downloader_->GetIpPool();
        ip_pool->OnDisConnect(end_point_);

        //
        assert(p2p_downloader_->GetStatistic());
        assert(statistic_);
        p2p_downloader_->GetStatistic()->DetachPeerConnectionStatistic(statistic_);

        p2p_downloader_.reset();
        subpiece_request_manager_.reset();
        statistic_.reset();

        // recent_avg_delt_times_.reset();

        is_running_ = false;
    }

    bool PeerConnection::RequestTillFullWindow(bool need_check)
    {
        if (is_running_ == false) return false;

        // P2P_EVENT("PeerConnection::RequestTillFullWindow() task_queue size:" << task_queue_.size() << " requesting count: " << requesting_count_ << " windowsize " << window_size_);
        // while (requesting_count_ < window_size_)
        //   从task_queue中pop一个SubPieceInfo
        //       如果没有了 返回false
        //   RequestSubPiece(protocol::SubPieceInfo);
        //
        // 返回true
        /*
            LOG(__DEBUG, "P2P", "P2PDownloader = " << p2p_downloader_ << ", Endpoint = " << end_point_
                        << ", RequestTillFullWindow Begin, TaskQueueSize = " << task_queue_.size());*/

        uint32_t delta = 0;
        curr_request_count_ = 0;
        if (false == need_check && window_size_ - requesting_count_ < curr_delta_size_)
        {
            delta = curr_delta_size_ + requesting_count_ - window_size_;
            window_size_ += delta;
            /*
                LOG(__DEBUG, "P2P", "P2PDownloader = " << p2p_downloader_ << ", Endpoint = " << end_point_
                                << ", WindowSize Need Increase, Delta = " << delta);*/

        }
        curr_time_out_ = statistic_->GetAverageRTT() + p2p_downloader_->GetRTTPlus();
        while (CanRequest())
        {
            if (task_queue_.size() != 0)
            {
                if (peer_version_ >= 0x00000101)
                {
                    boost::int32_t remaining = window_size_ - requesting_count_;
                    if (remaining >= (boost::int32_t)P2SPConfigs::P2P_DOWNLOAD_MULTI_SUBPIECES_COUNT)
                    {
                        remaining = (boost::int32_t)P2SPConfigs::P2P_DOWNLOAD_MULTI_SUBPIECES_COUNT;
                    }
                    boost::int32_t copy = (remaining) / 2;
                    LIMIT_MIN(copy, (boost::int32_t)P2SPConfigs::PEERCONNECTION_MIN_PACKET_COPY_COUNT);
                    // 新版peer
                    RequestSubPieces(remaining, copy, need_check);
                }
                else
                {
                    // 旧版Peer，1个1个请求
                    /*
                    RequestSubPieces(2, 1);
                    /*/
                    RequestSubPiece(task_queue_.front(), need_check);
                    task_queue_.pop_front();
                    // */
                }
            }
            else
            {
                P2P_EVENT("no more subpieces");
                break;
            }
        }
        // check
        /*
            LOG(__DEBUG, "timeout", "P2PDownloader = " << p2p_downloader_ << ", Endpoint = " << end_point_
                        << ", RequestTillFullWindow End, delta = " << delta << ", curr_request_count_ = " << curr_request_count_);*/
        if (delta > 0)
        {
            uint32_t v = delta;  // min(delta, curr_request_count_);
            window_size_ -= v;
            requesting_count_ -= v;
            P2PModule::Inst()->RemoveRequestCount(v);
        }
        return true;
    }


    void PeerConnection::RequestNextSubpiece()
    {
        ++accumulative_subpiece_num;
        if (accumulative_subpiece_num < 4)
        {
            P2P_DEBUG(shared_from_this() << ", accumulative_subpiece_num:" << accumulative_subpiece_num);
            return;
        }
        P2P_DEBUG(shared_from_this() << ", accumulative_subpiece_num:" << accumulative_subpiece_num << ", RequestTillFullWindow");
        accumulative_subpiece_num = 0;
        RequestTillFullWindow(true);
    }
    // bool PeerConnection::RequestNextSubpiece(const SubPieceInfo& subpiece_info)
    // {
    //    if (is_running_ == false) return false;

    //    if (P2PModule::Inst()->CanRequest())
    //    {
    //            RequestSubPiece(subpiece_info);
    //            return true;
    //    }
    //    return false;
    // }

    bool PeerConnection::RequestSubPieces(uint32_t piece_count, uint32_t copy_count, bool need_check)
    {
        if (false == is_running_)
        {
            return false;
        }

        if (piece_count == 0 || task_queue_.empty())
        {
            return false;
        }

        if (requesting_count_ == 0)
        {
            last_receive_time_.reset();
        }

        std::vector<protocol::SubPieceInfo> subpieces;
        for (uint32_t i = 0; i < piece_count; ++i)
        {
            if (task_queue_.empty())
            {
                break;
            }
            const protocol::SubPieceInfo & sp = task_queue_.front();
            if (!need_check || !p2p_downloader_->HasSubPiece(sp))
            {
                subpieces.push_back(sp.GetSubPieceInfoStruct());
            }
            task_queue_.pop_front();
        }

        if (subpieces.size() == 0)
        {
            return false;
        }

        // 检查对方Peer版本，旧版本的只发1个
        if (peer_version_ >= 0x00000101)
        {
            // check copy_count
            LIMIT_MIN_MAX(copy_count, 1, subpieces.size());
        }
        else
        {
            copy_count = 1;
        }

        uint32_t trans_id = protocol::Packet::NewTransactionID();

        if (peer_version_ >= protocol::PEER_VERSION_V4)
        {
            // 对方协议是PEER_VERSION_V4以上版本，priority字段必须设置
            protocol::RequestSubPiecePacket packet(trans_id,
                p2p_downloader_->GetRid(), subpieces, end_point_, p2p_downloader_->GetDownloadPriority());

            for (uint32_t i = 0; i < copy_count; ++i) 
            {
                p2p_downloader_->DoSendPacket(packet, peer_version_);
            }
            statistic_->SubmitUploadedBytes(packet.length());    
        }
        else if (peer_version_ == protocol::PEER_VERSION_V3)
        {
            // 对方协议是0x00000104版本，priority字段必须设置
            protocol::RequestSubPiecePacket packet(trans_id,
                p2p_downloader_->GetRid(), subpieces, end_point_, protocol::RequestSubPiecePacket::INVALID_PRIORITY);

            for (uint32_t i = 0; i < copy_count; ++i) {
                p2p_downloader_->DoSendPacket(packet, peer_version_);
            }
            statistic_->SubmitUploadedBytes(copy_count * packet.length());
        }
        else
        {
            protocol::RequestSubPiecePacketOld packet(trans_id,
                p2p_downloader_->GetRid(), AppModule::Inst()->GetPeerGuid(), subpieces, end_point_);

            // send multiple packets
            for (uint32_t i = 0; i < copy_count; ++i) {
                p2p_downloader_->DoSendPacket(packet, peer_version_);
            }

            statistic_->SubmitUploadedBytes(copy_count * packet.length());
        }

        last_request_time_.reset();

        // 打印日志，这是PPLogMonitor请求包的来源
        if (p2p_downloader_->GetDownloadDrivers().size() != 0)
        {
            for (uint32_t i = 0; i < subpieces.size(); ++i)
            {
                P2P_EVENT("PeerConnection::RequestSubPiece " << (*(p2p_downloader_->GetDownloadDrivers().begin()))->GetDownloadDriverID() << " " << 0 << " " << 1 << " " << shared_from_this() << " " << protocol::SubPieceInfo(subpieces[i]));
                P2P_EVENT("PeerConnection::SubpieceTimeOut: " << *(p2p_downloader_->GetDownloadDrivers().begin()) << " " << shared_from_this() << " " << protocol::SubPieceInfo(subpieces[i]) << " " << curr_time_out_);
            }
        }

        curr_request_count_ += subpieces.size();
        requesting_count_ += subpieces.size();
        sent_count_ += subpieces.size();
        requested_size_ += subpieces.size();

        // curr_time_out_ += avg_delt_time_ * (copy_count / 2);
        /*
        LOG(__DEBUG, "P2P", "P2PDownloader = " << p2p_downloader_ << ", Endpoint = " << end_point_
                    << ", SendSubpieceCount = " << subpieces.size());*/

        for (uint32_t i = 0; i < subpieces.size(); ++i)
        {
            P2P_EVENT("bingo Subpiece = " << subpieces[i] << " trans_id = " << trans_id);
            subpiece_request_manager_->Add(subpieces[i], curr_time_out_, shared_from_this());
            /*
                LOG(__DEBUG, "timeout", "P2PDownloader = " << p2p_downloader_
                                << ", Endpoint = " << end_point_ << ", SubPiece = " << SubPieceInfo(subpieces[i])
                                << ", Send, TimeOut = " << curr_time_out_);*/

            curr_time_out_ += avg_delt_time_;
            P2PModule::Inst()->AddRequestCount();
        }

        return true;
    }

    bool PeerConnection::RequestSubPiece(const protocol::SubPieceInfo& subpiece_info, bool need_check)
    {
        if (is_running_ == false)
            return false;
        assert(statistic_);
        // statistic_->SetAssignedSubPieceCount(task_queue_.size());

        // if (requesting_count_ == 0)
        //    last_recieve_time_.sync();

        if (requesting_count_ == 0)
            last_receive_time_.reset();

        // 计算出timeout
        if (need_check && p2p_downloader_->HasSubPiece(subpiece_info))
        {
            return false;
        }

        uint32_t trans_id = protocol::Packet::NewTransactionID();

        if (peer_version_ >= protocol::PEER_VERSION_V4)
        {
            // 对方协议是PEER_VERSION_V4以上版本，priority字段必须设置
            protocol::RequestSubPiecePacket packet(trans_id,
                p2p_downloader_->GetRid(), subpiece_info, end_point_, protocol::RequestSubPiecePacket::DEFAULT_PRIORITY);

            p2p_downloader_->DoSendPacket(packet, peer_version_);
            statistic_->SubmitUploadedBytes(packet.length());
        }
        else if (peer_version_ == protocol::PEER_VERSION_V3)
        {
            protocol::RequestSubPiecePacket packet(trans_id,
                p2p_downloader_->GetRid(), subpiece_info, end_point_, protocol::RequestSubPiecePacket::INVALID_PRIORITY);

            p2p_downloader_->DoSendPacket(packet, peer_version_);
            statistic_->SubmitUploadedBytes(packet.length());
        }
        else
        {
            protocol::RequestSubPiecePacketOld packet(trans_id,
                p2p_downloader_->GetRid(), AppModule::Inst()->GetPeerGuid(), subpiece_info, end_point_);

            p2p_downloader_->DoSendPacket(packet, peer_version_);
            statistic_->SubmitUploadedBytes(packet.length());
        }

        last_request_time_.reset();

        if (p2p_downloader_->GetDownloadDrivers().size() != 0)
        {
            P2P_EVENT("PeerConnection::RequestSubPiece " << (*(p2p_downloader_->GetDownloadDrivers().begin()))->GetDownloadDriverID() << " " << 0 << " " << 1 << " " << shared_from_this() << " " << subpiece_info);
            P2P_EVENT("PeerConnection::SubpieceTimeOut: " << *(p2p_downloader_->GetDownloadDrivers().begin()) << " " << shared_from_this() << " " << subpiece_info << " " << curr_time_out_);
        }

        ++curr_request_count_;
        ++requesting_count_;
        ++sent_count_;
        ++requested_size_;

        /*
            LOG(__DEBUG, "timeout", "P2PDownloader = " << p2p_downloader_
                        << ", Endpoint = " << end_point_ << ", SubPiece = " << subpiece_info
                        << ", Send, TimeOut = " << curr_time_out_ << ", single");*/


        subpiece_request_manager_->Add(subpiece_info, curr_time_out_, shared_from_this());
        curr_time_out_ += avg_delt_time_;
        P2PModule::Inst()->AddRequestCount();

        return true;
    }

    void PeerConnection::DoAnnounce()
    {
        if (is_running_ == false)
            return;

        P2P_EVENT("PeerConnection::DoAnnounce");

        last_request_time_.reset();

        protocol::RequestAnnouncePacket  packet(protocol::Packet::NewTransactionID(),
            p2p_downloader_->GetRid(), AppModule::Inst()->GetPeerGuid(), end_point_);

        p2p_downloader_->DoSendPacket(packet, peer_version_);

        assert(statistic_);
        //   statistic_->SubmitUploadedBytes(packet->GetBuffer().Length());
        statistic_->SubmitUploadedBytes(packet.length());

        // 请求rid信息
        if (peer_version_ >= 0x00000007)
        {
            DoRequestRIDInfo();
        }

    }

    void PeerConnection::DoRequestRIDInfo()
    {
        if (false == is_running_)
            return;

        protocol::RIDInfoRequestPacket packet(protocol::Packet::NewTransactionID(),
            p2p_downloader_->GetRid(), peer_guid_, end_point_);

        p2p_downloader_->DoSendPacket(packet, peer_version_);
        p2p_downloader_->DoSendPacket(packet, peer_version_);

        //  assert(statistic_);
        //   statistic_->SubmitUploadedBytes(packet->GetBuffer().Length());
        statistic_->SubmitUploadedBytes(packet.length());
    }

    void PeerConnection::DoReportDownloadSpeed()
    {
        if (false == is_running_)
            return;

        P2P_EVENT("PeerConnection::DoReportDownloadSpeed " << shared_from_this());

        protocol::ReportSpeedPacket packet(protocol::Packet::NewTransactionID(),
            p2p_downloader_->GetRid(), peer_guid_,
            statistic_->GetSpeedInfo().NowDownloadSpeed, end_point_);

        p2p_downloader_->DoSendPacket(packet, peer_version_);
        p2p_downloader_->DoSendPacket(packet, peer_version_);

        assert(statistic_);
        statistic_->SubmitUploadedBytes(2 * packet.length());
    }

    void PeerConnection::OnP2PTimer(boost::uint32_t times)
    {
        if (is_running_ == false) return;

        // 每250ms集中发出请求
        if (accumulative_subpiece_num != 0)
        {
            P2P_DEBUG("OnP2PTimer:" << shared_from_this() << ", accumulative_subpiece_num:" << accumulative_subpiece_num << ", RequestTillFullWindow");
            accumulative_subpiece_num = 0;
            RequestTillFullWindow(true);
        }

        // P2P_EVENT("PeerConnection::OnP2PTimer " << shared_from_this());

        // if (times % (4*10))
        //        如果block_map_为空 则
        //            DoAnnounce();
        //            return;
        if (last_request_time_.elapsed() > 20 * 1000)
        {
            DoAnnounce();
        }
        else if (times % (4*10) == 0)
        {
            if (!block_map_.GetCount())
            {
                DoAnnounce();
            }
            else if (times % (4 * 20) == 0)
            {
                // 如果block_map_满
                //    return;
                // 判断peer_is_downloading_
                //    为true 则DoAnnounce();
                if (peer_download_info_.IsDownloading
                    && false == block_map_.IsFull())
                {
                    DoAnnounce();
                }
            }
        }

        // 每5秒钟向上传的peer汇报一次下载速度
        if (times % 20 == 0)
            DoReportDownloadSpeed();

        // 修正windows_size
        if (times % 4 == 0)
        {
            // window_size_ = statistic_->GetSpeedInfo().NowDownloadSpeed / 1024 * longest_rtt_ * (sent_count_ - requesting_count_ + 2) / (1000*(received_count_ + 1)) * 4 / 3;
            uint32_t curr_win = (uint32_t)(statistic_->GetSpeedInfo().NowDownloadSpeed / 1000.0 + 0.5);
            window_size_ = (uint32_t)(curr_win < window_size_ ? (curr_win * 0.1 + window_size_ * 0.9 + 0.5) : curr_win);
            // window_size_ = (statistic_->GetAverageRTT() / avg_delt_time_ + 1.5);

            if (window_size_ > 0 && window_size_ < window_size_init_)
            {
                if (window_size_init_ > 6)
                {
                    window_size_ = (boost::uint32_t)(window_size_ * 0.3 + window_size_init_ * 0.7 + 0.5);
                }
                else
                {
                    window_size_ = window_size_init_;
                }
            }

            LIMIT_MIN_MAX(window_size_, P2SPConfigs::PEERCONNECTION_MIN_WINDOW_SIZE, P2SPConfigs::PEERCONNECTION_MAX_WINDOW_SIZE);

            statistic_->SetWindowSize(window_size_);

            //
            if (GetConnectedTime() <= 3000)
            {
                curr_delta_size_ = P2SPConfigs::PEERCONNECTION_MIN_DELTA_SIZE + 2;
                LOG(__DEBUG, "timeout", "P2PDownloader = " << p2p_downloader_
                    << ", Endpoint = " << end_point_ << ", curr_delta_size = " << curr_delta_size_
                    << ", GetConnectedTime() <= 3000");
            }
            else if (GetConnectedTime() <= 10000 && p2p_downloader_->GetCurrentDownloadSpeed()
                <= p2p_downloader_->GetDataRate() + 10 * 1024)
            {
                curr_delta_size_ = P2SPConfigs::PEERCONNECTION_MIN_DELTA_SIZE;
                LOG(__DEBUG, "timeout", "P2PDownloader = " << p2p_downloader_
                    << ", Endpoint = " << end_point_ << ", curr_delta_size = " << curr_delta_size_
                    << ", NowDownloadSpeed <= DataRate + 10*1024");
            }
            else
            {
                curr_delta_size_ = 0;  // P2SPConfigs::PEERCONNECTION_MIN_DELTA_SIZE;
                LOG(__DEBUG, "timeout", "P2PDownloader = " << p2p_downloader_
                    << ", Endpoint = " << end_point_ << ", curr_delta_size = " << curr_delta_size_
                    << ", OK");
            }
        }

        if (times % 4 == 0)
        {
            statistic_->SetSentCount(sent_count_);
            statistic_->SetRequestingCount(requesting_count_);
            statistic_->SetReceivedCount(received_count_);
        }
    }

    void PeerConnection::OnSubPiece(uint32_t subpiece_rtt, uint32_t buffer_length)
    {
        if (is_running_ == false)
            return;

        //
        last_live_response_time_.reset();
        // ip可能改变
        //    end_point_ = end_point;
        assert(statistic_);
        statistic_->SubmitDownloadedBytes(buffer_length);

        recent_avg_delt_times_.Push(last_receive_time_.elapsed());
        avg_delt_time_ = (uint32_t)recent_avg_delt_times_.Average();

        LIMIT_MAX(avg_delt_time_, 1000);
        last_receive_time_.reset();
        if (requesting_count_ > 0)
            requesting_count_--;
        P2PModule::Inst()->RemoveRequestCount();
        received_count_++;

        // !
        // window_size_ = 6 * 1000 / avg_delt_time_;
        // LIMIT_MIN_MAX(window_size_, 2, P2SPConfigs::PEERCONNECTION_MAX_WINDOW_SIZE);

        // statistic_->SetWindowSize(window_size_);
        // statistic_->SetWindowSize(request_size_);
        statistic_->SubmitRTT(subpiece_rtt);

        P2P_EVENT(__FUNCTION__ << " " << shared_from_this() << " RTT = " << subpiece_rtt);

        // window_size_++;
        RequestNextSubpiece();
        // window_size_--;
    }

    void PeerConnection::OnRIDInfoResponse(protocol::RIDInfoResponsePacket const & packet)
    {
        if (false == is_running_)
            return;

        assert(statistic_);
        statistic_->SubmitDownloadedBytes(sizeof(protocol::RIDInfoResponsePacket));

        rid_info_.rid_ = packet.resource_id_;
        rid_info_.file_length_ = packet.file_length_;
        rid_info_.block_size_ = packet.block_size_;
        rid_info_.block_count_ = packet.block_md5_.size();
        rid_info_.block_md5_s_ = packet.block_md5_;

        // 校验
        protocol::RidInfo rid_info;
        p2p_downloader_->GetInstance()->GetRidInfo(rid_info);
        CheckRidInfo(rid_info);
        statistic_->SetIsRidInfoValid(IsRidInfoValid());
    }

    void PeerConnection::OnTimeOut(SubPieceRequestTask::p subpiece_request_task)
    {
        if (is_running_ == false)
            return;

        P2P_EVENT("PeerConnection::OnTimeOut " << shared_from_this());

        // requesting_count--;
        // 更改统计值
        // Request_Till_Full_Window();
        if (requesting_count_ > 0)
            requesting_count_--;

        P2PModule::Inst()->RemoveRequestCount();

        // resend current
        // task_queue_.push_front(subpiece_request_task->subpiece_info_);

        // P2SPConfigs::PEERCONNECTION_MIN_PACKET_COPY_COUNT++;
        RequestNextSubpiece();
        // P2SPConfigs::PEERCONNECTION_MIN_PACKET_COPY_COUNT--;

        // p2p_downloader_->GetAssigner()->RequestNextSubpiece();
    }

    bool PeerConnection::IsRequesting() const
    {
        if (is_running_ == false) return false;

        return requesting_count_ == 0;
    }

    boost::uint32_t PeerConnection::GetUsedTime()
    {
        if (is_running_ == false)
            return 0;

        uint32_t v = avg_delt_time_;
        if (avg_delt_time_init_ > 0 && v > avg_delt_time_init_)
        {
            v = (boost::uint32_t)((avg_delt_time_init_ * 7 + avg_delt_time_ * 3 + 5) / 10);
        }
        LIMIT_MIN(v, 10);
        return v;
    }

    uint32_t PeerConnection::GetUsedTimeForAssigner()
    {
        if (is_running_ == false)
            return 0;

        uint32_t t = 0;
        if (window_size_ < requesting_count_)
        {
            t = GetUsedTime();
            if (t > last_receive_time_.elapsed())
            {
                t -= last_receive_time_.elapsed();
            }
        }
        if (statistic_->GetTotalRTTCount() < 3)
        {
            t = 10 + statistic_->GetTotalRTTCount() * 30;
        }
        return t;

        // return GetTickCount - last_receive_time_ + avg_delt_time;
        // if (requesting_count_ == 0)
        //    return avg_delt_time_;
        // uint32_t recv_time;
        // if (GetUsedTime() < last_receive_time_.elapsed() || window_size_ > requesting_count_)
        //    recv_time = 0;
        // else
        //    recv_time = GetUsedTime() - last_receive_time_.elapsed();
        // LIMIT_MAX(recv_time, 10000);
        // uint32_t value = recv_time + GetUsedTime();

        // LIMIT_MIN_MAX(value, 150, 10000);
        /*return value;*/
    }

    boost::uint32_t PeerConnection::GetRequestedCount() const
    {
        if (is_running_ == false) return 0;

        // return requesting_count_;
        return requested_size_;
    }

    bool PeerConnection::CanKick()
    {
        if (false == is_running_) {
            LOGX(__DEBUG, "kick", "Endpoint = " << end_point_ << ", Not Running, CanKick return true");
            return true;
        }
        // if (connected_time_.GetElapsed() >= 3*1000 && statistic_->GetAverageRTT() == statistic_->GetLongestRtt())
        if (connected_time_.elapsed() >= 3 * 1000 && statistic_->GetSpeedInfo().NowDownloadSpeed < 5*1024)
        {
            LOGX(__DEBUG, "kick", "Endpoint = " << end_point_ << ", Elapsed >= 3000, CanKick return true"
                << ", sent_count = " << sent_count_ << ", received_count = " << received_count_);
            return true;
        }
        if (connected_time_.elapsed() >= P2SPConfigs::PEERCONNECTION_CAN_KICK_TIME_IN_MILLISEC)
        {
            LOGX(__DEBUG, "kick", "Endpoint = " << end_point_ << ", Elapsed >= 8000, CanKick return true");
            return true;
        }
        // if (connected_time_.elapsed() >= P2SPConfigs::PEERCONNECTION_CAN_KICK_TIME_IN_MILLISEC)
        // {
        //    if (p2p_downloader_->GetSpeedInfo().NowDownloadSpeed >= p2p_downloader_->GetDataRate() + 5 * 1024)
        //    {
        //        if (statistic_->GetSpeedInfo().NowDownloadSpeed < P2SPConfigs::PEERCONNECTION_CAN_KICK_SPEED_IN_BPS * 0.5)
        //        {
        //            LOGX(__DEBUG, "kick", "Endpoint = " << end_point_ << ", Elapsed >= 12000, 0.7, CanKick return true");
        //            return true;
        //        }
        //    }
        //    else
        //    {
        //        if (statistic_->GetSpeedInfo().NowDownloadSpeed < P2SPConfigs::PEERCONNECTION_CAN_KICK_SPEED_IN_BPS)
        //        {
        //            LOGX(__DEBUG, "kick", "Endpoint = " << end_point_ << ", Elapsed >= 12000, 1.0, CanKick return true");
        //            return true;
        //        }
        //    }
        // }
        return false;
    }

    void PeerConnection::KeepAlive()
    {
        DoReportDownloadSpeed();
    }
}
