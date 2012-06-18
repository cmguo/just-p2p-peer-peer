//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "storage/storage_base.h"
#include "storage/Storage.h"
#include "storage/StorageThread.h"
#include "storage/IStorage.h"
#include "storage/CfgFile.h"
#include "storage/Resource.h"
#include "storage/FileResource.h"
#include "storage/Instance.h"
#include "random.h"

#ifdef DISK_MODE
#include "storage/FileResourceInfo.h"
#endif

#include "base/util.h"

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/convenience.hpp>
namespace fs = boost::filesystem;

namespace storage
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("storage");

#ifdef DISK_MODE

    SpaceManager::p SpaceManager::Create(
        boost::asio::io_service & io_svc,
        string stor_path,
        boost::int64_t space_size,
        bool b_use_disk)
    {
        return SpaceManager::p(new SpaceManager(io_svc, stor_path, space_size, b_use_disk));
    }

    SpaceManager::SpaceManager(
        boost::asio::io_service & io_svc,
        string store_path,
        boost::int64_t space_size,
        bool b_use_disk)
        : io_svc_(io_svc)
        , store_size_(space_size),
        b_use_disk_(b_use_disk)
    {
        // need_space_size_ = 0;
        will_be_free_space_size_ = 0;
        curr_resource_files_total_size_ = 0;
        free_size_ = space_size;
        STORAGE_DEBUG_LOG("free_size_ = " << free_size_);

        boost::filesystem::path store_p(store_path);

        store_path_ = store_p.file_string();
        hidden_sub_path_ = (store_p / ("InvisibleFolder")).file_string();
#ifdef BOOST_WINDOWS_API
        TCHAR old_dir[MAX_PATH];
        uint32_t pathname_len = GetCurrentDirectory(MAX_PATH, old_dir);
        assert(pathname_len<MAX_PATH);
#endif
        if (STORAGE_MODE_NORMAL == Storage::Inst_Storage()->GetStorageMode())
        {
            OpenAndCreateStoreDir();
            Storage::Inst_Storage()->CreateHiddenDir(hidden_sub_path_);
        }
#ifdef BOOST_WINDOWS_API
        SetCurrentDirectory(old_dir);
#endif

#ifdef BOOST_WINDOWS_API
        framework::configure::Config conf;
        framework::configure::ConfigModule & storage_conf =
            conf.register_module("Storage");

        file_size_max_ = 200 * 1024;
        file_size_min_ = 100;
        storage_conf(CONFIG_PARAM_NAME_RDONLY("ResourceFileSizeMax", file_size_max_));
        storage_conf(CONFIG_PARAM_NAME_RDONLY("ResourceFileSizeMin", file_size_min_));
        file_size_max_ *= 1024 * 1024;
        file_size_min_ *= 1024;
#endif
    }

#ifdef BOOST_WINDOWS_API
    __int64 SpaceManager::GetDirectoryAvialbeSize(string DirPathName)
    {
        ULARGE_INTEGER ulDiskFreeSize;
        ulDiskFreeSize.QuadPart = 0;

        if (DirPathName.size() < 3)
        {
            return ulDiskFreeSize.QuadPart;
        }

        TCHAR DiskName[4];
        base::util::memcpy2(DiskName, sizeof(DiskName), DirPathName.c_str(), sizeof(TCHAR) * 3);
        DiskName[3] = 0;

        if (!::GetDiskFreeSpaceEx(DiskName, &ulDiskFreeSize, NULL, NULL))
        {
            ulDiskFreeSize.QuadPart = 0;
            return ulDiskFreeSize.QuadPart;
        }

        WIN32_FIND_DATA FindFileData;
        HANDLE hFind = INVALID_HANDLE_VALUE;

        string tmp_find_str(DirPathName);
        tmp_find_str.append(TEXT("\\*"));

        hFind = ::FindFirstFile(tmp_find_str.c_str(), &FindFileData);
        if (hFind == INVALID_HANDLE_VALUE)
        {
            return ulDiskFreeSize.QuadPart;
        }

        string cfilename(FindFileData.cFileName);
        assert((cfilename.size() == 1)&&(cfilename.find(TEXT('.')) == 0));

        while (::FindNextFile(hFind, &FindFileData) != 0)
        {
            cfilename.assign(FindFileData.cFileName);
            if ((cfilename.size() == 2) && (cfilename.find(TEXT("..")) == 0))
                continue;
            ulDiskFreeSize.QuadPart += FindFileData.nFileSizeLow;
            ulDiskFreeSize.HighPart += FindFileData.nFileSizeHigh;
        }
        ::FindClose(hFind);
        return ulDiskFreeSize.QuadPart;
    }
#endif

    void SpaceManager::OnRemoveResourceFinish(Resource::p resource_p, bool need_remove_file)
    {
        STORAGE_DEBUG_LOG(" file_name:" << resource_p->GetLocalFileName() << " file_size:" << resource_p->GetLocalFileSize());

        StorageThread::Post(boost::bind(&Resource::FreeDiskSpace, resource_p, need_remove_file));
    }

    void SpaceManager::OnFreeDiskSpaceFinish(uint32_t filesize, Resource::p resource_p)
    {
        if (removing_fileresource_set_.find(resource_p) == removing_fileresource_set_.end())
        {
            STORAGE_ERR_LOG("removing_fileresource_set_");
            return;
        }
        if (filesize != 0)
        {
            removing_fileresource_set_.erase(resource_p);
            OnReleaseDiskSpace(filesize, resource_p->GetDownMode());
        }
        else
        {
            STORAGE_ERR_LOG("Delete file error!");
        }

        DiskSpaceMaintain();
    }

    bool SpaceManager::CancelResourceRequest(Instance::p inst_p)
    {
        STORAGE_EVENT_LOG("inst_p = " << inst_p);
        std::set<Instance::p>::iterator it = pending_instance_need_resource_set.find(inst_p);
        if (it != pending_instance_need_resource_set.end())
        {
            pending_instance_need_resource_set.erase(inst_p);
            // need_space_size_ -= inst_p->GetResourceLength();
            return true;
        }
        return false;
    }

    void SpaceManager::GetDirFileNameList(std::set<string> &filename_list)
    {
        namespace fs = boost::filesystem;
        //// 这里不能直接clear, 因为传进来是个引用，如果clear会把原来里面的值清楚，导致BUG
        // filename_list.clear();
        if (b_use_disk_)
        {
            fs::path p(store_path_);
            try
            {
                if (fs::is_directory(p))
                {
                    for (fs::directory_iterator itr(p); itr != fs::directory_iterator(); ++itr)
                    {
                        try
                        {
                            if (fs::is_regular_file(itr->status()))
                            {
                                fs::path file_path = itr->path();
                                filename_list.insert(file_path.file_string());
                            }
                        }
                        catch(fs::filesystem_error &)
                        {
                        }
                    }
                }
            }
            catch(fs::filesystem_error &)
            {
            }
        }
    }

    void SpaceManager::AddToRemoveResourceList(Resource::p resource_p, uint32_t disk_file_size)
    {
        assert(resource_p);
        if (removing_fileresource_set_.find(resource_p) != removing_fileresource_set_.end())
        {
            return;
        }
        removing_fileresource_set_.insert(resource_p);

        if (DM_BY_ACCELERATE == resource_p->GetDownMode())
        {
            will_be_free_space_size_ += disk_file_size;
        }
    }

    void SpaceManager::DiskSpaceMaintain()
    {
        boost::uint64_t curr_free_disk_size = 0;
        if (!GetDiskFreeSpace(store_path_, curr_free_disk_size))
        {
            STORAGE_ERR_LOG("DiskSpaceMaintain::GetDiskFreeSpace error!");
            return;
        }
        //
        boost::uint64_t reserve_space_size = GetMinDiskFreeSpace(store_path_);
        boost::uint64_t min_free_size = curr_free_disk_size < free_size_ ? curr_free_disk_size : free_size_;
        boost::uint64_t min_init_size = 0;
        for (std::set<Instance::p>::iterator it = pending_instance_need_resource_set.begin(); it
            != pending_instance_need_resource_set.end(); ++it)
        {
            Instance::p resource_inst = (*it);
            boost::uint64_t inst_init_size = std::min((uint32_t)(2 * 1024 * 1024), resource_inst->GetResourceLength());
            if (0 == min_init_size)
            {
                min_init_size = inst_init_size;
                continue;
            }
            if (min_init_size > inst_init_size)
            {
                min_init_size = inst_init_size;
            }
        }

        STORAGE_DEBUG_LOG("min_free_size=" << min_free_size << ", min_init_size=" << min_init_size
            << ", reserve_space_size= " << reserve_space_size << ", free_size_" << free_size_
            << ", curr_free_disk_size=" << curr_free_disk_size << " "
            << removing_fileresource_set_.size());

        if (min_free_size <= min_init_size)
        {
            Storage::Inst_Storage()->RemoveOneInstance();
            return;
        }
        if (curr_free_disk_size <= reserve_space_size || free_size_ < 20 * 1024 * 1024)
        {
            Storage::Inst_Storage()->RemoveOneInstance();
        }

        Instance::p resource_inst;
        std::set<Instance::p>::iterator it = pending_instance_need_resource_set.begin();
        for (; it != pending_instance_need_resource_set.end();)
        {
            Instance::p resource_inst = (*it);

            if (resource_inst->GetStatus() != INSTANCE_APPLY_RESOURCE)
            {
                STORAGE_DEBUG_LOG("SpaceManager::DiskSpaceMaintain() resource_inst->GetStatus()!=INSTANCE_APPLY_RESOURCE ");
                pending_instance_need_resource_set.erase(it++);
                continue;
            }

            uint32_t init_size = std::min((uint32_t)(2 * 1024 * 1024), resource_inst->GetResourceLength());

            STORAGE_DEBUG_LOG("curr_free_disk_size = " << curr_free_disk_size << ", init_size = " << init_size << ", free_size_ = " << free_size_ << ", reult: " << ((curr_free_disk_size >= init_size)&&(free_size_ >= init_size)));
            if ((curr_free_disk_size >= init_size) && (free_size_ >= init_size))
            {
                if (OnCreateResource(resource_inst, init_size))
                {
                    OnAllocDiskSpace(init_size, resource_inst->GetResDownMode());
                    pending_instance_need_resource_set.erase(it++);
                    continue;
                }
            }
            it++;
        }  // for
    }

    Resource::p SpaceManager::OpenResource(const FileResourceInfo &resource_info)
    {
        if (b_use_disk_)
        {
            FileResource::p resource_p = FileResource::CreateResource(io_svc_, resource_info);

            if (resource_p)
            {
                OnAllocDiskSpace(resource_p->GetActualSize(), resource_p->GetDownMode());
                return resource_p;
            }
        }

        string cfgfilename = Storage::Inst_Storage()->GetCfgFilename(resource_info.file_path_);

        if (STORAGE_MODE_NORMAL == Storage::Inst_Storage()->GetStorageMode())
        {
            base::filesystem::remove_nothrow(boost::filesystem::path(resource_info.file_path_));
            base::filesystem::remove_nothrow(boost::filesystem::path(cfgfilename));
        }

        return Resource::p();
    }

    void SpaceManager::RequestResource(Instance::p resource_inst)
    {
        // uint32_t resource_length = resource_inst->GetResourceLength();
        // STORAGE_DEBUG_LOG(" free_size:" << free_size_ << " instance-size:" << resource_length);
        assert(pending_instance_need_resource_set.find(resource_inst) == pending_instance_need_resource_set.end());
        pending_instance_need_resource_set.insert(resource_inst);
        DiskSpaceMaintain();
        return;
    }

    bool SpaceManager::OnCreateResource(Instance::p resource_inst, uint32_t init_size)
    {
        if (false == b_use_disk_)
        {
            return false;
        }

        assert(resource_inst->GetStatus() == INSTANCE_APPLY_RESOURCE);
        string filename = resource_inst->GetResourceName();
        assert(filename.size()>0);
        uint32_t file_size = resource_inst->GetResourceLength();
        assert(file_size>0);
        STORAGE_DEBUG_LOG(" try create resource_name_" << (filename));
        string full_file_name;
        if (Storage::Inst_Storage()->IsFileNameNeedHide(filename))
        {
            full_file_name = (fs::path(GetHiddenSubPath()) / filename).file_string();
        }
        else
        {
            full_file_name = (fs::path(store_path_) / filename).file_string();
        }

        if (false == resource_inst->is_have_rename_)
        {
            string frn_url;
            if (!resource_inst->origanel_url_info_.url_.empty())
            {
                frn_url = resource_inst->origanel_url_info_.url_;
            }
            else if (!resource_inst->url_info_s_.empty())
            {
                protocol::UrlInfo t_ui = *resource_inst->url_info_s_.begin();
                frn_url = t_ui.url_;
            }
            string realname = Storage::Inst_Storage()->FindRealName(frn_url);
            if (!realname.empty())
            {
                string full_realname;
                boost::filesystem::path file_path(store_path_);
                Storage::Inst_Storage()->DoVerifyName(realname, file_path, full_realname);
                full_file_name = full_realname;
            }
        }

        string last_filename;
        FILE* file_handle = TryCreateFile(full_file_name, last_filename, init_size);
        if (file_handle != NULL)
        {
            STORAGE_DEBUG_LOG(" success! full_file_name" << (full_file_name));
            Resource::p resource_p = FileResource::CreateResource(io_svc_, file_size, last_filename, file_handle, Instance::p(),
                init_size);
            assert(resource_p);
            Storage::Inst_Storage()->OnCreateResourceSuccess(resource_p, resource_inst);
            return true;
        }
        STORAGE_DEBUG_LOG(" fail! full_file_name" << (full_file_name));
        return false;
    }

    FILE* SpaceManager::TryCreateFile(string filename, string &last_filename, uint32_t file_size)
    {
        if (false == b_use_disk_)
        {
            return NULL;
        }

        namespace fs = boost::filesystem;
        FILE* file_handle = NULL;

        boost::uint64_t curr_free_disk_size = 0;
        if (GetDiskFreeSpace(store_path_, curr_free_disk_size))
        {
            if (curr_free_disk_size < boost::uint64_t(file_size))
            {
                return NULL;
            }
        }
        else
        {
            return NULL;
        }

        fs::path source_file_name(filename);
        string ext = source_file_name.extension();
        string stem = (source_file_name.parent_path()/"/").file_string() + source_file_name.stem();

        string file_name(stem);

        int i = 0;
        while (true)
        {
            if (!Storage::Inst_Storage()->IsFilenameExist(filename))
            {
                string tppfilename = filename + tpp_extname;
                if (!Storage::Inst_Storage()->IsFilenameExist(tppfilename))
                {
                    break;
                }
            }
            std::ostringstream oss;
            oss << "(" << i << ")";
            string change_str = oss.str();
            ++i;
            filename = file_name + change_str + ext;
        }
        last_filename = filename + tpp_extname;
        file_handle = fopen(last_filename.c_str(), "w+b");
        if (NULL == file_handle)
        {
            if (ENOENT != errno)  // 2 -> no such file or directory
            {
                STORAGE_EVENT_LOG("can not create file! filename:" << (last_filename) << " error code: " << errno);
                return NULL;
            }
            std::ostringstream oss;
            oss << Random::GetGlobal().Next() << Random::GetGlobal().Next() << ".flv";
            string change_str = oss.str();
            if (Storage::Inst_Storage()->IsFileNameNeedHide(change_str))
            {
                last_filename = GetHiddenSubPath() + change_str;
            }
            else
            {
                last_filename = store_path_ + (change_str);
            }

            file_handle = fopen(last_filename.c_str(), "w+b");
            if (NULL == file_handle)
            {
                STORAGE_EVENT_LOG("can not create file! filename:" << last_filename << " error code:" << errno);
                return NULL;
            }
        }
        // resize
        if (file_size > 0)
        {
            if (0 == fseek(file_handle, file_size - 1, SEEK_SET) &&
                1 == fwrite("", 1, 1, file_handle) &&
                0 == fseek(file_handle, 0, SEEK_SET))
                return file_handle;
        }
        fclose(file_handle);
        file_handle = NULL;
        string tppfilename = filename + tpp_extname;
        base::filesystem::remove_nothrow(fs::path(tppfilename));
        return file_handle;
    }

    bool SpaceManager::GetDiskFreeSpace(string path, boost::uintmax_t &free_space_size)
    {
        if (false == b_use_disk_)
        {
            // any problem ?
            free_space_size = 50 * 1024 * 1024;  // 50MB
            return true;
        }

        namespace fs = boost::filesystem;
        try
        {
            fs::space_info info = fs::space(fs::path(path));
            free_space_size = info.free;
            return true;
        }
        catch(const fs::filesystem_error&)
        {
            return false;
        }
    }

    boost::uintmax_t SpaceManager::GetMinDiskFreeSpace(const string& path)
    {
        return 200*1024*1024;  // 200MB
    }

    bool SpaceManager::OpenAndCreateStoreDir()
    {
        if (false == b_use_disk_)
        {
            return true;
        }

        namespace fs = boost::filesystem;
        fs::path store_path(store_path_);
        try
        {
            // prevent create_directories from throwing exception
            if (base::filesystem::exists_nothrow(store_path) && !is_directory(store_path))
            {
                return false;
            }
            fs::create_directories(store_path);
            return true;
        }
        catch(const fs::filesystem_error&)
        {
            return false;
        }
    }

    void SpaceManager::OnAllocDiskSpace(uint32_t alloc_space, unsigned char down_mode)
    {
        if (DM_BY_ACCELERATE == down_mode)
        {
            free_size_ -= alloc_space;
            curr_resource_files_total_size_ += alloc_space;
            STORAGE_DEBUG_LOG("free_size=" << free_size_ << ", alloc_space=" << alloc_space);
        }
        else if (DM_BY_BHOSAVE == down_mode)
        {
        }
    }

    void SpaceManager::OnReleaseDiskSpace(uint32_t alloc_space, unsigned char down_mode)
    {
        if (DM_BY_ACCELERATE == down_mode)
        {
            free_size_ += alloc_space;
            curr_resource_files_total_size_ -= alloc_space;
            will_be_free_space_size_ -= alloc_space;
            STORAGE_DEBUG_LOG("free_size=" << free_size_ << ", alloc_space=" << alloc_space);
        }
        else if (DM_BY_BHOSAVE == down_mode)
        {
        }
    }

#endif  // DISK_MODE

}
