//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "p2sp/AppModule.h"
#include "p2sp/p2p/DownloadSpeedLimiter.h"
#include "p2sp/p2p/P2PDownloader.h"
#include "statistic/P2PDownloaderStatistic.h"

#define DOWNLIMITER_DEBUG(msg) LOG(__DEBUG, "p2p_speed_limiter", __FUNCTION__ << " " << msg)

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("p2p_speed_limiter");
    const boost::uint32_t DOWNLIMIT_MIN_INTERVAL_IN_MS = 1000;

    DownloadSpeedLimiter::DownloadSpeedLimiter(boost::uint32_t max_data_queue_length, boost::uint32_t packet_life_limit_in_ms)
        : speed_limit_in_KBps_(-1)
        , max_data_queue_length_(max_data_queue_length)
        , packet_life_limit_in_ms_(packet_life_limit_in_ms)
        , last_trans_id_(0)
        , packet_number_per_tick_(-1)
        , is_running_(true)
        , tick_timer_(global_second_timer(), DOWNLIMIT_MIN_INTERVAL_IN_MS, boost::bind(&DownloadSpeedLimiter::OnTimerElapsed, this, &tick_timer_))
    {
        LIMIT_MIN_MAX(max_data_queue_length, 100, 1000);   // 100K - 1M
        LIMIT_MIN_MAX(packet_life_limit_in_ms, 0, 10000);  // 2s - 10s

        tick_timer_.start();
    }

    DownloadSpeedLimiter::~DownloadSpeedLimiter()
    {
        Stop();
    }

    void DownloadSpeedLimiter::Stop()
    {
        if (is_running_)
        {
            is_running_ = false;
            tick_timer_.stop();
            data_queue_.clear();
        }
    }

    bool DownloadSpeedLimiter::IsDataQueueFull() const
    {
        return data_queue_.size() >= max_data_queue_length_;
    }

    void DownloadSpeedLimiter::OnTimerElapsed(framework::timer::Timer * pointer)
    {
        if (!is_running_)
        {
            return;
        }

        if (pointer == &tick_timer_)
        {
            DOWNLIMITER_DEBUG("data_queue_.size= " << data_queue_.size() << " packet_number_per_tick_ = " << packet_number_per_tick_);
            for (sent_count_ = 0; sent_count_ < packet_number_per_tick_ && !data_queue_.empty();)
            {
                EndpointPacketInfo data = data_queue_.front();
                boost::uint32_t life = data.life_time_.elapsed();
                if (packet_life_limit_in_ms_ == 0 || life <= packet_life_limit_in_ms_ + tick_interval_in_ms_)
                {
                    // 发送请求并统计
                    statistic::P2PDownloaderStatistic::p p2p_statistic = data.p2p_downloader_->GetStatistic();
                    if (data.trans_id != last_trans_id_)
                    {
                        if (p2p_statistic) {
                            p2p_statistic->SubmitRequestSubPieceCount(data.piece_count);
                        }
                        sent_count_ += data.piece_count;
                        last_trans_id_ = data.trans_id;
                    }
                    if (p2p_statistic) 
                    {
                        p2p_statistic->SubmitPeerUploadedBytes(data.packet_len);
                    }

                    if (data.is_old_packet)
                    {
                        AppModule::Inst()->DoSendPacket(data.subpiece_packet_old_, data.dest_protocol_version_);
                    }
                    else
                    {
                        AppModule::Inst()->DoSendPacket(data.subpiece_packet_, data.dest_protocol_version_);
                    }
                }
                data_queue_.pop_front();
            }
        }
    }

    void DownloadSpeedLimiter::DoRequestSubPiece(p2sp::P2PDownloader::p p2p_downloader, 
        protocol::RequestSubPiecePacket const & packet, boost::uint16_t dest_protocol_version)
    {
        if (!is_running_)
        {
            return;
        }

        // 队列满
        if (data_queue_.size() >= max_data_queue_length_ || GetSpeedLimitInKBps() == 0)
        {
            DOWNLIMITER_DEBUG("Data queue is full: size=" << data_queue_.size());
            // 抛弃最前面的包
            if (!data_queue_.empty())
            {
                data_queue_.pop_front();
            }
            data_queue_.push_back(EndpointPacketInfo(packet, p2p_downloader, dest_protocol_version));
            return;
        }

        // 不限速
        if (GetSpeedLimitInKBps() < 0)
        {
            statistic::P2PDownloaderStatistic::p p2p_statistic = p2p_downloader->GetStatistic();
            if (p2p_statistic)
            {
                if (packet.transaction_id_ != last_trans_id_)
                {
                    p2p_statistic->SubmitRequestSubPieceCount(packet.subpiece_infos_.size());
                    last_trans_id_ = packet.transaction_id_;
                }
                p2p_statistic->SubmitPeerUploadedBytes(packet.length());
            }

            DOWNLIMITER_DEBUG("DoRequestSubPiece < 0");

            AppModule::Inst()->DoSendPacket(packet, dest_protocol_version);

            return;
        }
        else
        {
            DOWNLIMITER_DEBUG("DoRequestSubPiece >= 0 size = " << packet.subpiece_infos_.size() << " send_count = " << sent_count_);
            if (sent_count_ < packet_number_per_tick_)
            {
                statistic::P2PDownloaderStatistic::p p2p_statistic = p2p_downloader->GetStatistic();
                if (p2p_statistic)
                {
                    if (packet.transaction_id_ != last_trans_id_)
                    {
                        p2p_statistic->SubmitRequestSubPieceCount(packet.subpiece_infos_.size());
                        last_trans_id_ = packet.transaction_id_;
                        sent_count_ += packet.subpiece_infos_.size();
                    }
                    p2p_statistic->SubmitPeerUploadedBytes(packet.length());
                }

                AppModule::Inst()->DoSendPacket(packet, dest_protocol_version);
            }
            else
            {
                data_queue_.push_back(EndpointPacketInfo(packet, p2p_downloader, dest_protocol_version));
            }
        }
    }


    void DownloadSpeedLimiter::DoRequestSubPiece(p2sp::P2PDownloader::p p2p_downloader, 
        protocol::RequestSubPiecePacketOld const & packet, boost::uint16_t dest_protocol_version)
    {
        if (!is_running_)
        {
            return;
        }

        // 队列满
        if (data_queue_.size() >= max_data_queue_length_ || GetSpeedLimitInKBps() == 0)
        {
            DOWNLIMITER_DEBUG("Data queue is full: size=" << data_queue_.size());
            // 抛弃最前面的包
            if (!data_queue_.empty()) {
                data_queue_.pop_front();
            }
            data_queue_.push_back(EndpointPacketInfo(packet, p2p_downloader, dest_protocol_version));
            return;
        }

        // 不限速
        if (GetSpeedLimitInKBps() < 0)
        {
            statistic::P2PDownloaderStatistic::p p2p_statistic = p2p_downloader->GetStatistic();
            if (p2p_statistic)
            {
                if (packet.transaction_id_ != last_trans_id_)
                {
                    p2p_statistic->SubmitRequestSubPieceCount(packet.subpiece_infos_.size());
                    last_trans_id_ = packet.transaction_id_;
                }
                p2p_statistic->SubmitPeerUploadedBytes(packet.length());
            }
            AppModule::Inst()->DoSendPacket(packet, dest_protocol_version);

            return;
        }
        else
        {
            DOWNLIMITER_DEBUG("DoRequestSubPiece >= 0 size = " << packet.subpiece_infos_.size() << " send_count = " << sent_count_);
            if (sent_count_ < packet_number_per_tick_)
            {
                statistic::P2PDownloaderStatistic::p p2p_statistic = p2p_downloader->GetStatistic();
                if (p2p_statistic)
                {
                    if (packet.transaction_id_ != last_trans_id_)
                    {
                        p2p_statistic->SubmitRequestSubPieceCount(packet.subpiece_infos_.size());
                        last_trans_id_ = packet.transaction_id_;
                        sent_count_ += packet.subpiece_infos_.size();
                    }
                    p2p_statistic->SubmitPeerUploadedBytes(packet.length());
                }

                AppModule::Inst()->DoSendPacket(packet, dest_protocol_version);
            }
            else
            {
                data_queue_.push_back(EndpointPacketInfo(packet, p2p_downloader, dest_protocol_version));
            }
        }
    }

    void DownloadSpeedLimiter::SetSpeedLimitInKBps(uint32_t speed_limit_in_KBps)
    {
        if (!is_running_)
        {
            return;
        }

        if (speed_limit_in_KBps_ == speed_limit_in_KBps)
        {
            return;
        }

        speed_limit_in_KBps_ = speed_limit_in_KBps;
        packet_number_per_tick_ = speed_limit_in_KBps_ * DOWNLIMIT_MIN_INTERVAL_IN_MS / 1000;

        DOWNLIMITER_DEBUG("speed_limit_in_KBps_ = " << speed_limit_in_KBps_);
    }

    boost::int32_t DownloadSpeedLimiter::GetSpeedLimitInKBps() const
    {
        return speed_limit_in_KBps_;
    }

    boost::uint32_t DownloadSpeedLimiter::GetRestCount() const
    {
        if (speed_limit_in_KBps_ < 0)
        {
            return p2sp::P2SPConfigs::P2P_DOWNLOAD_SPEED_LIMIT;
        }
        else
        {
            return std::min(packet_number_per_tick_ - sent_count_ > 0 ? packet_number_per_tick_ - sent_count_ : 0, p2sp::P2SPConfigs::P2P_DOWNLOAD_SPEED_LIMIT);
        }
    }
}