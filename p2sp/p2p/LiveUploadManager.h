#ifndef LIVE_UPLOAD_MANAGER_H
#define LIVE_UPLOAD_MANAGER_H

#include "statistic/SpeedInfoStatistic.h"
#include "p2sp/p2p/UploadStruct.h"
#include "p2sp/p2p/UploadBase.h"
#include "p2sp/bootstrap/BootStrapGeneralConfig.h"
#include "storage/Storage.h"

namespace p2sp
{
    class UploadSpeedLimiter;
    typedef boost::shared_ptr<UploadSpeedLimiter> UploadSpeedLimiter__p;

    class LiveUploadManager
        : public UploadBase
        , public ConfigUpdateListener
        , public boost::enable_shared_from_this<LiveUploadManager>
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

        virtual void OnConfigUpdated();

        void Start();
        void Stop();

        void GetUploadingPeersExcludeSameSubnet(std::set<boost::asio::ip::address> & uploading_peers) const;

    private:
        LiveUploadManager(UploadSpeedLimiter__p upload_speed_limiter)
            : UploadBase(upload_speed_limiter)
            , timer_(global_second_timer(), BootStrapGeneralConfig::Inst()->GetSendPeerInfoPacketIntervalInSecond() * 1000, boost::bind(&LiveUploadManager::OnTimerElapsed, this, &timer_))
        {

        }

        void OnConnectPacket(const protocol::ConnectPacket & packet);
        void OnLiveRequestAnnouncePacket(protocol::LiveRequestAnnouncePacket const & packet);
        void OnLiveRequestSubPiecePacket(protocol::LiveRequestSubPiecePacket const & packet);
        void OnPeerInfoPacket(protocol::PeerInfoPacket const & packet);
        void OnCloseSessionPacket(protocol::CloseSessionPacket const & packet);

        void OnTimerElapsed(framework::timer::Timer * pointer);
        void SendPeerInfo();

        bool ShouldAddPeerAsDownloadCandidate(const protocol::LiveAnnounceMap& announce_map_for_peer, storage::LiveInstance::p live_instance) const;

    private:
        framework::timer::PeriodicTimer timer_;
    };
}

#endif
