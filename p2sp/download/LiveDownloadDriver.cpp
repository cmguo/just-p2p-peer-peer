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

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("live_download");

    boost::uint32_t LiveDownloadDriver::s_id_ = 0;

    LiveDownloadDriver::LiveDownloadDriver(boost::asio::io_service &io_svc, p2sp::ProxyConnection__p proxy_connetction)
        : io_svc_(io_svc)
        , proxy_connection_(proxy_connetction)
        , replay_(false)
        , pause_(false)
        , timer_(global_second_timer(), 1000, boost::bind(&LiveDownloadDriver::OnTimerElapsed, this, &timer_))
        , id_(++s_id_)
        , http_download_max_speed_(0)
        , p2p_download_max_speed_(0)
        , jump_times_(0)
        , checksum_failed_times_(0)
        , source_type_(PlayInfo::SOURCE_LIVE_DEFAULT)
        , bwtype_(JBW_NORMAL)
        , elapsed_seconds_since_started_(0)
    {
    }

    void LiveDownloadDriver::StartBufferringMonitor()
    {
        elapsed_seconds_since_started_ = 0;
        if (live_instance_ && 
            false == live_instance_->GetRID().is_empty())
        {
            bufferring_monitor_ = AppModule::Inst()->CreateBufferringMonitor(live_instance_->GetRID());
        }
        else
        {
            if (!live_instance_)
            {
                LOG(__DEBUG, "live_download", __FUNCTION__ << " live_instance_ is NULL.");
            }
            else
            {
                LOG(__DEBUG, "live_download", __FUNCTION__ << " live_instance_ does not have a valid RID.");
            }
        }
    }

    void LiveDownloadDriver::Start(const protocol::UrlInfo& url_info, const vector<RID>& rids, uint32_t start_position, uint32_t live_interval,  bool replay,
        const vector<boost::uint32_t>& data_rate_s, const RID& channel_id, boost::uint32_t source_type, JumpBWType bwtype)
    {
        data_rate_manager_.Start(rids, data_rate_s);
        start_position_.SetBlockId(start_position);

        playing_position_.SetBlockId(start_position);

        replay_ = replay;

        url_info_ = url_info;

        source_type_ = source_type;

        bwtype_ = bwtype;

        channel_id_ = channel_id;

        timer_->start();

        rest_time_tracker_.Start(start_position, live_interval);

        // TODO: 选取合适的码流开始播放

        // 创建Instance
        live_instance_ = boost::dynamic_pointer_cast<storage::LiveInstance>(
            storage::Storage::Inst()->CreateLiveInstance(data_rate_manager_.GetCurrentRID(), live_interval));

        live_instance_->AttachDownloadDriver(shared_from_this());
        live_instance_->SetCurrentLivePoint(storage::LivePosition(start_position));

        // 创建HttpDownloader
        live_http_downloader_ = LiveHttpDownloader::Create(io_svc_, url_info, data_rate_manager_.GetCurrentRID(), shared_from_this());
        live_http_downloader_->Start();
        live_http_downloader_->Pause();

        if (bwtype_ != JBW_HTTP_ONLY)
        {
            // 创建P2PDownloader
            live_p2p_downloader_ = p2sp::P2PModule::Inst()->CreateLiveP2PDownloader(data_rate_manager_.GetCurrentRID(), live_instance_);
            live_p2p_downloader_->AttachDownloadDriver(shared_from_this());
        }

        // 启动BlockRequestManager
        live_block_request_manager_.Start(shared_from_this());

        // 直播状态机
        switch_control_mode_ = SwitchController::CONTROL_MODE_VIDEO;

        // 创建直播状态机
        if (SwitchController::IsValidControlMode(SwitchController::CONTROL_MODE_LIVE))
        {
            switch_control_mode_ = static_cast<SwitchController::ControlModeType> (SwitchController::CONTROL_MODE_LIVE);
        }

        // 直播状态机启动
        switch_controller_ = SwitchController::Create(shared_from_this());
        switch_controller_->Start(switch_control_mode_);

#ifndef STATISTIC_OFF
        live_download_driver_statistic_info_.Clear();
        live_download_driver_statistic_info_.LiveDownloadDriverID = id_;
        live_download_driver_statistic_info_.ResourceID = live_instance_->GetRID();
        live_download_driver_statistic_info_.ChannelID = channel_id;
        framework::string::Url::truncate_to(url_info.url_, live_download_driver_statistic_info_.OriginalUrl);
        statistic_ = statistic::StatisticModule::Inst()->AttachLiveDownloadDriverStatistic(id_);
        assert(statistic_);
        statistic_->Start(shared_from_this());
#endif

#if ((defined _DEBUG || defined DEBUG) && (defined CHECK_DOWNLOADED_FILE))
        fp_ = fopen("longstream.flv", "wb+");
#endif

        StartBufferringMonitor();
    }

    void LiveDownloadDriver::Stop()
    {
        timer_->stop();

        SendDacStopData();

        live_instance_->DetachDownloadDriver(shared_from_this());

        live_http_downloader_->Stop();
        live_http_downloader_.reset();

        if (live_p2p_downloader_)
        {
            live_p2p_downloader_->DetachDownloadDriver(shared_from_this());
            live_p2p_downloader_.reset();
        }

        switch_controller_->Stop();
        switch_controller_.reset();

        live_block_request_manager_.Stop();

        proxy_connection_.reset();

        if (bufferring_monitor_)
        {
            bufferring_monitor_.reset();
        }

#ifndef STATISTIC_OFF
        // 取消共享内存
        statistic::StatisticModule::Inst()->DetachLiveDownloadDriverStatistic(statistic_);
        statistic_->Stop();
#endif
    }

    IHTTPControlTarget::p LiveDownloadDriver::GetHTTPControlTarget()
    {
        if (live_http_downloader_)
        {
            return live_http_downloader_;
        }

        return IHTTPControlTarget::p();
    }

    IP2PControlTarget::p LiveDownloadDriver::GetP2PControlTarget()
    {
        if (live_p2p_downloader_)
        {
            return live_p2p_downloader_;
        }
        return IP2PControlTarget::p();
    } 

    bool LiveDownloadDriver::RequestNextBlock(LiveDownloader__p downloader)
    {
        if (!live_block_request_manager_.GetNextBlockForDownload(playing_position_.GetBlockId(), downloader))
        {
            // 没有可下载的Block
            return false;
        }

        return true;
    }

    bool LiveDownloadDriver::OnRecvLivePiece(uint32_t block_id, std::vector<protocol::LiveSubPieceBuffer> const & buffs,
        uint8_t progress_percentage)
    {
        LOG(__DEBUG, "live_download", "Recv piece_id " << block_id);

        rest_time_tracker_.UpdateCurrentProgress(block_id, progress_percentage);

        // progress_percentage保证当前block数据全部发送完毕
        if (progress_percentage == 100)
        {
            // 跳跃算法
            // 1. 回放不会跳跃
            // 2. 落后直播点120s
            if (!replay_ && !pause_)
            {
                while (playing_position_.GetBlockId() + 120 < live_instance_->GetCurrentLivePoint().GetBlockId())
                {
                    storage::LivePosition new_playing_position = live_p2p_downloader_ ?
                        live_p2p_downloader_->Get75PercentPointInBitmap() : 0;

                    if (playing_position_ < new_playing_position)
                    {
                        JumpTo(new_playing_position);
                    }
                    else
                    {
                        JumpTo(storage::LivePosition(playing_position_.GetBlockId() + 90));
                    }
                }
            }

            // 码流切换算法
            // 1. 当前block的数据必须全部发送完毕
            if (data_rate_manager_.SwitchToHigherDataRateIfNeeded(GetRestPlayableTime()) ||
                data_rate_manager_.SwitchToLowerDataRateIfNeeded(GetRestPlayableTime()))
            {
                OnDataRateChanged();
            }
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

    storage::LiveInstance__p LiveDownloadDriver::GetInstance()
    {
        return live_instance_;
    }

    void LiveDownloadDriver::OnBlockComplete(const protocol::LiveSubPieceInfo & live_block)
    {
        live_block_request_manager_.RemoveBlockTask(live_block.GetBlockId());
    }

    void LiveDownloadDriver::OnBlockTimeout(const protocol::LiveSubPieceInfo & live_block)
    {
        live_block_request_manager_.RemoveBlockTask(live_block.GetBlockId());
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
            rest_time_tracker_.UpdateRestTime();
#ifndef STATISTIC_OFF
            UpdateStatisticInfo();
            statistic_->UpdateShareMemory();
#endif

            // DAC Log
            statistic::DACStatisticModule::Inst()->SubmitDataRateLevel(data_rate_manager_.GetCurrentDataRatePos());
            statistic::DACStatisticModule::Inst()->SubmitPlayingPosition(playing_position_.GetBlockId());
            statistic::DACStatisticModule::Inst()->SubmitLivePosition(live_instance_->GetCurrentLivePoint().GetBlockId());
            statistic::DACStatisticModule::Inst()->SubmitRestPlayTime(GetRestPlayableTime());
            statistic::DACStatisticModule::Inst()->SubmitChannelID(channel_id_);
            if (live_p2p_downloader_)
            {
                statistic::DACStatisticModule::Inst()->SubmitConnectedPeers(live_p2p_downloader_->GetConnectedPeersCount());
                statistic::DACStatisticModule::Inst()->SubmitQueryedPeers(live_p2p_downloader_->GetPooledPeersCount());
            }
            else
            {
                statistic::DACStatisticModule::Inst()->SubmitConnectedPeers(0);
                statistic::DACStatisticModule::Inst()->SubmitQueryedPeers(0);
            }

            if (live_http_downloader_ && !live_http_downloader_->IsPausing())
            {
                statistic::DACStatisticModule::Inst()->SubmitHttpDownloadTime(pointer->interval());
            }
            if (live_p2p_downloader_ && !live_p2p_downloader_->IsPausing())
            {
                statistic::DACStatisticModule::Inst()->SubmitP2PDownloadTime(pointer->interval());
            }

            if (http_download_max_speed_ < live_http_downloader_->GetSpeedInfo().NowDownloadSpeed)
            {
                http_download_max_speed_ = live_http_downloader_->GetSpeedInfo().NowDownloadSpeed;
            }

            if (live_p2p_downloader_)
            {
                if (p2p_download_max_speed_ < live_p2p_downloader_->GetSpeedInfo().NowDownloadSpeed)
                {
                    p2p_download_max_speed_ = live_p2p_downloader_->GetSpeedInfo().NowDownloadSpeed;
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
                        uint32_t bufferring_position_in_seconds = live_position.GetBlockId()*live_instance_->GetLiveInterval() + 
                            live_position.GetSubPieceIndex()*storage::HeaderSubPiece::Constants::SubPieceSizeInBytes/data_rate;
                        bufferring_monitor_->BufferringOccurs(bufferring_position_in_seconds);
                    }
                }
            }
        }
    }

    boost::uint32_t LiveDownloadDriver::GetRestPlayableTime()
    {
        return rest_time_tracker_.GetRestTimeInSeconds();
    }

    void LiveDownloadDriver::JumpTo(const storage::LivePosition & new_playing_position)
    {
        assert(new_playing_position.GetBlockId() % live_instance_->GetLiveInterval() == 0);

        // 改变播放位置
        playing_position_ = new_playing_position;

        // 跳跃算法启用时，需要把剩余时间重新计算
        rest_time_tracker_.Start(playing_position_.GetBlockId(), live_instance_->GetLiveInterval());

        ++jump_times_;
    }

    void LiveDownloadDriver::OnDataRateChanged()
    {
        // 保留在切换码流前的校验失败次数，但是在下面这种情况，统计的校验失败次数会比实际的偏大，
        // 如果A->B->A，而切换前 A 对应的LiveInstance的校验计算结果>0，而且全部切换的时间小于120秒，
        // 那A对应的LiveInstance还在缓存中而被重用，因而A的校验次数会被重复统计。
        // 我们认为偏大的值可以接受，所以在这没有精确计算校验失败次数
        checksum_failed_times_ += live_instance_->GetChecksumFailedTimes();

        // 切换Instance
        int32_t live_interval = live_instance_->GetLiveInterval();

        storage::LivePosition live_point = live_instance_->GetCurrentLivePoint();

        live_instance_->DetachDownloadDriver(shared_from_this());

        live_instance_ = boost::dynamic_pointer_cast<storage::LiveInstance>(
            storage::Storage::Inst()->CreateLiveInstance(data_rate_manager_.GetCurrentRID(), live_interval));

        live_instance_->AttachDownloadDriver(shared_from_this());

        live_instance_->SetCurrentLivePoint(live_point);

        // 切换LiveHttpDownloader
        // 这时live_http_downloader_的堆栈在OnRecvHttpDataSucced
        // 切换码流仅仅是切换RID,这样下次去连接就是用另外一个RID了
        live_http_downloader_->OnDataRateChanged(data_rate_manager_.GetCurrentRID());

        // 切换P2PDownloader
        // 这个live_p2p_downloader_的堆栈在OnUdpRecv
        // 切换码流需要把当前码流对应的P2PDownloader删除
        // 同时新建一个P2PDownloader插入P2PModule
        if (live_p2p_downloader_)
        {
            bool p2p_paused = live_p2p_downloader_->IsPausing();

            live_p2p_downloader_->Pause();
            live_p2p_downloader_->DetachDownloadDriver(shared_from_this());
            live_p2p_downloader_.reset();

            live_p2p_downloader_ = p2sp::P2PModule::Inst()->CreateLiveP2PDownloader(data_rate_manager_.GetCurrentRID(), live_instance_);
            live_p2p_downloader_->AttachDownloadDriver(shared_from_this());

            // P2PDownloader的下载状态恢复
            if (!p2p_paused)
            {
                live_p2p_downloader_->Resume();
            }
        }
    }

    void LiveDownloadDriver::SetSwitchState(boost::int32_t h, boost::int32_t p, boost::int32_t tu, boost::int32_t t)
    {
        switch_state_http_ = h;
        switch_state_p2p_ = p;
    }

#ifndef STATISTIC_OFF
    void LiveDownloadDriver::UpdateStatisticInfo()
    {
        // rid
        live_download_driver_statistic_info_.ResourceID = live_instance_->GetRID();

        // speed info
        live_download_driver_statistic_info_.LiveHttpSpeedInfo = live_http_downloader_->GetSpeedInfo();
        live_download_driver_statistic_info_.LiveP2PSpeedInfo = live_p2p_downloader_ ?
            live_p2p_downloader_->GetSpeedInfo() : statistic::SPEED_INFO();
        live_download_driver_statistic_info_.LiveP2PSubPieceSpeedInfo = live_p2p_downloader_ ?
            live_p2p_downloader_->GetSubPieceSpeedInfo() : statistic::SPEED_INFO();

        // http status code
        live_download_driver_statistic_info_.LastHttpStatusCode = live_http_downloader_->GetHttpStatusCode();

        // switch state
        live_download_driver_statistic_info_.http_state = switch_state_http_;
        live_download_driver_statistic_info_.p2p_state = switch_state_p2p_;

        // peer count
        live_download_driver_statistic_info_.PeerCount = live_p2p_downloader_ ? live_p2p_downloader_->GetConnectedPeersCount() : 0;

        // ip pool
        live_download_driver_statistic_info_.IpPoolPeerCount = live_p2p_downloader_ ? live_p2p_downloader_->GetPooledPeersCount() : 0;

        // total unused subpiece count
        live_download_driver_statistic_info_.TotalUnusedSubPieceCount = live_p2p_downloader_ ?
            live_p2p_downloader_->GetTotalUnusedSubPieceCount() : 0;

        // total all request subpiece count
        live_download_driver_statistic_info_.TotalAllRequestSubPieceCount = live_p2p_downloader_ ?
            live_p2p_downloader_->GetTotalAllRequestSubPieceCount() : 0;

        // total recieved subpiece count
        live_download_driver_statistic_info_.TotalRecievedSubPieceCount = live_p2p_downloader_ ?
            live_p2p_downloader_->GetTotalRecievedSubPieceCount() : 0;

        // total request subpiece count
        live_download_driver_statistic_info_.TotalRequestSubPieceCount = live_p2p_downloader_ ?
            live_p2p_downloader_->GetTotalRequestSubPieceCount() : 0;

        // total p2p data bytes
        live_download_driver_statistic_info_.TotalP2PDataBytes = live_p2p_downloader_ ? live_p2p_downloader_->GetTotalP2PDataBytes() : 0;

        // cache size
        live_download_driver_statistic_info_.CacheSize = live_instance_->GetCacheSize();

        // first cache block id
        live_download_driver_statistic_info_.CacheFirstBlockId = live_instance_->GetCacheFirstBlockId();

        // last cache block id
        live_download_driver_statistic_info_.CacheLastBlockId = live_instance_->GetCacheLastBlockId();

        // playing position
        live_download_driver_statistic_info_.PlayingPosition = playing_position_.GetBlockId();

        // rest play time
        live_download_driver_statistic_info_.RestPlayTime = GetRestPlayableTime();

        // data rate
        live_download_driver_statistic_info_.DataRate = GetDataRate();

        // play position block full or not
        live_download_driver_statistic_info_.IsPlayingPositionBlockFull = IsPlayingPositionBlockFull();

        // peer connection info
        if (live_p2p_downloader_)
        {
            const std::map<boost::asio::ip::udp::endpoint, boost::shared_ptr<p2sp::LivePeerConnection> > & peer_connections
                = live_p2p_downloader_->GetPeerConnectionInfo();

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
        live_download_driver_statistic_info_.LivePointBlockId = live_instance_->GetCurrentLivePoint().GetBlockId();

        // data rate level
        live_download_driver_statistic_info_.DataRateLevel = data_rate_manager_.GetCurrentDataRatePos();

        boost::shared_ptr<SwitchController::QueryControlMode> query_control_mode =
            boost::shared_dynamic_cast<SwitchController::QueryControlMode>(switch_controller_->GetControlMode());

        if (query_control_mode)
        {
            // P2P failed times
            live_download_driver_statistic_info_.P2PFailedTimes = query_control_mode->GetP2PFailedTimes();

            // 2300 http speed status
            live_download_driver_statistic_info_.HttpSpeedStatus = query_control_mode->Get2300HttpSpeedStatus();

            // http status
            live_download_driver_statistic_info_.HttpStatus = query_control_mode->GetHttpStatus();

            // p2p status
            live_download_driver_statistic_info_.P2PStatus = query_control_mode->GetP2PStatus();

            // is 3200 p2p slow
            live_download_driver_statistic_info_.Is3200P2PSlow = query_control_mode->GetWhether3200P2PSlow();
        }
        else
        {
            // P2P failed times
            live_download_driver_statistic_info_.P2PFailedTimes = 0;

            // http speed status
            live_download_driver_statistic_info_.HttpSpeedStatus = 0;

            // http status
            live_download_driver_statistic_info_.HttpStatus = 0;

            // p2p status
            live_download_driver_statistic_info_.P2PStatus = 0;

            // is 3200 p2p slow
            live_download_driver_statistic_info_.Is3200P2PSlow = 0;
        }

        // jump times
        live_download_driver_statistic_info_.JumpTimes = jump_times_;

        // checksum failed
        live_download_driver_statistic_info_.NumOfChecksumFailedPieces =
            checksum_failed_times_ + live_instance_->GetChecksumFailedTimes();
    }
#endif

    void LiveDownloadDriver::SendDacStopData()
    {
#ifdef NEED_TO_POST_MESSAGE

        // 内核在看完一个直播频道后提交的数据
        // A: 接口类别，固定为*(TODO(emma): 确定下来固定为几)
        // B: 用户的ID
        // C: 资源ID(不同码流的rid以@符号隔开)
        // D: 内核版本
        // E: 码流(不同码流以@符号隔开)
        // F: url
        // G: P2P下载字节数
        // H: Http下载字节数
        // I: 总下载字节数
        // J: P2P平均速度
        // K: P2P最大速度
        // L: Http最大速度
        // M: 连接上的节点数目
        // N: 查询到的节点数目
        // O: 开始播放点
        // P: 跳跃次数
        // Q: 校验失败次数
        // R: SourceType
        // S: 频道ID

        LIVE_DOWNLOADDRIVER_STOP_DAC_DATA_STRUCT info;
        info.ResourceIDs = data_rate_manager_.GetRids();
        info.DataRates = data_rate_manager_.GetDataRates();

        info.PeerVersion[0] = AppModule::GetKernelVersionInfo().Major;
        info.PeerVersion[1] = AppModule::GetKernelVersionInfo().Minor;
        info.PeerVersion[2] = AppModule::GetKernelVersionInfo().Micro;
        info.PeerVersion[3] = AppModule::GetKernelVersionInfo().Extra;

        string originalUrl(url_info_.url_);
        u_int pos = originalUrl.find_first_of('/');
        pos = originalUrl.find_first_of('/', pos + 1);
        pos = originalUrl.find_first_of('/', pos + 1);
        info.OriginalUrl = string(originalUrl, 0, pos);

        info.P2PDownloadBytes = live_p2p_downloader_ ? live_p2p_downloader_->GetTotalP2PDataBytes() : 0;
        info.HttpDownloadBytes = live_http_downloader_ ? live_http_downloader_->GetSpeedInfo().TotalDownloadBytes : 0;
        info.TotalDownloadBytes = info.P2PDownloadBytes + info.HttpDownloadBytes;
        info.AvgP2PDownloadSpeed = live_p2p_downloader_ ? live_p2p_downloader_->GetSpeedInfo().AvgDownloadSpeed : 0;
        info.MaxHttpDownloadSpeed = http_download_max_speed_;
        info.MaxP2PDownloadSpeed = p2p_download_max_speed_;

        info.ConnectedPeerCount = live_p2p_downloader_ ? live_p2p_downloader_->GetConnectedPeersCount() : 0;
        info.QueriedPeerCount = live_p2p_downloader_ ? live_p2p_downloader_->GetPooledPeersCount() : 0;

        info.StartPosition = start_position_.GetBlockId();
        info.JumpTimes = jump_times_;
        info.NumOfCheckSumFailedPieces = checksum_failed_times_ + live_instance_->GetChecksumFailedTimes();

        info.SourceType = source_type_;
        info.ChannelID = channel_id_;

        std::ostringstream log_stream;

        log_stream << "C=";
        for (boost::uint32_t i = 0; i < info.ResourceIDs.size(); ++i)
        {
            if (i != 0)
            {
                log_stream << "@";
            }
            log_stream << info.ResourceIDs[i].to_string();
        }

        log_stream << "&D=" << info.PeerVersion[0] << "." << info.PeerVersion[1] << "."
            << info.PeerVersion[2] << "." << info.PeerVersion[3];

        log_stream << "&E=";
        for (boost::uint32_t i = 0; i < info.DataRates.size(); ++i)
        {
            if (i != 0)
            {
                log_stream << "@";
            }
            log_stream << info.DataRates[i];
        }

        log_stream << "&F=" << info.OriginalUrl;
        log_stream << "&G=" << info.P2PDownloadBytes;
        log_stream << "&H=" << info.HttpDownloadBytes;
        log_stream << "&I=" << info.TotalDownloadBytes;
        log_stream << "&J=" << info.AvgP2PDownloadSpeed;
        log_stream << "&K=" << info.MaxP2PDownloadSpeed;
        log_stream << "&L=" << info.MaxHttpDownloadSpeed;
        log_stream << "&M=" << info.ConnectedPeerCount;
        log_stream << "&N=" << info.QueriedPeerCount;
        log_stream << "&O=" << info.StartPosition;
        log_stream << "&P=" << info.JumpTimes;
        log_stream << "&Q=" << info.NumOfCheckSumFailedPieces;
        log_stream << "&R=" << info.SourceType;
        log_stream << "&S=" << info.ChannelID;

        string log = log_stream.str();

        DebugLog("%s", log.c_str());

        LPLIVE_DOWNLOADDRIVER_STOP_DAC_DATA dac_data =
            MessageBufferManager::Inst()->NewStruct<LIVE_DOWNLOADDRIVER_STOP_DAC_DATA> ();

        memset(dac_data, 0, sizeof(LIVE_DOWNLOADDRIVER_STOP_DAC_DATA));

        dac_data->uSize = sizeof(LIVE_DOWNLOADDRIVER_STOP_DAC_DATA);
        strncpy(dac_data->szLog, log.c_str(), sizeof(dac_data->szLog)-1);

        WindowsMessage::Inst().PostWindowsMessage(UM_LIVE_DAC_STATISTIC, (WPARAM)id_, (LPARAM)dac_data);
#endif
    }

    boost::uint32_t LiveDownloadDriver::GetBandWidth()
    {
        return statistic::StatisticModule::Inst()->GetBandWidth();
    }
}