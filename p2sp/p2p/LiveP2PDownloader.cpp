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
        , p2p_max_connect_count_(30)
        , total_all_request_subpiece_count_(0)
        , total_request_subpiece_count_(0)
        , dolist_time_interval_(1000)
    {

    }

    void LiveP2PDownloader::Start()
    {
        // IPPool
        ippool_ = IpPool::create();
        ippool_->Start();

        // Exchange
        exchanger_ = Exchanger::create(shared_from_this(), ippool_);
        exchanger_->Start();

        // PeerConnector
        connector_ = PeerConnector::create(shared_from_this(), ippool_);
        connector_->Start();

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

        is_running_ = true;
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
        else
        {
            return;
        }

        // 每秒
        // 驱动一次发起连接和断开连接
        if (times % 4 == 0)
        {
            if (connector_)
            {
                connector_->OnP2PTimer(times);
            }

            KickPeerConnection();

            InitPeerConnection();
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
            if (GetMinRestTimeInSeconds() < 10)
            {
                live_assigner_.OnP2PTimer(times, true);
            }
            else
            {
                live_assigner_.OnP2PTimer(times, false);
            }
            
        }
        
        for (std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::iterator iter = peers_.begin();
            iter != peers_.end(); ++iter)
        {
            iter->second->OnP2PTimer(times);
        }
    }

    // 查询到的节点加入IPPool
    void LiveP2PDownloader::AddCandidatePeers(std::vector<protocol::CandidatePeerInfo> peers)
    {
        if (ippool_->GetPeerCount() == 0)
        {
            ippool_->AddCandidatePeers(peers);
            InitPeerConnection();
        }
        else
        {
            ippool_->AddCandidatePeers(peers);
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
        std::list<protocol::LiveSubPieceInfo>::iterator iter = block_tasks_.begin();
        for (; iter != block_tasks_.end(); ++iter)
        {
            if ((*iter).GetBlockId() == block_id)
            {
                block_tasks_.erase(iter);
                return;
            }
        }
    }

    void LiveP2PDownloader::PutBlockTask(const protocol::LiveSubPieceInfo & live_block)
    {
        LOG(__DEBUG, "live_p2p", __FUNCTION__ << " " << __LINE__  << " " << live_block);
        std::list<protocol::LiveSubPieceInfo>::iterator iter = block_tasks_.begin();

        for (; iter != block_tasks_.end(); ++iter)
        {
            if (iter->GetBlockId() == live_block.GetBlockId())
            {
                return;
            }
        }

        block_tasks_.push_back(live_block);
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

    void LiveP2PDownloader::InitPeerConnection()
    {
        if (!is_running_)
        {
            return;
        }

        if (!connector_)
        {
            return ;
        }

        boost::int32_t connect_count = (p2p_max_connect_count_ - peers_.size()) * 2 - connector_->GetConnectingPeerCount();
        LIMIT_MIN(connect_count, 1);

        for (boost::int32_t i = 0; i < connect_count; ++i)
        {
            protocol::CandidatePeerInfo candidate_peer_info;
            if (false == ippool_->GetForConnect(candidate_peer_info)) 
            {
                break;
            }

            connector_->Connect(candidate_peer_info);
        }
    }

    void LiveP2PDownloader::KickPeerConnection()
    {
        boost::int32_t kick_count = 0;
        std::multimap<uint32_t, LivePeerConnection::p> peer_kick_map;

        for (std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::iterator 
            iter = peers_.begin(); iter != peers_.end(); )
        {
            if (iter->second->LongTimeNoAnnounceResponse())
            {
                ippool_->OnDisConnect(iter->first);
                DelPeer((iter++)->second);
            }
            else
            {
                boost::uint32_t peer_now_speed = iter->second->GetSpeedInfo().NowDownloadSpeed;
                peer_kick_map.insert(std::make_pair(peer_now_speed, iter->second));
                ++iter;
            }
        }

        if (peers_.size() > p2p_max_connect_count_)
        {
            kick_count = peers_.size() - p2p_max_connect_count_;

            std::multimap<uint32_t, LivePeerConnection::p>::iterator iter = peer_kick_map.begin();
            for (boost::int32_t i = 0; i < kick_count; i++)
            {
                if (iter == peer_kick_map.end())
                {
                    break;
                }

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
            connector_->OnReConectPacket((protocol::ConnectPacket const &)packet);
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
        }
    }

    void LiveP2PDownloader::CheckBlockComplete()
    {
        // 检查block是否完成
        std::set<protocol::LiveSubPieceInfo> completed_block_set;
        for (std::list<protocol::LiveSubPieceInfo>::iterator iter = block_tasks_.begin();
            iter != block_tasks_.end();)
        {
            protocol::LiveSubPieceInfo & live_block = *(iter++);
            if (live_instance_->HasCompleteBlock(live_block.GetBlockId()))
            {
                // 删除block_count_map中相对应的节点
                map<uint32_t, uint16_t>::iterator it = block_count_map_.find(live_block.GetBlockId());
                if (it != block_count_map_.end())
                {
                    block_count_map_.erase(it);
                }

                // 删除PeerConnection中想对应的节点
                for (std::map<boost::asio::ip::udp::endpoint, LivePeerConnection__p>::iterator peer_iter = peers_.begin();
                    peer_iter != peers_.end(); ++peer_iter)
                {
                    peer_iter->second->EliminateBlockBitMap(live_block.GetBlockId());
                }

                completed_block_set.insert(live_block);
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

    std::list<protocol::LiveSubPieceInfo> & LiveP2PDownloader::GetBlockTasks()
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
        if (GetConnectedPeersCount() < GetMaxConnectCount() &&
            last_dolist_time_.elapsed() > dolist_time_interval_)
        {
            // DoList 的间隔按照指数增长，最多128秒
            dolist_time_interval_ *= 2;
            if (dolist_time_interval_ > 128000)
            {
                dolist_time_interval_ = 128000;
            }

            last_dolist_time_.reset();

            p2sp::TrackerModule::Inst()->DoList(GetRid(), false);
        }
    }

    boost::uint32_t LiveP2PDownloader::GetMinRestTimeInSeconds() const
    {
        boost::uint32_t min_rest_time = 32768;
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
}
