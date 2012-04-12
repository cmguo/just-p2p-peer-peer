#include "Common.h"
#include "p2sp/p2p/P2PModule.h"
#include "LiveDownloadDriver.h"
#include "p2sp/proxy/ProxyConnection.h"
#include "storage/Storage.h"
#include "statistic/StatisticModule.h"
#include "statistic/BufferringMonitor.h"
#include "statistic/StatisticModule.h"
#include "framework/string/Url.h"
#include "p2sp/proxy/MessageBufferManager.h"
#include "WindowsMessage.h"
#include "statistic/DACStatisticModule.h"
#include "p2sp/tracker/TrackerModule.h"
#include "p2sp/config/Config.h"
#include "LiveDacStopDataStruct.h"
#include "p2sp/download/LiveStream.h"

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("live_download");

    boost::uint32_t LiveDownloadDriver::s_id_ = 0;

    const boost::uint8_t LiveDownloadDriver::InitialChangedToP2PConditionWhenStart = 255;

    const std::string LiveDownloadDriver::LiveHistorySettings::RatioOfUploadToDownload = "r";
    const std::string LiveDownloadDriver::LiveHistorySettings::UdpServerScore = "uss";
    const std::string LiveDownloadDriver::LiveHistorySettings::UdpServerIpAddress = "usia";

    LiveDownloadDriver::LiveDownloadDriver(boost::asio::io_service &io_svc, p2sp::ProxyConnection__p proxy_connetction)
        : io_svc_(io_svc)
        , proxy_connection_(proxy_connetction)
        , replay_(false)
        , timer_(global_second_timer(), 1000, boost::bind(&LiveDownloadDriver::OnTimerElapsed, this, &timer_))
        , id_(++s_id_)
        , http_download_max_speed_(0)
        , p2p_download_max_speed_(0)
        , udp_server_max_speed_(0)
        , jump_times_(0)
        , checksum_failed_times_(0)
        , source_type_(PlayInfo::SOURCE_LIVE_DEFAULT)
        , bwtype_(JBW_NORMAL)
        , elapsed_seconds_since_started_(0)
        , ratio_delim_of_upload_speed_to_datarate_(200)
        , total_times_of_use_cdn_because_of_large_upload_(0)
        , total_time_elapsed_use_cdn_because_of_large_upload_(0)
        , total_download_bytes_use_cdn_because_of_large_upload_(0)
        , changed_to_http_times_when_urgent_(0)
        , block_times_when_use_http_under_urgent_situation_(0)
        , max_upload_speed_during_this_connection_(0)
        , total_upload_connection_count_(0)
        , time_of_receiving_first_connect_request_(0)
        , time_of_sending_first_subpiece_(0)
        , time_of_nonblank_upload_connections_(0)
        , has_received_connect_packet_(false)
        , has_sended_subpiece_packet_(false)
        , small_ratio_delim_of_upload_speed_to_datarate_(100)
        , using_cdn_time_at_least_when_large_upload_(30)
        , http_download_bytes_when_start_(0)
        , upload_bytes_when_start_(0)
        , changed_to_p2p_condition_when_start_(InitialChangedToP2PConditionWhenStart)
        , is_notify_restart_(false)
        , max_push_data_interval_(0)
        , total_upload_bytes_when_using_cdn_because_of_large_upload_(0)
        , is_history_upload_good_(false)
    {
        history_record_count_ = BootStrapGeneralConfig::Inst()->GetMaxTimesOfRecord();
        LoadConfig();
        tick_counter_since_last_advance_using_cdn_.start();
    }

    void LiveDownloadDriver::StartBufferringMonitor()
    {
        elapsed_seconds_since_started_ = 0;
        if (GetInstance() && 
            false == GetInstance()->GetRID().is_empty())
        {
            bufferring_monitor_ = AppModule::Inst()->CreateBufferringMonitor(GetInstance()->GetRID());
        }
        else
        {
            if (!GetInstance())
            {
                LOG(__DEBUG, "live_download", __FUNCTION__ << " live_instance_ is NULL.");
            }
            else
            {
                LOG(__DEBUG, "live_download", __FUNCTION__ << " live_instance_ does not have a valid RID.");
            }
        }
    }

    void LiveDownloadDriver::Start(const string & url, const vector<RID>& rids, uint32_t start_position, uint32_t live_interval,  bool replay,
        const vector<boost::uint32_t>& data_rate_s, const RID& channel_id, boost::uint32_t source_type, JumpBWType bwtype, uint32_t unique_id)
    {
        data_rate_manager_.Start(rids, data_rate_s);
        start_position_.SetBlockId(start_position);

        playing_position_.SetBlockId(start_position);

        replay_ = replay;

        url_ = url;

        source_type_ = source_type;

        bwtype_ = bwtype;

        channel_id_ = channel_id;

        unique_id_ = unique_id;

        timer_->start();

        tick_count_since_last_recv_subpiece_.start();

        rest_time_tracker_.Start(start_position, live_interval);

        // TODO: 选取合适的码流开始播放

        for (boost::uint32_t i = 0; i < rids.size(); ++i)
        {
            LiveStream__p live_stream = LiveStream::Create(shared_from_this(), url_, rids[i], live_interval, data_rate_s[i]);
            live_streams_.push_back(live_stream);
        }

        live_streams_[data_rate_manager_.GetCurrentDataRatePos()]->Start(start_position);

        // 直播状态机
        live_switch_controller_.Start(shared_from_this());
        
#ifndef STATISTIC_OFF
        live_download_driver_statistic_info_.Clear();
        live_download_driver_statistic_info_.LiveDownloadDriverID = id_;
        live_download_driver_statistic_info_.ResourceID = GetInstance()->GetRID();
        live_download_driver_statistic_info_.ChannelID = channel_id;
        live_download_driver_statistic_info_.UniqueID = unique_id;
        framework::string::Url::truncate_to(url_, live_download_driver_statistic_info_.OriginalUrl);
        statistic_ = statistic::StatisticModule::Inst()->AttachLiveDownloadDriverStatistic(id_);
        assert(statistic_);
        statistic_->Start(shared_from_this());
#endif

#if ((defined _DEBUG || defined DEBUG) && (defined CHECK_DOWNLOADED_FILE))
        fp_ = fopen("longstream.flv", "wb+");
#endif

        StartBufferringMonitor();

        p2sp::TrackerModule::Inst()->DoReport(false);

        BootStrapGeneralConfig::Inst()->AddUpdateListener(shared_from_this());

        // 由于检测到配置文件有修改会发生在LiveDownloadDriver创建之前，所以在此手动调用一次OnConfigUpdated
        OnConfigUpdated();

        download_time_.start();

        upload_bytes_when_start_ = statistic::StatisticModule::Inst()->GetUploadDataBytes();
    }

    void LiveDownloadDriver::Stop()
    {
        timer_->stop();

        tick_count_since_last_recv_subpiece_.stop();

        if (GetP2PControlTarget())
        {
            GetP2PControlTarget()->CalcTimeOfUsingUdpServerWhenStop();
        }

        CalcCdnAccelerationStatusWhenStop(data_rate_manager_.GetCurrentDataRatePos());

        if (GetP2PControlTarget())
        {
            GetP2PControlTarget()->CalcDacDataBeforeStop();
        }

        SendDacStopData();

        SaveHistoryConfig();

        for (vector<LiveStream__p>::iterator iter = live_streams_.begin();
            iter != live_streams_.end(); ++iter)
        {
            (*iter)->Stop();
        }

        live_streams_.clear();

        live_switch_controller_.Stop();

        proxy_connection_.reset();

        if (bufferring_monitor_)
        {
            bufferring_monitor_.reset();
        }

#ifndef STATISTIC_OFF
        // 取消共享内存
        live_download_driver_statistic_info_.Clear();
        statistic::StatisticModule::Inst()->DetachLiveDownloadDriverStatistic(statistic_);
        statistic_->Stop();
#endif

        BootStrapGeneralConfig::Inst()->RemoveUpdateListener(shared_from_this());

        tick_counter_since_last_advance_using_cdn_.stop();

        download_time_.stop();
    }

    void LiveDownloadDriver::SaveHistoryConfig() 
    {
        std::vector<boost::uint32_t> udp_server_ip_address_on_history, udp_server_score_on_history;

        std::map<boost::uint32_t, boost::uint32_t> udpservers_score_history = udpservers_score_history_.GetServicesScore(history_record_count_);
        for(std::map<boost::uint32_t, boost::uint32_t>::const_iterator iter = udpservers_score_history.begin();
            iter != udpservers_score_history.end();
            ++iter)
        {
            udp_server_ip_address_on_history.push_back(iter->first);
            udp_server_score_on_history.push_back(iter->second);
        }

        Config::Inst()->SetConfig(LiveHistorySettings::RatioOfUploadToDownload, ratio_of_upload_to_download_on_history_);
        Config::Inst()->SetConfig(LiveHistorySettings::UdpServerIpAddress, udp_server_ip_address_on_history);
        Config::Inst()->SetConfig(LiveHistorySettings::UdpServerScore, udp_server_score_on_history);

        Config::Inst()->SaveConfig();
    }

    LiveHttpDownloader__p LiveDownloadDriver::GetHTTPControlTarget() const
    {
        return live_streams_[data_rate_manager_.GetCurrentDataRatePos()]->GetHttpDownloader();
    }

    LiveP2PDownloader__p LiveDownloadDriver::GetP2PControlTarget() const
    {
        return live_streams_[data_rate_manager_.GetCurrentDataRatePos()]->GetP2PDownloader();
    } 

    bool LiveDownloadDriver::OnRecvLivePiece(uint32_t block_id, std::vector<protocol::LiveSubPieceBuffer> const & buffs,
        uint8_t progress_percentage)
    {
        LOG(__DEBUG, "live_download", "Recv piece_id " << block_id);

        rest_time_tracker_.UpdateCurrentProgress(block_id, progress_percentage);

        if (tick_count_since_last_recv_subpiece_.elapsed() > max_push_data_interval_)
        {
            max_push_data_interval_ = tick_count_since_last_recv_subpiece_.elapsed();
        }

        tick_count_since_last_recv_subpiece_.reset();

        // progress_percentage保证当前block数据全部发送完毕
        if (progress_percentage == 100)
        {
            JumpOrSwitchIfNeeded();
        }

        // 把收到的数据往播放器推送
        vector<base::AppBuffer> app_buffers;
        for (std::vector<protocol::LiveSubPieceBuffer>::const_iterator iter = buffs.begin(); 
            iter != buffs.end(); ++iter)
        {
            base::AppBuffer app_buffer(*iter);
            app_buffers.push_back(app_buffer);

#if ((defined _DEBUG || defined DEBUG) && (defined CHECK_DOWNLOADED_FILE))
            if (fp_)
            {
                fwrite(app_buffer.Data(), sizeof(boost::uint8_t), app_buffer.Length(), fp_);
            }
#endif
        }

        return proxy_connection_->OnRecvLivePiece(block_id, app_buffers);
    }

    storage::LiveInstance__p LiveDownloadDriver::GetInstance() const
    {
        return live_streams_[data_rate_manager_.GetCurrentDataRatePos()]->GetInstance();
    }

    // 获得起始播放点
    const storage::LivePosition & LiveDownloadDriver::GetStartPosition()
    {
        return start_position_;
    }

    // 获得当前数据推送点
    storage::LivePosition & LiveDownloadDriver::GetPlayingPosition()
    {
        return playing_position_;
    }

    void LiveDownloadDriver::OnTimerElapsed(framework::timer::Timer * pointer)
    {
        if (pointer == &timer_)
        {
            if (playing_position_.GetSubPieceIndex() == 0)
            {
                JumpOrSwitchIfNeeded();
            }

#ifndef STATISTIC_OFF
            UpdateStatisticInfo();
            statistic_->UpdateShareMemory();
#endif
            if (http_download_max_speed_ < GetHTTPControlTarget()->GetSpeedInfo().NowDownloadSpeed)
            {
                http_download_max_speed_ = GetHTTPControlTarget()->GetSpeedInfo().NowDownloadSpeed;
            }

            if (GetP2PControlTarget())
            {
                if (p2p_download_max_speed_ < GetP2PControlTarget()->GetSpeedInfo().NowDownloadSpeed)
                {
                    p2p_download_max_speed_ = GetP2PControlTarget()->GetSpeedInfo().NowDownloadSpeed;
                }

                if (udp_server_max_speed_ < GetP2PControlTarget()->GetUdpServerSpeedInfo().NowDownloadSpeed)
                {
                    udp_server_max_speed_ = GetP2PControlTarget()->GetUdpServerSpeedInfo().NowDownloadSpeed;
                }
            }

            ++elapsed_seconds_since_started_;
            if (bufferring_monitor_)
            {
                if (elapsed_seconds_since_started_ > 10 &&
                    GetRestPlayableTime() <= 1)
                {
                    uint32_t data_rate = GetDataRate();
                    if (data_rate > 0)
                    {
                        storage::LivePosition live_position = GetPlayingPosition();
                        uint32_t bufferring_position_in_seconds = live_position.GetBlockId()*GetInstance()->GetLiveInterval() + 
                            live_position.GetSubPieceIndex()*storage::HeaderSubPiece::Constants::SubPieceSizeInBytes/data_rate;
                        bufferring_monitor_->BufferringOccurs(bufferring_position_in_seconds);
                    }
                }
            }

            if (max_upload_speed_during_this_connection_ < statistic::UploadStatisticModule::Inst()->GetUploadSpeed())
            {
                max_upload_speed_during_this_connection_ = statistic::UploadStatisticModule::Inst()->GetUploadSpeed();
            }

            total_upload_connection_count_ += statistic::UploadStatisticModule::Inst()->GetUploadCount();

            if (statistic::UploadStatisticModule::Inst()->GetUploadCount() != 0)
            {
                ++time_of_nonblank_upload_connections_;
            }

            rest_playable_times_.push_back(GetRestPlayableTime());

            if (tick_count_since_last_recv_subpiece_.elapsed() > 180 * 1000 && !is_notify_restart_
                && ((GetHTTPControlTarget() && !GetHTTPControlTarget()->IsPausing()) ||
                (GetP2PControlTarget() && !GetP2PControlTarget()->IsPausing())))
            {
#ifdef PEER_PC_CLIENT
                WindowsMessage::Inst().PostWindowsMessage(UM_LIVE_RESTART, NULL, NULL);
                is_notify_restart_ = true;

                max_push_data_interval_ = tick_count_since_last_recv_subpiece_.elapsed();
#endif
            }
        }
    }

    boost::uint32_t LiveDownloadDriver::GetRestPlayableTime()
    {
        return rest_time_tracker_.GetRestTimeInSeconds();
    }

    void LiveDownloadDriver::JumpTo(const storage::LivePosition & new_playing_position)
    {
        assert(new_playing_position.GetBlockId() % GetInstance()->GetLiveInterval() == 0);

        // 改变播放位置
        playing_position_ = new_playing_position;

        // 跳跃算法启用时，需要把剩余时间重新计算
        rest_time_tracker_.Start(playing_position_.GetBlockId(), GetInstance()->GetLiveInterval());
    }

    void LiveDownloadDriver::OnDataRateChanged()
    {
        // 保留在切换码流前的校验失败次数，但是在下面这种情况，统计的校验失败次数会比实际的偏大，
        // 如果A->B->A，而切换前 A 对应的LiveInstance的校验计算结果>0，而且全部切换的时间小于120秒，
        // 那A对应的LiveInstance还在缓存中而被重用，因而A的校验次数会被重复统计。
        // 我们认为偏大的值可以接受，所以在这没有精确计算校验失败次数
        checksum_failed_times_ += live_streams_[data_rate_manager_.GetLastDataRatePos()]->GetInstance()->GetChecksumFailedTimes();

        bool http_paused = live_streams_[data_rate_manager_.GetLastDataRatePos()]->GetHttpDownloader()->IsPausing();
        bool p2p_paused = true;
        
        if (live_streams_[data_rate_manager_.GetLastDataRatePos()]->GetP2PDownloader())
        {
            live_streams_[data_rate_manager_.GetLastDataRatePos()]->GetP2PDownloader()->IsPausing();
            p2p_paused = false;
        }

        CalcCdnAccelerationStatusWhenStop(data_rate_manager_.GetLastDataRatePos());

        for (vector<LiveStream__p>::iterator iter = live_streams_.begin();
            iter != live_streams_.end(); ++iter)
        {
            (*iter)->Stop();
        }

        live_streams_[data_rate_manager_.GetCurrentDataRatePos()]->Start(GetPlayingPosition().GetBlockId());

        // HttpDownloader的下载状态恢复
        if (!http_paused)
        {
            GetHTTPControlTarget()->Resume();
        }

        // P2PDownloader的下载状态恢复
        if (!p2p_paused)
        {
            GetP2PControlTarget()->Resume();
        }
    }

    void LiveDownloadDriver::SetSwitchState(boost::int32_t h, boost::int32_t p)
    {
        switch_state_http_ = h;
        switch_state_p2p_ = p;
    }

#ifndef STATISTIC_OFF
    void LiveDownloadDriver::UpdateStatisticInfo()
    {
        // rid
        live_download_driver_statistic_info_.ResourceID = GetInstance()->GetRID();

        // speed info
        live_download_driver_statistic_info_.LiveHttpSpeedInfo = GetHTTPControlTarget()->GetSpeedInfo();
        live_download_driver_statistic_info_.LiveP2PSpeedInfo = GetP2PControlTarget() ?
            GetP2PControlTarget()->GetSpeedInfo() : statistic::SPEED_INFO();
        live_download_driver_statistic_info_.LiveP2PSubPieceSpeedInfo = GetP2PControlTarget() ?
            GetP2PControlTarget()->GetSubPieceSpeedInfo() : statistic::SPEED_INFO();

        // http status code
        live_download_driver_statistic_info_.LastHttpStatusCode = GetHTTPControlTarget()->GetHttpStatusCode();

        // switch state
        live_download_driver_statistic_info_.http_state = switch_state_http_;
        live_download_driver_statistic_info_.p2p_state = switch_state_p2p_;

        // peer count
        live_download_driver_statistic_info_.PeerCount = GetP2PControlTarget() ? GetP2PControlTarget()->GetConnectedPeersCount() : 0;

        // ip pool
        live_download_driver_statistic_info_.IpPoolPeerCount = GetP2PControlTarget() ? GetP2PControlTarget()->GetPooledPeersCount() : 0;

        // total unused subpiece count
        live_download_driver_statistic_info_.TotalUnusedSubPieceCount = GetP2PControlTarget() ?
            GetP2PControlTarget()->GetTotalUnusedSubPieceCount() : 0;

        // total all request subpiece count
        live_download_driver_statistic_info_.TotalAllRequestSubPieceCount = GetP2PControlTarget() ?
            GetP2PControlTarget()->GetTotalAllRequestSubPieceCount() : 0;

        // total recieved subpiece count
        live_download_driver_statistic_info_.TotalRecievedSubPieceCount = GetP2PControlTarget() ?
            GetP2PControlTarget()->GetTotalRecievedSubPieceCount() : 0;

        // total request subpiece count
        live_download_driver_statistic_info_.TotalRequestSubPieceCount = GetP2PControlTarget() ?
            GetP2PControlTarget()->GetTotalRequestSubPieceCount() : 0;

        // total p2p data bytes
        live_download_driver_statistic_info_.TotalP2PDataBytes = GetP2PControlTarget() ? GetP2PControlTarget()->GetTotalP2PDataBytes() : 0;

        // cache size
        live_download_driver_statistic_info_.CacheSize = GetInstance()->GetCacheSize();

        // first cache block id
        live_download_driver_statistic_info_.CacheFirstBlockId = GetInstance()->GetCacheFirstBlockId();

        // last cache block id
        live_download_driver_statistic_info_.CacheLastBlockId = GetInstance()->GetCacheLastBlockId();

        // playing position
        live_download_driver_statistic_info_.PlayingPosition = playing_position_.GetBlockId();

        // rest play time
        live_download_driver_statistic_info_.RestPlayTime = GetRestPlayableTime();

        // data rate
        live_download_driver_statistic_info_.DataRate = GetDataRate();

        // play position block full or not
        live_download_driver_statistic_info_.IsPlayingPositionBlockFull = IsPlayingPositionBlockFull();

        // peer connection info
        if (GetP2PControlTarget())
        {
            const std::map<boost::asio::ip::udp::endpoint, boost::shared_ptr<p2sp::LivePeerConnection> > & peer_connections
                = GetP2PControlTarget()->GetPeers();

            int i = 0;
            for (std::map<boost::asio::ip::udp::endpoint, boost::shared_ptr<p2sp::LivePeerConnection> >::const_iterator iter
                = peer_connections.begin(); iter != peer_connections.end(); ++iter, ++i)
            {
                live_download_driver_statistic_info_.P2PConnections[i] = iter->second->GetPeerConnectionInfo();
            }
        }

        // left capacity
#ifdef USE_MEMORY_POOL
        live_download_driver_statistic_info_.LeftCapacity = protocol::LiveSubPieceContent::get_left_capacity();
#endif

        // live point block id
        live_download_driver_statistic_info_.LivePointBlockId = GetInstance()->GetCurrentLivePoint().GetBlockId();

        // data rate level
        live_download_driver_statistic_info_.DataRateLevel = data_rate_manager_.GetCurrentDataRatePos();

        // jump times
        live_download_driver_statistic_info_.JumpTimes = jump_times_;

        // checksum failed
        live_download_driver_statistic_info_.NumOfChecksumFailedPieces =
            checksum_failed_times_ + GetInstance()->GetChecksumFailedTimes();

        // udp server download bytes
        live_download_driver_statistic_info_.TotalUdpServerDataBytes = GetP2PControlTarget() ?
            GetP2PControlTarget()->GetTotalUdpServerDataBytes() : 0;

        // pms status
        live_download_driver_statistic_info_.PmsStatus = GetHTTPControlTarget()->GetPmsStatus() ? 0 : 1;

        // udpserver speed
        live_download_driver_statistic_info_.UdpServerSpeedInfo = GetP2PControlTarget() ?
            GetP2PControlTarget()->GetUdpServerSpeedInfo() : statistic::SPEED_INFO();

        // pause
        live_download_driver_statistic_info_.IsPaused = rest_time_tracker_.IsPaused() ? 1 : 0;

        // replay
        live_download_driver_statistic_info_.IsReplay = replay_ ? 1 : 0;

        // missing subpiece count of first block
        live_download_driver_statistic_info_.MissingSubPieceCountOfFirstBlock = GetInstance()->GetMissingSubPieceCount(playing_position_.GetBlockId());

        // exist subpiece count of first block
        live_download_driver_statistic_info_.ExistSubPieceCountOfFirstBlock = GetInstance()->GetExistSubPieceCount(playing_position_.GetBlockId());

        // peer 一秒的速度
        live_download_driver_statistic_info_.P2PPeerSpeedInSecond = GetP2PControlTarget() ?
            GetP2PControlTarget()->GetSubPieceSpeedInfoEx().SecondDownloadSpeed : 0;

        // udpserver 一秒的速度
        live_download_driver_statistic_info_.P2PUdpServerSpeedInSecond = GetP2PControlTarget() ?
            GetP2PControlTarget()->GetUdpServerSpeedInfoEx().SecondDownloadSpeed : 0;
    }
#endif

    void LiveDownloadDriver::SendDacStopData()
    {
#ifdef NEED_TO_POST_MESSAGE
        // A: 接口类别，固定为*(TODO(emma): 确定下来固定为几)
        // B: 用户的ID
        // C: 资源ID(不同码流的rid以@符号隔开)
        // D: 内核版本
        // E: 码流(不同码流以@符号隔开)
        // F: url
        // G: P2P下载字节数
        // H: Http下载字节数
        // I: 总下载字节数
        // J: P2P平均速度(会偏小)
        // K: P2P最大速度
        // L: Http最大速度
        // M: 连接上的节点数目
        // N: 查询到的节点数目
        // O: 开始播放点
        // P: 跳跃次数
        // Q: 校验失败次数
        // R: SourceType
        // S: 频道ID
        // T: 从UdpServer下载的字节数
        // U: UdpServer下载最大速度
        // V: 上传字节数
        // W: 下载时间
        // X: 因为上传比较大而使用CDN的次数
        // Y: 因为上传比较大而使用CDN的时间
        // Z: 因为上传比较大使用CDN时的下载字节数
        // A1: 因为紧急而使用UdpServer的次数
        // B1: 因为紧急而使用UdpServer的时间
        // C1: 因为紧急而使用UdpServer时的下载字节数
        // D1: 因为上传比较大而使用UdpServer的次数
        // E1: 因为上传比较大而使用UdpServer的时间
        // F1: 因为上传比较大而使用UdpServer时的下载字节数
        // G1: 最大上传速度(包括同一内网)
        // H1: 最大上传速度(不包括同一内网)
        // I1: 历史最大上传速度
        // J1: 启动后切换到P2P的情况(0: 剩余时间足够大，1: 跑的时间足够长，2: 卡了)
        // K1: 紧急情况下切到Http的次数
        // L1: 紧急情况下切到Http后卡的次数
        // M1: 本次连接的最大上传速度
        // N1: 本次连接的平均上传连接数
        // O1: 连接建立后多久收到Connect请求
        // P1: 连接建立后多久发出第一个SubPiece
        // Q1: 上传连接数非0的时间
        // R1: NatType
        // S1: Http启动时下载的字节数
        // T1: 本次连接上传字节数
        // U1: 是否通知客户端重新播放
        // V1: 最大数据推送间隔
        // W1: 剩余时间的平均值
        // X1: 剩余时间的方差
        // Y1: 平均1分钟之内发起连接数
        // Z1: 总共收到的数据包的个数
        // A2: 收到的逆序数据包的个数
        // B2: 带宽
        // C2: 因为大上传时使用CDN期间的上传量
        // D2: 在需要使用UdpServer时，已经list到的UdpServer个数的最小值
        // E2: 在需要使用UdpServer时，已经list到的UdpServer个数的最大值
        // F2: 在每次使用UdpServer期间，连接上的UdpServer的平均值的最小值
        // G2: 在每次使用UdpServer期间，连接上的UdpServer的平均值的最大值
        // H2: 在每次使用UdpServer期间，收到Announce的次数的最小值
        // I2: 在每次使用UdpServer期间，收到Announce的次数的最大值
        // J2: 在每次使用UdpServer期间，收到/请求的最小值
        // K2: 在每次使用UdpServer期间，收到/请求的最大值

        LIVE_DAC_STOP_DATA_STRUCT info;
        info.ResourceIDs = data_rate_manager_.GetRids();
        info.DataRates = data_rate_manager_.GetDataRates();

        info.PeerVersion[0] = AppModule::GetKernelVersionInfo().Major;
        info.PeerVersion[1] = AppModule::GetKernelVersionInfo().Minor;
        info.PeerVersion[2] = AppModule::GetKernelVersionInfo().Micro;
        info.PeerVersion[3] = AppModule::GetKernelVersionInfo().Extra;

        u_int pos = url_.find_first_of('/');
        pos = url_.find_first_of('/', pos + 1);
        pos = url_.find_first_of('/', pos + 1);
        info.OriginalUrl = string(url_, 0, pos);

        info.P2PDownloadBytes = GetP2PControlTarget() ? GetP2PControlTarget()->GetTotalP2PDataBytes() : 0;
        info.HttpDownloadBytes = GetHTTPControlTarget() ? GetHTTPControlTarget()->GetSpeedInfo().TotalDownloadBytes : 0;
        info.UdpDownloadBytes = GetP2PControlTarget() ? GetP2PControlTarget()->GetTotalUdpServerDataBytes() : 0;
        info.TotalDownloadBytes = info.P2PDownloadBytes + info.HttpDownloadBytes + info.UdpDownloadBytes;
        info.AvgP2PDownloadSpeed = GetP2PControlTarget() ? GetP2PControlTarget()->GetSpeedInfo().AvgDownloadSpeed : 0;
        info.MaxHttpDownloadSpeed = http_download_max_speed_;
        info.MaxP2PDownloadSpeed = p2p_download_max_speed_;

        info.ConnectedPeerCount = GetP2PControlTarget() ? GetP2PControlTarget()->GetConnectedPeersCount() : 0;
        info.QueriedPeerCount = GetP2PControlTarget() ? GetP2PControlTarget()->GetPooledPeersCount() : 0;

        info.StartPosition = start_position_.GetBlockId();
        info.JumpTimes = jump_times_;
        info.NumOfCheckSumFailedPieces = checksum_failed_times_ + GetInstance()->GetChecksumFailedTimes();

        info.SourceType = source_type_;
        info.ChannelID = channel_id_;

        info.MaxUdpServerDownloadSpeed = udp_server_max_speed_;

        info.UploadBytes = statistic::StatisticModule::Inst()->GetUploadDataBytes();
        info.DownloadTime = download_time_.elapsed() / 1000;

        info.TimesOfUseCdnBecauseLargeUpload = total_times_of_use_cdn_because_of_large_upload_;
        info.TimeElapsedUseCdnBecauseLargeUpload = total_time_elapsed_use_cdn_because_of_large_upload_;
        info.DownloadBytesUseCdnBecauseLargeUpload = total_download_bytes_use_cdn_because_of_large_upload_;

        if (GetP2PControlTarget())
        {
            info.TimesOfUseUdpServerBecauseUrgent = GetP2PControlTarget()->GetTimesOfUseUdpServerBecauseOfUrgent();
            info.TimeElapsedUseUdpServerBecauseUrgent = GetP2PControlTarget()->GetTimeElapsedUseUdpServerBecauseOfUrgent();
            info.DownloadBytesUseUdpServerBecauseUrgent = GetP2PControlTarget()->GetDownloadBytesUseUdpServerBecauseOfUrgent();
            info.TimesOfUseUdpServerBecauseLargeUpload = GetP2PControlTarget()->GetTimesOfUseUdpServerBecauseOfLargeUpload();
            info.TimeElapsedUseUdpServerBecauseLargeUpload = GetP2PControlTarget()->GetTimeElapsedUseUdpServerBecauseOfLargeUpload();
            info.DownloadBytesUseUdpServerBecauseLargeUpload = GetP2PControlTarget()->GetDownloadBytesUseUdpServerBecauseOfLargeUpload();
        }
        else
        {
            info.TimesOfUseUdpServerBecauseUrgent = 0;
            info.TimeElapsedUseUdpServerBecauseUrgent = 0;
            info.DownloadBytesUseUdpServerBecauseUrgent = 0;
            info.TimesOfUseUdpServerBecauseLargeUpload = 0;
            info.TimeElapsedUseUdpServerBecauseLargeUpload = 0;
            info.DownloadBytesUseUdpServerBecauseLargeUpload = 0;
        }

        info.MaxUploadSpeedIncludeSameSubnet = UploadModule::Inst()->GetMaxUploadSpeedIncludeSameSubnet();
        info.MaxUploadSpeedExcludeSameSubnet = UploadModule::Inst()->GetMaxUploadSpeedExcludeSameSubnet();

        info.MaxUnlimitedUploadSpeedInRecord = UploadModule::Inst()->GetMaxUnlimitedUploadSpeedInRecord();

        info.ChangeToP2PConditionWhenStart = changed_to_p2p_condition_when_start_;
        info.ChangedToHttpTimesWhenUrgent = changed_to_http_times_when_urgent_;
        info.BlockTimesWhenUseHttpUnderUrgentSituation = block_times_when_use_http_under_urgent_situation_;

        info.MaxUploadSpeedDuringThisConnection = max_upload_speed_during_this_connection_;

        if (time_of_nonblank_upload_connections_ == 0)
        {
            info.AverageUploadConnectionCount = 0;
        }
        else
        {
            info.AverageUploadConnectionCount = static_cast<boost::uint32_t>((total_upload_connection_count_ + 0.0) / time_of_nonblank_upload_connections_ + 0.5);
        }

        info.TimeOfReceivingFirstConnectRequest = time_of_receiving_first_connect_request_;
        info.TimeOfSendingFirstSubPiece = time_of_sending_first_subpiece_;
        info.TimeOfNonblankUploadConnections = time_of_nonblank_upload_connections_;
        info.NatType = AppModule::Inst()->GetCandidatePeerInfo().PeerNatType;

        if (changed_to_p2p_condition_when_start_ == InitialChangedToP2PConditionWhenStart)
        {
            info.HttpDownloadBytesWhenStart = GetHTTPControlTarget() ?
                GetHTTPControlTarget()->GetSpeedInfo().TotalDownloadBytes : 0;
        }
        else
        {
            info.HttpDownloadBytesWhenStart = http_download_bytes_when_start_;
        }

        assert(statistic::StatisticModule::Inst()->GetUploadDataBytes() >= upload_bytes_when_start_);
        info.UploadBytesDuringThisConnection = statistic::StatisticModule::Inst()->GetUploadDataBytes() - upload_bytes_when_start_;

        info.IsNotifyRestart = (boost::uint32_t)is_notify_restart_;

        if (max_push_data_interval_ < tick_count_since_last_recv_subpiece_.elapsed())
        {
            max_push_data_interval_ = tick_count_since_last_recv_subpiece_.elapsed();
        }

        info.MaxPushDataInterval = max_push_data_interval_;

        info.AverageOfRestPlayableTime = CalcAverageOfRestPlayableTime();
        info.VarianceOfRestPlayableTime = CalcVarianceOfRestPlayableTime(info.AverageOfRestPlayableTime);

        info.AverageConnectPeersCountInMinute = 0;
        if (GetP2PControlTarget() && download_time_.elapsed() > 2000)
        {
            info.AverageConnectPeersCountInMinute = GetP2PControlTarget()->GetTotalConnectPeersCount() * 60 / (download_time_.elapsed() / 1000);
        }

        if (GetP2PControlTarget())
        {
            info.TotalReceivedSubPiecePacketCount = GetP2PControlTarget()->GetTotalReceivedSubPiecePacketCount();
            info.ReverseSubPiecePacketCount = GetP2PControlTarget()->GetReverseOrderSubPiecePacketCount();
        }
        else
        {
            info.TotalReceivedSubPiecePacketCount = 0;
            info.ReverseSubPiecePacketCount = 0;
        }

        info.BandWidth = statistic::StatisticModule::Inst()->GetBandWidth();
        info.UploadBytesWhenUsingCDNBecauseOfLargeUpload = total_upload_bytes_when_using_cdn_because_of_large_upload_;

        if (GetP2PControlTarget())
        {
            info.MinUdpServerCountWhenNeeded = GetP2PControlTarget()->GetUdpServerCountWhenNeeded().Min();
            info.MaxUdpServerCountWhenNeeded = GetP2PControlTarget()->GetUdpServerCountWhenNeeded().Max();
            info.MinConnectUdpServerCountWhenNeeded = GetP2PControlTarget()->GetConnectUdpServerCountWhenNeeded().Min();
            info.MaxConnectUdpServerCountWhenNeeded = GetP2PControlTarget()->GetConnectUdpServerCountWhenNeeded().Max();
            info.MinAnnounceResponseFromUdpServer = GetP2PControlTarget()->GetAnnounceResponseFromUdpServer().Min();
            info.MaxAnnounceResponseFromUdpServer = GetP2PControlTarget()->GetAnnounceResponseFromUdpServer().Max();
            info.MinRatioOfResponseToRequestFromUdpserver = GetP2PControlTarget()->GetRatioOfResponseToRequestFromUdpserver().Min();
            info.MaxRatioOfResponseToRequestFromUdpserver = GetP2PControlTarget()->GetRatioOfResponseToRequestFromUdpserver().Max();
        }
        else
        {
            info.MinUdpServerCountWhenNeeded = 0;
            info.MaxUdpServerCountWhenNeeded = 0;
            info.MinConnectUdpServerCountWhenNeeded = 0;
            info.MaxConnectUdpServerCountWhenNeeded = 0;
            info.MinAnnounceResponseFromUdpServer = 0;
            info.MaxAnnounceResponseFromUdpServer = 0;
            info.MinRatioOfResponseToRequestFromUdpserver = 0;
            info.MaxRatioOfResponseToRequestFromUdpserver = 0;
        }

        string log = info.ToString();

        DebugLog("%s", log.c_str());

        LPDOWNLOADDRIVER_STOP_DAC_DATA dac_data =
            MessageBufferManager::Inst()->NewStruct<DOWNLOADDRIVER_STOP_DAC_DATA> ();
        memset(dac_data, 0, sizeof(DOWNLOADDRIVER_STOP_DAC_DATA));

        dac_data->uSize = sizeof(DOWNLOADDRIVER_STOP_DAC_DATA);
        dac_data->uSourceType = source_type_;
        strncpy(dac_data->szLog, log.c_str(), sizeof(dac_data->szLog)-1);

        WindowsMessage::Inst().PostWindowsMessage(UM_LIVE_DAC_STATISTIC, (WPARAM)id_, (LPARAM)dac_data);
#endif
    }

    void LiveDownloadDriver::OnPause(bool pause)
    {
        rest_time_tracker_.OnPause(pause);
    }

    void LiveDownloadDriver::JumpOrSwitchIfNeeded()
    {
        // 跳跃算法
        // 1. 回放不会跳跃
        // 2. 落后直播点120s
        if (!replay_ && !rest_time_tracker_.IsPaused())
        {
            bool is_jump = false;

            if (playing_position_.GetBlockId() + 90 < GetInstance()->GetCurrentLivePoint().GetBlockId())
            {
                JumpTo(storage::LivePosition(GetInstance()->GetCurrentLivePoint().GetBlockId() - 2 * GetInstance()->GetLiveInterval()));

                is_jump = true;
            }

            if (is_jump)
            {
                jump_times_++;
            }
        }

        // 码流切换算法
        // 1. 当前block的数据必须全部发送完毕
        if (BootStrapGeneralConfig::Inst()->AutoSwitchStream() && (
            data_rate_manager_.SwitchToHigherDataRateIfNeeded(GetRestPlayableTime()) ||
            data_rate_manager_.SwitchToLowerDataRateIfNeeded(GetRestPlayableTime())))
        {
            OnDataRateChanged();
        }
    }

    void LiveDownloadDriver::OnConfigUpdated()
    {
        ratio_delim_of_upload_speed_to_datarate_ = BootStrapGeneralConfig::Inst()->GetRatioDelimOfUploadSpeedToDatarate();
        small_ratio_delim_of_upload_speed_to_datarate_ = BootStrapGeneralConfig::Inst()->GetSmallRatioDelimOfUploadSpeedToDatarate();
        using_cdn_time_at_least_when_large_upload_ = BootStrapGeneralConfig::Inst()->GetUsingCDNOrUdpServerTimeDelim();
    }

    bool LiveDownloadDriver::IsUploadSpeedLargeEnough()
    {
        return statistic::StatisticModule::Inst()->GetMinuteUploadDataSpeed() > GetDataRate() * ratio_delim_of_upload_speed_to_datarate_ / 100;
    }

    bool LiveDownloadDriver::IsUploadSpeedSmallEnough()
    {
        return statistic::StatisticModule::Inst()->GetMinuteUploadDataSpeed() < GetDataRate() * small_ratio_delim_of_upload_speed_to_datarate_ / 100;
    }

    bool LiveDownloadDriver::GetUsingCdnTimeAtLeastWhenLargeUpload() const
    {
        return using_cdn_time_at_least_when_large_upload_;
    }

    void LiveDownloadDriver::SetUseCdnBecauseOfLargeUpload()
    {
        live_streams_[data_rate_manager_.GetCurrentDataRatePos()]->SetUseCdnBecauseOfLargeUpload();
    }

    void LiveDownloadDriver::SetUseP2P()
    {
        live_streams_[data_rate_manager_.GetCurrentDataRatePos()]->SetUseP2P();
    }

    void LiveDownloadDriver::SubmitChangedToP2PCondition(boost::uint8_t condition)
    {
        changed_to_p2p_condition_when_start_ = condition;

        assert(GetHTTPControlTarget());
        http_download_bytes_when_start_ = GetHTTPControlTarget()->GetSpeedInfo().TotalDownloadBytes;
    }

    void LiveDownloadDriver::SubmitChangedToHttpTimesWhenUrgent(boost::uint32_t times)
    {
        changed_to_http_times_when_urgent_ += times;
    }

    void LiveDownloadDriver::SubmitBlockTimesWhenUseHttpUnderUrgentCondition(boost::uint32_t times)
    {
        block_times_when_use_http_under_urgent_situation_ += times;
    }

    boost::uint8_t LiveDownloadDriver::GetLostRate() const
    {
        return GetP2PControlTarget() ? GetP2PControlTarget()->GetLostRate() : 0;
    }

    boost::uint8_t LiveDownloadDriver::GetRedundancyRate() const
    {
        return GetP2PControlTarget() ? GetP2PControlTarget()->GetRedundancyRate() : 0;
    }

    boost::uint32_t LiveDownloadDriver::GetTotalRequestSubPieceCount() const
    {
        return GetP2PControlTarget() ? GetP2PControlTarget()->GetTotalRequestSubPieceCount() : 0;
    }

    boost::uint32_t LiveDownloadDriver::GetTotalRecievedSubPieceCount() const
    {
        return GetP2PControlTarget() ? GetP2PControlTarget()->GetTotalRecievedSubPieceCount() : 0;
    }

    void LiveDownloadDriver::SetReceiveConnectPacket()
    {
        if (has_received_connect_packet_ == false)
        {
            has_received_connect_packet_ = true;
            time_of_receiving_first_connect_request_ = download_time_.elapsed() / 1000;
        }
    }

    void LiveDownloadDriver::SetSendSubPiecePacket()
    {
        if (has_sended_subpiece_packet_ == false)
        {
            has_sended_subpiece_packet_ = true;
            time_of_sending_first_subpiece_ = download_time_.elapsed() / 1000;
        }
    }

    boost::uint32_t LiveDownloadDriver::GetDownloadTime() const
    {
        return download_time_.elapsed();
    }

    boost::uint32_t LiveDownloadDriver::CalcAverageOfRestPlayableTime()
    {
        if (rest_playable_times_.size() == 0)
        {
            return 0;
        }

        boost::uint32_t sum_of_rest_playable_time = 0;
        for (size_t i = 0; i < rest_playable_times_.size(); ++i)
        {
            sum_of_rest_playable_time += rest_playable_times_[i];
        }

        return static_cast<boost::uint32_t>((sum_of_rest_playable_time + 0.0) / rest_playable_times_.size() + 0.5);
    }

    boost::uint32_t LiveDownloadDriver::CalcVarianceOfRestPlayableTime(boost::uint32_t average_of_rest_playable_time)
    {
        if (rest_playable_times_.size() == 0)
        {
            return 0;
        }

        boost::uint32_t sum_of_squares = 0;
        for (size_t i = 0; i < rest_playable_times_.size(); ++i)
        {
            sum_of_squares += pow(abs((double)rest_playable_times_[i] - (double)average_of_rest_playable_time), 2);
        }

        return static_cast<boost::uint32_t>((sum_of_squares + 0.0)/ rest_playable_times_.size() + 0.5);
    }

    void LiveDownloadDriver::SetRestTimeInSecond(boost::uint32_t rest_time_in_second)
    {
        rest_time_tracker_.SetRestTimeInSecond(rest_time_in_second);
    }

    bool LiveDownloadDriver::DoesFallBehindTooMuch() const
    {
        if (GetP2PControlTarget() &&
            playing_position_.GetBlockId() + BootStrapGeneralConfig::Inst()->GetFallBehindSecondsThreshold() < GetP2PControlTarget()->GetMinFirstBlockID())
        {
            return true;
        }

        return false;
    }

    void LiveDownloadDriver::LoadConfig()
    {
        std::map<std::string, boost::uint32_t> config_count;
        config_count.insert(std::make_pair(LiveHistorySettings::RatioOfUploadToDownload, history_record_count_));
        config_count.insert(std::make_pair(LiveHistorySettings::UdpServerIpAddress, history_record_count_));
        config_count.insert(std::make_pair(LiveHistorySettings::UdpServerScore, history_record_count_));

        Config::Inst()->SetConfigCount(config_count);
        Config::Inst()->LoadConfig();

        Config::Inst()->GetConfig(LiveHistorySettings::RatioOfUploadToDownload, ratio_of_upload_to_download_on_history_);

        std::vector<boost::uint32_t> udp_server_ip_address_on_history, udp_server_score_on_history;
        Config::Inst()->GetConfig(LiveHistorySettings::UdpServerIpAddress, udp_server_ip_address_on_history);
        Config::Inst()->GetConfig(LiveHistorySettings::UdpServerScore, udp_server_score_on_history);
        
        udpservers_score_history_.Initialize(udp_server_ip_address_on_history, udp_server_score_on_history);

        CalcHistoryUploadStatus();
    }

    void LiveDownloadDriver::UpdateUdpServerServiceScore(const boost::asio::ip::udp::endpoint& udp_server, int service_score)
    {
        udpservers_score_history_.UpdateServiceScore(udp_server, service_score);
    }

    const std::map<boost::uint32_t, boost::uint32_t> LiveDownloadDriver::GetUdpServerServiceScore() const
    {
        return udpservers_score_history_.GetServicesScore(0);
    }

    void LiveDownloadDriver::CalcHistoryUploadStatus()
    {
        if (ratio_of_upload_to_download_on_history_.size() < BootStrapGeneralConfig::Inst()->GetMinTimesOfRecord())
        {
            is_history_upload_good_ = false;
        }
        else
        {
            boost::uint32_t max_ratio_of_upload_to_download = 0;
            boost::uint32_t large_ratio_count = 0;

            for (boost::uint32_t i = 0; i < ratio_of_upload_to_download_on_history_.size(); ++i)
            {
                if (max_ratio_of_upload_to_download < ratio_of_upload_to_download_on_history_[i])
                {
                    max_ratio_of_upload_to_download = ratio_of_upload_to_download_on_history_[i];
                }

                if (ratio_of_upload_to_download_on_history_[i] > BootStrapGeneralConfig::Inst()->GetLargeRatioOfUploadToDownloadDelim())
                {
                    ++large_ratio_count;
                }
            }

            is_history_upload_good_ = (max_ratio_of_upload_to_download > BootStrapGeneralConfig::Inst()->GetMaxRatioOfUploadToDownloadDelim() ||
                large_ratio_count * 100 >= InitialChangedToP2PConditionWhenStart * BootStrapGeneralConfig::Inst()->GetRatioOfLargeUploadTimesToTotalTimesDelim());
        }
    }

    bool LiveDownloadDriver::ShouldUseCdnToAccelerate()
    {
        if (!is_history_upload_good_)
        {
            return false;
        }

        if (live_streams_[data_rate_manager_.GetCurrentDataRatePos()]->GetTimesOfUseCdnBecauseOfLargeUpload() != 0 &&
            tick_counter_since_last_advance_using_cdn_.elapsed() < BootStrapGeneralConfig::Inst()->GetMinIntervalOfCdnAccelerationDelim() * 1000)
        {
            return false;
        }

        if (GetP2PControlTarget() && GetP2PControlTarget()->GetPooledPeersCount() >= BootStrapGeneralConfig::Inst()->GetDesirableLiveIpPoolSize() &&
            statistic::UploadStatisticModule::Inst()->GetUploadCount() > BootStrapGeneralConfig::Inst()->GetUploadConnectionCountDelim() &&
            statistic::UploadStatisticModule::Inst()->GetUploadSpeed() > GetDataRate() * BootStrapGeneralConfig::Inst()->GetNotStrictRatioDelimOfUploadToDatarate() / 100)
        {
            tick_counter_since_last_advance_using_cdn_.reset();
            return true;
        }

        return false;
    }

    boost::uint32_t LiveDownloadDriver::GetDataRate() const
    {
        return live_streams_[data_rate_manager_.GetCurrentDataRatePos()]->GetDataRate();
    }

    void LiveDownloadDriver::AddCdnAccelerationHistory(boost::uint32_t ratio_of_upload_to_download)
    {
        ratio_of_upload_to_download_on_history_.push_back(ratio_of_upload_to_download);
    }

    void LiveDownloadDriver::CalcCdnAccelerationStatusWhenStop(boost::uint32_t data_rate_pos)
    {
        live_streams_[data_rate_pos]->CalcCdnAccelerationStatusWhenStop();

        live_streams_[data_rate_pos]->UpdateCdnAccelerationHistory();

        total_times_of_use_cdn_because_of_large_upload_ += live_streams_[data_rate_pos]->GetTimesOfUseCdnBecauseOfLargeUpload();
        total_time_elapsed_use_cdn_because_of_large_upload_ += live_streams_[data_rate_pos]->GetTimeElapsedUseCdnBecauseOfLargeUpload();
        total_download_bytes_use_cdn_because_of_large_upload_ += live_streams_[data_rate_pos]->GetDownloadBytesUseCdnBecauseOfLargeUpload();
        total_upload_bytes_when_using_cdn_because_of_large_upload_ += live_streams_[data_rate_pos]->GetTotalUploadBytesWhenUsingCdnBecauseOfLargeUpload();
    }
}