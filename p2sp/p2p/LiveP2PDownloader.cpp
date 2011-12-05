#include "Common.h"
#include "LiveP2PDownloader.h"
#include "p2sp/download/LiveDownloadDriver.h"
#include "p2sp/p2p/P2PModule.h"
#include "p2sp/tracker/TrackerModule.h"
#include "random.h"

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("live_p2p");
    LiveP2PDownloader::LiveP2PDownloader(const RID & rid, storage::LiveInstance__p live_instance)
        : rid_(rid)
        , live_instance_(live_instance)
        , is_running_(false)
        , is_p2p_pausing_(true)
        , p2p_max_connect_count_(20)
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
    {
        send_peer_info_packet_interval_in_second_ = BootStrapGeneralConfig::Inst()->GetSendPeerInfoPacketIntervalInSecond();
        urgent_rest_playable_time_delim_ = BootStrapGeneralConfig::Inst()->GetUrgentRestPlayableTimeDelim();
        safe_rest_playable_time_delim_ = BootStrapGeneralConfig::Inst()->GetSafeRestPlayableTimeDelim();
        safe_enough_rest_playable_time_delim_ = BootStrapGeneralConfig::Inst()->GetSafeEnoughRestPlayabelTimeDelim();
        using_udpserver_time_in_second_delim_ = BootStrapGeneralConfig::Inst()->GetUsingUdpServerTimeDelim();
        using_udpserver_time_at_least_when_large_upload_ = BootStrapGeneralConfig::Inst()->GetUsingCDNOrUdpServerTimeDelim();
        use_udpserver_count_ = BootStrapGeneralConfig::Inst()->GetUseUdpserverCount();
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
        exchanger_ = Exchanger::create(shared_from_this(), ippool_);
        exchanger_->Start();

        // PeerConnector
        connector_ = PeerConnector::create(shared_from_this(), ippool_);
        connector_->Start();

        // UdpServer Connector
        udpserver_connector_ = PeerConnector::create(shared_from_this(), udpserver_pool_);
        udpserver_connector_->Start();

        // Assigner
        live_assigner_.Start(shared_from_this());
        // SubpieceRequestManager
        live_subpiece_request_manager_.Start(shared_from_this());

        // SpeedInfo
        p2p_speed_info_.Start();
        p2p_subpiece_speed_info_.Start();
        udp_server_speed_info_.Start();
        udp_server_subpiece_speed_info_.Start();

        last_dolist_time_.start();
        last_dolist_udpserver_time_.start();

        send_peer_info_packet_interval_in_second_ = BootStrapGeneralConfig::Inst()->GetSendPeerInfoPacketIntervalInSecond();

        BootStrapGeneralConfig::Inst()->AddUpdateListener(shared_from_this());

        is_running_ = true;
    }

    LIVE_CONNECT_LEVEL LiveP2PDownloader::GetConnectLevel()
    {
        LIVE_CONNECT_LEVEL level = MEDIUM;
        int32_t rest_time = this->GetMinRestTimeInSeconds();

        if (rest_time >= 25 && !this->is_p2p_pausing_)
        {
            // enough time; don't need to rush for new connections
            // under http only mode, still try new connection
            level = LOW;
        }
        else if (rest_time >= 12)
        {   
            level = MEDIUM;
        }
        else
        {
            // urgent. Replace connection quickly
            level = HIGH;
        }        

        return level;
    }

    // 250ms 被调用一次
    void LiveP2PDownloader::OnP2PTimer(boost::uint32_t times)
    {
        // s List / Exchange 一次
        if (times % 4 == 0)
        {
            if (exchanger_)
            {
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

            LIVE_CONNECT_LEVEL connect_level = GetConnectLevel();
            KickPeerConnection(connect_level);
            InitPeerConnection(connect_level);
        }

        // 每秒
        // 删除 block_count_map 和 bitmap 中过期的数据
        if (times % 4 == 0)
        {
            // 删除block_count_map中过期的节点
            EliminateElapsedBlockCountMap(GetMinPlayingPosition().GetBlockId());

            // 删除PeerConnection中过期的节点
            for (std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::iterator peer_iter = peers_.begin();
                peer_iter != peers_.end(); ++peer_iter)
            {
                peer_iter->second->EliminateElapsedBlockBitMap(GetMinPlayingPosition().GetBlockId());
            }
        }

        if (!is_p2p_pausing_)
        {
            LOG(__DEBUG, "live_p2p", "block_count_map_.size() = " << block_count_map_.size());
            // 检查block是否完成
            CheckBlockComplete();

            if (times % 4 == 0)
            {
                live_subpiece_request_manager_.OnP2PTimer(times);
            }

            // 预分配
            live_assigner_.OnP2PTimer(times, GetConnectLevel() == HIGH, should_use_udpserver_);
        }
        
        for (std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::iterator iter = peers_.begin();
            iter != peers_.end(); ++iter)
        {
            iter->second->OnP2PTimer(times);
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
                        ++times_of_use_udpserver_because_of_urgent_;
                        break;
                    case LARGE_UPLOAD:
                        ++times_of_use_udpserver_because_of_large_upload_;
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
                        time_elapsed_use_udpserver_because_of_urgent_ += use_udpserver_tick_counter_.elapsed();
                        break;
                    case LARGE_UPLOAD:
                        time_elapsed_use_udpserver_because_of_large_upload_ += use_udpserver_tick_counter_.elapsed();
                        break;
                    default:
                        break;
                    }

                    use_udpserver_reason_ = NO_USE_UDPSERVER;
                }
            }

            if (should_use_udpserver_ == false)
            {
                assert(!download_driver_s_.empty());

                if (connected_udpserver_count_ > 0 &&
                    (*download_driver_s_.begin())->GetRestPlayableTime() > safe_enough_rest_playable_time_delim_)
                {
                    udpserver_pool_->DisConnectAll();

                    DeleteAllUdpServer();
                }
            }
        }

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

    // 查询到的节点加入IPPool
    void LiveP2PDownloader::AddCandidatePeers(std::vector<protocol::CandidatePeerInfo> peers, bool is_live_udpserver)
    {
        if (is_live_udpserver)
        {
            udpserver_pool_->AddCandidatePeers(peers);
        }
        else
        {
            if (ippool_->GetPeerCount() == 0)
            {
                ippool_->AddCandidatePeers(peers);
                LIVE_CONNECT_LEVEL connect_level = GetConnectLevel();
                InitPeerConnection(connect_level);
            }
            else
            {
                ippool_->AddCandidatePeers(peers);
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

        for (std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::iterator iter = peers_.begin();
            iter != peers_.end(); ++iter)
        {
            iter->second->Stop();
        }

        peers_.clear();

        if (live_instance_)
        {
            live_instance_.reset();
        }

        if (use_udpserver_reason_ == URGENT)
        {
            time_elapsed_use_udpserver_because_of_urgent_ += use_udpserver_tick_counter_.elapsed();
        }
        else if (use_udpserver_reason_ == LARGE_UPLOAD)
        {
            time_elapsed_use_udpserver_because_of_large_upload_ += use_udpserver_tick_counter_.elapsed();
        }

        BootStrapGeneralConfig::Inst()->RemoveUpdateListener(shared_from_this());
    }

    bool LiveP2PDownloader::IsPausing()
    {
        return is_p2p_pausing_;
    }

    boost::int32_t  LiveP2PDownloader::GetBlockTaskNum()
    {
        return block_tasks_.size();
    }

    bool LiveP2PDownloader::HasBlockTask() const
    {
        return true;
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
        block_tasks_.erase(block_id);
    }

    void LiveP2PDownloader::PutBlockTask(const protocol::LiveSubPieceInfo & live_block)
    {
        LOG(__DEBUG, "live_p2p", __FUNCTION__ << " " << __LINE__  << " " << live_block);
        
        uint32_t block_id = live_block.GetBlockId();
        if (block_tasks_.find(block_id) == block_tasks_.end())
        {
            block_tasks_.insert(std::make_pair(block_id, live_block));
        }
    }

    // IP2PControlTarget
    void LiveP2PDownloader::SetAssignPeerCountLimit(uint32_t assign_peer_count_limit)
    {

    }

    void LiveP2PDownloader::NoticeHttpBad(bool is_http_bad)
    {

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
    uint32_t LiveP2PDownloader::GetFullBlockActivePeersCount()
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
        uint32_t peer_left = peers_.size();
        uint32_t candidates_left = P2SPConfigs::P2P_MAX_EXCHANGE_PEER_COUNT;
        std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::iterator iter;
        for (iter = peers_.begin(); iter != peers_.end();iter++)
        {
            Random random;
            if ((uint32_t)random.Next(peer_left) < candidates_left)
            {
                candidate_peers.push_back(iter->second->GetCandidatePeerInfo());
                candidates_left--;
            }
            peer_left--;
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
            connect_count = (p2p_max_connect_count_ * 3 / 4 - peers_.size()) - connector_->GetConnectingPeerCount();
        }
        else if (connect_level <= MEDIUM)
        {
            connect_count = (p2p_max_connect_count_ - peers_.size()) * 2 - connector_->GetConnectingPeerCount();
        }
        else
        {
            // high mode
            assert(connect_level == HIGH);
            connect_count = (p2p_max_connect_count_ - peers_.size()) * 2 - connector_->GetConnectingPeerCount();
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
        }

        if (should_connect_udpserver_ || should_use_udpserver_)
        {
            for (boost::uint32_t i = connected_udpserver_count_; i < use_udpserver_count_ && i < udpserver_pool_->GetPeerCount(); ++i)
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
        std::multimap<KickLiveConnectionIndicator, LivePeerConnection::p> peer_kick_map;

        for (std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::iterator 
            iter = peers_.begin(); iter != peers_.end(); )
        {
            if (iter->second->LongTimeNoResponse())
            {
                // 普通的tracker也会返回UdpServer，所以ippool_中可能有UdpServer
                ippool_->OnDisConnect(iter->first, true);

                if (iter->second->GetConnectType() == protocol::CONNECT_LIVE_UDPSERVER)
                {
                    udpserver_pool_->OnDisConnect(iter->first, true);
                }

                DelPeer((iter++)->second);
            }
            else
            {
                if (connect_level >= MEDIUM)
                {
                    if (iter->second->GetConnectedTimeInMillseconds() >= 15000)
                    {
                        // only kick peers which are connected longer than 15 seconds
                        peer_kick_map.insert(std::make_pair(KickLiveConnectionIndicator(iter->second), iter->second));
                    }
                }

                ++iter;
            }
        }

        if (connect_level <= LOW)
        {
            // no more kicking
            return;
        }

        boost::int32_t kick_count = 0;
        if (connect_level <= MEDIUM)
        {
            // make sure to kick at least one "bad" connection
            kick_count = peers_.size() + 1 - p2p_max_connect_count_;
        }
        else
        {
            assert(connect_level == HIGH);

            // kick peers more aggressively in case of urgency
            kick_count = peers_.size() - p2p_max_connect_count_ * 3 / 4;
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

                DelPeer(iter->second);
                iter++;
            }
        }
    }

    void LiveP2PDownloader::AttachDownloadDriver(LiveDownloadDriver__p download_driver)
    {
        if (download_driver_s_.find(download_driver) == download_driver_s_.end())
        {
            download_driver_s_.insert(download_driver);
        }
    }

    void LiveP2PDownloader::DetachDownloadDriver(LiveDownloadDriver__p download_driver)
    {
        if (download_driver_s_.find(download_driver) != download_driver_s_.end())
        {
            download_driver_s_.erase(download_driver);

            if (download_driver_s_.size() == 0)
            {
                Stop();
                P2PModule::Inst()->OnLiveP2PDownloaderStop(shared_from_this());
            }
        }
    }

    void LiveP2PDownloader::OnUdpRecv(protocol::Packet const & packet)
    {
        // 统计速度时不只包括SubPiecePacket，而是包括收到的所有的包
        boost::uint8_t connect_type = protocol::CONNECT_LIVE_PEER;
        if (peers_.find(packet.end_point) != peers_.end())
        {
            if (peers_[packet.end_point]->GetConnectType() == protocol::CONNECT_LIVE_PEER)
            {
                p2p_speed_info_.SubmitDownloadedBytes(packet.length());
            }
            else
            {
                assert(peers_[packet.end_point]->GetConnectType() == protocol::CONNECT_LIVE_UDPSERVER);
                connect_type = protocol::CONNECT_LIVE_UDPSERVER;
                udp_server_speed_info_.SubmitDownloadedBytes(packet.length());
            }
        }

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
            if (peers_.find(packet.end_point) != peers_.end())
            {
                LivePeerConnection::p peer_connection = peers_.find(packet.end_point)->second;
                peer_connection->OnAnnounce((protocol::LiveAnnouncePacket const &)packet);
            }
            return;
        }
        else if (packet.PacketAction == protocol::LiveSubPiecePacket::Action)
        {
            // 收到数据包
            if (connect_type == protocol::CONNECT_LIVE_PEER)
            {
                p2p_subpiece_speed_info_.SubmitDownloadedBytes(LIVE_SUB_PIECE_SIZE);
            }
            else
            {
                assert(connect_type == protocol::CONNECT_LIVE_UDPSERVER);
                udp_server_subpiece_speed_info_.SubmitDownloadedBytes(LIVE_SUB_PIECE_SIZE);
            }
            live_subpiece_request_manager_.OnSubPiece((const protocol::LiveSubPiecePacket &)packet);
        }
        else if (packet.PacketAction == protocol::ErrorPacket::Action)
        {
            // 收到Error报文
            if (peers_.find(packet.end_point) != peers_.end())
            {
                protocol::ErrorPacket error_packet = (const protocol::ErrorPacket &) packet;
                if (error_packet.error_code_ == protocol::ErrorPacket::PPV_ANNOUCE_NO_RESOURCEID ||
                    error_packet.error_code_ == protocol::ErrorPacket::PPV_SUBPIECE_NO_RESOURCEID)
                {
                    // 正在Announce 或则 请求Subpiece
                    // 对方没有该资源 或者 我被T了
                    LivePeerConnection::p peer_connection = peers_.find(packet.end_point)->second;
                    DelPeer(peer_connection);
                }
            }
            else
            {
                // 还没连上，说明对方没有该资源
                ippool_->OnConnectFailed(packet.end_point);
                udpserver_pool_->OnConnectFailed(packet.end_point);
            }
        }
        else if (packet.PacketAction == protocol::PeerInfoPacket::Action)
        {
            if (peers_.find(packet.end_point) != peers_.end())
            {
                const protocol::PeerInfoPacket peer_info_packet = (const protocol::PeerInfoPacket &)packet;

                statistic::PEER_INFO peer_info(peer_info_packet.peer_info_.download_connected_count_, peer_info_packet.peer_info_.upload_connected_count_,
                    peer_info_packet.peer_info_.upload_speed_, peer_info_packet.peer_info_.max_upload_speed_, peer_info_packet.peer_info_.rest_playable_time_,
                    peer_info_packet.peer_info_.lost_rate_, peer_info_packet.peer_info_.redundancy_rate_);

                peers_[packet.end_point]->UpdatePeerInfo(peer_info);
            }
        }
        else if (packet.PacketAction == protocol::CloseSessionPacket::Action)
        {
            for (std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::iterator iter = peers_.begin();
                iter != peers_.end(); ++iter)
            {
                if (iter->first == packet.end_point)
                {
                    if (iter->second->GetConnectType() == protocol::CONNECT_LIVE_UDPSERVER)
                    {
                        udpserver_pool_->OnDisConnect(iter->second->GetEndpoint(), false);
                    }

                    ippool_->OnDisConnect(iter->second->GetEndpoint(), false);

                    DelPeer(iter->second);
                    break;
                }
            }
        }
    }

    void LiveP2PDownloader::AddPeer(LivePeerConnection__p peer_connection)
    {
        if (!is_running_)
        {
            return;
        }

        if (peers_.find(peer_connection->GetEndpoint()) == peers_.end())
        {
            peers_[peer_connection->GetEndpoint()] = peer_connection;
            if (peer_connection->GetConnectType() == protocol::CONNECT_LIVE_UDPSERVER)
            {
                ++connected_udpserver_count_;
            }
        }
    }

    void LiveP2PDownloader::DelPeer(LivePeerConnection__p peer_connection)
    {
        if (!is_running_ || !peer_connection)
        {
            return;
        }

        if (peers_.find(peer_connection->GetEndpoint()) != peers_.end())
        {
            peer_connection->Stop();
            peers_.erase(peer_connection->GetEndpoint());
            if (peer_connection->GetConnectType() == protocol::CONNECT_LIVE_UDPSERVER)
            {
                assert(connected_udpserver_count_ > 0);
                --connected_udpserver_count_;
            }
        }
    }

    void LiveP2PDownloader::CheckBlockComplete()
    {
        // 检查block是否完成
        std::set<protocol::LiveSubPieceInfo> completed_block_set;
        for (std::map<uint32_t, protocol::LiveSubPieceInfo>::iterator iter = block_tasks_.begin();
            iter != block_tasks_.end();
            iter++)
        {
            if (live_instance_->HasCompleteBlock(iter->first))
            {
                completed_block_set.insert(iter->second);
            }
        }

        for (std::set<protocol::LiveSubPieceInfo>::iterator iter = completed_block_set.begin();
            iter != completed_block_set.end(); ++iter)
        {
            for (std::set<LiveDownloadDriver__p>::iterator dditer = download_driver_s_.begin();
                dditer != download_driver_s_.end(); ++dditer)
            {
                (*dditer)->OnBlockComplete(*iter);
            }
        }
    }

    bool LiveP2PDownloader::HasSubPiece(const protocol::LiveSubPieceInfo & sub_piece)
    {
        return live_instance_->HasSubPiece(sub_piece);
    }

    void LiveP2PDownloader::SetBlockCountMap(boost::uint32_t block_id, std::vector<boost::uint16_t> subpiece_count)
    {
        for (boost::uint32_t i=0; i<subpiece_count.size(); i++)
        {
            block_count_map_.insert(std::make_pair(
                block_id+i*(*download_driver_s_.begin())->GetInstance()->GetLiveInterval(),
                subpiece_count[i]
            ));
        }
    }

    std::map<uint32_t,protocol::LiveSubPieceInfo> & LiveP2PDownloader::GetBlockTasks()
    {
        return block_tasks_;
    }

    bool LiveP2PDownloader::HasSubPieceCount(boost::uint32_t block_id)
    {
        return block_count_map_.find(block_id) != block_count_map_.end();
    }

    boost::uint16_t LiveP2PDownloader::GetSubPieceCount(boost::uint32_t block_id)
    {
        if (block_count_map_.find(block_id) != block_count_map_.end())
        {
            return block_count_map_[block_id];
        }

        return 0;
    }


    bool LiveP2PDownloader::IsRequesting(const protocol::LiveSubPieceInfo & subpiece_info)
    {
        return live_subpiece_request_manager_.IsRequesting(subpiece_info);
    }

    void LiveP2PDownloader::AddRequestingSubpiece(const protocol::LiveSubPieceInfo & subpiece_info,
        boost::uint32_t timeout, LivePeerConnection__p peer_connection)
    {
        live_subpiece_request_manager_.Add(subpiece_info, timeout, peer_connection);
    }

    const std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p> & LiveP2PDownloader::GetPeerConnectionInfo() const
    {
        return peers_;
    }

    const map<uint32_t, uint16_t> & LiveP2PDownloader::GetSubPieceCountMap()
    {
        return block_count_map_;
    }

    // 传说中一代直播的经典算法
    storage::LivePosition LiveP2PDownloader::Get75PercentPointInBitmap()
    {
        if (peers_.empty())
        {
            return storage::LivePosition(0, 0);
        }

        uint32_t live_interval = live_instance_->GetLiveInterval();

        map<uint32_t, uint32_t> pos;
        for (std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::iterator iter = peers_.begin();
            iter != peers_.end(); ++iter)
        {
            pos.insert(std::make_pair(iter->second->Get75PercentPointInBitmap(live_interval), 0));
        }

        // 需要除去0
        if (pos.begin()->first == 0)
        {
            pos.erase(pos.begin());
        }

        if (pos.empty())
        {
            return storage::LivePosition(0, 0);
        }

        map<uint32_t, uint32_t>::iterator max_iter = pos.begin();

        for (map<uint32_t, uint32_t>::iterator pos_iter = pos.begin();
            pos_iter != pos.end(); ++pos_iter)
        {
            for (std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::iterator
                peer_iter = peers_.begin(); peer_iter != peers_.end(); ++peer_iter)
            {
                pos_iter->second += peer_iter->second->GetSubPieceCountInBitmap(pos_iter->first);
            }

            // 这里如果多个位置对应的值相等的话，会选择比较小的那个值，从P2P效果来说，更加稳一点
            // 如果需要跳得更多，修改为>=就好
            if (pos_iter->second > max_iter->second)
            {
                max_iter = pos_iter;
            }
        }

        assert(max_iter->first > 0);

        return storage::LivePosition(max_iter->first, 0);
    }

    void LiveP2PDownloader::DoList()
    {
        if (!is_p2p_pausing_ && 
            GetMinRestTimeInSeconds() < 13 &&
            dolist_time_interval_ >= 16000)
        {
            // reset list cycle.
            dolist_time_interval_ = 1000;
        }

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

    boost::uint32_t LiveP2PDownloader::GetMinRestTimeInSeconds() const
    {
        boost::uint32_t min_rest_time = 32768;

        assert(!download_driver_s_.empty());

        for (std::set<LiveDownloadDriver::p>::const_iterator iter = download_driver_s_.begin();
            iter != download_driver_s_.end(); iter++)
        {
            if ((*iter)->GetRestPlayableTime() < min_rest_time)
            {
                min_rest_time = (*iter)->GetRestPlayableTime();
            }
        }

        return min_rest_time;
    }

    storage::LivePosition LiveP2PDownloader::GetMinPlayingPosition() const
    {
        std::set<LiveDownloadDriver__p>::const_iterator iter = download_driver_s_.begin();

        storage::LivePosition min_position = (*iter)->GetPlayingPosition();

        do 
        {
            if ((*iter)->GetPlayingPosition() < min_position)
            {
                min_position = (*iter)->GetPlayingPosition();
            }

            ++iter;
        } while (iter != download_driver_s_.end());

        return min_position;
    }

    void LiveP2PDownloader::EliminateElapsedBlockCountMap(boost::uint32_t block_id)
    {
        for (map<uint32_t, uint16_t>::iterator iter = block_count_map_.begin(); 
            iter != block_count_map_.end(); )
        {
            if (iter->first < block_id)
            {
                block_count_map_.erase(iter++);
            }
            else
            {
                break;
            }
        }
    }

    void LiveP2PDownloader::CheckShouldUseUdpServer()
    {
        // 剩余时间小，使用UdpServer来补带宽
        if (GetMinRestTimeInSeconds() < urgent_rest_playable_time_delim_)
        {
            use_udpserver_reason_ = URGENT;
            should_use_udpserver_ = true;
            should_connect_udpserver_ = true;
            return;
        }

        if (GetMinRestTimeInSeconds() < urgent_rest_playable_time_delim_ + 2)
        {
            should_connect_udpserver_ = true;
        }
        else
        {
            should_connect_udpserver_ = false;
        }

        assert(!download_driver_s_.empty());
        std::set<LiveDownloadDriver::p>::const_iterator download_driver = download_driver_s_.begin();

        // 上传足够大，剩余时间足够大，并且跟视野中的peer相比，跑的足够靠前，使用UdpServer来快速分发
        if ((*download_driver)->IsUploadSpeedLargeEnough() &&
            (*download_driver)->GetRestPlayableTime() > (*download_driver)->GetRestPlayTimeDelim() &&
            IsAheadOfMostPeers())
        {
            use_udpserver_reason_ = LARGE_UPLOAD;
            should_use_udpserver_ = true;
            return;
        }

        // 如果是因为上传大而使用UdpServer的，若上传变小并且也使用UdpServer使用了一段时间了，则暂停使用UdpServer
        if (use_udpserver_reason_ == LARGE_UPLOAD &&
            (*download_driver)->IsUploadSpeedLargeEnough() &&
            use_udpserver_tick_counter_.elapsed() > using_udpserver_time_at_least_when_large_upload_ * 1000)
        {
            should_use_udpserver_ = false;
            return;
        }

        // 如果是因为紧急而使用的UdpServer，若剩余时间足够大了，则暂停使用UdpServer
        if (use_udpserver_reason_ == URGENT &&
            (*download_driver)->GetRestPlayableTime() > safe_enough_rest_playable_time_delim_)
        {
            should_use_udpserver_ = false;
            return;
        }

        // 如果是因为紧急而使用的UdpServer，若跑了很长时间剩余时间还可以，则暂停使用UdpServer
        if (use_udpserver_reason_ == URGENT &&
            use_udpserver_tick_counter_.elapsed() > using_udpserver_time_in_second_delim_ * 1000 &&
            (*download_driver)->GetRestPlayableTime() > safe_rest_playable_time_delim_)
        {
            should_use_udpserver_ = false;
            return;
        }
    }

    void LiveP2PDownloader::DeleteAllUdpServer()
    {
        for (std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::iterator iter = peers_.begin();
            iter != peers_.end();)
        {
            if (iter->second->GetConnectType() == protocol::CONNECT_LIVE_UDPSERVER)
            {
                assert(connected_udpserver_count_ > 0);
                --connected_udpserver_count_;

                // 有可能把UdpServer当做普通的peer来连了，所以需要在ippool_中去尝试disconnect
                ippool_->OnDisConnect(iter->first, true);

                peers_.erase(iter++);
            }
            else
            {
                ++iter;
            }
        }
    }

    bool LiveP2PDownloader::IsAheadOfMostPeers() const
    {
        boost::uint32_t large_bitmap_peer_count = 0;
        for (std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::const_iterator iter = peers_.begin();
            iter != peers_.end(); ++iter)
        {
            if (iter->second->GetConnectType() == protocol::CONNECT_LIVE_UDPSERVER)
            {
                continue;
            }

            if (iter->second->GetBlockBitmapSize() > 1)
            {
                ++large_bitmap_peer_count;

                if (large_bitmap_peer_count >= 2)
                {
                    return false;
                }
            }
        }

        return true;
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
        assert(!download_driver_s_.empty());

        protocol::PeerInfo peer_info(peers_.size(),
            statistic::UploadStatisticModule::Inst()->GetUploadCount(),
            statistic::UploadStatisticModule::Inst()->GetUploadSpeed(),
            UploadModule::Inst()->GetMaxUploadSpeedIncludeSameSubnet(),
            (*download_driver_s_.begin())->GetRestPlayableTime(),
            GetLostRate(),
            GetRedundancyRate());

        protocol::PeerInfoPacket peer_info_packet(protocol::Packet::NewTransactionID(), protocol::PEER_VERSION, peer_info);

        for (std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::iterator iter = peers_.begin();
            iter != peers_.end(); ++iter)
        {
            peer_info_packet.end_point = iter->first;
            DoSendPacket(peer_info_packet);
        }
    }

    boost::uint8_t LiveP2PDownloader::GetLostRate() const
    {
        boost::uint32_t total_request = total_request_subpiece_count_;
        boost::uint32_t total_receive = GetTotalUnusedSubPieceCount();
        boost::uint32_t requesting_count = 0;

        for (std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::const_iterator iter = peers_.begin();
            iter != peers_.end(); ++iter)
        {
            requesting_count += iter->second->GetPeerConnectionInfo().Requesting_Count;
        }

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

    void LiveP2PDownloader::OnConfigUpdated()
    {
        send_peer_info_packet_interval_in_second_ = BootStrapGeneralConfig::Inst()->GetSendPeerInfoPacketIntervalInSecond();
        urgent_rest_playable_time_delim_ = BootStrapGeneralConfig::Inst()->GetUrgentRestPlayableTimeDelim();
        safe_rest_playable_time_delim_ = BootStrapGeneralConfig::Inst()->GetSafeRestPlayableTimeDelim();
        safe_enough_rest_playable_time_delim_ = BootStrapGeneralConfig::Inst()->GetSafeEnoughRestPlayabelTimeDelim();
        using_udpserver_time_in_second_delim_ = BootStrapGeneralConfig::Inst()->GetUsingUdpServerTimeDelim();
        using_udpserver_time_at_least_when_large_upload_ = BootStrapGeneralConfig::Inst()->GetUsingCDNOrUdpServerTimeDelim();
        use_udpserver_count_ = BootStrapGeneralConfig::Inst()->GetUseUdpserverCount();
    }
}
