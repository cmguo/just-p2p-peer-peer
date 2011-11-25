#ifndef UPLOAD_BASE_H
#define UPLOAD_BASE_H

#include "p2sp/p2p/UploadSpeedLimiter.h"
#include "p2sp/p2p/UdpBasedUploadConnectionsManagement.h"

namespace p2sp
{
    class UploadSpeedLimiter;
    typedef boost::shared_ptr<UploadSpeedLimiter> UploadSpeedLimiter__p;

    struct SubPieceLoadListener
    {
        typedef boost::shared_ptr<SubPieceLoadListener> p;

        virtual void OnAsyncGetSubPieceSucced(const RID& rid, protocol::SubPieceInfo const& subpiece_info,
            const protocol::Packet & packet, const protocol::SubPieceBuffer & buffer) = 0;

        virtual void OnAsyncGetSubPieceFailed(const RID& rid, protocol::SubPieceInfo const& subpiece_info,
            const protocol::Packet & packet) = 0;

        virtual ~SubPieceLoadListener() {}
    };

    class IUploadManager
    {
    public:
        IUploadManager(UploadSpeedLimiter__p upload_speed_limiter)
            : upload_speed_limiter_(upload_speed_limiter)
        {

        }

        virtual bool TryHandlePacket(const protocol::Packet & packet) = 0;
        virtual boost::uint32_t MeasureCurrentUploadSpeed() const = 0;
        virtual void AdjustConnections() = 0;
        virtual void GetUploadingPeers(std::set<boost::asio::ip::address> & uploading_peers) = 0;
    protected:
        UploadSpeedLimiter__p upload_speed_limiter_;
    };

    class UploadBase
        : public IUploadManager
    {
    protected:
        UploadBase(UploadSpeedLimiter__p upload_speed_limiter)
            : IUploadManager(upload_speed_limiter)
        {
        }

        virtual boost::uint32_t MeasureCurrentUploadSpeed() const
        {
            return connections_management_.GetCurrentUploadSpeed();
        }

        virtual void GetUploadingPeers(std::set<boost::asio::ip::address> & uploading_peers)
        {
            connections_management_.GetUploadingPeers(uploading_peers);
        }

        void SendErrorPacket(const protocol::CommonPeerPacket & packet, boost::uint16_t error_code)
        {
            protocol::ErrorPacket error_packet(packet);
            error_packet.peer_guid_ = AppModule::Inst()->GetPeerGuid();
            error_packet.error_code_ =  error_code;
            AppModule::Inst()->DoSendPacket(error_packet, packet.protocol_version_);
        }

    protected:
        UdpBasedUploadConnectionsManagement connections_management_;
    };
}

#endif