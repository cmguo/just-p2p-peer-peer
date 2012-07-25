//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "SNConnection.h"
#include "p2sp/p2p/P2PDownloader.h"

namespace p2sp
{
    void SNConnection::Start()
    {
        if (is_running_)
        {
            return;
        }

        if (!p2p_downloader_->GetStatistic())
        {
            assert(false);
            base::util::DoCrash(100);
            return;
        }

        statistic_ = p2p_downloader_->GetStatistic()->AttachPeerConnectionStatistic(endpoint_);

        assert(statistic_);

        is_running_ = true;
        peer_version_ =  protocol::PEER_VERSION;
        window_size_ = 60;
        connected_time_.reset();
        //在不初始化candidate_peer_info_的情况下，会造成候选节点列表里SN节点的endpoint为空
        //观看ikan时，下载下一段会copy之前的候选节点，里面空的candidate_peer_info会导致assert无法通过
        candidate_peer_info_ = protocol::CandidatePeerInfo(endpoint_.address().to_v4().to_ulong(),endpoint_.port(),AppModule::Inst()->GetPeerVersion());
    }

    void SNConnection::Stop()
    {
        if (!is_running_)
        {
            return;
        }

        p2p_downloader_->GetStatistic()->DetachPeerConnectionStatistic(statistic_);
        statistic_.reset();

        if (is_subpiece_requested_)
        {
            protocol::CloseSessionPacket packet(protocol::Packet::NewTransactionID(), 
                peer_version_, endpoint_);
            p2p_downloader_->DoSendPacket(packet, peer_version_);
        }

        is_running_ = false;
    }

    void SNConnection::OnP2PTimer(boost::uint32_t times)
    {
        window_size_ = statistic_->GetSpeedInfo().NowDownloadSpeed / 1000;
        LIMIT_MIN_MAX(window_size_, 20, 60);

        RequestTillFullWindow();

        if (times % 4 == 0)
        {
            RecordStatisticInfo();
        }
    }

    void SNConnection::RequestTillFullWindow(bool need_check)
    {
        curr_time_out_ = statistic_->GetAverageRTT() + p2p_downloader_->GetRTTPlus();
        boost::uint32_t sn_request_number = BootStrapGeneralConfig::Inst()->GetSNRequestNumber();

        while (requesting_count_ < window_size_ && !task_queue_.empty())
        {
            RequestSubPieces(sn_request_number, 2, false);
        }
    }

    void SNConnection::SendPacket(const std::vector<protocol::SubPieceInfo> & subpieces,
        boost::uint32_t copy_count)
    {
        is_subpiece_requested_ = true;

        protocol::RequestSubPiecePacketFromSN packet(protocol::Packet::NewTransactionID(),
            p2p_downloader_->GetRid(), p2p_downloader_->GetOpenServiceFileName(), subpieces,
            endpoint_, protocol::RequestSubPiecePacket::DEFAULT_PRIORITY);

        p2p_downloader_->GetStatistic()->SubmitRequestSubPieceCount(packet.subpiece_infos_.size());

        for (boost::uint32_t i=0; i < copy_count; i++)
        {
            statistic_->SubmitUploadedBytes(packet.length());
            p2p_downloader_->DoSendPacket(packet, peer_version_);
        }

        for (uint32_t i = 0; i < subpieces.size(); ++i)
        {
            p2p_downloader_->AddRequestingSubpiece(subpieces[i], curr_time_out_, this);

            curr_time_out_ += avg_delta_time_;
        }
    }

    void SNConnection::SubmitDownloadedBytes(boost::uint32_t length)
    {
        p2p_downloader_->GetStatistic()->SubmitDownloadedBytes(length);
        p2p_downloader_->GetStatistic()->SubmitSnDownloadedBytes(length);
    }

    void SNConnection::SubmitP2PDataBytesWithoutRedundance(boost::uint32_t length)
    {
        p2p_downloader_->GetStatistic()->SubmitP2PSnDataBytesWithoutRedundance(length);
    }

    void SNConnection::SubmitP2PDataBytesWithRedundance(boost::uint32_t length)
    {
        p2p_downloader_->GetStatistic()->SubmitP2PSnDataBytesWithRedundance(length);
    }

    boost::uint32_t SNConnection::GetConnectRTT() const
    {
        return 0;
    }
}