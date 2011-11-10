#ifndef UPLOAD_BASE_H
#define UPLOAD_BASE_H

#include "p2sp/p2p/UploadSpeedLimiter.h"
#include "p2sp/p2p/PeerHelper.h"
#include "p2sp/p2p/UploadStruct.h"

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
        // TODO: 后面有了TCP之后，这个需要改成ip::endpoint
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
            boost::uint32_t current_upload_speed = 0;
            std::map<boost::asio::ip::udp::endpoint, std::pair<boost::uint32_t, framework::timer::TickCounter> >::const_iterator iter = uploading_peers_speed_.begin();
            for (; iter != uploading_peers_speed_.end(); ++iter)
            {
                if (iter->second.second.elapsed() < 5000)
                {
                    if (!IsPeerFromSameSubnet(iter->first))
                    {
                        current_upload_speed += iter->second.first;
                    }
                }
            }

            return current_upload_speed;
        }

        virtual void GetUploadingPeers(std::set<boost::asio::ip::address> & uploading_peers)
        {
            for (std::set<boost::asio::ip::udp::endpoint>::iterator iter = accept_uploading_peers_.begin();
                iter != accept_uploading_peers_.end(); ++iter)
            {
                uploading_peers.insert(iter->address());
            }
        }

        void SendErrorPacket(const protocol::CommonPeerPacket & packet, boost::uint16_t error_code)
        {
            protocol::ErrorPacket error_packet(packet);
            error_packet.peer_guid_ = AppModule::Inst()->GetPeerGuid();
            error_packet.error_code_ =  error_code;
            AppModule::Inst()->DoSendPacket(error_packet, packet.protocol_version_);
        }

        bool IsPeerFromSameSubnet(const boost::asio::ip::udp::endpoint & peer_endpoint) const
        {
            std::map<boost::asio::ip::udp::endpoint, PEER_UPLOAD_INFO>::const_iterator peer_upload_info_iter = 
                accept_connecting_peers_.find(peer_endpoint);

            if (peer_upload_info_iter != accept_connecting_peers_.end())
            {
                return PeerHelper::IsPeerFromSameSubnet(peer_upload_info_iter->second.peer_info);
            }

            return false;
        }

        void KickTimeoutConnection()
        {
            for (std::map<boost::asio::ip::udp::endpoint, PEER_UPLOAD_INFO>::iterator 
                iter = accept_connecting_peers_.begin(); iter != accept_connecting_peers_.end();)
            {
                if (iter->second.last_talk_time.elapsed() >= 10000)
                {
                    accept_uploading_peers_.erase(iter->first);
                    uploading_peers_speed_.erase(iter->first);
                    accept_connecting_peers_.erase(iter++);
                }
                else
                {
                    ++iter;
                }
            }
        }

    protected:
        std::map<boost::asio::ip::udp::endpoint, PEER_UPLOAD_INFO> accept_connecting_peers_;
        std::set<boost::asio::ip::udp::endpoint> accept_uploading_peers_;
        std::map<boost::asio::ip::udp::endpoint, std::pair<boost::uint32_t, framework::timer::TickCounter> > uploading_peers_speed_;
    };
}

#endif