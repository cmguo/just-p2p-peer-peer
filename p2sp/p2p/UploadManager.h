//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// UploadManager.h

#ifndef _P2SP_P2P_UPLOAD_MANAGER_H_
#define _P2SP_P2P_UPLOAD_MANAGER_H_

/********************************************************************************
//
// Filename: UploadManager.h
// Comment: 从Storage获取资源
//
********************************************************************************/

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>

#include "storage/IStorage.h"
#include "storage/StatisticTools.h"
#include "storage/Performance.h"
#include "p2sp/p2p/UploadSpeedLimiter.h"
#include "p2sp/p2p/UploadSpeedLimitTracker.h"
#include "p2sp/bootstrap/BootStrapGeneralConfig.h"
#include "statistic/SpeedInfoStatistic.h"

namespace p2sp
{
    struct PEER_UPLOAD_INFO
    {
        static const uint32_t TRANS_ID_SIZE = 5;
        //
        framework::timer::TickCounter last_talk_time;
        framework::timer::TickCounter last_data_time;
        uint32_t ip_pool_size;
        // uint32_t last_data_trans_id;
        uint32_t last_data_trans_ids[TRANS_ID_SIZE];
        Guid peer_guid;
        bool is_open_service;
        RID resource_id;
        protocol::CandidatePeerInfo peer_info;
        bool is_live;
        statistic::SpeedInfoStatistic speed_info;
        framework::timer::TickCounter connected_time;

        PEER_UPLOAD_INFO()
            : last_talk_time(0)
            , last_data_time(0)
            , ip_pool_size(0)
            , is_open_service(false)
            , is_live(false)
        {
            memset(last_data_trans_ids, 0, sizeof(last_data_trans_ids));
        }

        bool IsInLastDataTransIDs(uint32_t trans_id)
        {
            for (uint32_t i = 0; i < TRANS_ID_SIZE; ++i)
            {
                if (last_data_trans_ids[i] == trans_id)
                {
                    return true;
                }
            }
            return false;
        }

        bool UpdateLastDataTransID(uint32_t trans_id)
        {
            uint32_t min_pos = 0;
            for (uint32_t i = 0; i < TRANS_ID_SIZE; ++i)
            {
                if (last_data_trans_ids[i] == trans_id)
                {
                    return false;
                }
                if (last_data_trans_ids[i] < last_data_trans_ids[min_pos])
                {
                    min_pos = i;
                }
            }
            // check
            if (last_data_trans_ids[min_pos] < trans_id)
            {
                last_data_trans_ids[min_pos] = trans_id;
                return true;
            }
            return false;
        }
    };

    struct RBIndex
    {
        RID rid;
        uint32_t block_index;

        bool operator == (const RBIndex& other) const
        {
            return block_index == other.block_index && rid == other.rid;
        }
        bool operator<(const RBIndex& other) const
        {
            if (rid != other.rid)
            {
                return rid < other.rid;
            }
            return block_index < other.block_index;
        }
        bool operator <= (const RBIndex& other) const
        {
            if (rid != other.rid)
            {
                return rid < other.rid;
            }
            return block_index <= other.block_index;
        }
    };

    struct RidBlock
    {
        RBIndex index;
        base::AppBuffer buf;
        framework::timer::TickCounter touch_time_counter_;

        bool operator<(const RidBlock& other) { return index < other.index; }
        bool operator == (const RidBlock& other) { return index == other.index; }
        bool operator <= (const RidBlock& other) { return index <= other.index; }
    };

    struct ApplySubPiece
    {
        protocol::SubPieceInfo subpiece_info;
        protocol::Packet packet;
        boost::asio::ip::udp::endpoint end_point;
        uint32_t priority;
        uint16_t request_peer_version_;

        ApplySubPiece(){}
        ApplySubPiece(const ApplySubPiece& that)
        {
            if (this != &that)
            {
                subpiece_info = that.subpiece_info;
                packet = that.packet;
                end_point = that.end_point;
                priority = that.priority;
                request_peer_version_ = that.request_peer_version_;
            }
        }
        ApplySubPiece& operator = (const ApplySubPiece& that)
        {
            if (this != &that)
            {
                subpiece_info = that.subpiece_info;
                packet = that.packet;
                end_point = that.end_point;
                priority = that.priority;
                request_peer_version_ = that.request_peer_version_;
            }
            return *this;
        }

        bool operator<(const ApplySubPiece& other) const
        {
            return subpiece_info < other.subpiece_info;
        }
        bool operator == (const ApplySubPiece& other) const
        {
            return subpiece_info == other.subpiece_info
                && end_point == other.end_point;
        }
        bool operator <= (const ApplySubPiece& other) const { return (*this) < other || (*this) == other; }
    };

    struct UploadControlParam
    {
    private:
        // -1  不限连接；0  禁止连接
        boost::int32_t max_connect_peers_;
        // -1  不限上传连接；0 禁止上传连接
        boost::int32_t max_upload_peers_;
        // -1  不限速度；0  禁止上传
        boost::int32_t max_speed_in_KBps_;
        // -1  用户未设置; 其他 用户设置值 (最大上传值上限)
        boost::int32_t user_speed_in_KBps_;

    public:

        UploadControlParam()
            : max_connect_peers_(-1)
            , max_upload_peers_(-1)
            , max_speed_in_KBps_(-1)
            , user_speed_in_KBps_(-1)
        {
        }

        UploadControlParam(boost::int32_t max_connect_peers, boost::int32_t max_upload_peers, boost::int32_t max_upload_speed_in_KBps, boost::int32_t user_speed_in_KBps)
            : max_connect_peers_(max_connect_peers)
            , max_upload_peers_(max_upload_peers)
            , max_speed_in_KBps_(max_upload_speed_in_KBps)
            , user_speed_in_KBps_(user_speed_in_KBps)
        {

        }

        // max connect peers
        void SetMaxConnectPeers(boost::int32_t count)
        {
            max_connect_peers_ = count;
        }
        boost::int32_t GetMaxConnectPeers() const
        {
            return max_connect_peers_;
        }
        // max upload peers
        void SetMaxUploadPeers(boost::int32_t count)
        {
            max_upload_peers_ = count;
        }
        boost::int32_t GetMaxUploadPeers() const
        {
            return max_upload_peers_;
        }
        // user speed
        void SetUserSpeedInKBps(boost::int32_t speed)
        {
            user_speed_in_KBps_ = speed;
        }
        boost::int32_t GetUserSpeedInKBps() const
        {
            return user_speed_in_KBps_;
        }
        // max speed
        void SetMaxSpeedInKBps(boost::int32_t speed)
        {
            max_speed_in_KBps_ = speed;
        }
        boost::int32_t GetMaxSpeedInKBps() const
        {
            // 用户未设置
            if (user_speed_in_KBps_ <= -1) {
                return max_speed_in_KBps_;
            }
            // 大于用户值
            if (max_speed_in_KBps_ <= -1 || max_speed_in_KBps_ > user_speed_in_KBps_) {
                return user_speed_in_KBps_;
            }
            // ok
            return max_speed_in_KBps_;
        }
    };

    // class UploadSpeedLimiter;
    // typedef boost::shared_ptr<UploadSpeedLimiter> UploadSpeedLimiter__p;

    class NetworkQualityMonitor;

    class UploadManager
        : public boost::noncopyable
        , public boost::enable_shared_from_this<UploadManager>
        , public storage::IUploadListener
        , public ConfigUpdateListener
#ifdef DUMP_OBJECT
        , public count_object_allocate<UploadManager>
#endif
    {
        friend class AppModule;

    public:
        typedef boost::multi_index::multi_index_container<
            ApplySubPiece,
            boost::multi_index::indexed_by<
                boost::multi_index::sequenced<>,  // std::list-like index
                boost::multi_index::ordered_unique<boost::multi_index::identity<ApplySubPiece> >
            >
        > ApplyList;
        typedef boost::shared_ptr<UploadManager> p;
        typedef boost::asio::ip::udp::endpoint EndPoint;
        typedef boost::shared_ptr<ApplyList> ApplyListPtr;
        typedef std::map<RBIndex, ApplyListPtr> NeedResourceMap;
        typedef NeedResourceMap::iterator NeedResourceMapIterator;

        static p create() { return p(new UploadManager()); }

    public:
        // 启动 停止
        void Start(const string& config_path);
        void Stop();

        // 消息
        void OnP2PTimer(uint32_t times);

        void OnConnectPacket(protocol::ConnectPacket const & packet);
        void OnRequestAnnouncePacket(protocol::RequestAnnouncePacket const & packet);
        void OnLiveRequestAnnouncePacket(protocol::LiveRequestAnnouncePacket const & packet);
        void OnRequestSubPiecePacket(protocol::RequestSubPiecePacket const & packet);
        void OnRequestSubPiecePacketOld(protocol::RequestSubPiecePacketOld const & packet);
        void OnLiveRequestSubPiecePacket(protocol::LiveRequestSubPiecePacket const & packet);
        void OnRIDInfoRequestPacket(protocol::RIDInfoRequestPacket const & packet);
        void OnReportSpeedPacket(protocol::ReportSpeedPacket const & packet);

        void OnAsyncGetSubPieceSucced(const RID& rid, protocol::SubPieceInfo const& subpiece_info,
            const EndPoint& end_point, protocol::SubPieceBuffer buffer,
            protocol::Packet const & packet, uint32_t priority, uint16_t request_peer_version);
        void OnAsyncGetSubPieceFailed(const RID& rid, protocol::SubPieceInfo const& subpiece_info, const EndPoint& end_point, int failed_code, protocol::Packet const & packet);
        virtual void OnAsyncGetBlockSucced(const RID& rid, boost::uint32_t block_index, base::AppBuffer const & buffer);
        virtual void OnAsyncGetBlockFailed(const RID& rid, boost::uint32_t block_index, int failed_code);

        //
        void SetUploadUserSpeedLimitInKBps(boost::int32_t user_speed_in_KBps);
        void SetUploadMaxConnectionCount(boost::int32_t max_connection_count);
        void SetUploadMaxUploadConnectionCount(boost::int32_t max_upload_connection_count);
        void SetUploadControlParam(UploadControlParam param);

        void SetUploadSpeedLimitInKBps(boost::int32_t upload_speed_limit_in_KBps);
        boost::int32_t GetUploadSpeedLimitInKBps() const;

        bool IsConnectionFull(uint32_t ip_pool_size) const;
        bool IsUploadConnectionFull(boost::asio::ip::udp::endpoint const& end_point);
        bool AddApplySubPiece(const protocol::SubPieceInfo& subpiece_info, const RID& rid, const EndPoint& end_point, protocol::Packet const & packet);
        void SetMaxUploadCacheLength(boost::uint32_t nMaxUploadCacheLen) { max_upload_cache_len_ = nMaxUploadCacheLen;}

        // 设置上传开关，用于控制是否启用上传
        void SetUploadSwitch(bool is_disable_upload);
        boost::uint32_t GetUploadBandWidthInBytes() {return GetMaxUploadSpeedForControl();}
        size_t GetCurrentCacheSize() const;
        void SetCurrentCacheSize(size_t cache_size);

        void OnConfigUpdated();
        bool NeedUseUploadPingPolicy();

    private:
        void ShrinkCacheListIfNeeded(size_t cache_size);

        protocol::SubPieceBuffer GetSubPieceFromBlock(const protocol::SubPieceInfo& subpiece_info, const RID& rid, const base::AppBuffer& block_buf);
        bool GetSubPieceFromCache(const protocol::SubPieceInfo& subpiece_info, const RID& rid, protocol::SubPieceBuffer& buf);

        void OnUploadControl(uint32_t times);
        void UpdateParameters(boost::int32_t miniutes, boost::int32_t type = 0);
        void UpdateSpeedLimit(boost::int32_t speed_limit);

        void LoadHistoricalMaxUploadSpeed();
        void SaveHistoricalMaxUploadSpeed();

        void CheckCacheList();

        ApplyListPtr CreateApplyList();

        boost::uint32_t GetMaxUploadSpeedForControl() const;

        boost::uint32_t MeasureCurrentUploadSpeed() const;
        bool IsPeerFromSameSubnet(const boost::asio::ip::udp::endpoint& peer_endpoint) const;
        boost::uint16_t GetPeerVersionFromPacket(const protocol::Packet& packet) const;

        void OnVodConnectPacket(protocol::ConnectPacket const & packet);
        void OnLiveConnectPacket(protocol::ConnectPacket const & packet);

        void SendErrorPacket(protocol::CommonPeerPacket const &packet, boost::uint16_t error_code);

        void KickUploadConnections();

        void KickVodUploadConnections();

        void UploadControlOnPingPolicy();
        int32_t CalcUploadSpeedLimitOnPingPolicy(bool is_network_good);

    private:
        // 状态
        volatile bool is_running_;
        std::map<boost::asio::ip::udp::endpoint, PEER_UPLOAD_INFO> accept_connecting_peers_;    // 请求连接的Peer信息
        std::set<boost::asio::ip::udp::endpoint> accept_uploading_peers_;  // 正在向其上传的Peers
        // std::set<Guid> accept_uploading_peer_guids_;
        std::map<boost::asio::ip::udp::endpoint, std::pair<boost::uint32_t, framework::timer::TickCounter> > uploading_peers_speed_;

        // 用于上传限速
        bool is_upload_too_more_;
        UploadControlParam upload_param_;
        storage::DTType desktop_type_;

        uint32_t local_ip_from_ini_;
        UploadSpeedLimiter upload_limiter_;
        UploadSpeedLimitTracker upload_speed_limit_tracker_;
        bool is_locking_;

        // cache
        std::list<RidBlock> cache_list_;
        NeedResourceMap need_resource_map_;
        boost::uint64_t apply_subpiece_num_;
        boost::uint64_t get_from_cache_;

        // boost::int32_t recent_played_num;
        uint32_t recent_play_series_;

        //
        string ppva_config_path_;
        boost::uint32_t max_upload_cache_len_;

        bool is_disable_upload_;

        boost::shared_ptr<NetworkQualityMonitor> network_quality_monitor_;
        BootStrapGeneralConfig::UploadPolicy upload_policy_;
    private:
        UploadManager()
            : is_running_(false)
            , apply_subpiece_num_(0)
            , get_from_cache_(0)
            , recent_play_series_(0)
            , upload_limiter_()
            , is_disable_upload_(false)
            , upload_policy_(BootStrapGeneralConfig::policy_defalut)
        {}
    };

}
#endif  // _P2SP_P2P_UPLOAD_MANAGER_H_
