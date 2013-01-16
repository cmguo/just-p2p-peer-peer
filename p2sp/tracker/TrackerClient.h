//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#pragma once

#include "p2sp/AppModule.h"
#include <protocol/TrackerPacket.h>
#include <util/protocol/http/HttpClient.h>

namespace p2sp
{
    class TrackerClient
        : public boost::noncopyable
        , public boost::enable_shared_from_this<TrackerClient>
#ifdef DUMP_OBJECT
        , public count_object_allocate<TrackerClient>
#endif
    {
    public:

        typedef boost::shared_ptr<TrackerClient> p;

        static p Create(boost::asio::ip::udp::endpoint end_point, bool is_vod, const protocol::TRACKER_INFO& tracker_info)
        {
            assert(!(is_vod && tracker_info.IsTrackerForLiveUdpServer()));
            return p(new TrackerClient(end_point, is_vod, tracker_info));
        }

    public:

        void Start();

        void Stop();

        void DoList(const RID& rid, bool list_for_live_udpserver);

        void OnListResponsePacket(protocol::ListPacket const & packet);

        void OnReportResponsePacket(protocol::ReportPacket const & packet);

        /**
         * @brief 返回值为TransationID
         */
        boost::uint32_t DoSubmit();

        void SetRidCount(boost::uint32_t rid_count);

        void PPLeave();

        bool IsSync() const { return is_sync_; }

    public:

        const protocol::TRACKER_INFO& GetTrackerInfo() const;

        void SetGroupCount(boost::uint32_t group_count);

        boost::uint32_t GetGroupCount() const;

        std::set<RID> GetClientResource() const;

        boost::uint32_t DoReport();

        bool IsTrackerForLiveUdpServer() const;

        void HttpTrackerList(string const & server, RID const & rid);

    private:

        void UpdateIpStatistic(const protocol::SocketAddr& detected_addr);
        void HandleFetchResult(boost::shared_ptr<util::protocol::HttpClient> http_client, 
            const boost::system::error_code & err);
        std::string Hex2Bin(string const & hex);
        void UpdatePeerInfo(std::vector<protocol::CandidatePeerInfo> & peer_infos_);

    private:
        TrackerClient() {}
        TrackerClient(boost::asio::ip::udp::endpoint end_point, bool is_vod, 
            const protocol::TRACKER_INFO& tracker_info)
            : end_point_(end_point)
            , is_vod_(is_vod)
            , tracker_info_(tracker_info)
            , is_tracker_for_live_udpserver_(tracker_info.IsTrackerForLiveUdpServer())
        {}

    private:

        boost::asio::ip::udp::endpoint end_point_;

        /**
         * @brief 记录上次收到的Tracker返回的上报资源情况
         */
        boost::uint32_t last_response_rid_count_;

        /**
         * @brief
         */
        boost::uint32_t last_transaction_id_;

        /**
         * @brief 本地认为的服务器资源集合
         */
        std::set<RID> local_resources_;
        bool is_sync_;

        /**
         * @brief Tracker Info
         */
        protocol::TRACKER_INFO tracker_info_;

        boost::uint32_t group_count_;

        /**
         * @brief 上次report的内容
         */
        std::vector<protocol::REPORT_RESOURCE_STRUCT> last_updates_;

        bool is_vod_;

        bool is_tracker_for_live_udpserver_;

    private:
        /**
         *
         */
        static const boost::uint16_t MAX_REQUEST_PEER_COUNT_ = 50;

        static const boost::uint32_t MAX_UINT = 0xFFFFFFFFU;

        /**
         * 每次最多Report的RID数
         */
        static const boost::uint32_t MAX_REPORT_RID_COUNT = 50;
    };
}
