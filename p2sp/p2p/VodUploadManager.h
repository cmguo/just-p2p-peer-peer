#ifndef VOD_UPLOAD_MANAGER_H
#define VOD_UPLOAD_MANAGER_H

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>

#include "storage/IStorage.h"
#include "storage/Storage.h"
#include "p2sp/p2p/UploadStruct.h"
#include "p2sp/p2p/UploadBase.h"

namespace p2sp
{
    class VodUploadManager
        : public boost::enable_shared_from_this<VodUploadManager>
        , public UploadBase
        , public SubPieceLoadListener
    {
        typedef boost::shared_ptr<VodUploadManager> VodUploadManager__p;

    public:
        static VodUploadManager__p create(UploadSpeedLimiter__p upload_speed_limiter)
        {
            return VodUploadManager__p(new VodUploadManager(upload_speed_limiter));
        }

        virtual bool TryHandlePacket(const protocol::Packet & packet);

        virtual bool IsValidPacket(const protocol::Packet & packet);

        virtual void AdjustConnections();

    private:
        VodUploadManager(UploadSpeedLimiter__p upload_speed_limiter)
            : UploadBase(upload_speed_limiter)
        {

        }

        void OnConnectPacket(const protocol::ConnectPacket & packet);
        void OnRequestAnnouncePacket(const protocol::RequestAnnouncePacket & packet);
        void OnRequestSubPiecePacketOld(const protocol::RequestSubPiecePacketOld & packet);
        void OnRequestSubPiecePacket(const protocol::RequestSubPiecePacket & packet);
        void OnRIDInfoRequestPacket(const protocol::RIDInfoRequestPacket & packet);
        void OnReportSpeedPacket(const protocol::ReportSpeedPacket & packet);

        void OnAsyncGetSubPieceSucced(const RID& rid, protocol::SubPieceInfo const& subpiece_info,
            const protocol::Packet & packet, const protocol::SubPieceBuffer & buffer);

        void OnAsyncGetSubPieceFailed(const RID& rid, protocol::SubPieceInfo const& subpiece_info,
            const protocol::Packet & packet);

        void SendAnnouncePacket(const protocol::CommonPeerPacket & packet, storage::Instance::p inst);

        void SendRIDInfoPacket(const protocol::CommonPeerPacket & packet, storage::Instance::p inst);

        bool IsConnectionFull(uint32_t ip_pool_size) const;

        bool IsUploadConnectionFull(boost::asio::ip::udp::endpoint const& end_point);

        void KickAllUploadConnections();

        void KickUploadConnections();
    };
}

#endif
