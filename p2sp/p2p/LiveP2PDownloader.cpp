#include "Common.h"
#include "p2sp/download/LiveStream.h"
#include "LiveP2PDownloader.h"
#include "p2sp/p2p/P2PModule.h"
#include "p2sp/tracker/TrackerModule.h"
#include "random.h"
#include "p2sp/proxy/PlayInfo.h"
#include "statistic/DACStatisticModule.h"

namespace p2sp
{
    LiveP2PDownloader::LiveP2PDownloader(const RID & rid, LiveStream__p live_stream)
        : rid_(rid)
        , live_stream_(live_stream)
        , is_running_(false)
        , is_p2p_pausing_(true)
        , total_all_request_subpiece_count_(0)
        , total_request_subpiece_count_(0)
        , dolist_time_interval_(1000)
        , dolist_udpserver_times_(0)
        , dolist_udpserver_time_interval_(1000)
        , connected_udpserver_count_(0)
        , should_use_udpserver_(false)
        , use_udpserver_reason_(NO_USE_UDPSERVER)
        , times_of_use_udpserver_because_of_urgent_(0)
        , times_of_use_udpserver_because_of_large_upload_(0)
        , time_elapsed_use_udpserver_because_of_urgent_(0)
        , time_elapsed_use_udpserver_because_of_large_upload_(0)
        , download_bytes_use_udpserver_because_of_urgent_(0)
        , download_bytes_use_udpserver_because_of_large_upload_(0)
        , should_connect_udpserver_(false)
        , total_connect_peers_count_(0)
        , live_subpiece_count_manager_(live_stream_->GetInstance()->GetLiveInterval())
        , total_unused_subpiece_count_(0)
        , total_received_subpiece_count_(0)
        , total_p2p_data_bytes_(0)
        , total_udpserver_data_bytes_(0)
    {
        send_peer_info_packet_interval_in_second_ = BootStrapGeneralConfig::Inst()->GetSendPeerInfoPacketIntervalInSecond();
        urgent_rest_playable_time_delim_ = BootStrapGeneralConfig::Inst()->GetUrgentRestPlayableTimeDelim(live_stream_->IsSavingMode());
        safe_rest_playable_time_delim_ = BootStrapGeneralConfig::Inst()->GetSafeRestPlayableTimeDelim(live_stream_->IsSavingMode());
        safe_enough_rest_playable_time_delim_ = BootStrapGeneralConfig::Inst()->GetSafeEnoughRestPlayabelTimeDelim(live_stream_->IsSavingMode());
        using_udpserver_time_in_second_delim_ = BootStrapGeneralConfig::Inst()->GetUsingUdpServerTimeDelim(live_stream_->IsSavingMode());
        using_udpserver_time_at_least_when_large_upload_ = BootStrapGeneralConfig::Inst()->GetUsingCDNOrUdpServerTimeDelim(live_stream_->IsSavingMode());
        use_udpserver_count_ = BootStrapGeneralConfig::Inst()->GetUseUdpserverCount();
        udpserver_protect_time_when_start_ = BootStrapGeneralConfig::Inst()->GetUdpServerProtectTimeWhenStart(live_stream_->IsSavingMode());
        should_use_bw_type_ = BootStrapGeneralConfig::Inst()->GetShouldUseBWType();
        p2p_max_connect_count_ = default_connection_limit_ = BootStrapGeneralConfig::Inst()->GetLivePeerMaxConnections();
        live_connect_low_normal_threshold_ = BootStrapGeneralConfig::Inst()->GetLiveConnectLowNormalThresHold();
        live_connect_normal_high_threshold_ = BootStrapGeneralConfig::Inst()->GetLiveConnectNormalHighThresHold();
        live_exchange_interval_in_second_ = BootStrapGeneralConfig::Inst()->GetLiveExchangeIntervalInSecond();
        live_extended_connections_ = BootStrapGeneralConfig::Inst()->GetLiveExtendedConnections();
    }

    void LiveP2PDownloader::Start()
    {
        // IPPool
        ippool_ = IpPool::create(BootStrapGeneralConfig::Inst()->GetDesirableLiveIpPoolSize());
        ippool_->Start();

        // UdpServer Pool
        const size_t DesirableUdpServerIpPoolSize = 100;
        udpserver_pool_ = IpPool::create(DesirableUdpServerIpPoolSize);
        udpserver_pool_->Start();

        // Exchange
        exchanger_ = Exchanger::create(shared_from_this(), ippool_, true);
        exchanger_->Start();

        // PeerConnector
        connector_ = PeerConnector::create(shared_from_this(), ippool_);
        connector_->Start();

        // UdpServer Connector
        udpserver_connector_ = PeerConnector::create(shared_from_this(), udpserver_pool_);
        udpserver_connector_->Start(shared_from_this());

        // Assigner
        live_assigner_.Start(shared_from_this());

        // SpeedInfo
        p2p_speed_info_.Start();
        p2p_subpiece_speed_info_.Start();
        udp_server_speed_info_.Start();
        udp_server_subpiece_speed_info_.Start();

        last_dolist_time_.start();
        last_dolist_udpserver_time_.start();

        live_exchange_tick_counter_.start();

        is_running_ = true;
    }

    LIVE_CONNECT_LEVEL LiveP2PDownloader::GetConnectLevel()
    {
        LIVE_CONNECT_LEVEL level = MEDIUM;
        int32_t rest_time = live_stream_->GetRestPlayableTimeInSecond();
        int32_t downloadable_peers_count = this->GetDownloadablePeersCount();
        int32_t connection_threshold = p2p_max_connect_count_ / 3;

        if (rest_time >= live_connect_low_normal_threshold_ && !this->is_p2p_pausing_)
        {
            // enough time; don't need to rush for new connections
            // under http only mode, still try new connection
            if (downloadable_peers_count >= connection_threshold)
            {
                level = LOW;
            }
            else
            {
                level = MEDIUM;
            }
        }
        else if (rest_time >= live_connect_normal_high_threshold_)
        {   
            if (downloadable_peers_count >= connection_threshold)
            {
                level = MEDIUM;
            }
            else
            {
                level = HIGH;
            }
        }
        else
        {
            // urgent. Replace connection quickly
            level = HIGH;
        }        

        //DebugLog("rest: %d good peers: %d level: %d", rest_time, downloadable_peers_count, level);

        return level;
    }

    storage::LiveInstance__p LiveP2PDownloader::GetInstance() const
    {
        return live_stream_->GetInstance();
    }

    uint32_t LiveP2PDownloader::GetDownloadablePeersCount() const
    {
        return live_connection_manager_.GetDownloadablePeersCount();
    }

    // 250ms 被调用一次
    void LiveP2PDownloader::OnP2PTimer(boost::uint32_t times)
    {
        // s List / Exchange 一次
        if (times % 4 == 0)
        {
            if (exchanger_)
            {
                if (GetConnectLevel() == HIGH && live_exchange_tick_counter_.elapsed() > live_exchange_interval_in_second_ * 1000)
                {
                    protocol::CandidatePeerInfo candidate_peer_info;
                    if (ippool_->GetForExchange(candidate_peer_info))
                    {
                        exchanger_->DoPeerExchange(candidate_peer_info);
                        live_exchange_tick_counter_.reset();
                    }
                }

                exchanger_->OnP2PTimer(times);
            }

            DoList();
        }

        // 每秒
        // 驱动一次发起连接和断开连接
        if (times % 4 == 0)
        {
            if (connector_)
            {
                connector_->OnP2PTimer(times);
            }

            if (udpserver_connector_)
            {
                udpserver_connector_->OnP2PTimer(times);
            }

            if (GetRestTimeInSeconds() >= 20)
            {
                urgent_tick_counter_.reset();
            }
            else
            {
                safe_tick_counter_.reset();
            }

            if (urgent_tick_counter_.elapsed() > 30 * 1000)
            {
                p2p_max_connect_count_ = default_connection_limit_ + live_extended_connections_;
            }

            if (safe_tick_counter_.elapsed() > 30 * 1000)
            {
                p2p_max_connect_count_ = default_connection_limit_;
            }

            LIVE_CONNECT_LEVEL connect_level = GetConnectLevel();
            KickPeerConnection(connect_level);
            InitPeerConnection(connect_level);
        }

        // 每秒
        // 删除 block_count_map 和 bitmap 中过期的数据
        if (times % 4 == 0)
        {
            // 删除block_count_map中过期的节点
            live_subpiece_count_manager_.EliminateElapsedSubPieceCountMap(live_stream_->GetPlayingPosition().GetBlockId());

            // 删除PeerConnection中过期的节点
            live_connection_manager_.EliminateElapsedBlockBitMap(live_stream_->GetPlayingPosition().GetBlockId());
        }

        if (!is_p2p_pausing_)
        {
            if (times % 4 == 0)
            {
                live_subpiece_request_manager_.OnP2PTimer(times);
            }
        }

        if (times % 4 == 0)
        {
            bool tmp_should_use_udpserver = should_use_udpserver_;

            CheckShouldUseUdpServer();

            // 需要切换
            if (should_use_udpserver_ != tmp_should_use_udpserver)
            {
                // 由不用改为用
                if (should_use_udpserver_ == true)
                {
                    switch (use_udpserver_reason_)
                    {
                    case URGENT:
                        WillUseUdpServerBecauseOfUrgent();
                        break;

                    case LARGE_UPLOAD:
                        WillUseUdpServerBecauseOfLargeUpload();
                        break;

                    default:
                        break;
                    }

                    use_udpserver_tick_counter_.reset();
                }
                else
                {
                    switch (use_udpserver_reason_)
                    {
                    case URGENT:
                        WillNotUseUdpServerBecauseOfNotUrgent();
                        break;

                    case LARGE_UPLOAD:
                        WillNotUseUdpServerBecauseOfSmallUpload();
                        break;

                    default:
                        break;
                    }

                    use_udpserver_reason_ = NO_USE_UDPSERVER;
                }
            }

            if (should_use_udpserver_ == false)
            {
                if (live_connection_manager_.GetConnectedUdpServerCount() > 0 &&
                    live_stream_->GetRestPlayableTimeInSecond() > safe_enough_rest_playable_time_delim_)
                {
                    DeleteAllUdpServer();
                }
            }

            UpdateUsingUdpServerStatus();
        }

        // 预分配
        live_assigner_.OnP2PTimer(times, GetConnectLevel() == HIGH, should_use_udpserver_, is_p2p_pausing_);

        live_connection_manager_.OnP2PTimer(times);

        assert(send_peer_info_packet_interval_in_second_ != 0);

        if (times % (send_peer_info_packet_interval_in_second_ * 4) == 0)
        {
            SendPeerInfo();
        }

        if (times % (4*30) == 0)
        {
            ippool_->KickTrivialCandidatePeers();
            udpserver_pool_->KickTrivialCandidatePeers();
        }
    }

    class UdpServerHistoricalScoreCalculator
        : public PeersScoreCalculator
    {
    private:
        std::map<boost::uint32_t, boost::uint32_t> historical_score_;
    public:
        UdpServerHistoricalScoreCalculator(LiveDownloadDriver__p live_download_driver)
        {
            historical_score_ = live_download_driver->GetUdpServerServiceScore();
        }

        size_t GetPeerScore(const protocol::SocketAddr& socket_address) const
        {
            std::map<boost::uint32_t, boost::uint32_t>::const_iterator iter = historical_score_.find(socket_address.IP);
            if (iter != historical_score_.end())
            {
                return iter->second;
            }

            return static_cast<size_t>(UdpServerScore::DefaultUdpServerScore);
        }
    };

    // 查询到的节点加入IPPool
    void LiveP2PDownloader::AddCandidatePeers(const std::vector<protocol::CandidatePeerInfo> & peers, bool is_live_udpserver,
        bool is_live_udpserver_from_cdn, bool is_live_udpserver_from_bs)
    {
        if (is_live_udpserver)
        {
            if (BootStrapGeneralConfig::Inst()->UdpServerUsageHistoryEnabled())
            {
                UdpServerHistoricalScoreCalculator score_calculator(live_stream_->GetDownloadDriver());
                udpserver_pool_->AddCandidatePeers(peers, is_live_udpserver_from_bs, score_calculator, is_live_udpserver_from_cdn);
            }
            else
            {
                udpserver_pool_->AddCandidatePeers(peers, is_live_udpserver_from_bs, is_live_udpserver_from_cdn);
            }
        }
        else
        {
            if (ippool_->GetPeerCount() == 0)
            {
                ippool_->AddCandidatePeers(peers, false, false);
                LIVE_CONNECT_LEVEL connect_level = GetConnectLevel();
                InitPeerConnection(connect_level);
            }
            else
            {
                ippool_->AddCandidatePeers(peers, false, false);
            }
        }
    }

    void LiveP2PDownloader::Pause()
    {
        is_p2p_pausing_ = true;
    }

    void LiveP2PDownloader::Resume()
    {
        is_p2p_pausing_ = false;
    }

    void LiveP2PDownloader::SetSpeedLimitInKBps(boost::int32_t speed_limit_in_KBps)
    {

    }

    void LiveP2PDownloader::Stop()
    {
        is_running_ = false;

        p2p_subpiece_speed_info_.Stop();
        p2p_speed_info_.Stop();
        udp_server_speed_info_.Stop();
        udp_server_subpiece_speed_info_.Stop();

        if (ippool_)
        {
            ippool_->Stop();
            ippool_.reset();
        }

        if (udpserver_pool_)
        {
            udpserver_pool_->Stop();
            udpserver_pool_.reset();
        }

        if (exchanger_)
        {
            exchanger_->Stop();
            exchanger_.reset();
        }

        if (connector_)
        {
            connector_->Stop();
            connector_.reset();
        }

        if (udpserver_connector_)
        {
            udpserver_connector_->Stop();
            udpserver_connector_.reset();
        }

        live_connection_manager_.Stop();
    }

    bool LiveP2PDownloader::IsPausing()
    {
        return is_p2p_pausing_;
    }

    statistic::SPEED_INFO_EX LiveP2PDownloader::GetSpeedInfoEx()
    {
        return p2p_speed_info_.GetSpeedInfoEx();
    }

    statistic::SPEED_INFO_EX LiveP2PDownloader::GetSubPieceSpeedInfoEx()
    {
        return p2p_subpiece_speed_info_.GetSpeedInfoEx();
    }

    statistic::SPEED_INFO_EX LiveP2PDownloader::GetUdpServerSpeedInfoEx()
    {
        return udp_server_speed_info_.GetSpeedInfoEx();
    }

    statistic::SPEED_INFO_EX LiveP2PDownloader::GetUdpServerSubpieceSpeedInfoEx()
    {
        return udp_server_subpiece_speed_info_.GetSpeedInfoEx();
    }

    boost::uint32_t LiveP2PDownloader::GetCurrentDownloadSpeed() 
    {
        if (false == is_running_)
            return 0;
        
        return GetSpeedInfoEx().NowDownloadSpeed;
    }

    boost::uint32_t LiveP2PDownloader::GetSecondDownloadSpeed() 
    {
        if (false == is_running_)
            return 0;
        return GetSpeedInfoEx().SecondDownloadSpeed;
    }

    uint32_t LiveP2PDownloader::GetMinuteDownloadSpeed()
    {
        if (false == is_running_)
            return 0;
        return GetSpeedInfoEx().MinuteDownloadSpeed;
    }

    uint32_t LiveP2PDownloader::GetRecentDownloadSpeed() 
    {
        if (false == is_running_)
            return 0;
        return GetSpeedInfoEx().RecentDownloadSpeed;
    }

    // LiveDownloader
    void LiveP2PDownloader::OnBlockTimeout(boost::uint32_t block_id)
    {
        live_assigner_.OnBlockTimeout(block_id);
    }

    void LiveP2PDownloader::PutBlockTask(const protocol::LiveSubPieceInfo & live_block)
    {
        live_assigner_.PutBlockTask(live_block);
    }

    void LiveP2PDownloader::SetDownloadMode(P2PDwonloadMode mode)
    {

    }

    void LiveP2PDownloader::SetMaxConnectCount(boost::int32_t max_connect_count)
    {

    }

    uint32_t LiveP2PDownloader::GetFullBlockPeersCount()
    {
        return 0;
    }

    uint32_t LiveP2PDownloader::GetActivePeersCount()
    {
        return 0;
    }
    uint32_t LiveP2PDownloader::GetAvailableBlockPeerCount()
    {
        return 0;
    }

    RID LiveP2PDownloader::GetRid()
    {
        return rid_;
    }

    void LiveP2PDownloader::GetCandidatePeerInfos(std::vector<protocol::CandidatePeerInfo> &candidate_peers)
    {
        if (false == is_running_)
        {
            return;
        }

        assert(candidate_peers.size() == 0);

        std::set<protocol::CandidatePeerInfo> selected_peers;
        GetCandidatePeerInfosBasedOnUploadAbility(selected_peers);
        GetCandidatePeerInfosBasedOnUploadSpeed(selected_peers);

        for (std::set<protocol::CandidatePeerInfo>::const_iterator iter = selected_peers.begin();
            iter != selected_peers.end(); ++iter)
        {
            candidate_peers.push_back(*iter);
        }
    }

    uint32_t LiveP2PDownloader::GetMaxConnectCount()
    {
        return p2p_max_connect_count_;
    }

    void LiveP2PDownloader::InitPeerConnection(LIVE_CONNECT_LEVEL connect_level)
    {
        if (!is_running_)
        {
            return;
        }

        if (!connector_ && !udpserver_connector_)
        {
            return;
        }

        boost::int32_t connect_count = 0;
        if (connect_level <= LOW)
        {   
            connect_count = (p2p_max_connect_count_ * 3 / 4 - GetConnectedPeersCount()) - connector_->GetConnectingPeerCount();
        }
        else if (connect_level <= MEDIUM)
        {
            connect_count = (p2p_max_connect_count_ - GetConnectedPeersCount()) * 2 - connector_->GetConnectingPeerCount();
        }
        else
        {
            // high mode
            assert(connect_level == HIGH);
            connect_count = (p2p_max_connect_count_ - GetConnectedPeersCount()) * 2 - connector_->GetConnectingPeerCount();
            LIMIT_MIN(connect_count, 1);
        }

        if (connect_count <= 0 && !should_connect_udpserver_ && !should_use_udpserver_)
        {
            // no need to connect new peers. skip.
            return;
        }

        for (boost::int32_t i = 0; i < connect_count; ++i)
        {
            protocol::CandidatePeerInfo candidate_peer_info;
            if (false == ippool_->GetForConnect(candidate_peer_info)) 
            {   
                break;
            }

            connector_->Connect(candidate_peer_info);

            ++total_connect_peers_count_;
        }

        if (should_connect_udpserver_ || should_use_udpserver_)
        {
            for (boost::uint32_t i = live_connection_manager_.GetConnectedUdpServerCount();
                i < use_udpserver_count_ && i < udpserver_pool_->GetPeerCount(); ++i)
            {
                protocol::CandidatePeerInfo candidate_peer_info;

                if (false == udpserver_pool_->GetForConnect(candidate_peer_info, true))
                {
                    break;
                }

                udpserver_connector_->Connect(candidate_peer_info);
            }
        }
    }

    void LiveP2PDownloader::KickPeerConnection(LIVE_CONNECT_LEVEL connect_level)
    {   
        std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p> no_response_peer_set;
        live_connection_manager_.GetNoResponsePeers(no_response_peer_set);

        for (std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::iterator 
            iter = no_response_peer_set.begin(); iter != no_response_peer_set.end(); ++iter)
        {
            // 普通的tracker也会返回UdpServer，所以ippool_中可能有UdpServer
            ippool_->OnDisConnect(iter->first, true);

            if (iter->second->GetConnectType() == protocol::CONNECT_LIVE_UDPSERVER)
            {
                udpserver_pool_->OnDisConnect(iter->first, true);
            }

            DelPeer(iter->first);
        }

        std::multimap<KickLiveConnectionIndicator, LivePeerConnection::p> peer_kick_map;

        if (connect_level >= MEDIUM)
        {
            live_connection_manager_.GetKickMap(peer_kick_map);
        }

        boost::int32_t kick_count = 0;
        if (connect_level <= LOW)
        {
            // at most kick one
            kick_count = 1;            
        }
        else if (connect_level <= MEDIUM)
        {
            // make sure to kick at least one "bad" connection
            kick_count = GetConnectedPeersCount() + 1 - p2p_max_connect_count_;
        }
        else
        {
            assert(connect_level == HIGH);

            // kick peers more aggressively in case of urgency
            kick_count = GetConnectedPeersCount() - p2p_max_connect_count_ * 3 / 4;
        }        

        if (kick_count > 0)
        {
            std::multimap<KickLiveConnectionIndicator, LivePeerConnection::p>::iterator iter = peer_kick_map.begin();
            for (boost::int32_t i = 0; i < kick_count; i++)
            {
                if (iter == peer_kick_map.end() ||
                    !iter->first.ShouldKick())
                {   
                    break;
                }

                if (iter->second->GetConnectType() == protocol::CONNECT_LIVE_UDPSERVER)
                {
                    udpserver_pool_->OnDisConnect(iter->second->GetEndpoint(), true);
                }

                ippool_->OnDisConnect(iter->second->GetEndpoint(), true);

                DelPeer(iter->second->GetEndpoint());
                iter++;
            }
        }
    }

    void LiveP2PDownloader::OnUdpRecv(protocol::Packet const & packet)
    {
        // 统计
        OnUdpPacketStatistic(packet);

        if (packet.PacketAction == protocol::ConnectPacket::Action)
        {
            // Connect 回包
            const protocol::ConnectPacket & connect_packet = (const protocol::ConnectPacket &)packet;

            // 考虑到普通的tracker也会返回UdpServer，所以即便现在是UdpServer的回包，也可能是connector去连的
            connector_->OnReConectPacket(connect_packet);

            if (connect_packet.connect_type_ == protocol::CONNECT_LIVE_UDPSERVER)
            {
                udpserver_connector_->OnReConectPacket(connect_packet);
            }

            return;
        }
        else if (packet.PacketAction == protocol::PeerExchangePacket::Action)
        {
            exchanger_->OnPeerExchangePacket((const protocol::PeerExchangePacket&)packet);
        }
        else if (packet.PacketAction == protocol::LiveRequestAnnouncePacket::Action)
        {
            // 收到RequestAnounce包 (0xC0)
        }
        else if (packet.PacketAction == protocol::LiveAnnouncePacket::Action)
        {
            // 收到annouce回包 (0xC1)
            live_connection_manager_.OnAnnouncePacket((protocol::LiveAnnouncePacket const &)packet);
            if (live_connection_manager_.IsFromUdpServer(packet.end_point))
            {
                ++total_announce_response_from_udpserver_this_time_;
            }
        }
        else if (packet.PacketAction == protocol::LiveSubPiecePacket::Action)
        {
            // 收到数据包
            const protocol::LiveSubPiecePacket & subpiece_packet = (const protocol::LiveSubPiecePacket &)packet;

            live_subpiece_request_manager_.OnSubPiece(subpiece_packet);

            if (false == HasSubPiece(subpiece_packet.sub_piece_info_))
            {
                ++total_received_subpiece_count_;

                protocol::LiveSubPieceBuffer buffer(subpiece_packet.sub_piece_content_,
                    subpiece_packet.sub_piece_length_);

                live_stream_->GetInstance()->AddSubPiece(subpiece_packet.sub_piece_info_, buffer);

                if (live_connection_manager_.IsFromUdpServer(packet.end_point))
                {
                    subpieces_responsed_from_udpserver_.insert(subpiece_packet.sub_piece_info_);
                }
            }

            ++total_unused_subpiece_count_;
        }
        else if (packet.PacketAction == protocol::ErrorPacket::Action)
        {
            // 收到Error报文
            live_connection_manager_.OnErrorPacket((const protocol::ErrorPacket &)packet);
            if (!HasPeer(packet.end_point))
            {
                // 还没连上，说明对方没有该资源
                ippool_->OnConnectFailed(packet.end_point);
                udpserver_pool_->OnConnectFailed(packet.end_point);
            }
        }
        else if (packet.PacketAction == protocol::PeerInfoPacket::Action)
        {
            live_connection_manager_.OnPeerInfoPacket((const protocol::PeerInfoPacket &)packet);
        }
        else if (packet.PacketAction == protocol::CloseSessionPacket::Action)
        {
            if (live_connection_manager_.HasPeer(packet.end_point))
            {
                if (live_connection_manager_.IsLiveUdpServer(packet.end_point))
                {
                    udpserver_pool_->OnDisConnect(packet.end_point, false);
                }
                ippool_->OnDisConnect(packet.end_point, false);
                DelPeer(packet.end_point);
            }
        }
    }

    void LiveP2PDownloader::OnUdpPacketStatistic(protocol::Packet const & packet)
    {
        if (live_connection_manager_.IsLivePeer(packet.end_point))
        {
            p2p_speed_info_.SubmitDownloadedBytes(packet.length());

            if (packet.PacketAction != protocol::LiveSubPiecePacket::Action)
            {
                return;
            }

            const protocol::LiveSubPiecePacket & subpiece_packet = (const protocol::LiveSubPiecePacket &)packet;
            p2p_subpiece_speed_info_.SubmitDownloadedBytes(LIVE_SUB_PIECE_SIZE);

            if (HasSubPiece(subpiece_packet.sub_piece_info_))
            {
                return;
            }

            total_p2p_data_bytes_ += subpiece_packet.sub_piece_length_;
        }
        else if (live_connection_manager_.IsLiveUdpServer(packet.end_point))
        {
            udp_server_speed_info_.SubmitDownloadedBytes(packet.length());

            if (packet.PacketAction != protocol::LiveSubPiecePacket::Action)
            {
                return;
            }

            udp_server_subpiece_speed_info_.SubmitDownloadedBytes(LIVE_SUB_PIECE_SIZE);

            const protocol::LiveSubPiecePacket & subpiece_packet = (const protocol::LiveSubPiecePacket &)packet;

            if (HasSubPiece(subpiece_packet.sub_piece_info_))
            {
                return;
            }

            total_udpserver_data_bytes_ += subpiece_packet.sub_piece_length_;
        }
    }

    void LiveP2PDownloader::AddPeer(LivePeerConnection__p peer_connection)
    {
        live_connection_manager_.AddPeer(peer_connection);
    }

    void LiveP2PDownloader::DelPeer(const boost::asio::ip::udp::endpoint & endpoint)
    {
        LivePeerConnection__p removed_peer = live_connection_manager_.DelPeer(endpoint);
        if (removed_peer && removed_peer->GetConnectType() == protocol::CONNECT_LIVE_UDPSERVER)
        {
            live_stream_->GetDownloadDriver()->UpdateUdpServerServiceScore(endpoint, removed_peer->GetServiceScore());
        }
    }

    void LiveP2PDownloader::OnBlockComplete(const protocol::LiveSubPieceInfo & live_block)
    {
        live_stream_->RemoveBlockTask(live_block);
    }

    bool LiveP2PDownloader::HasSubPiece(const protocol::LiveSubPieceInfo & sub_piece)
    {
        return live_stream_->GetInstance()->HasSubPiece(sub_piece);
    }

    void LiveP2PDownloader::SetBlockCountMap(boost::uint32_t block_id, std::vector<boost::uint16_t> subpiece_count)
    {
        live_subpiece_count_manager_.SetSubPieceCountMap(block_id, subpiece_count);
    }

    bool LiveP2PDownloader::HasSubPieceCount(boost::uint32_t block_id)
    {
        return live_subpiece_count_manager_.HasSubPieceCount(block_id);
    }

    boost::uint16_t LiveP2PDownloader::GetSubPieceCount(boost::uint32_t block_id)
    {
        return live_subpiece_count_manager_.GetSubPieceCount(block_id);
    }


    bool LiveP2PDownloader::IsRequesting(const protocol::LiveSubPieceInfo & subpiece_info)
    {
        return live_subpiece_request_manager_.IsRequesting(subpiece_info);
    }

    uint32_t LiveP2PDownloader::GetRequestingCount(const protocol::LiveSubPieceInfo & subpiece_info)
    {
        return live_subpiece_request_manager_.GetRequestingCount(subpiece_info);
    }

    void LiveP2PDownloader::AddRequestingSubpiece(const protocol::LiveSubPieceInfo & subpiece_info,
        boost::uint32_t timeout, LivePeerConnection__p peer_connection, uint32_t transaction_id)
    {
        live_subpiece_request_manager_.Add(subpiece_info, timeout, peer_connection, transaction_id);
    }

    void LiveP2PDownloader::DoList()
    {
        if (GetConnectedPeersCount() < GetMaxConnectCount() &&
            last_dolist_time_.elapsed() >= dolist_time_interval_)
        {
            // DoList 的间隔按照指数增长，最多128秒
            dolist_time_interval_ *= 2;
            if (dolist_time_interval_ > 128000)
            {
                dolist_time_interval_ = 128000;
            }

            last_dolist_time_.reset();
            p2sp::TrackerModule::Inst()->DoList(GetRid(), false, false);
        }

        if (dolist_udpserver_times_ < 5 && last_dolist_udpserver_time_.elapsed() > dolist_udpserver_time_interval_)
        {
            ++dolist_udpserver_times_;
            dolist_udpserver_time_interval_ *= 2;
            last_dolist_udpserver_time_.reset();
            p2sp::TrackerModule::Inst()->DoList(GetRid(), false, true);
        }
    }

    boost::uint32_t LiveP2PDownloader::GetRestTimeInSeconds() const
    {
        return live_stream_->GetRestPlayableTimeInSecond();
    }

    void LiveP2PDownloader::CheckShouldUseUdpServer()
    {
        if (GetRestTimeInSeconds() < urgent_rest_playable_time_delim_ + 2 && !IsInUdpServerProtectTimeWhenStart())
        {
            should_connect_udpserver_ = true;
        }
        else
        {
            should_connect_udpserver_ = false;
        }

        if (is_p2p_pausing_)
        {
            should_use_udpserver_ = false;
            return;
        }

        // 剩余时间小，使用UdpServer来补带宽
        if (GetRestTimeInSeconds() < urgent_rest_playable_time_delim_ && !IsInUdpServerProtectTimeWhenStart())
        {
            use_udpserver_reason_ = URGENT;
            should_use_udpserver_ = true;
            should_connect_udpserver_ = true;
            return;
        }

        // 上传足够大，剩余时间足够大，并且跟视野中的peer相比，跑的足够靠前，使用UdpServer来快速分发
        if (live_stream_->IsUploadSpeedLargeEnough() &&
            live_stream_->GetRestPlayableTimeInSecond() > BootStrapGeneralConfig::Inst()->GetRestPlayTimeDelim(live_stream_->IsSavingMode()) &&
            IsAheadOfMostPeers())
        {
            use_udpserver_reason_ = LARGE_UPLOAD;
            should_use_udpserver_ = true;
            should_connect_udpserver_ = true;
            return;
        }

        // 如果是因为上传大而使用UdpServer的，若上传变小并且也使用UdpServer使用了一段时间了，则暂停使用UdpServer
        if (use_udpserver_reason_ == LARGE_UPLOAD &&
            live_stream_->IsUploadSpeedSmallEnough() &&
            use_udpserver_tick_counter_.elapsed() > using_udpserver_time_at_least_when_large_upload_ * 1000)
        {
            should_use_udpserver_ = false;
            return;
        }

        // 如果是因为紧急而使用的UdpServer，若剩余时间足够大了，则暂停使用UdpServer
        if (use_udpserver_reason_ == URGENT &&
            GetRestTimeInSeconds() > safe_enough_rest_playable_time_delim_)
        {
            should_use_udpserver_ = false;
            return;
        }

        // 如果是因为紧急而使用的UdpServer，若跑了很长时间剩余时间还可以，则暂停使用UdpServer
        if (use_udpserver_reason_ == URGENT &&
            use_udpserver_tick_counter_.elapsed() > using_udpserver_time_in_second_delim_ * 1000 &&
            GetRestTimeInSeconds() > safe_rest_playable_time_delim_)
        {
            should_use_udpserver_ = false;
            return;
        }
    }

    void LiveP2PDownloader::OnConnectTimeout(const boost::asio::ip::udp::endpoint& end_point)
    {
        //这个handler只处理udpserver的connect timeout
        live_stream_->GetDownloadDriver()->UpdateUdpServerServiceScore(end_point, -2);
    }

    void LiveP2PDownloader::DeleteAllUdpServer()
    {
        udpserver_pool_->DisConnectAll();

        std::set<boost::asio::ip::udp::endpoint> udp_server_endpoint_set;

        live_connection_manager_.GetUdpServerEndpoints(udp_server_endpoint_set);

        for (std::set<boost::asio::ip::udp::endpoint>::iterator iter = udp_server_endpoint_set.begin();
            iter != udp_server_endpoint_set.end(); ++iter)
        {
            const boost::asio::ip::udp::endpoint& udpserver_endpoint = *iter;
            // 有可能把UdpServer当做普通的peer来连了，所以需要在ippool_中去尝试disconnect
            ippool_->OnDisConnect(udpserver_endpoint, true);
            DelPeer(udpserver_endpoint);
        }
    }

    bool LiveP2PDownloader::IsAheadOfMostPeers() const
    {
        return live_connection_manager_.IsAheadOfMostPeers();
    }

    void LiveP2PDownloader::SubmitUdpServerDownloadBytes(boost::uint32_t bytes)
    {
        switch (use_udpserver_reason_)
        {
        case URGENT:
            download_bytes_use_udpserver_because_of_urgent_ += bytes;
            break;
        case LARGE_UPLOAD:
            download_bytes_use_udpserver_because_of_large_upload_ += bytes;
            break;
        case NO_USE_UDPSERVER:
            break;
        default:
            break;
        }
    }

    boost::uint32_t LiveP2PDownloader::GetTimesOfUseUdpServerBecauseOfUrgent() const
    {
        return times_of_use_udpserver_because_of_urgent_;
    }

    boost::uint32_t LiveP2PDownloader::GetTimesOfUseUdpServerBecauseOfLargeUpload() const
    {
        return times_of_use_udpserver_because_of_large_upload_;
    }

    boost::uint32_t LiveP2PDownloader::GetTimeElapsedUseUdpServerBecauseOfUrgent() const
    {
        return time_elapsed_use_udpserver_because_of_urgent_;
    }

    boost::uint32_t LiveP2PDownloader::GetTimeElapsedUseUdpServerBecauseOfLargeUpload() const
    {
        return time_elapsed_use_udpserver_because_of_large_upload_;
    }

    boost::uint32_t LiveP2PDownloader::GetDownloadBytesUseUdpServerBecauseOfUrgent() const
    {
        return download_bytes_use_udpserver_because_of_urgent_;
    }

    boost::uint32_t LiveP2PDownloader::GetDownloadBytesUseUdpServerBecauseOfLargeUpload() const
    {
        return download_bytes_use_udpserver_because_of_large_upload_;
    }

    void LiveP2PDownloader::SendPeerInfo()
    {
        protocol::PeerInfo peer_info(GetConnectedPeersCount(),
            statistic::UploadStatisticModule::Inst()->GetUploadCount(),
            statistic::UploadStatisticModule::Inst()->GetUploadSpeed(),
            UploadModule::Inst()->GetMaxUploadSpeedIncludeSameSubnet(),
            GetRestTimeInSeconds(),
            GetLostRate(),
            GetRedundancyRate());

        protocol::PeerInfoPacket peer_info_packet(protocol::Packet::NewTransactionID(), protocol::PEER_VERSION, peer_info);

        for (std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::const_iterator 
            iter = live_connection_manager_.GetPeers().begin();
            iter != live_connection_manager_.GetPeers().end(); ++iter)
        {
            peer_info_packet.end_point = iter->first;
            DoSendPacket(peer_info_packet);
        }
    }

    boost::uint8_t LiveP2PDownloader::GetLostRate() const
    {
        boost::uint32_t total_request = total_request_subpiece_count_;
        boost::uint32_t total_receive = GetTotalUnusedSubPieceCount();
        boost::uint32_t requesting_count = live_connection_manager_.GetRequestingCount();

        if (total_request_subpiece_count_ <= requesting_count + total_receive)
        {
            return 0;
        }

        return (total_request - requesting_count - total_receive) * 100
            / (total_request - requesting_count);
    }

    boost::uint8_t LiveP2PDownloader::GetRedundancyRate() const
    {
        boost::uint32_t total_receive = GetTotalUnusedSubPieceCount();
        boost::uint32_t unique_receive = GetTotalRecievedSubPieceCount();

        if (total_receive == 0)
        {
            return 0;
        }

        return (total_receive - unique_receive) * 100 / total_receive;
    }

    void LiveP2PDownloader::CalcTimeOfUsingUdpServerWhenStop()
    {
        if (use_udpserver_reason_ == URGENT)
        {
            time_elapsed_use_udpserver_because_of_urgent_ += use_udpserver_tick_counter_.elapsed();
        }
        else if (use_udpserver_reason_ == LARGE_UPLOAD)
        {
            time_elapsed_use_udpserver_because_of_large_upload_ += use_udpserver_tick_counter_.elapsed();
        }
    }

    bool LiveP2PDownloader::IsInUdpServerProtectTimeWhenStart()
    {
        // 在保护时间之内 并且bs开关为开 并且bwtype为0
        return (live_stream_->GetDownloadTime() < udpserver_protect_time_when_start_ &&
            (live_stream_->GetBWType() == JBW_NORMAL || live_stream_->IsSavingMode()) &&
            should_use_bw_type_ &&
            live_stream_->GetSourceType() == PlayInfo::SOURCE_PPLIVE_LIVE2 &&
            live_stream_->GetReplay() == false);
    }

    boost::uint32_t LiveP2PDownloader::GetTotalConnectPeersCount() const
    {
        return total_connect_peers_count_;
    }

    void LiveP2PDownloader::GetCandidatePeerInfosBasedOnUploadAbility(std::set<protocol::CandidatePeerInfo> & selected_peers)
    {
        live_connection_manager_.GetCandidatePeerInfosBasedOnUploadAbility(selected_peers);
    }

    void LiveP2PDownloader::GetCandidatePeerInfosBasedOnUploadSpeed(std::set<protocol::CandidatePeerInfo> & selected_peers)
    {
        live_connection_manager_.GetCandidatePeerInfosBasedOnUploadSpeed(selected_peers);
    }

    boost::uint32_t LiveP2PDownloader::GetReverseOrderSubPiecePacketCount() const
    {
        return live_connection_manager_.GetReverseOrderSubPiecePacketCount();
    }

    boost::uint32_t LiveP2PDownloader::GetTotalReceivedSubPiecePacketCount() const
    {
        return live_connection_manager_.GetTotalReceivedSubPiecePacketCount();
    }

    boost::uint32_t LiveP2PDownloader::GetMinFirstBlockID() const
    {
        return live_connection_manager_.GetMinFirstBlockID();
    }

    void LiveP2PDownloader::UpdateUsingUdpServerStatus()
    {
        if (!should_use_udpserver_ || (should_use_udpserver_ && use_udpserver_reason_ != URGENT))
        {
            return;
        }

        total_connect_udpserver_count_this_time_ += live_connection_manager_.GetConnectedUdpServerCount();
    }

    void LiveP2PDownloader::WillUseUdpServerBecauseOfUrgent()
    {
        ++times_of_use_udpserver_because_of_urgent_;

        udpserver_count_when_needed_.Update(udpserver_pool_->GetPeerCount());

        total_connect_udpserver_count_this_time_ = 0;
        total_announce_response_from_udpserver_this_time_ = 0;

        live_connection_manager_.ClearSubPiecesRequestdToUdpServer();
        subpieces_responsed_from_udpserver_.clear();
    }

    void LiveP2PDownloader::WillUseUdpServerBecauseOfLargeUpload()
    {
        ++times_of_use_udpserver_because_of_large_upload_;
    }

    void LiveP2PDownloader::WillNotUseUdpServerBecauseOfNotUrgent()
    {
        time_elapsed_use_udpserver_because_of_urgent_ += use_udpserver_tick_counter_.elapsed();

        if (use_udpserver_tick_counter_.elapsed() < 1000)
        {
            return;
        }

        boost::uint32_t average_connect_udpserver_count = static_cast<boost::uint32_t>(total_connect_udpserver_count_this_time_ * 10 /
            static_cast<float>((use_udpserver_tick_counter_.elapsed() / 1000.0)));

        connect_udpserver_count_when_needed_.Update(average_connect_udpserver_count);

        boost::uint32_t average_announce_response_from_udpserver = static_cast<boost::uint32_t>(total_announce_response_from_udpserver_this_time_ * 10 /
            static_cast<float>((use_udpserver_tick_counter_.elapsed() / 1000.0)));

        announce_response_from_udpserver_.Update(average_announce_response_from_udpserver);

        if (subpieces_responsed_from_udpserver_.size() != 0)
        {
            boost::uint32_t ratio_of_response_to_request = live_connection_manager_.GetSubPieceRequestedToUdpServerCount() * 100 /
                subpieces_responsed_from_udpserver_.size();
            ratio_of_response_to_request_from_udpserver_.Update(ratio_of_response_to_request);
        }
    }

    void LiveP2PDownloader::WillNotUseUdpServerBecauseOfSmallUpload()
    {
        time_elapsed_use_udpserver_because_of_large_upload_ += use_udpserver_tick_counter_.elapsed();
    }

    const NumericRange<boost::uint32_t> & LiveP2PDownloader::GetUdpServerCountWhenNeeded() const
    {
        return udpserver_count_when_needed_;
    }

    const NumericRange<boost::uint32_t> & LiveP2PDownloader::GetConnectUdpServerCountWhenNeeded() const
    {
        return connect_udpserver_count_when_needed_;
    }

    const NumericRange<boost::uint32_t> & LiveP2PDownloader::GetAnnounceResponseFromUdpServer() const
    {
        return announce_response_from_udpserver_;
    }

    const NumericRange<boost::uint32_t> & LiveP2PDownloader::GetRatioOfResponseToRequestFromUdpserver() const
    {
        return ratio_of_response_to_request_from_udpserver_;
    }

    void LiveP2PDownloader::CalcDacDataBeforeStop()
    {
        if (should_use_udpserver_ && use_udpserver_reason_ == URGENT)
        {
            WillNotUseUdpServerBecauseOfNotUrgent();
        }
    }

    bool LiveP2PDownloader::RequestNextBlock()
    {
        return live_stream_->RequestNextBlock(shared_from_this());
    }

    boost::uint32_t LiveP2PDownloader::GetDataRate() const
    {
        return live_stream_->GetDataRateInBytes();
    }

    storage::LivePosition & LiveP2PDownloader::GetPlayingPosition() const
    {
        return live_stream_->GetPlayingPosition();
    }
}
