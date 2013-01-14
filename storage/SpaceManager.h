//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef STORAGE_SPACE_MANAGER_H
#define STORAGE_SPACE_MANAGER_H

#ifndef PEER_PC_CLIENT
#pragma once
#endif

#include "storage/IStorage.h"
#include "storage/CfgFile.h"
#include "storage/Resource.h"
#include "storage/Instance.h"

namespace storage
{
#ifdef DISK_MODE
    struct FileResourceInfo;

class SpaceManager: public boost::noncopyable, public boost::enable_shared_from_this<SpaceManager>
#ifdef DUMP_OBJECT
    , public count_object_allocate<SpaceManager>
#endif
{
    public:
    typedef boost::shared_ptr<SpaceManager> p;

    static SpaceManager::p Create(
        boost::asio::io_service & io_svc,
        string stor_path,
        boost::int64_t space_size,
        bool b_use_disk);

    friend class Storage;
    protected:
    SpaceManager(
        boost::asio::io_service & io_svc,
        string store_path,
        boost::int64_t space_size,
        bool b_use_disk);
    public:
    // 空间管理策略
    void RequestResource(Instance::p resource_inst);
    Resource::p OpenResource(const FileResourceInfo &resource_info);

    // 磁盘空间管理策略
    void DiskSpaceMaintain();
    void AddToRemoveResourceList(Resource::p resource_p, boost::uint32_t disk_file_size);
    bool CancelResourceRequest(Instance::p inst_p);
    void OnRemoveResourceFinish(Resource::p resource_p, bool need_remove_file);
    void OnFreeDiskSpaceFinish(boost::uint32_t filesize, Resource::p resource_p);
    // void OnFreeDiskSpaceFinish(boost::uint32_t filesize, Resource::p resource_p);
#ifdef PEER_PC_CLIENT
    boost::int64_t GetDirectoryAvialbeSize(string DirPathName);
#endif
    void OnAllocDiskSpace(boost::uint32_t alloc_space, unsigned char down_mode);
    void OnReleaseDiskSpace(boost::uint32_t alloc_space, unsigned char down_mode);

    string GetStorePath() const { return store_path_;}
    boost::int64_t GetFreeSize() const { return free_size_;}
    boost::int64_t GetStoreSize() const { return store_size_;}
    string GetHiddenSubPath() const { return hidden_sub_path_;}

    private:
    bool OnCreateResource(Instance::p resource_inst, boost::uint32_t init_size);
    void TryCreateResourceFile(string filename, boost::uint32_t file_size, Instance::p resource_inst);
    void GetDirFileNameList(std::set<string> &filename_list);
    bool OpenAndCreateStoreDir();
    bool GetDiskFreeSpace(string path, boost::uintmax_t &free_space_size);
    boost::uintmax_t GetMinDiskFreeSpace(const string& path);
    FILE* TryCreateFile(string filename, string &last_filename, boost::uint32_t file_size);

    private:
    boost::asio::io_service & io_svc_;
    std::set<Resource::p> removing_fileresource_set_;
    std::set<Instance::p> pending_instance_need_resource_set;

    bool b_use_disk_;

    string hidden_sub_path_;
    string store_path_;
    boost::int64_t store_size_;
    boost::int64_t curr_resource_files_total_size_;
    // boost::int64_t    need_space_size_;
    boost::int64_t will_be_free_space_size_;
    boost::int64_t free_size_;

    // config
    boost::uint32_t file_size_min_;
    boost::uint32_t file_size_max_;

};
#endif  // DISK_MODE

}

#endif  // STORAGE_SPACE_MANAGER_H
