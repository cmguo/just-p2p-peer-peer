#ifndef UPLOAD_CACHE_MODULE_H
#define UPLOAD_CACHE_MODULE_H

#include "UploadStruct.h"
#include "storage/IStorage.h"
#include "storage/Storage.h"
#include "p2sp/p2p/UploadBase.h"
#include "p2sp/p2p/P2SPConfigs.h"

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/identity.hpp>

// 主要点播 P2P上传的数据缓存
namespace p2sp
{
    enum PacketType
    {
        UDP_VOD = 0,
        TCP_VOD = 1
    };

    struct ApplySubPiece
    {
        ApplySubPiece()
        {

        }

        ApplySubPiece(const protocol::SubPieceInfo & subpiece_info,
            const protocol::RequestSubPiecePacket & packet,
            SubPieceLoadListener::p upload_manager) 
            : subpiece_info(subpiece_info)
            , packet(packet)
            , upload_manager(upload_manager)
            , packet_type(UDP_VOD)
        {

        }

        ApplySubPiece(const protocol::SubPieceInfo & subpiece_info,
            const protocol::TcpSubPieceRequestPacket & packet,
            SubPieceLoadListener::p upload_manager) 
            : subpiece_info(subpiece_info)
            , tcp_packet(packet)
            , upload_manager(upload_manager)
            , packet_type(TCP_VOD)
        {

        }

        boost::asio::ip::address GetIpAddress() const
        {
            if (packet_type == UDP_VOD)
            {
                return packet.end_point.address();
            }
            else
            {
                assert(packet_type == TCP_VOD);
                boost::system::error_code ec;
                boost::asio::ip::tcp::endpoint remote_endpoint = tcp_packet.tcp_connection_->socket().remote_endpoint(ec);
                if (ec)
                {
                    return boost::asio::ip::address();
                }

                return remote_endpoint.address();
            }
        }

        bool operator < (const ApplySubPiece& other) const
        {

            if (subpiece_info < other.subpiece_info)
            {
                return true;
            }
            else if (subpiece_info == other.subpiece_info)
            {
                const boost::asio::ip::address & first_address = GetIpAddress();
                const boost::asio::ip::address & second_address = other.GetIpAddress();

                return first_address < second_address;
            }

            return false;
        }

        protocol::SubPieceInfo subpiece_info;
        protocol::RequestSubPiecePacket packet;
        protocol::TcpSubPieceRequestPacket tcp_packet;
        SubPieceLoadListener::p upload_manager;
        PacketType packet_type;
    };

    struct ApplySubPieceIndexTag{};

    class UploadCacheModule
        : public boost::enable_shared_from_this<UploadCacheModule>
        , public storage::IUploadListener
    {
    public:
        typedef boost::shared_ptr<UploadCacheModule> p;

        typedef boost::multi_index::multi_index_container<
            ApplySubPiece,
            boost::multi_index::indexed_by
            <
                boost::multi_index::sequenced<>,  // std::list-like index
                boost::multi_index::ordered_unique
                <
                    boost::multi_index::tag<ApplySubPieceIndexTag>,
                    boost::multi_index::identity<ApplySubPiece>
                >
            >
        > ApplyList;

        typedef boost::shared_ptr<ApplyList> ApplyListPtr;
        typedef std::map<RBIndex, ApplyListPtr> NeedResourceMap;
        typedef NeedResourceMap::iterator NeedResourceMapIterator;

        static boost::shared_ptr<UploadCacheModule> Inst()
        {
            if (!inst_)
            {
                inst_.reset(new UploadCacheModule());
            }

            return inst_;
        }

        void GetSubPieceForUpload(
            const protocol::SubPieceInfo & subpiece_info, 
            const protocol::Packet & packet, 
            storage::Instance::p instance,
            SubPieceLoadListener::p upload_manager);

        virtual void OnAsyncGetBlockSucced(const RID& rid, uint32_t block_index, base::AppBuffer const & buffer);
        virtual void OnAsyncGetBlockFailed(const RID& rid, uint32_t block_index, int failed_code);

        void SetCurrentCacheSize(size_t cache_size);

        size_t GetCurrentCacheSize() const;

        void OnP2PTimer(boost::uint32_t times);

    private:

        void EraseExpiredCache();

        void ShrinkCacheListIfNeeded();

        bool AddApplySubPiece(
            const protocol::SubPieceInfo& subpiece_info,
            const RID& rid,
            const protocol::Packet & packet,
            SubPieceLoadListener::p upload_manager);

        // 将某个Block对应的buffer加入缓存
        void AddUploadCache(const RBIndex & rb_index, const base::AppBuffer & buffer);

        // 获取某subpiece_info对应的数据
        // 如果返回为false，说明缓存没有该数据
        bool GetSubPieceFromCache(const protocol::SubPieceInfo& subpiece_info, const RID& rid, 
            protocol::SubPieceBuffer& buf);

    private:

        UploadCacheModule()
            : max_upload_cache_len_(P2SPConfigs::UPLOAD_MAX_CACHE_LENGTH)
            , apply_subpiece_num_(0)
            , get_from_cache_(0)
        {

        }

        static boost::shared_ptr<UploadCacheModule> inst_;

        boost::uint32_t max_upload_cache_len_;

        std::list<RidBlock> cache_list_;

        boost::uint32_t apply_subpiece_num_;

        boost::uint32_t get_from_cache_;

        NeedResourceMap need_resource_map_;
    };
}

#endif