#ifndef LIVE_UPLOAD_MANAGER_H
#define LIVE_UPLOAD_MANAGER_H

#include "statistic/SpeedInfoStatistic.h"
#include "p2sp/p2p/UploadStruct.h"
#include "p2sp/p2p/UploadBase.h"

namespace p2sp
{
    class UploadSpeedLimiter;
    typedef boost::shared_ptr<UploadSpeedLimiter> UploadSpeedLimiter__p;

    class LiveUploadManager
        : public UploadBase
    {
        typedef boost::shared_ptr<LiveUploadManager> LiveUploadManager__p;
    public:
        static LiveUploadManager__p create(UploadSpeedLimiter__p upload_speed_limiter)
        {
            return LiveUploadManager__p(new LiveUploadManager(upload_speed_limiter));
        }

        virtual bool TryHandlePacket(const protocol::Packet & packet);

        virtual bool IsValidPacket(const protocol::Packet & packet);

        virtual void AdjustConnections();

        static const size_t DesirableUploadSpeedPerPeerInKBps;

    private:
        LiveUploadManager(UploadSpeedLimiter__p upload_speed_limiter)
            : UploadBase(upload_speed_limiter)
        {

        }

        void OnConnectPacket(const protocol::ConnectPacket & packet);
        void OnLiveRequestAnnouncePacket(protocol::LiveRequestAnnouncePacket const & packet);
        void OnLiveRequestSubPiecePacket(protocol::LiveRequestSubPiecePacket const & packet);
    };
}

#endif
