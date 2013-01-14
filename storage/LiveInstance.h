//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _LIVEINSTANCE_H
#define _LIVEINSTANCE_H

#include "IStorage.h"
#include "LiveCacheManager.h"

namespace p2sp
{
    class ILiveStream;
    typedef boost::shared_ptr<ILiveStream> ILiveStreamPointer;
}

namespace storage
{
    class LiveInstance;
    class ILiveInstanceStoppedListener
    {
    public:
        virtual void OnLiveInstanceStopped(IInstance::p live_instance) = 0;
        virtual ~ILiveInstanceStoppedListener(){}
    };

    class LiveInstance
        : public boost::noncopyable
        , public boost::enable_shared_from_this<LiveInstance>
        , public IInstance
#ifdef DUMP_OBJECT
        , public count_object_allocate<LiveInstance>
#endif
    {
        friend class LiveInstanceMemoryConsumer;

        //在最后一个download driver被detach后120秒，这个LiveInstance才会被自动stopped掉。
        //这样做的目的是为了让这个LiveInstance还有机会用于上传。
        static const size_t ExpectedP2PCoverageInSeconds = 120;

    public:
        typedef boost::shared_ptr<LiveInstance> p;

        LiveInstance(const RID& rid_info, boost::uint16_t live_interval);

        void Start(boost::shared_ptr<ILiveInstanceStoppedListener> stopped_event_listener);
        void Stop();

        //downloader通过此接口添加刚下载好的subpiece
        void AddSubPiece(const protocol::LiveSubPieceInfo & subpiece, const protocol::LiveSubPieceBuffer & subpiece_buffer);

        //downloader通过此接口查询接下来待下载的block
        void GetNextIncompleteBlock(boost::uint32_t start_block_id, protocol::LiveSubPieceInfo & next_incomplete_block) const;

        bool BlockExists(boost::uint32_t block_id) const;
        bool IsBlockValid(boost::uint32_t block_id) const; //TBD – will be available if needed
        bool IsPieceValid(boost::uint32_t block_id, boost::uint16_t piece_index) const;
        bool IsBlockHeaderValid(boost::uint32_t block_id) const;
        bool HasCompleteBlock(boost::uint32_t block_id) const;
        bool IsPieceComplete(boost::uint32_t block_id, boost::uint16_t piece_index) const;
        bool HasSubPiece(const protocol::LiveSubPieceInfo &subpiece) const;

        //返回指定block的总长度，前提是对应的block的第一个subpiece (header subpiece)已经获得，不然将返回0
        //另外这个结果不一定正确，调用者（比如Upload Manager）如果在乎的话，应该额外查询IsBlockHeaderValid(block_id)来进行核实.
        boost::uint32_t GetBlockSizeInBytes(boost::uint32_t block_id) const;

        void AttachStream(p2sp::ILiveStreamPointer live_stream);

        //在卸掉最后一个livestream时，会启动一个timer，在timer到时后LiveInstance
        //才会从Storage的lookup table中移除。这样做的目的是为了让这个peer还能继续为直播上传一段时间。
        void DetachStream(p2sp::ILiveStreamPointer live_stream);

        RID GetRID() const { return cache_manager_.GetRID(); }
        void GetSubPiece(protocol::LiveSubPieceInfo subpiece_info, protocol::LiveSubPieceBuffer & subpiece_buffer) const;

        void BuildAnnounceMap(boost::uint32_t request_block_id, protocol::LiveAnnounceMap& announce_map);

        //为统计模块服务的接口
        boost::uint32_t GetDataRate() const;
        boost::uint16_t GetCacheSize() const;
        boost::uint32_t GetCacheFirstBlockId() const;
        boost::uint32_t GetCacheLastBlockId() const;
        boost::uint16_t GetLiveInterval() const;
        boost::uint32_t GetChecksumFailedTimes() const;

        const LivePosition GetCurrentLivePoint() const;
        void SetCurrentLivePoint(const LivePosition & live_point);

        void GetAllPlayPoints(std::vector<LivePosition>& play_points) const;

        boost::uint32_t GetMissingSubPieceCount(boost::uint32_t block_id) const;
        boost::uint32_t GetExistSubPieceCount(boost::uint32_t block_id) const;

    private:
        void PushDataToDownloaderDrivers();
        void PushDataToDownloaderDriver(p2sp::ILiveStreamPointer live_streams);
        bool SendSubPieces(p2sp::ILiveStreamPointer live_streams, boost::uint16_t last_subpiece_index);
        void RemoveFlashHeader(std::vector<protocol::LiveSubPieceBuffer> & subpiece_buffers);
        void OnTimerElapsed(framework::timer::Timer * timer);

    private:
        LiveCacheManager cache_manager_;
        std::set<p2sp::ILiveStreamPointer> live_streams_;
        framework::timer::PeriodicTimer play_timer_;

        boost::shared_ptr<ILiveInstanceStoppedListener> stopped_event_listener_;
        framework::timer::OnceTimer stop_timer_;
    };

    inline bool LiveInstance::HasSubPiece(const protocol::LiveSubPieceInfo & subpiece) const
    {
        return cache_manager_.HasSubPiece(subpiece);
    }

    inline bool LiveInstance::BlockExists(boost::uint32_t block_id) const
    {
        return cache_manager_.BlockExists(block_id);
    }

    inline bool LiveInstance::HasCompleteBlock(boost::uint32_t piece_id) const
    {
        return cache_manager_.HasCompleteBlock(piece_id);
    }

    inline bool LiveInstance::IsBlockHeaderValid(boost::uint32_t block_id) const
    {
        return cache_manager_.IsBlockHeaderValid(block_id);
    }

    inline bool LiveInstance::IsPieceValid(boost::uint32_t block_id, boost::uint16_t piece_index) const
    {
        return cache_manager_.IsPieceValid(block_id, piece_index);
    }

    inline bool LiveInstance::IsPieceComplete(boost::uint32_t block_id, boost::uint16_t piece_index) const
    {
        return cache_manager_.IsPieceComplete(block_id, piece_index);
    }

    inline boost::uint32_t LiveInstance::GetBlockSizeInBytes(boost::uint32_t block_id) const
    {
        return cache_manager_.GetBlockSizeInBytes(block_id);
    }

    inline void LiveInstance::GetSubPiece(protocol::LiveSubPieceInfo subpiece_info, protocol::LiveSubPieceBuffer & subpiece_buffer) const
    {
        return cache_manager_.GetSubPiece(subpiece_info, subpiece_buffer);
    }

    inline boost::uint32_t LiveInstance::GetDataRate() const
    {
        return cache_manager_.GetDataRate();
    }

    inline boost::uint16_t LiveInstance::GetCacheSize() const
    {
        return cache_manager_.GetCacheSize();
    }

    inline boost::uint32_t LiveInstance::GetCacheFirstBlockId() const
    {
        return cache_manager_.GetCacheFirstBlockId();
    }

    inline boost::uint32_t LiveInstance::GetCacheLastBlockId() const
    {
        return cache_manager_.GetCacheLastBlockId();
    }

    inline boost::uint16_t LiveInstance::GetLiveInterval() const
    {
        return cache_manager_.GetLiveInterval();
    }

    inline const LivePosition LiveInstance::GetCurrentLivePoint() const 
    { 
        return cache_manager_.GetCurrentLivePoint(); 
    }

    inline void LiveInstance::SetCurrentLivePoint(const LivePosition & live_point)
    {
        cache_manager_.SetCurrentLivePoint(live_point);
    }

    inline boost::uint32_t LiveInstance::GetChecksumFailedTimes() const
    {
        return cache_manager_.GetChecksumFailedTimes();
    }
}

#endif  //_LIVEINSTANCE_H
