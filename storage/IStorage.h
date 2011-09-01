//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef STORAGE_ISTORAGE_H
#define STORAGE_ISTORAGE_H

#include <protocol/PeerPacket.h>
#include "base/AppBuffer.h"

namespace downloadcenter
{
    struct DownloadResourceData;
}

namespace storage
{
    struct IUploadListener
    {
        typedef boost::shared_ptr<IUploadListener> p;
        virtual void OnAsyncGetBlockSucced(const RID& rid, uint32_t block_index, base::AppBuffer const & buffer) = 0;
        virtual void OnAsyncGetBlockFailed(const RID& rid, uint32_t block_index, int failed_code) = 0;

        virtual ~IUploadListener()
        {}
    };

    struct IInstanceListener
    {
        typedef boost::shared_ptr<IInstanceListener> p;
        virtual void OnDownloadComplete(string url, protocol::RidInfo rid_info);
        virtual void OnMakeBlockSucced(uint32_t block_info);
        virtual void OnMakeBlockFailed(uint32_t block_info);

        virtual ~IInstanceListener()
        {}
    };

    struct IInstance;
    class Instance;

    struct IDownloadDriver
    {
        typedef boost::shared_ptr<IDownloadDriver> p;

        virtual void OnNoticeChangeResource(boost::shared_ptr<Instance> instance_old, boost::shared_ptr<Instance> instance_new) = 0;

        virtual void OnNoticeRIDChange() = 0;

        virtual void OnNoticeDownloadComplete() = 0;

        virtual void OnNoticeMakeBlockSucced(uint32_t block_info) = 0;

        virtual void OnNoticeMakeBlockFailed(uint32_t block_info) = 0;

        virtual void OnNoticeContentHashSucced(string url, MD5 content_md5, uint32_t content_bytes, uint32_t file_length) = 0;

        virtual void OnNoticeGetFileName(const string& file_name) = 0;

        virtual void OnNoticeFileDownloadComplete() = 0;

        virtual void OnNoticeSetWebUrl(const string& web_url) = 0;

        virtual void OnRecvSubPiece(uint32_t position, const protocol::SubPieceBuffer& buffer) = 0;

        virtual uint32_t GetPlayingPosition() const = 0;

        virtual bool IsHeaderResopnsed() = 0;

        virtual ~IDownloadDriver()
        {}
    };

    struct MetaData;
    struct IInstance
    {
        typedef boost::shared_ptr<IInstance> p;

        /*
        virtual protocol::BlockMap::p GetBlockMap() = 0;
        virtual void AsyncGetBlock(uint32_t block_index, IUploadListener::p listener) = 0;
        virtual void AsyncGetSubPiece(const protocol::SubPieceInfo& subpiece_info, const boost::asio::ip::udp::endpoint& end_point, protocol::RequestSubPiecePacket const & packet, IUploadListener::p listener) = 0;
        virtual void AsyncAddSubPiece(const protocol::SubPieceInfo& subpiece_info, const SubPieceBuffer& buffer) = 0;
        // virtual void AsyncGetSubPieceForPlay(uint32_t start_postion, uint32_t max_count, IPlayerListener::p listener) = 0;
        // virtual void AsyncGetSubPieceForPlay(uint32_t start_postion, IPlayerListener::p listener) = 0;
        // virtual void AsyncGetSubPieceForPlay(const protocol::SubPieceInfo& subpiece_info, IPlayerListener::p listener) = 0;
        virtual bool GetSubPieceForPlay(uint32_t start_postion, std::vector<SubPieceBuffer> & buffers) = 0;
        virtual bool HasPiece(const protocol::PieceInfo& piece_info) = 0;
        virtual bool HasPiece(uint32_t start_postion) = 0;
        virtual bool HasSubPiece(const protocol::SubPieceInfo& subpiece_info) = 0;
        virtual bool HasRID() const = 0;

        virtual bool GetNextPieceForDownload(const protocol::PieceInfo &start_piece_index, protocol::PieceInfoEx& piece_for_download) = 0;
        virtual bool GetNextPieceForDownload(uint32_t start_position, protocol::PieceInfoEx& piece_for_download) = 0;
        virtual bool GetNextSubPieceForDownload(const protocol::SubPieceInfo &sub_subpiece_index, protocol::SubPieceInfo& subpiece_for_download) = 0;
        virtual bool GetNextSubPieceForDownload(uint32_t start_position, protocol::SubPieceInfo& subpiece_for_download) = 0;
        virtual RID GetRID() = 0;
        virtual void GetRidInfo(protocol::RidInfo &rid_info) = 0;
        virtual void GetUrls(std::set<protocol::UrlInfo>& url_s) = 0;
        virtual protocol::UrlInfo GetOriginalUrl() = 0;

        virtual void SetFileLength(uint32_t file_length) = 0;
        virtual uint32_t GetFileLength() = 0;
        virtual uint32_t GetDownloadBytes() = 0;
        virtual uint32_t GetBlockSize() = 0;
        virtual uint32_t GetBlockCount() = 0;
        virtual bool IsComplete() = 0;
        virtual bool IsFileComplete() = 0;

        virtual bool IsDownloading() = 0;
        virtual void FreeResourceHandle() = 0;
        virtual void AttachDownloadDriver(IDownloadDriver::p download_driver) = 0;
        virtual void DettachDownloadDriver(IDownloadDriver::p download_driver) = 0;

        virtual bool DoMakeContentMd5AndQuery(base::AppBuffer content_buffer) = 0;

        virtual void WeUploadSubPiece(uint32_t num) = 0;

        virtual float GetInstanceValue() = 0;
        virtual uint32_t GetDownloadedBlockCount() const = 0;

        virtual void GetBlockPosition(uint32_t block_index, uint32_t &offset, uint32_t &length) = 0;
        virtual void GetSubPiecePosition(const protocol::SubPieceInfo &subpiec_info, uint32_t &offset, uint32_t &length) = 0;
        virtual bool ParseMetaData(base::AppBuffer const & buffer) = 0;
        virtual MetaData& GetMetaData() = 0;
        virtual int GetRidOriginFlag() const = 0;
        virtual void SetRidOriginFlag(int flag) = 0;

        virtual void GetDownloadResourceData(downloadcenter::DownloadResourceData& res_data) = 0;
        virtual unsigned char GetResDownMode() const = 0;
        virtual bool IsSaveMode() const = 0;
        virtual void SetSaveMode(bool save_mode) = 0;

        virtual string GetResourceName() = 0;

        virtual void OnFileDownComplete() = 0;

        virtual void OnFileWriteFinish() = 0;

        virtual void SetIsOpenService(bool is_open_service) = 0;

        virtual bool IsOpenService() const = 0;*/


        virtual ~IInstance()
        {}
    };

    struct IStorage
    {
        typedef boost::shared_ptr<IStorage> p;

        virtual void Start(bool bUseDisk,  // 是否使用磁盘
            boost::uint64_t ullDiskLimit,   // 使用磁盘上限
            string DiskPathName,            // 磁盘使用路径
            string ConfigPath,              // 资源文件路径
            uint32_t storage_mode    // storage mode
           ) = 0;
        virtual void Start() = 0;
        virtual void Stop() = 0;

        // 根据创建Instance，此时没有RID以及文件长度等信息
        virtual IInstance::p CreateInstance(const protocol::UrlInfo& url_info, bool is_force = false) = 0;
        virtual IInstance::p CreateInstance(const protocol::UrlInfo& url_info, const protocol::RidInfo& rid_info, bool is_force = false) = 0;
        virtual IInstance::p CreateLiveInstance(const RID& rid, boost::uint16_t live_interval_in_seconds) = 0;

        // 通过RID来查询Instance
        virtual IInstance::p GetInstanceByRID(const RID& rid, bool is_check = true) = 0;
        virtual IInstance::p GetLiveInstanceByRid(const RID& rid) = 0;

        // 通过url来查询Instance
        virtual IInstance::p GetInstanceByUrl(const string& url, bool is_check = true) = 0;
        // 通过文件名来查询Instance
        virtual IInstance::p GetInstanceByFileName(const string& filename, bool is_check = true) = 0;
        // 该URL获得ResourceInfo, 包括文件大小等信息
        virtual void AttachRidByUrl(const string& url, const protocol::RidInfo& rid, MD5 content_md5, uint32_t content_bytes, int flag) = 0;
        // 获得了新的关于该Rid的Url
        virtual void AttachHttpServerByRid(const RID& rid, const std::vector<protocol::UrlInfo>& url_info_s) = 0;
        // 获得对应的mod_number, group_count的RID资源，即 RID % group_count == mod_number
        virtual void GetVodResources(std::set<RID>& rid_s, uint32_t mod_number, uint32_t group_count) = 0;
        virtual void GetLiveResources(std::set<RID>& rid_s, uint32_t mod_number, uint32_t group_count) = 0;
        // 删除url_info所对应的信息
        virtual void RemoveUrlInfo(const protocol::UrlInfo& url_info) = 0;
        // 根据查询Content信息返回的ERRORCODE获得信息
        virtual void AttachContentStatusByUrl(const string& url, bool is_need_to_add) = 0;
        // 获得了新的关于该url的文件名
        virtual void AttachFilenameByUrl(const string& url, string filename) = 0;

#ifdef DISK_MODE
        virtual void AttachSaveModeFilenameByUrl(const string& url, const string& web_url, string q_filename) = 0;
        // 获得总共使用了的磁盘空间
        virtual boost::int64_t GetUsedDiskSpace() = 0;
        virtual boost::int64_t GetFreeSize() const = 0;
        virtual boost::int64_t GetStoreSize() const = 0;
#endif  // #ifdef DISK_MODE

        virtual void SetWebUrl(string url, string web_url) = 0;

        virtual ~IStorage()
        {}
    };
}

#endif  // STORAGE_ISTORAGE_H
