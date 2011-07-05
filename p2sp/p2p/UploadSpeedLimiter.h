//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef P2SP_UPLOAD_SPEED_LIMITER_H
#define P2SP_UPLOAD_SPEED_LIMITER_H

#include "p2sp/AppModule.h"

namespace p2sp
{
    class PacketBase
    {
    public:
        virtual void SendPacket(boost::uint16_t dest_protocol_version) = 0;
        virtual size_t GetPacketSize() const = 0;
        virtual ~PacketBase(){}
    };

    class VodPacket : public PacketBase
    {
    private:
        protocol::SubPiecePacket subpiece_packet_;
    public:
        VodPacket(const protocol::SubPiecePacket & packet) : subpiece_packet_(packet)
        {
        }
        void SendPacket(boost::uint16_t dest_protocol_version)
        {
            AppModule::Inst()->DoSendPacket(subpiece_packet_, dest_protocol_version);
        }
        size_t GetPacketSize() const
        {
            return SUB_PIECE_SIZE;
        }
    };

    class LivePacket : public PacketBase
    {
    private:
        protocol::LiveSubPiecePacket live_subpiece_packet_;
    public:
        LivePacket(const protocol::LiveSubPiecePacket & packet) : live_subpiece_packet_(packet)
        {
        }
        void SendPacket(boost::uint16_t dest_protocol_version)
        {
            AppModule::Inst()->DoSendPacket(live_subpiece_packet_, dest_protocol_version);
        }
        size_t GetPacketSize() const
        {
            return LIVE_SUB_PIECE_SIZE;
        }
    };

    class UploadSpeedLimiter
    {
    public:
        UploadSpeedLimiter();
        ~UploadSpeedLimiter();
        void SendPacket(const protocol::SubPiecePacket & packet, bool ignoreUploadSpeedLimit,
            uint16_t priority, uint16_t dest_protocol_version);

        void SendPacket(const protocol::LiveSubPiecePacket & packet, bool ignoreUploadSpeedLimit,
            uint16_t priority, uint16_t dest_protocol_version);

        void SetSpeedLimitInKBps(uint32_t speed_limit_in_KBps);
        uint32_t GetSpeedLimitInKBps() const;

    private:
        void OnTimerElapsed(framework::timer::Timer * pointer);
        void DoSendPacket(boost::shared_ptr<PacketBase> packet, bool ignoreUploadSpeedLimit,
            uint16_t priority, uint16_t dest_protocol_version);
        struct EndpointPacketInfo
        {
            framework::timer::TickCounter life_time_;
            boost::shared_ptr<PacketBase> packet_;
            boost::uint16_t priority_;
            boost::uint16_t dest_protocol_version_;

            EndpointPacketInfo(boost::shared_ptr<PacketBase> packet, boost::uint16_t priority,
                boost::uint16_t dest_protocol_version)
                : packet_(packet), priority_(priority), dest_protocol_version_(dest_protocol_version)
            {
                life_time_.reset();
            }

            EndpointPacketInfo(const EndpointPacketInfo& endpoint_packet_info)
            {
                if (&endpoint_packet_info != this)
                {
                    life_time_ = endpoint_packet_info.life_time_;
                    packet_ = endpoint_packet_info.packet_;
                    priority_ = endpoint_packet_info.priority_;
                    dest_protocol_version_ = endpoint_packet_info.dest_protocol_version_;
                }
            }

            bool IsTimeOut() const
            {
                return life_time_.elapsed() > UPLIMIT_PACKET_LIFE_LIMIT_IN_MS;
            }

            friend bool operator < (const EndpointPacketInfo & epi1, const EndpointPacketInfo & epi2)
            {
                return epi1.priority_ < epi2.priority_;
            }
        };

    private:
        uint32_t speed_limit_in_KBps_;
        uint32_t packet_number_per_tick_;
        uint32_t sent_count_;
        framework::timer::PeriodicTimer tick_timer_;
        std::multiset<EndpointPacketInfo> data_queue_;

        const static uint32_t UPLIMIT_MAX_DATA_QUEUE_LENGTH = 400;
        const static uint32_t UPLIMIT_PACKET_LIFE_LIMIT_IN_MS = 2750;
    };
}

#endif
