//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef STORAGE_INSTANCE_H
#define STORAGE_INSTANCE_H

/*******************************************************************************
*   Instance.h
*   class Instance, friend class Storage
*    记录资源动态信息
*            支持存储文件的延迟创立和延迟删除！
*            文件的管理需要Storage的集中管理
*******************************************************************************/

#include "storage/Resource.h"
#include "storage/StatisticTools.h"
#include "storage/SubPieceManager.h"
#include "struct/BlockMap.h"

namespace storage
{
#ifdef DISK_MODE
    struct FileResourceInfo;
#endif

    struct MetaData
#ifdef DUMP_OBJECT
        : public count_object_allocate<MetaData>
#endif
    {
        uint32_t Duration;  // total duration of file in seconds
        uint32_t FileLength;  // file length in bytes
        uint32_t VideoDataRate;  // bytes per second
        boost::uint16_t Width;  // width
        boost::uint16_t Height;  // height
        string FileFormat;  // lower case file extension

        // constructor
        MetaData()
        {
            Duration = 0;
            FileLength = 0;
            VideoDataRate = 0;
            Width = 0;
            Height = 0;
            FileFormat = "none";
        }
    };

    class Instance:
        public boost::noncopyable,
        public boost::enable_shared_from_this<Instance>,
        public IInstance
#ifdef DUMP_OBJECT
        , public count_object_allocate<Instance>
#endif
    {
        friend class InstanceMemoryConsumer;
    public:
        typedef boost::shared_ptr<Instance> p;

        friend class Resource;
        friend class SpaceManager;
        friend class Storage;

    public:
        // 创建一个拥有该url的instance
        static Instance::p Create(const protocol::UrlInfo& url_info);

#ifdef DISK_MODE
        // 根据FileResourceInfo和Resource_p创建一个新的instance
        static Instance::p Open(FileResourceInfo r_f, Resource::p resource_p);
#endif

        ~Instance();

    private:
        Instance();
        void Start();

        void Stop();

        // 关闭本instance并通知storage，释放空间
        void Remove(bool need_remove_file);

        bool IsRunning();

        void NotifyGetFileName(const string& file_name);

    protected:
        protocol::SubPieceBuffer ToBuffer();

        // ms什么都没做
        void Merge(Instance::p);

        // 将本instance合并到new_instance中，并通知download_driver，然后删除本instance
        void BeMergedTo(Instance::p);

        // 合并，然后通知Storage删除本instance
        void MergeResourceTo();

    public:
        bool Rename(const string& newname);

        void OnRenameFinish(const string newname);

        // 如果某个block已被鉴定，则将该block交给upload_driver，否则...
        void AsyncGetBlock(uint32_t block_index, IUploadListener::p listener);

        bool GetSubPieceForPlay(boost::shared_ptr<IDownloadDriver> dd, uint32_t start_postion, std::vector<protocol::SubPieceBuffer> & buffers);
        // 从pending_subpiece_manager中或文件中找到某个subpiece，然后merge到merge_to_instance_p
        void MergeSubPiece(const protocol::SubPieceInfo& subpiece_info, Instance::p merge_to_instance_p);
        // 将subpiece添加到准备写入的队列，检查是否该写入文件，检查block, 上传等, (针对merge_to_instance_p)
        void OnMergeSubPieceSuccess(const protocol::SubPieceInfo& subpiece_info, protocol::SubPieceBuffer buffer);

        bool GetNextPieceForDownload(const protocol::PieceInfoEx &start_piece_index, protocol::PieceInfoEx& piece_for_download);
        bool GetNextPieceForDownload(uint32_t start_position, protocol::PieceInfoEx& piece_for_download);

        // 将subpiece添加到准备写入的队列，并检查是否该写入文件，并检查block, 上传等操作
        void AsyncAddSubPiece(const protocol::SubPieceInfo& subpiece_info, const protocol::SubPieceBuffer& buffer);

        // 根据file_length创建文件资源
        void SetFileLength(uint32_t file_length);

        // 告诉instance上传了一个subpiece，用于上传流量统计
        void WeUploadSubPiece(uint32_t num);

        // 如果资源描述为空(正常情况)，则根据rid_info创建资源描述符，进而创建文件资源
        bool SetRidInfo(const protocol::RidInfo& rid_info, MD5 content_md5, uint32_t content_bytes);

        void SetContentNeedToAdd(bool is_need_to_add)
        {
            content_need_to_add_ = is_need_to_add;
        }

        int GetRidOriginFlag() const
        {
            return flag_rid_origin_;
        }

        void SetRidOriginFlag(int flag)
        {
            flag_rid_origin_ = flag;
        }

    public:
        // -----------------------------------------------------------------
        //  一系列获取属性信息的函数
        RID GetRID();
        uint32_t GetBlockSize();
        uint32_t GetBlockCount();
        uint32_t GetFileLength();  // 等价于GetResourceLength()
        uint32_t GetResourceLength();
        uint32_t GetDiskFileSize();
        string GetResourceName();
        uint32_t GetDownloadBytes();
        float GetInstanceValue();
        string GetFileName(){return file_name_;}

        void GetDownloadResourceData(downloadcenter::DownloadResourceData& res_data);

        uint32_t GetDDNum() { return download_driver_s_.size();}
        int GetStatus() { return instance_state_;}
        unsigned char GetResDownMode() const { return down_mode_;}
        bool IsSaveMode() const;
        void SetSaveMode(bool save_mode);

        bool HasPiece(const protocol::PieceInfo& piece_info);
        bool HasPiece(uint32_t start_postion);
        bool HasSubPiece(const protocol::SubPieceInfo& subpiece_info);
        bool IsComplete();
        bool IsFileComplete();
        bool CanRemove();
        void SetPureDownloadMode(bool b_mode);
        bool IsPureDownloadMode();
        bool HasRID() const { return (!!subpiece_manager_) ? subpiece_manager_->HasRID() : false;}

        bool IsDownloading();
        bool IsUploading();
        void FreeResourceHandle();

        void GetUrls(std::set<protocol::UrlInfo>& url_s);
        protocol::UrlInfo GetOriginalUrl();
        void GetRidInfo(protocol::RidInfo &rid_info);

        protocol::BlockMap::p GetBlockMap() const;
        MetaData& GetMetaData() { return meta_data_;}
        bool ParseMetaData(base::AppBuffer const & buffer);
        uint32_t GetDownloadedBlockCount() const;
        void SetIsOpenService(bool is_open_service) { is_open_service_ = is_open_service;}
        bool IsOpenService() const { return is_open_service_ > 0;}
        void SetIsPush(bool is_push) { is_push_ = is_push;}
        bool IsPush() const { return is_push_;}

#ifdef DISK_MODE
        bool GetFileResourceInfo(FileResourceInfo &r_info);

        // 查询某个rid的校验失败的次数
        boost::int32_t GetBlockHashFailed();
#endif

    protected:
        void GetUrls(std::vector<protocol::UrlInfo>& url_s);
        // -----------------------------------------------------------------
    public:
        /**`
        * @brief 给资源RID添加 多个 HttpServer
        */
        // 添加url_info(如果已存在不做操作)，并通知download_driver
        void AddUrlInfo(const std::vector<protocol::UrlInfo>& url_info_s);

        // 添加url(如果已存在，替换本地refer)，并通知download_driver
        void AddUrlInfo(const protocol::UrlInfo& url_info);

        // 从url_info_s中删除某个url，并通知download_driver
        void RemoveUrl(const string& url_str);

        // 向instance加入某个download_driver
        void AttachDownloadDriver(IDownloadDriver::p download_driver);

        // 从instance中删掉某个download_driver
        void DettachDownloadDriver(IDownloadDriver::p download_driver);

    public:
        // 将content写入文件，并通知download_driver
        bool DoMakeContentMd5AndQuery(base::AppBuffer content_buffer);



        // 通知DownloadDriver makeblock成功或失败
        void OnNotifyHashBlock(uint32_t block_index, bool b_success);

        void OnWriteSubPieceFinish(protocol::SubPieceInfo subpiece_info);
        void OnWriteBlockFinish(uint32_t block_index);

        // 通知Storage做相应操作，通知appmodule发AddRidUrlRequestPacket包，通知download_driver下载完毕
        void OnHashResourceFinish();

        // block写入完毕，赋予MD5值，检查文件是否写入完毕，完毕则通知其他模块下载完毕
        // 读取需要上传的block，上传
        void OnPendingHashBlockFinish(uint32_t block_index, MD5 hash_val);

        void OnReadBlockForUploadFinishWithHash(uint32_t block_index, base::AppBuffer& buf, IUploadListener::p listener,
            MD5 hash_val);
        void OnReadBlockForUploadFinish(uint32_t block_index, base::AppBuffer& buf, IUploadListener::p listener);

        // content写入完毕，通知download_driver
        void OnPendingHashContentFinish(MD5 hash_val, uint32_t content_bytes);

        // 通知storage关闭instance，释放资源空间
        void OnResourceCloseFinish(Resource::p resource_p, bool need_remove_file);

        // 将resource_p_中的instance指向本对象，并将信息写入ResourceInfo文件中(ResourceInfo.dat)
        void AttachResource(Resource::p resource_p, bool is_open);

        // 资源文件改名，通知storage将文件信息写入资源信息文件
        void OnDiskFileNameChange(string file_name);
        void OnFileWriteFinish();
        void NotifySetWebUrl(string web_url);
        void GetBlockPosition(uint32_t block_index, uint32_t &offset, uint32_t &length);
        void GetSubPiecePosition(const protocol::SubPieceInfo &subpiec_info, uint32_t &offset, uint32_t &length);
        void OnThreadReadSubPieceSucced(const protocol::SubPieceInfo & subpiece_info, protocol::SubPieceBuffer buff);

    protected:
        // 从资源描述, subpiece_manager_中删除block
        // 通知upload_listener获取subpiece失败，通知download_driver，makeblock失败
        void OnRemoveResourceBlockFinish(uint32_t block_index);

    private:
        // 从Url中获取文件名，如果获取失败，则生成一串随机数表示文件名, 生成的文件名写入resource_name_中
        void ParseFileNameFromUrl(const protocol::UrlInfo &url_info);

        // 通过subpiece计算block的MD5值，然后检查上传等操作
        void PendingHashBlock(uint32_t block_index);

        // 通过subpiece计算content的MD5值
        void PendingHashContent();

        // 根据url_info_s的第一个Url生成资源文件的文件名，然后向Storage申请资源
        // instance状态：NEED_RESOURCE --> APPLY_RESOURCE
        void TryCreateResource();

        void SetIsUploadingBlock(bool is_up){ is_uploading_block_ = is_up;}
        void ReleaseData();
#ifdef DISK_MODE
        void ReadFromDisk(protocol::SubPieceInfo & start_s_info);
#endif

        // Timers
        void OnConfigTimerElapsed(framework::timer::Timer * pointer);
        void OnTrafficTimerElapsed(framework::timer::Timer * pointer);
        void OnMergeTimerElapsed(framework::timer::Timer * pointer);
        void OnDeAttachTimerElapsed(framework::timer::Timer * pointer);

        void UpdateBlockHashTime(uint32_t block_index);
        bool CheckBlockNeedHash(uint32_t block_index);

    protected:
        volatile bool is_running_;
        bool b_pure_download_mode_;
        bool local_complete_;

        int instance_state_;                            // instance的状态，初始为APPLY_RESOURCE
        framework::timer::PeriodicTimer cfg_timer_;     // killself and cfg_save timer! 10s
        framework::timer::PeriodicTimer traffic_timer_;  // 1s
        framework::timer::OnceTimer merge_timer_;       // 250ms
        framework::timer::OnceTimer deattach_timer_;
        framework::timer::TickCounter delete_tc_;
        framework::timer::TickCounter traffic_tc_;
        framework::timer::TickCounter not_upload_tc_;

        std::set<IDownloadDriver::p> download_driver_s_;
        string resource_name_;  // 初始状态应为"x.flv"，创建资源后应为完整路径文件名，且为.tpp文件
        uint32_t disk_file_size_;
        std::set<protocol::UrlInfo> url_info_s_;
        protocol::UrlInfo origanel_url_info_;
        Resource::p resource_p_;
        SubPieceManager::p subpiece_manager_;
        Instance::p merge_to_instance_p_;
        uint32_t last_push_time_;
        std::list<uint32_t> traffic_list_;
        // version 3
        unsigned char down_mode_;           // 资源下载方式
        string web_url_;                    // 页面地址
        uint32_t file_duration_in_sec_;       // 时长
        boost::uint64_t last_write_time_;   // 该资源上一次写磁盘的时间
        uint32_t data_rate_;                  // 码流率
        // version 4
        string file_name_;  //
        // version 5
        uint32_t is_open_service_;  // is_open_service
        // version 6
        bool is_push_;
        //version 7
        boost::int64_t filesystem_last_write_time_;    //操作系统获取的资源文件最后修改时间
        std::map<uint32_t, boost::int64_t> block_hash_time_map_; //每个block的最近校验时间

        //
        string qname_;

        // 记录 Block 校验失败的次数
        boost::int32_t md5_hash_failed_;

    private:
        protocol::SubPieceInfo merging_pos_subpiece;

        // 内容感知
        MD5 content_md5_;
        uint32_t content_bytes_;
        base::AppBuffer content_buffer_;

        bool content_need_to_add_;
        bool content_need_to_query_;
        HitRate hit_rate_;
        HitRate upload_hit_rate_;

        MetaData meta_data_;
        int flag_rid_origin_;
        bool is_uploading_block_;
        bool is_have_rename_;

        uint32_t send_speed_limit_;
        uint32_t send_count_;
    };

    // ---------------------------------------------------------------
    //  一系列获取信息的Get函数
    inline RID Instance::GetRID()
    {
        if (subpiece_manager_)
            return subpiece_manager_->GetRID();
        else
            return RID();
    }

    inline void Instance::GetRidInfo(protocol::RidInfo &rid_info)
    {
        if (subpiece_manager_)
            rid_info = subpiece_manager_->GetRidInfo();
        else
            rid_info = protocol::RidInfo();
    }

    inline uint32_t Instance::GetFileLength()
    {
        if (is_running_ == false)
            return 0;
        return GetResourceLength();
    }

    inline uint32_t Instance::GetResourceLength()
    {
        if (subpiece_manager_)
            return subpiece_manager_->GetFileLength();
        else
            return 0;
    }

    inline uint32_t Instance::GetDiskFileSize()
    {
        return disk_file_size_;
    }

    inline uint32_t Instance::GetDownloadBytes()
    {
        if (!subpiece_manager_)
        {
            return 0;
        }
        return subpiece_manager_->GetDownloadBytes();
    }

    inline uint32_t Instance::GetBlockSize()
    {
        if (subpiece_manager_)
            return subpiece_manager_->GetBlockSize();
        else
            return 0;  // 怪异！
    }

    inline uint32_t Instance::GetBlockCount()
    {
        if (subpiece_manager_)
            return subpiece_manager_->GetBlockCount();
        return 0;
    }

    inline void Instance::SetPureDownloadMode(bool b_mode)
    {
        if (is_running_ == false)
            return;
        b_pure_download_mode_ = b_mode;
    }

    inline bool Instance::IsPureDownloadMode()
    {
        return b_pure_download_mode_;
    }

    inline protocol::BlockMap::p Instance::GetBlockMap() const
    {
        if (is_running_ == false)
            return protocol::BlockMap::p();

        if (!subpiece_manager_)
            return protocol::BlockMap::p();

        return subpiece_manager_->GetBlockMap();
    }

    inline uint32_t Instance::GetDownloadedBlockCount() const
    {
        if (is_running_ == false)
            return 0;

        if (!subpiece_manager_)
            return 0;

        return subpiece_manager_->GetDownloadedBlockCount();
    }

    inline void Instance::GetBlockPosition(uint32_t block_index, uint32_t &offset, uint32_t &length)
    {
        if (subpiece_manager_)
        {
            subpiece_manager_->GetBlockPosition(block_index, offset, length);
        }
        else
        {
            offset = 0;
            length = 0;
        }
    }

    inline void Instance::GetSubPiecePosition(const protocol::SubPieceInfo &subpiec_info, uint32_t &offset, uint32_t &length)
    {
        if (subpiece_manager_)
        {
            subpiece_manager_->GetSubPiecePosition(subpiec_info, offset, length);
        }
        else
        {
            offset = 0;
            length = 0;
        }
    }

    inline bool Instance::IsDownloading()
    {
        if (download_driver_s_.empty() && delete_tc_.elapsed() > default_delay_tickcount_for_delete)
        {
            return false;
        }
        return true;
    }

    inline bool Instance::IsUploading()
    {
        if (not_upload_tc_.elapsed() > 10 * 1000)
        {
            return false;
        }
        return true;
    }

    inline bool Instance::IsRunning()
    {
        return is_running_;
    }

    inline bool Instance::HasPiece(const protocol::PieceInfo& piece_info)
    {
        if (is_running_ == false)
            return false;
        if (!subpiece_manager_)
            return false;

        return subpiece_manager_->HasPiece(piece_info);
    }

    inline bool Instance::HasPiece(uint32_t start_postion)
    {
        if (is_running_ == false)
            return false;
        if (!subpiece_manager_)
            return false;

        protocol::PieceInfoEx piece_info;
        if (!subpiece_manager_->PosToPieceInfoEx(start_postion, piece_info))
            return false;

        return subpiece_manager_->HasPiece(piece_info);
    }

    inline bool Instance::HasSubPiece(const protocol::SubPieceInfo& subpiece_info)
    {
        if (is_running_ == false)
            return false;
        if (!subpiece_manager_)
            return false;

        return subpiece_manager_->HasSubPiece(subpiece_info);
    }

    inline string Instance::GetResourceName()
    {
        return resource_name_;
    }

    inline protocol::UrlInfo Instance::GetOriginalUrl()
    {
        if (is_running_ == false) return protocol::UrlInfo();
        return origanel_url_info_;
    }
}// storage

#endif  // STORAGE_INSTANCE_H
