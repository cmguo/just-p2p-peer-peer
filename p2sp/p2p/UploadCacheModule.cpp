#include "Common.h"
#include "p2sp/p2p/P2SPConfigs.h"
#include "UploadCacheModule.h"
#include "storage/Storage.h"

namespace p2sp
{
    boost::shared_ptr<UploadCacheModule> UploadCacheModule::inst_;

    void UploadCacheModule::GetSubPieceForUpload(
        const protocol::SubPieceInfo & subpiece_info, 
        const protocol::Packet & packet, 
        storage::Instance::p instance,
        SubPieceLoadListener::p upload_manager)
    {
        apply_subpiece_num_++;
        protocol::SubPieceBuffer upload_buf;

        RID rid;
        if (packet.PacketAction == protocol::RequestSubPiecePacket::Action)
        {
            rid = ((const protocol::RequestSubPiecePacket &)packet).resource_id_;
        }
        else
        {
            assert(packet.PacketAction == protocol::TcpSubPieceRequestPacket::Action);
            rid = ((const protocol::TcpSubPieceRequestPacket &)packet).resource_id_;
        }
        
        if (GetSubPieceFromCache(subpiece_info, rid, upload_buf))
        {
            get_from_cache_++;
            upload_manager->OnAsyncGetSubPieceSucced(rid, subpiece_info, packet, upload_buf);
        }
        else
        {
            if (false == AddApplySubPiece(subpiece_info, rid, packet, upload_manager))
            {
                instance->AsyncGetBlock(subpiece_info.block_index_, shared_from_this());
            }
        }
    }

    void UploadCacheModule::OnAsyncGetBlockSucced(const RID& rid, 
        uint32_t block_index, base::AppBuffer const & buffer)
    {
        // 遍历RBIndex所对应的需要获取的subpiece，发送SubPiece报文
        RBIndex rb_index(rid, block_index);
        NeedResourceMapIterator it = need_resource_map_.find(rb_index);
        if (it != need_resource_map_.end())
        {
            ApplyListPtr apply_list = it->second;
            for (ApplyList::iterator itl = apply_list->begin(); itl != apply_list->end(); ++itl)
            {
                // 获取subpiece对应的buf，合成subpiece报文并发送
                const ApplySubPiece & apply_subpiece = *itl;
                protocol::SubPieceBuffer subpiece_buf = 
                    storage::Storage::Inst_Storage()->GetSubPieceFromBlock(apply_subpiece.subpiece_info, rid, buffer);

                if (subpiece_buf)
                {
                    if (apply_subpiece.packet_type == UDP_VOD)
                    {
                        apply_subpiece.upload_manager->OnAsyncGetSubPieceSucced(rb_index.rid, apply_subpiece.subpiece_info,
                            apply_subpiece.packet, subpiece_buf);
                    }
                    else
                    {
                        apply_subpiece.upload_manager->OnAsyncGetSubPieceSucced(rb_index.rid, apply_subpiece.subpiece_info,
                            apply_subpiece.tcp_packet, subpiece_buf);
                    }
                }
            }
            need_resource_map_.erase(it);
        }

        AddUploadCache(rb_index, buffer);
    }

    void UploadCacheModule::OnAsyncGetBlockFailed(const RID& rid,
        uint32_t block_index, int failed_code)
    {
        // 遍历RBIndex所对应的需要获取的subpiece，发送SubPiece报文
        RBIndex rb_index(rid, block_index);

        NeedResourceMapIterator it = need_resource_map_.find(rb_index);
        if (it != need_resource_map_.end())
        {
            ApplyListPtr apply_list = it->second;

            assert(apply_list);
            
            for (ApplyList::iterator itl = apply_list->begin(); itl != apply_list->end(); ++itl)
            {
                const ApplySubPiece & apply_subpiece = *itl;

                if (apply_subpiece.packet_type == UDP_VOD)
                {
                    apply_subpiece.upload_manager->OnAsyncGetSubPieceFailed(rb_index.rid, apply_subpiece.subpiece_info,
                        apply_subpiece.packet);
                }
                else
                {
                    apply_subpiece.upload_manager->OnAsyncGetSubPieceFailed(rb_index.rid, apply_subpiece.subpiece_info,
                        apply_subpiece.tcp_packet);
                }
            }
            
            need_resource_map_.erase(it);
        }
    }

    // 加入需要资源的队列，返回值为false时，需要向instance申请block
    // udp vod 上传
    bool UploadCacheModule::AddApplySubPiece(
        const protocol::SubPieceInfo& subpiece_info,
        const RID& rid,
        const protocol::Packet & packet,
        SubPieceLoadListener::p upload_manager)
    {
        RBIndex rb_index(rid, subpiece_info.block_index_);

        ApplySubPiece apply_subpiece;

        if (packet.PacketAction == protocol::RequestSubPiecePacket::Action)
        {
            ApplySubPiece apply_sub_piece(subpiece_info, (const protocol::RequestSubPiecePacket &)packet, upload_manager);
            apply_subpiece = apply_sub_piece;
        }
        else
        {
            assert(packet.PacketAction == protocol::TcpSubPieceRequestPacket::Action);
            ApplySubPiece apply_sub_piece(subpiece_info, (const protocol::TcpSubPieceRequestPacket &)packet, upload_manager);
            apply_subpiece = apply_sub_piece;
        }

        NeedResourceMapIterator it = need_resource_map_.find(rb_index);
        if (it == need_resource_map_.end())
        {
            ApplyListPtr as_list = ApplyListPtr(new ApplyList());
            as_list->push_back(apply_subpiece);
            need_resource_map_.insert(std::make_pair(rb_index, as_list));
            return false;
        }
        else
        {
            ApplyListPtr apply_list = it->second;
            ApplyList::index<ApplySubPieceIndexTag>::type & set_index = apply_list->get<ApplySubPieceIndexTag>();

            if (set_index.find(apply_subpiece) == set_index.end())
            {
                apply_list->push_back(apply_subpiece);
            }
            return true;
        }
    }

    bool UploadCacheModule::GetSubPieceFromCache(const protocol::SubPieceInfo& subpiece_info, const RID& rid, 
                              protocol::SubPieceBuffer& buf)
    {
        RBIndex rb_index(rid, subpiece_info.block_index_);

        std::list<RidBlock>::iterator it = cache_list_.begin();
        for (; it != cache_list_.end(); ++it)
        {
            if (it->index == rb_index)
            {
                buf = storage::Storage::Inst_Storage()->GetSubPieceFromBlock(subpiece_info, rid, it->buf);

                it->touch_time_counter_.reset();
                // 将it所指向的元素移动cache_list_的begin()位置
                cache_list_.splice(cache_list_.begin(), cache_list_, it);

                return true;
            }
        }

        return false;
    }

    void UploadCacheModule::AddUploadCache(const RBIndex & rb_index, const base::AppBuffer & buffer)
    {
        RidBlock rid_block(rb_index, buffer);

        std::list<RidBlock>::iterator it = cache_list_.begin();
        for (;it != cache_list_.end(); ++it)
        {
            if ((*it).index == rb_index)
            {
                break;
            }
        }

        if (it != cache_list_.end())
        {
            it->touch_time_counter_.reset();
            // 将it所指向的元素移动cache_list_的begin()位置
            cache_list_.splice(cache_list_.begin(), cache_list_, it);
        }
        else
        {
            cache_list_.push_front(rid_block);
        }

        ShrinkCacheListIfNeeded();
    }

    void UploadCacheModule::SetCurrentCacheSize(size_t cache_size)
    {
        if (cache_size > P2SPConfigs::UPLOAD_MAX_CACHE_LENGTH)
        {
            max_upload_cache_len_ = P2SPConfigs::UPLOAD_MAX_CACHE_LENGTH;
        }
        else
        {
            max_upload_cache_len_ = cache_size;
        }

        ShrinkCacheListIfNeeded();
    }

    size_t UploadCacheModule::GetCurrentCacheSize() const
    {
        assert(cache_list_.size() <= max_upload_cache_len_);
        return cache_list_.size();
    }

    void UploadCacheModule::OnP2PTimer(boost::uint32_t times)
    {
        statistic::StatisticModule::Inst()->SetUploadCacheHit(get_from_cache_);
        statistic::StatisticModule::Inst()->SetUploadCacheRequest(apply_subpiece_num_);

        EraseExpiredCache();
    }

    void UploadCacheModule::EraseExpiredCache()
    {
        while (!cache_list_.empty())
        {
            RidBlock rid_block = cache_list_.back();
            if (rid_block.touch_time_counter_.elapsed() >= P2SPConfigs::UPLOAD_CACHE_BLOCK_EXPIRE_TIME_IN_MILLISEC)
            {
                cache_list_.pop_back();
            }
            else
            {
                break;
            }
        }
    }

    void UploadCacheModule::ShrinkCacheListIfNeeded()
    {
        while (cache_list_.size() > max_upload_cache_len_)
        {
            cache_list_.pop_back();
        }
    }
}