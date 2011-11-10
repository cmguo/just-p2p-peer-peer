//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "p2sp/AppModule.h"
#include "p2sp/p2p/UploadSpeedLimiter.h"
#include "statistic/DACStatisticModule.h"

#include <boost/date_time.hpp>

#define UPLIMITER_DEBUG(msg) LOG(__DEBUG, "uplimiter", __FUNCTION__ << " " << msg)

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("uploadlimiter");

    UploadSpeedLimiter::UploadSpeedLimiter()
        : speed_limit_in_KBps_(-1)
        , tick_timer_(global_250ms_timer(), 250, boost::bind(&UploadSpeedLimiter::OnTimerElapsed, this, &tick_timer_))
    {
        tick_timer_.start();
    }

    UploadSpeedLimiter::~UploadSpeedLimiter()
    {
        tick_timer_.stop();
        data_queue_.clear();
    }

    void UploadSpeedLimiter::OnTimerElapsed(framework::timer::Timer * pointer)
    {
        if (pointer == &tick_timer_)
        {
            framework::timer::TickCounter counter_;

            LOG(__DEBUG, "upload", boost::posix_time::to_simple_string(boost::posix_time::microsec_clock::local_time())
                << ", packet_number_per_tick_ = " << packet_number_per_tick_
                << ", data_queue_.size()=" << data_queue_.size());
            std::multiset<EndpointPacketInfo>::iterator iter = data_queue_.begin();
            sent_count_ = 0;
            while (iter != data_queue_.end())
            {
                const EndpointPacketInfo & data = *iter;
                if (data.IsTimeOut())
                {
                    statistic::DACStatisticModule::Inst()->SubmitP2PUploadDisCardBytes(data.packet_->GetPacketSize());
                    data_queue_.erase(iter++);
                }
                else if (sent_count_ <= packet_number_per_tick_)
                {
                    data.packet_->SendPacket(data.dest_protocol_version_);
                    ++sent_count_;
                    data_queue_.erase(iter++);
                }
                else
                {
                    ++iter;
                }
            }
        }
    }

    void UploadSpeedLimiter::SendPacket(const protocol::SubPiecePacket & packet, bool ignoreUploadSpeedLimit,
        uint16_t priority, uint16_t dest_protocol_version)
    {
        DoSendPacket(boost::shared_ptr<VodPacket>(new VodPacket(packet)), ignoreUploadSpeedLimit, priority, dest_protocol_version);
    }

    void UploadSpeedLimiter::SendPacket(const protocol::LiveSubPiecePacket & packet, bool ignoreUploadSpeedLimit,
        uint16_t priority, uint16_t dest_protocol_version)
    {
        DoSendPacket(boost::shared_ptr<LivePacket>(new LivePacket(packet)), ignoreUploadSpeedLimit, priority, dest_protocol_version);
    }

    void UploadSpeedLimiter::SendPacket(const protocol::TcpSubPieceResponsePacket & packet, bool ignoreUploadSpeedLimit,
        uint16_t priority, uint16_t dest_protocol_viersion)
    {
        DoSendPacket(boost::shared_ptr<TcpPacket>(new TcpPacket(packet)), ignoreUploadSpeedLimit, priority, dest_protocol_viersion);
    }

    void UploadSpeedLimiter::DoSendPacket(boost::shared_ptr<PacketBase> packet, bool ignoreUploadSpeedLimit,
        uint16_t priority, uint16_t dest_protocol_version)
    {
        // 不限速
        if (ignoreUploadSpeedLimit || GetSpeedLimitInKBps() < 0)
        {
            packet->SendPacket(dest_protocol_version);
            LOG(__DEBUG, "upload", "DoSendSubPiecePacket GetSpeedLimitInKBps() < 0");
            return;
        }

        // 限速为0
        if (GetSpeedLimitInKBps() == 0)
        {
            LOG(__DEBUG, "upload", "DoSendSubPiecePacket GetSpeedLimitInKBps() == 0");
            return;
        }

        // 队列满时删除优先级最低的包
        if (data_queue_.size() >= UPLIMIT_MAX_DATA_QUEUE_LENGTH)
        {
            data_queue_.insert(EndpointPacketInfo(packet, priority, dest_protocol_version));
            const EndpointPacketInfo & data = *(data_queue_.rbegin());
            statistic::DACStatisticModule::Inst()->SubmitP2PUploadDisCardBytes(data.packet_->GetPacketSize());
            data_queue_.erase(--data_queue_.end());
            return;
        }

        if (sent_count_ <= packet_number_per_tick_)
        {
            packet->SendPacket(dest_protocol_version);
            LOG(__DEBUG, "upload", "DoSendSubPiecePacket packet sended");
            sent_count_++;
        }
        else
        {
            data_queue_.insert(EndpointPacketInfo(packet, priority, dest_protocol_version));
            LOG(__DEBUG, "upload", "DoSendSubPiecePacket packet inserted to queue");
        }
    }

    void UploadSpeedLimiter::SetSpeedLimitInKBps(uint32_t speed_limit_in_KBps)
    {
        if (speed_limit_in_KBps_ == speed_limit_in_KBps)
        {
            return;
        }

        speed_limit_in_KBps_ = speed_limit_in_KBps;

        packet_number_per_tick_ = speed_limit_in_KBps_ / 4;

        UPLIMITER_DEBUG("speed_limit_in_KBps_=" << speed_limit_in_KBps_ << " packet_number_per_tick_=" << packet_number_per_tick_);
    }

    uint32_t UploadSpeedLimiter::GetSpeedLimitInKBps() const
    {
        return speed_limit_in_KBps_;
    }
}
