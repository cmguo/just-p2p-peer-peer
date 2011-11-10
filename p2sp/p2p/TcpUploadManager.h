#ifndef TCP_UPLOAD_MANAGER_H
#define TCP_UPLOAD_MANAGER_H

#include "p2sp/p2p/UploadStruct.h"
#include "p2sp/p2p/UploadBase.h"

namespace storage
{
    class Instance;
    typedef boost::shared_ptr<Instance> Instance__p;
}

namespace p2sp
{
    class UploadSpeedLimiter;
    typedef boost::shared_ptr<UploadSpeedLimiter> UploadSpeedLimiter__p;

    class TcpUploadManager
        : public IUploadManager
        , public boost::enable_shared_from_this<TcpUploadManager>
        , public SubPieceLoadListener
    {
        typedef boost::shared_ptr<TcpUploadManager> TcpUploadManager__p;
    public:
        static TcpUploadManager__p create(UploadSpeedLimiter__p upload_speed_limiter)
        {
            return TcpUploadManager__p(new TcpUploadManager(upload_speed_limiter));
        }

        virtual bool TryHandlePacket(const protocol::Packet & packet);
        virtual boost::uint32_t MeasureCurrentUploadSpeed() const;
        virtual bool IsValidPacket(const protocol::Packet & packet);
        virtual void AdjustConnections();
        virtual void GetUploadingPeers(std::set<boost::asio::ip::address> & uploading_peers);

        virtual void OnAsyncGetSubPieceSucced(const RID& rid, protocol::SubPieceInfo const& subpiece_info,
            const protocol::Packet & packet, const protocol::SubPieceBuffer & buffer);

        virtual void OnAsyncGetSubPieceFailed(const RID& rid, protocol::SubPieceInfo const& subpiece_info,
            const protocol::Packet & packet);

    private:
        TcpUploadManager(UploadSpeedLimiter__p upload_speed_limiter)
            : IUploadManager(upload_speed_limiter)
        {

        }

        void OnTcpAnnounceRequestPacket(const protocol::TcpAnnounceRequestPacket & packet);
        void OnTcpSubPieceRequestPacket(const protocol::TcpSubPieceRequestPacket & packet);
        void OnTcpReportSpeedPacket(const protocol::TcpReportSpeedPacket & packet);
        void OnTcpConnectionClose(const protocol::TcpClosePacket & packet);

        void SendErrorPacket(const protocol::TcpCommonPacket & packet, boost::uint16_t error_code);
        void SendAnnouncePacket(const protocol::TcpCommonPacket & packet, storage::Instance__p inst);

    private:
        std::map<boost::shared_ptr<network::TcpConnection>, boost::uint32_t> tcp_speed_map_;
    };
}

#endif