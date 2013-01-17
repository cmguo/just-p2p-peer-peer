//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// PeerConnection.h

#ifndef _P2SP_P2P_PEER_CONNECTION_H_
#define _P2SP_P2P_PEER_CONNECTION_H_

#include "p2sp/p2p/P2PModule.h"
#include "p2sp/p2p/P2SPConfigs.h"

#include "p2sp/p2p/ConnectionBase.h"

#include <protocol/PeerPacket.h>
namespace p2sp
{
    class P2PDownloader;
    typedef boost::shared_ptr<P2PDownloader> P2PDownloader__p;
    class SubPieceRequestManager;
    typedef boost::shared_ptr<SubPieceRequestManager> SubPieceRequestManager__p;

    class PeerConnection
        : public boost::noncopyable
        , public ConnectionBase
#ifdef DUMP_OBJECT
        , public count_object_allocate<PeerConnection>
#endif
    {
    public:
        PeerConnection(P2PDownloader__p p2p_downloader,
            const boost::asio::ip::udp::endpoint & end_point)
            : ConnectionBase(p2p_downloader, end_point)
            , is_rid_info_valid_(false)
        {
        }

    public:
        // 启停
        void Start(protocol::ConnectPacket const & reconnect_packet, const boost::asio::ip::udp::endpoint &end_point, const protocol::CandidatePeerInfo& peer_info);
        virtual void Stop();

        // 操作
        void RequestTillFullWindow(bool need_check = false);
        
        bool CheckRidInfo(const protocol::RidInfo& rid_info);
        virtual bool IsRidInfoValid() const;
        virtual bool HasRidInfo() const;

        virtual void KeepAlive();
        virtual void UpdateConnectTime();

        // 消息
        virtual void OnP2PTimer(boost::uint32_t times);
        void OnAnnounce(protocol::AnnouncePacket const & packet);
        void OnRIDInfoResponse(protocol::RIDInfoResponsePacket const & packet);

        // 属性
        boost::uint32_t GetUsedTime();

        boost::uint32_t GetWindowSize() const;
        boost::uint32_t GetLongestRtt() const;
        boost::uint32_t GetAvgDeltaTime() const;

        bool HasSubPiece(const protocol::SubPieceInfo& subpiece_info);
        const protocol::CandidatePeerInfo & GetCandidatePeerInfo() const{ return candidate_peer_info_;}
        virtual bool LongTimeNoSee() {return P2SPConfigs::PEERCONNECTION_NORESPONSE_KICK_TIME_IN_MILLISEC < last_live_response_time_.elapsed();};
        bool CanKick();

        virtual boost::uint32_t GetConnectRTT() const;

        virtual bool HasBlock(boost::uint32_t block_index);
        virtual bool IsBlockFull();

        virtual void SendPacket(const std::vector<protocol::SubPieceInfo> & subpieces,
            boost::uint32_t copy_count);

        virtual void SubmitDownloadedBytes(boost::uint32_t length);

        virtual void SubmitP2PDataBytesWithoutRedundance(boost::uint32_t length);
        virtual void SubmitP2PDataBytesWithRedundance(boost::uint32_t length);

    private:
        void RequestSubPiece(const protocol::SubPieceInfo& subpiece_info, bool need_check = false);

        void DoAnnounce();
        void DoRequestRIDInfo();
        void DoReportDownloadSpeed();

        bool CanRequest() const;

    private:
        boost::uint32_t curr_delta_size_;

        // Peer对方的相关变量
        boost::uint32_t connect_rtt_;
        boost::uint32_t rtt_;
        
        protocol::PEER_DOWNLOAD_INFO peer_download_info_;
        protocol::BlockMap block_map_;
        boost::asio::ip::udp::endpoint end_point_;
        Guid peer_guid_;
        protocol::RidInfo rid_info_;
        bool is_rid_info_valid_;
    };

    inline bool PeerConnection::CanRequest() const
    {
        return requesting_count_ < window_size_;
    }

    inline bool PeerConnection::CheckRidInfo(const protocol::RidInfo& rid_info)
    {
        if (false == is_running_)
            return false;

        // 兼容老Peer
        if (peer_version_ < 0x00000007)
            return true;

        is_rid_info_valid_ = (rid_info_ == rid_info);
        return is_rid_info_valid_;
    }

    inline bool PeerConnection::HasRidInfo() const
    {
        if (false == is_running_)
            return false;

        if (peer_version_ < 0x00000007)
            return true;

        if (BootStrapGeneralConfig::Inst()->OpenRIDInfoRequestResponse())
        {
            return false == rid_info_.GetRID().is_empty();
        }
        else
        {
            return true;
        }
    }

    inline bool PeerConnection::IsRidInfoValid() const
    {
        if (false == is_running_)
            return false;

        if (peer_version_ < 0x00000007)
            return true;
        // Modified by jeffrey 支持ppbox发给内核的请求不带block_md5
#ifndef PEER_PC_CLIENT
        return true;
#endif
        if (BootStrapGeneralConfig::Inst()->OpenRIDInfoRequestResponse())
        {
            return is_rid_info_valid_;
        }
        else
        {
            return true;
        }
    }

    inline bool PeerConnection::HasSubPiece(const protocol::SubPieceInfo & subpiece_info)
    {
        if (is_running_ == false)
            return false;
        return block_map_.HasBlock(subpiece_info.block_index_);
    }

    inline void PeerConnection::OnAnnounce(protocol::AnnouncePacket const & packet)
    {
        if (is_running_ == false)
            return;

        last_live_response_time_.reset();
        statistic_->SubmitDownloadedBytes(sizeof(protocol::AnnouncePacket));
        //statistic_->SetBitmap(packet.block_map_);
        block_map_ = packet.block_map_;
        peer_download_info_ = packet.peer_download_info_;
        end_point_ = packet.end_point;
    }

}

#endif  // _P2SP_P2P_PEER_CONNECTION_H_