//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _DOWNLOAD_SPEED_LIMITER_H_
#define _DOWNLOAD_SPEED_LIMITER_H_

namespace p2sp
{
    class P2PDownloader;
    typedef boost::shared_ptr<P2PDownloader> P2PDownloader__p;

    class DownloadSpeedLimiter
    {
    public:
        DownloadSpeedLimiter(boost::uint32_t max_data_queue_length, boost::uint32_t packet_life_limit_in_ms);
        ~DownloadSpeedLimiter();

        void DoRequestSubPiece(P2PDownloader__p p2p_downloader, protocol::RequestSubPiecePacket const & packet
            , boost::uint16_t dest_protocol_version);
        void DoRequestSubPiece(P2PDownloader__p p2p_downloader, protocol::RequestSubPiecePacketOld const & packet
            , boost::uint16_t dest_protocol_version);

        void SetSpeedLimitInKBps(boost::uint32_t speed_limit_in_KBps);

        boost::int32_t GetSpeedLimitInKBps() const;

        bool IsDataQueueFull() const;

        void Stop();

        boost::uint32_t GetRestCount() const;

    private:

        void OnTimerElapsed(framework::timer::Timer * pointer);

    private:

        struct EndpointPacketInfo
        {
            framework::timer::TickCounter life_time_;
            P2PDownloader__p p2p_downloader_;
            protocol::RequestSubPiecePacket subpiece_packet_;
            protocol::RequestSubPiecePacketOld subpiece_packet_old_;
            bool is_old_packet;
            boost::uint16_t dest_protocol_version_;

            boost::uint16_t piece_count;
            boost::uint32_t trans_id;
            boost::uint32_t packet_len;

            EndpointPacketInfo(protocol::RequestSubPiecePacket subpiece_packet, P2PDownloader__p p2p_downloader,
                boost::uint16_t dest_protocol_version)
                : subpiece_packet_(subpiece_packet), p2p_downloader_(p2p_downloader)
                , dest_protocol_version_(dest_protocol_version)
            {
                life_time_.reset();
                piece_count = subpiece_packet_.subpiece_infos_.size();
                trans_id = subpiece_packet_.transaction_id_;
                packet_len = subpiece_packet_.length();
                is_old_packet = false;
            }

            EndpointPacketInfo(protocol::RequestSubPiecePacketOld subpiece_packet, P2PDownloader__p p2p_downloader,
                boost::uint16_t dest_protocol_version)
                : subpiece_packet_old_(subpiece_packet), p2p_downloader_(p2p_downloader)
                , dest_protocol_version_(dest_protocol_version)
            {
                life_time_.reset();
                piece_count = 1;
                trans_id = subpiece_packet_old_.transaction_id_;
                packet_len = subpiece_packet_old_.length();
                is_old_packet = true;
            }

            EndpointPacketInfo(const EndpointPacketInfo& endpoint_packet_info)
            {
                if (&endpoint_packet_info != this)
                {
                    trans_id = endpoint_packet_info.trans_id;
                    piece_count = endpoint_packet_info.piece_count;
                    packet_len = endpoint_packet_info.packet_len;

                    is_old_packet = endpoint_packet_info.is_old_packet;
                    dest_protocol_version_ = endpoint_packet_info.dest_protocol_version_;

                    life_time_ = endpoint_packet_info.life_time_;
                    p2p_downloader_ = endpoint_packet_info.p2p_downloader_;
                    subpiece_packet_ = endpoint_packet_info.subpiece_packet_;
                    subpiece_packet_old_ = endpoint_packet_info.subpiece_packet_old_;
                }
            };

            inline protocol::Packet GetPacket()
            {
                if (!is_old_packet)
                {
                    return subpiece_packet_;
                }
                else
                {
                    return subpiece_packet_old_;
                }
            }
        };

    private:

        volatile bool is_running_;

        uint32_t max_data_queue_length_;

        uint32_t packet_life_limit_in_ms_;

        boost::int32_t speed_limit_in_KBps_;

        uint32_t tick_interval_in_ms_;

        uint32_t packet_number_per_tick_;

        uint32_t packet_interval_in_ms_;

        uint32_t last_trans_id_;

        uint32_t sent_count_;

        framework::timer::PeriodicTimer tick_timer_;

        std::list<EndpointPacketInfo> data_queue_;

    };
}

#endif  // P2SP_DOWNLOAD_SPEED_LIMITER_H
