//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

/*******************************************************************************
*
*  Filename: Storage.cpp
*
*******************************************************************************/

#include "Common.h"

#include "storage/storage_base.h"
#include "storage/Storage.h"
#include "storage/StorageThread.h"
#include "storage/NullResource.h"
#ifdef DISK_MODE
#include "storage/FileResourceInfo.h"
#endif

#include "storage/MemoryConsumptionMonitor.h"
#include "storage/InstanceMemoryConsumer.h"
#include "storage/LiveInstanceMemoryConsumer.h"
#include "network/Uri.h"
#include "base/util.h"
#include "p2sp/p2p/UploadStruct.h"
#include "p2sp/p2p/UploadModule.h"

#include <framework/configure/Config.h>

#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/convenience.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/date_time.hpp>

namespace fs = boost::filesystem;

namespace storage
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("storage");

    Storage::p Storage::inst_;

    Storage::Storage(
        boost::asio::io_service & io_svc)
        : io_svc_(io_svc)
        , is_running_(false)
        , memory_consumption_monitor_timer_(global_second_timer(), 5 * 1000, boost::bind(&Storage::OnTimerElapsed, this, &memory_consumption_monitor_timer_))
#ifdef DISK_MODE
        , space_manager_timer_(global_second_timer(), 5 * 1000, boost::bind(&Storage::OnTimerElapsed, this, &space_manager_timer_))
        , res_info_timer_(global_second_timer(), 60 * 1000, boost::bind(&Storage::OnTimerElapsed, this, &res_info_timer_))
#endif
    {
#ifdef DISK_MODE
        need_save_info_to_disk = (false);
        prefix_filename_ = ("[PPVA]");
        storage_mode_ = STORAGE_MODE_NORMAL;
#endif  // #ifdef DISK_MODE
    }

    IStorage::p Storage::Inst()
    {
        return inst_;

    }

    Storage::p Storage::CreateInst(
        boost::asio::io_service & io_svc)
    {
        inst_.reset(new Storage(io_svc));
        return inst_;
    }

    Storage::p Storage::Inst_Storage()
    {
        return inst_;
    }

    void Storage::Start(
        bool bUseDisk,
        boost::uint64_t ullDiskLimit,
        string DiskPathName,
        string ConfigPath,
        uint32_t storage_mode
       )
    {
        STORAGE_EVENT_LOG("storage system start.............");
#ifdef BOOST_WINDOWS_API
        framework::configure::Config conf;
        framework::configure::ConfigModule & storage_conf =
            conf.register_module("Storage");
#endif

#ifdef DISK_MODE
        if (STORAGE_MODE_NORMAL == storage_mode)
        {
            storage_mode_ = STORAGE_MODE_NORMAL;
        }
        else if (STORAGE_MODE_READONLY == storage_mode)
        {
            storage_mode_ = STORAGE_MODE_READONLY;
        }
        else
        {
            storage_mode_ = STORAGE_MODE_NORMAL;
        }
#endif  // #ifdef DISK_MODE

#ifdef BOOST_WINDOWS_API
        string DefalutStorePath = "d:\\shareFLV\\";
        storage_conf(CONFIG_PARAM_RDONLY(DefalutStorePath));
        if (DiskPathName.size() == 0)
        {
            DiskPathName = DefalutStorePath;
        }
        else
        {
            if (DiskPathName.rfind(('\\')) != DiskPathName.size() - 1)
            {
                DiskPathName.append(("\\"));
            }
        }
#endif

#ifdef BOOST_WINDOWS_API
        boost::uint64_t disk_limit_size = ullDiskLimit / 1024 / 1024;
        storage_conf(CONFIG_PARAM_NAME_RDONLY("DiskLimitSize", disk_limit_size));
        disk_limit_size = disk_limit_size * 1024 * 1024;
#else
        boost::uint64_t disk_limit_size = ullDiskLimit;
#endif

        use_disk_ = bUseDisk;

        memory_consumption_monitor_timer_.start();

#ifdef DISK_MODE
        resource_data_path_ = GetResourceDataPath(ConfigPath);
        CreateDirectoryRecursive(resource_data_path_);

        boost::filesystem::path config_path(resource_data_path_);

        resourceinfo_file_ = (config_path / ("ResourceInfo.dat")).file_string();
        resourceinfo_bak_file_ = (config_path / ("ResourceInfo.dat.bak")).file_string();
        STORAGE_EVENT_LOG(" ConfigPath = " << (ConfigPath) << ", ResourceFile = " << (resourceinfo_file_));

        string store_path;
        store_path.assign(DiskPathName);
        space_manager_ = SpaceManager::Create(io_svc_, store_path, disk_limit_size, bUseDisk);
        STORAGE_EVENT_LOG(" store_path:" << (store_path) << " disk_limit_size:" << disk_limit_size << " bUseDisk" << bUseDisk);

        StorageThread::Inst().Start();
        space_manager_timer_->start();
        res_info_timer_->start();
#endif  // #ifdef DISK_MODE

#ifdef BOOST_WINDOWS_API
        Performance::Inst()->Start();
#endif

#ifdef DISK_MODE
        if (STORAGE_MODE_READONLY == GetStorageMode())
        {
            LoadResInfoOnReadMode();
        }
        else
        {
            LoadResourceInfoFromDisk();
        }
#endif  // #ifdef DISK_MODE

        is_running_ = true;

#ifdef DISK_MODE
        SaveResourceInfoToDisk();
        RemoveExpiredInvisibleFiles();
#endif  // #ifdef DISK_MODE

        STORAGE_EVENT_LOG("storage system start success!");
    }

    void Storage::Start()
    {
        string store_path(("FavoriteVideo"));
        string config_path;
        Start(true, 400 * 1024 * 1024, store_path, config_path, STORAGE_MODE_NORMAL);  // 400M
    }

    // storage被通知instance关闭，storage取消instance的资源申请，释放资源空间
    void Storage::OnInstanceCloseFinish(Instance::p instance_p)
    {
        instance_set_.erase(instance_p);
    }

    void Storage::OnResourceCloseFinish(Instance::p instance_p,
        Resource::p resource_p, bool need_remove_file)
    {
#ifdef DISK_MODE
        space_manager_->CancelResourceRequest(instance_p);

        if (instance_p->GetStatus() == INSTANCE_REMOVING)
        {
            if (resource_p)
            {
                space_manager_->OnRemoveResourceFinish(resource_p, need_remove_file);
            }
        }
#endif  // DISK_MODE
    }

    void Storage::Stop()
    {
        if (is_running_ == false)
            return;

        STORAGE_EVENT_LOG("storage system stop!");
#ifdef DISK_MODE
        space_manager_timer_->cancel();
        res_info_timer_->cancel();
        SaveResourceInfoToDisk();
#endif  // #ifdef DISK_MODE

        memory_consumption_monitor_timer_.cancel();

        is_running_ = false;
        std::set<Instance::p>::const_iterator it = instance_set_.begin();
        for (; it != instance_set_.end(); it++)
        {
            (*it)->Stop();
        }
#ifdef BOOST_WINDOWS_API
        Performance::Inst()->Stop();
#endif
        rid_instance_map_.clear();
        url_instance_map_.clear();
        instance_set_.clear();

#ifdef DISK_MODE
        StorageThread::Inst().Stop();
#endif  // #ifdef DISK_MODE

        STORAGE_TEST_DEBUG("Storage stop complate");
        inst_.reset();
    }

    IInstance::p Storage::CreateInstance(const protocol::UrlInfo& url_info, const protocol::RidInfo& rid_info, bool is_force)
    {
        if (is_running_ == false)
        {
            return IInstance::p();
        }
#ifdef DISK_MODE
        CheckResourceFromDisk();
#endif  // #ifdef DISK_MODE

        std::map<string, Instance::p>::const_iterator it = url_instance_map_.find(url_info.url_);
        std::map<RID, Instance::p>::const_iterator rid_it = rid_instance_map_.find(rid_info.GetRID());

        if (rid_info.HasRID() && IsRidInfoValid(rid_info))
        {
            if (rid_it != rid_instance_map_.end())
            {
                Instance::p t_inst = rid_it->second;
                // 找到一个Instance
                if (!is_force)
                {
                    STORAGE_EVENT_LOG("CreateInstance " << rid_info);
                    t_inst->AddUrlInfo(url_info);
                    url_instance_map_.insert(std::make_pair(url_info.url_, t_inst));
                    if (t_inst->IsComplete())
                    {
                        STORAGE_DEBUG_LOG("local_complete_ = true");
                        t_inst->local_complete_ = true;
                    }
                    return t_inst;
                }
                // 强迫性
                t_inst->RemoveUrl(url_info.url_);
                url_instance_map_.erase(url_info.url_);
                rid_instance_map_.erase(rid_info.GetRID());
                STORAGE_EVENT_LOG("CreateInstance OK, " << rid_info << (is_force?"force!":""));
            }
            else
            {
                // no such rid! create new
                if (it != url_instance_map_.end())
                {
                    Instance::p t_inst = it->second;
                    RemoveInstance(t_inst, true);
                }
            }
        }
        else if (it != url_instance_map_.end())
        {
            Instance::p t_inst = it->second;
            if (!is_force)
            {
                STORAGE_EVENT_LOG("CreateInstance " << url_info << (is_force?"force!":""));
                t_inst->AddUrlInfo(url_info);  // ?
                // do not insert to rid_instance_map_, should use AttachRidByUrl
                if (t_inst->IsComplete())
                {
                    STORAGE_DEBUG_LOG("local_complete_ = true");
                    t_inst->local_complete_ = true;
                }
                return t_inst;
            }
            t_inst->RemoveUrl(url_info.url_);
            url_instance_map_.erase(url_info.url_);
            STORAGE_EVENT_LOG("CreateInstance OK, " << url_info << (is_force?"force!":""));
        }

        STORAGE_EVENT_LOG("CreateInstance URL " << url_info << (is_force?"force!":""));
        Instance::p inst_p = Instance::Create(url_info);
        if (!inst_p)
        {
            return inst_p;
        }
        if (is_force)
        {
            inst_p->SetPureDownloadMode(true);
        }
        inst_p->Start();
        instance_set_.insert(inst_p);
        url_instance_map_.insert(std::make_pair(url_info.url_, inst_p));
        STORAGE_EVENT_LOG("CreateInstance success!");
        return inst_p;
    }

    IInstance::p Storage::CreateInstance(const protocol::UrlInfo& url_info, bool is_force)
    {
        return CreateInstance(url_info, protocol::RidInfo(), is_force);
    }

    IInstance::p Storage::CreateLiveInstance(const RID& rid, boost::uint16_t live_interval_in_seconds)
    {
        if (is_running_ == false)
        {
            return IInstance::p();
        }

        std::map<RID, LiveInstance::p>::const_iterator rid_iter = rid_to_live_instance_map_.find(rid);

        if (rid_iter != rid_to_live_instance_map_.end())
        {
            return rid_iter->second;
        }

        LiveInstance::p live_instance(new LiveInstance(rid, live_interval_in_seconds));

        live_instance->Start(shared_from_this());
        rid_to_live_instance_map_[rid] = live_instance;

        STORAGE_EVENT_LOG("CreateLiveInstance succeeded.");

        return live_instance;
    }

    void Storage::AddInstanceToRidMap(Instance::p pointer)
    {
        if (is_running_ == false)
            return;
        if (!pointer)
            return;
        STORAGE_EVENT_LOG("AddInstanceToRidMap!instance: " << pointer);

        RID rid = pointer->GetRID();
        assert(!rid.is_empty());
        std::map<RID, Instance::p>::iterator iter = rid_instance_map_.find(rid);
        // rid重复，并且文件存在
        if (iter != rid_instance_map_.end())
        {
            Instance::p tmp_inst = iter->second;
#ifdef DISK_MODE
            if (CheckInstanceResource(tmp_inst))
            {
                //  pointer是新的instance，tmp_inst是老的instance
                DoMerge(tmp_inst, pointer);
                return;
            }
#else
            return;
#endif  // #ifdef DISK_MODE

        }
        rid_instance_map_.insert(std::make_pair(rid, pointer));
        return;
    }

    void Storage::DoMerge(Instance::p instance_old, Instance::p instance_new)
    {
        if (false == is_running_)
        {
            return;
        }

        if (instance_old == instance_new)
            return;

        if (instance_new->GetDownloadBytes() >= instance_old->GetDownloadBytes())
        {
            MergeInstance(instance_new, instance_old);
        }
        else
        {
            MergeInstance(instance_old, instance_new);  // new -> old
        }
    }

    // 将instance2融合到instance1里，instance_p2 == > instance_p1
    void Storage::MergeInstance(Instance::p instance_p1, Instance::p instance_p2)
    {
        if (is_running_ == false)
        {
            return;
        }
        // 更新Storage的url和rid列表，将原来与inst2匹配的url和rid删除，并与inst1匹配
        STORAGE_EVENT_LOG("MergeInstance! merge inst2: " << instance_p2 << " To inst1: " << instance_p1);
        assert(!instance_p1->GetRID().is_empty());
        // check move
        if (instance_p2->IsSaveMode())
        {
            instance_p1->web_url_ = instance_p2->web_url_;
            instance_p1->SetSaveMode(true);
            // version 3
            instance_p1->down_mode_ = instance_p2->down_mode_;        // 资源下载方式
            instance_p1->web_url_ = instance_p2->web_url_;                // 页面地址
            instance_p1->file_duration_in_sec_ = instance_p2->file_duration_in_sec_;   // 时长
            instance_p1->last_write_time_ = instance_p2->last_write_time_;       // 该资源上一次写磁盘的时间
            instance_p1->data_rate_ = instance_p2->data_rate_;              // 码流率
            instance_p1->file_name_ = instance_p2->file_name_;  //
            instance_p1->is_open_service_ = instance_p2->is_open_service_;
            instance_p1->qname_ = instance_p2->qname_;
            instance_p2->SetSaveMode(false);
        }

        std::vector<protocol::UrlInfo> in_url_info_s;
        instance_p2->GetUrls(in_url_info_s);
        // url
        assert(in_url_info_s.size() >= 1);
        // instance_p1->AddUrlInfo(in_url_info_s);
        for (uint32_t i = 0; i < in_url_info_s.size(); i++)
        {
            uint32_t dc = url_instance_map_.erase(in_url_info_s[i].url_);
            assert(dc == 1);
            url_instance_map_.insert(std::make_pair(in_url_info_s[i].url_, instance_p1));
            STORAGE_DEBUG_LOG("合并URL: " << in_url_info_s[i].url_ << "到  instance: " << instance_p1);
        }
        instance_p1->AddUrlInfo(in_url_info_s);

        // rid
        if (!instance_p2->GetRID().is_empty())
        {
            uint32_t dc = rid_instance_map_.erase(instance_p2->GetRID());
            STORAGE_DEBUG_LOG("Erase " << instance_p2->GetRID() << " ( " << instance_p2 << " ) from rid_instance_map_");
            assert(dc == 1);
            assert(!instance_p1->GetRID().is_empty());
            rid_instance_map_.insert(std::make_pair(instance_p1->GetRID(), instance_p1));
            STORAGE_DEBUG_LOG("Add " << instance_p2->GetRID() << " ( " << instance_p1 << " ) to rid_instance_map_");
        }

        // 将instance_p2资源拷贝到instance_p1
        instance_p2->BeMergedTo(instance_p1);  // 完成合并，并通知DownloadDriver迁移
    }

    // instance向spacemanager申请资源
    void Storage::ApplyResource(Instance::p resource_inst)
    {
        if (is_running_ == false)
        {
            return;
        }
#ifdef DISK_MODE
        space_manager_->RequestResource(resource_inst);
#else
        // create NullResource and notify
        string filename = resource_inst->GetResourceName();
        uint32_t file_size = resource_inst->GetResourceLength();
        uint32_t init_size = (std::min)((uint32_t)(2 * 1024 * 1024), resource_inst->GetResourceLength());
        Resource::p resource_p = NullResource::CreateResource(io_svc_, file_size, filename, Instance::p(), init_size);
        assert(resource_p);
        Storage::Inst_Storage()->OnCreateResourceSuccess(resource_p, resource_inst);
#endif  // #ifdef DISK_MODE

    }

    // resource创建成功，绑定resource和instance, 将信息写入ResourceInfo文件中
    void Storage::OnCreateResourceSuccess(Resource::p resource_p, Instance::p instance_p)
    {
        assert(resource_p);
        assert(instance_p);
        instance_p->AttachResource(resource_p, false);
    }

#ifdef DISK_MODE
    // 遍历instance_set_, 将Instace信息保存到Resourceinfo，同时保存原资源信息到bak
    void Storage::SaveResourceInfoToDisk()
    {
        if (is_running_ == false)
        {
            return;
        }
        if (STORAGE_MODE_READONLY == Storage::Inst_Storage()->GetStorageMode())
        {
            return;
        }
        if (false == use_disk_)
        {
            return;
        }

        int count = 0;
        ResourceInfoListFile r_file;

        CheckResourceFromDisk();
        CreateDirectoryRecursive(resource_data_path_);
        string resourceinfo_file_tmp = resourceinfo_file_ + (".tmp");
        if (r_file.SecCreate(resourceinfo_file_tmp.c_str()))
        {
            // 遍历instance_set_, 将Instace信息保存到Resourceinfo，并加上安全防护
            std::vector<FileResourceInfo> r_info_vec;

            std::set<Instance::p>::const_iterator it = instance_set_.begin();
            for (; it != instance_set_.end(); it++)
            {
                Instance::p inst = *it;
                FileResourceInfo r_info;
                if (!inst->GetFileResourceInfo(r_info))
                {
                    continue;
                }
                r_info_vec.push_back(r_info);
            }
            count = r_info_vec.size();
            r_file.AddResourceInfo(r_info_vec);
            r_file.SecClose();

            // move to resourceinfo
#ifdef BOOST_WINDOWS_API
            ::MoveFileEx(resourceinfo_file_tmp.c_str(), resourceinfo_file_.c_str(), MOVEFILE_REPLACE_EXISTING
                | MOVEFILE_COPY_ALLOWED | MOVEFILE_WRITE_THROUGH);
#else
            boost::system::error_code ec;
            fs::remove(fs::path(resourceinfo_file_), ec);
            if (!ec)
            {
                boost::system::error_code ec_rename;
                base::filesystem::rename_nothrow(fs::path(resourceinfo_file_tmp), fs::path(resourceinfo_file_), ec_rename);
            }
            else
                STORAGE_DEBUG_LOG("ResourceInfoFile Delete Failed: " << e.what());
#endif
        }
        else
        {
            // 打不开，不尝试了
            r_file.SecClose();
        }

        const string resinfo = space_manager_->store_path_ + ("ResourceInfo.dat");
        if (resinfo != resourceinfo_file_ && base::filesystem::exists_nothrow(fs::path(resinfo)))
        {
            boost::system::error_code ec;
            fs::remove(fs::path(resinfo), ec);
        }
        STORAGE_EVENT_LOG("SaveResourceInfoToDisk! resource count:" << count);
    }

    bool Storage::CreateDirectoryRecursive(const string& directory)
    {
        if (use_disk_ && false == base::filesystem::exists_nothrow(fs::path(directory)))
        {
            try
            {
                return fs::create_directories(fs::path(directory));
            }
            catch(fs::filesystem_error &)
            {
                return false;
            }
        }
        return true;
    }

    bool Storage::CreateHiddenDir(const string& dirpath)
    {
        if (CreateDirectoryRecursive(dirpath))
        {
#ifdef BOOST_WINDOWS_API
        if (!SetFileAttributes(dirpath.c_str(), FILE_ATTRIBUTE_HIDDEN))
        {
            return false;
        }
#endif
            return true;
        }
        return false;
    }

    bool Storage::IsFileNameNeedHide(const string& filename)
    {
        fs::path filepath(filename);
        return IsFileExtNeedHide(filepath.extension());
    }

    bool Storage::IsFileExtNeedHide(const string& fileext)
    {
        if (fileext == (".flv")
            || fileext == (".hlv")
            || fileext == (".mp4")
            || fileext == (".f4v")
            || fileext == (".mp3")
            || fileext == (".wma")
            || fileext == (".tpp"))
        {
            return false;
        }
        return true;
    }

    bool Storage::CheckInstanceResource(Instance::p inst)
    {
        if (!inst)
        {
            return false;
        }
        if (inst->IsDownloading() || inst->IsUploading())
        {
            return true;
        }

        if (false == use_disk_)
        {
            return true;
        }

        const string resource_name = inst->GetResourceName();
        STORAGE_EVENT_LOG("检查文件资源：" << (resource_name));

        if (STORAGE_MODE_NORMAL == GetStorageMode() &&
            false == base::filesystem::exists_nothrow(fs::path(space_manager_->store_path_)))
        {
                RemoveInstance(inst, false);
                return false;
        }

        // 文件夹不存在，创建文件
        CreateDirectoryRecursive(space_manager_->store_path_.c_str());
        CreateHiddenDir(space_manager_->GetHiddenSubPath());

        string tmp_check_name = resource_name;
#ifdef BOOST_WINDOWS_API
        if (tmp_check_name.find(TEXT(":")) != 1)
#else
        if (tmp_check_name.find("/") != 0)
#endif
        {
            if (IsFileNameNeedHide(tmp_check_name))
            {
                tmp_check_name = (fs::path(space_manager_->GetHiddenSubPath()) / tmp_check_name).file_string();
            }
            else
            {
                tmp_check_name = (fs::path(space_manager_->store_path_) / tmp_check_name).file_string();
            }
        }

        if (false == resource_name.empty()  // 文件名非空
            && false == base::filesystem::exists_nothrow(fs::path(tmp_check_name)))
        {
                RemoveInstance(inst, false);
                if (STORAGE_MODE_NORMAL == GetStorageMode())
                {
                string tmp_file = GetCfgFilename(tmp_check_name);
                boost::system::error_code ec;
                fs::remove(fs::path(tmp_file), ec);
                if (ec)
                    return false;
                }
                return false;
            }
        else
        {
            return true;
        }
    }

    void Storage::CheckResourceFromDisk()
    {
        STORAGE_EVENT_LOG("检查磁盘资源 ");

        if (false == use_disk_)
        {
            return;
        }

        if (STORAGE_MODE_NORMAL == GetStorageMode())
        {
            CreateDirectoryRecursive(space_manager_->store_path_.c_str());
            CreateHiddenDir(space_manager_->GetHiddenSubPath());
        }

        std::map<Instance::p, string> need_remove_inst;

        for (std::set<Instance::p>::const_iterator i = instance_set_.begin(); i != instance_set_.end(); ++i)
        {
            if ((*i)->IsDownloading() || (*i)->IsUploading())
            {
                continue;
            }

            const string resource_name = (*i)->GetResourceName();
            string tmp_check_name = resource_name;
#ifdef BOOST_WINDOWS_API
            if (tmp_check_name.find(TEXT(":")) != 1)
#else
            if (tmp_check_name.find("/") != 0)
#endif
            {
                if (IsFileNameNeedHide(tmp_check_name))
                {
                    tmp_check_name = (fs::path(space_manager_->GetHiddenSubPath()) / tmp_check_name).file_string();
                }
                else
                {
                    tmp_check_name = (fs::path(space_manager_->store_path_) / tmp_check_name).file_string();
                }
            }

            if (false == resource_name.empty() && false == base::filesystem::exists_nothrow(fs::path(tmp_check_name)))
            {
                need_remove_inst.insert(std::make_pair(*i, tmp_check_name));
            }
        }

        for (std::map<Instance::p, string>::const_iterator i = need_remove_inst.begin();
            i != need_remove_inst.end(); ++i)
        {
            RemoveInstance(i->first, false);
            if (STORAGE_MODE_NORMAL == GetStorageMode())
            {
                base::filesystem::remove_nothrow(fs::path(GetCfgFilename(i->second)));
            }
        }
    }

    void Storage::LoadResInfoOnReadMode()
    {
        if (false == use_disk_)
        {
            return;
        }

        ResourceInfoListFile r_file;
        STORAGE_DEBUG_LOG("LoadResourceData: " << (resourceinfo_file_));
        if (!r_file.SecOpen(resourceinfo_file_.c_str()))
        {
            r_file.SecClose();
            STORAGE_DEBUG_LOG("");
        }
        else
        {
            bool bEof = false;
            while (true)
            {
                FileResourceInfo r_info;
                if (false == r_file.GetResourceInfo(r_info, bEof))
                {
                    break;
                }

                if (false == r_info.rid_info_.GetRID().is_empty() && rid_instance_map_.count(r_info.rid_info_.GetRID()))
                {
                    continue;
                }

                // 检查url一致性！
                int url_count = 0;
                for (std::vector<protocol::UrlInfo>::iterator it_url = r_info.url_info_.begin(); it_url != r_info.url_info_.end(); ++it_url)
                {
                    // 如果有Url，不计数
                    if (url_instance_map_.find((*it_url).url_) != url_instance_map_.end())
                    {
                        continue;
                    }
                    url_count++;
                }

                // 有Url，删除文件和cfg文件
                if ((url_count == 0) && (r_info.rid_info_.GetRID().is_empty()))
                {
                    continue;
                }

                string cfgfile = GetCfgFilename(r_info.file_path_);

                // 检查资源文件和长度
                Resource::p resource_p = space_manager_->OpenResource(r_info);
                if (!resource_p || r_info.url_info_.size() == 0)
                {
                    continue;
                }

                if (r_info.down_mode_ == DM_BY_ACCELERATE && false == IsRidInfoValid(r_info.rid_info_))
                {
                    STORAGE_WARN_LOG("RIDInfo Invalid: " << r_info.rid_info_);
                    continue;
                }

                Instance::p pointer = Instance::Open(r_info, resource_p);
                STORAGE_EVENT_LOG("OpenResource Success! local_file_name:" << r_info.file_path_
                    << " 已下载：" << pointer->GetDownloadBytes() << "/" << pointer->GetFileLength() << " rid_info:" << r_info.rid_info_);

                // 创建Instance
                assert(pointer);
                pointer->Start();

                // 添加Instance到Storage
                AddInstanceToStorage(pointer);
            }

            r_file.SecClose();
            if (false == bEof)
            {
                boost::system::error_code ec;
                fs::remove(fs::path(resourceinfo_file_), ec);
                STORAGE_EVENT_LOG("LoadResourceInfoFromDisk!资源文件非法!" << resourceinfo_file_);
            }

            CheckResourceFromDisk();

            for (std::set<Instance::p>::iterator it = instance_set_.begin(); it != instance_set_.end(); ++it)
            {
                (*it)->FreeResourceHandle();
            }
        }
    }

    // ----------------------------------------------------------------------
    // 装载磁盘资源信息
    void Storage::LoadResourceInfoFromDisk()
    {
        if (false == use_disk_)
        {
            return;
        }

        int count = 0;

        std::set<string> filename_list;  // 包含完整路径

        //  把磁盘缓存目录（正常目录和隐藏目录)中的所有文件读到filename_list中
        // 正常目录
        space_manager_->GetDirFileNameList(filename_list);
        // 隐藏目录
        GetDirFileList(space_manager_->GetHiddenSubPath(), filename_list);

        //
        // 确定资源信息文件的位置
        // TODO:yujinwu:2010/11/27:有没有必要在:\favorateVideo中查找resourceInfo.dat?
        //
        string resinfo, resinfo_bak;
        resinfo = space_manager_->store_path_ + ("ResourceInfo.dat");
        resinfo_bak = space_manager_->store_path_ + ("ResourceInfo.dat.bak");

        string read_resinfo_file = resourceinfo_file_;
        if (base::filesystem::exists_nothrow(fs::path(resinfo)))
        {
            if (base::filesystem::exists_nothrow(fs::path(resourceinfo_file_)))
            {
                if (resinfo != resourceinfo_file_)
                {
                    boost::system::error_code ec;
                    fs::remove(fs::path(resinfo), ec);
                }
            }
            else
            {
                read_resinfo_file = resinfo;
            }
        }

        if (base::filesystem::exists_nothrow(fs::path(resinfo_bak)))
        {
            boost::system::error_code ec;
            fs::remove(fs::path(resinfo_bak), ec);
        }

        if (base::filesystem::exists_nothrow(fs::path(resourceinfo_bak_file_)))
        {
            boost::system::error_code ec;
            fs::remove(fs::path(resourceinfo_bak_file_), ec);
        }

        bool is_name_use_ppva = false;

        //
        //  每一个未下载完全的视频文件有一个对应的配置文件，以.cfg结尾
        //  resource_data_path_是存放配置文件的目录
        //  以下读取resource_data_path_中的所有的配置文件列表
        //

        std::set<string> app_file_list;
        GetDirFileList(resource_data_path_, app_file_list);

        // 读取上次存放资源信息
        ResourceInfoListFile r_file;
        if (!r_file.SecOpen(read_resinfo_file.c_str()))
        {
            r_file.SecClose();
            boost::system::error_code ec;
            fs::remove(fs::path(read_resinfo_file), ec);
            STORAGE_EVENT_LOG("LoadResourceInfoFromDisk!读取资源信息失败!" << read_resinfo_file);
        }
        else
        {
            // TODO:yujinwu:2010/11/27:2.0以后是否还有必要做迁移？定义fs::path有许多的磁盘I/O操作
            // 迁移cfg文件，兼容老版本
            for (std::set<string>::iterator it = filename_list.begin(); it != filename_list.end(); it++)
            {
                fs::path filename(it->c_str());
                string extname = filename.extension();
                if (extname == cfg_extname)
                {
                    fs::path app_path_cfg = fs::path(resource_data_path_) / filename.filename();
                    boost::system::error_code ec;
                    base::filesystem::rename_nothrow(filename, app_path_cfg, ec);
                    STORAGE_EVENT_LOG("MoveFile From: " << filename << " To: " << app_path_cfg);
                }
            }

            bool bEof = false;
            while (true)
            {
                FileResourceInfo r_info;
                if (false == r_file.GetResourceInfo(r_info, bEof))
                {
                    break;
                }

                // 如果缓存目录位置改变，需要将隐藏的文件进行迁移
                if (IsFileNameNeedHide(r_info.file_path_) &&
                    false == boost::algorithm::istarts_with(r_info.file_path_, space_manager_->GetHiddenSubPath()))
                {
                    fs::path old_path(r_info.file_path_);
                    fs::path new_path(space_manager_->GetHiddenSubPath());
                    new_path /= old_path.filename();
                    if (false == base::filesystem::exists_nothrow(old_path))
                    {
                        // 旧文件已经不存在了，不需迁移
                        continue;
                    }
                    else if (old_path != new_path)
                    {
                        boost::system::error_code ec;
                        base::filesystem::rename_nothrow(old_path, new_path, ec);
                        if (ec)
                        {
                            // 迁移失败
                            continue;
                        }
                        r_info.file_path_ = new_path.file_string();
                    }
                }

                //
                // 检查RID一致性！
                // 若和其他资源RID相同，本资源为多余，需要删除
                //
                if (r_info.rid_info_.HasRID())
                {
                    // 有rid, 删除文件和cfg文件
                    if (rid_instance_map_.find(r_info.rid_info_.GetRID()) != rid_instance_map_.end())
                    {
                        RemoveFileAndConfigFile(r_info);
                        continue;
                    }
                }

                //
                // 检查url一致性！
                // 若所有Url都已经在url_instance_map_中出现过，本资源为多余，需要删除
                //
                bool all_urls_exist = true;
                for (std::vector<protocol::UrlInfo>::iterator it_url = r_info.url_info_.begin();
                     it_url != r_info.url_info_.end();
                     it_url++)
                {
                    if (url_instance_map_.find((*it_url).url_) == url_instance_map_.end())
                    {
                        // 只要有一个url没出现过，就不认为是多余资源
                        all_urls_exist = false;
                        break;
                    }
                }

                // 所有Url都已经存在，删除文件和cfg文件
                if (true == all_urls_exist &&
                    false == r_info.rid_info_.HasRID())
                {
                    RemoveFileAndConfigFile(r_info);
                    continue;
                }

                string cfgfile = GetCfgFilename(r_info.file_path_);
                app_file_list.erase(cfgfile);

                // 检查资源文件和长度
                Resource::p resource_p = space_manager_->OpenResource(r_info);
                if (!resource_p || r_info.url_info_.size() == 0)
                {
                    RemoveFileAndConfigFile(r_info);
                    continue;
                }

                if (r_info.down_mode_ == DM_BY_ACCELERATE && false == IsRidInfoValid(r_info.rid_info_))
                {
                    STORAGE_WARN_LOG("RIDInfo Invalid: " << r_info.rid_info_);
                    continue;
                }

                Instance::p pointer = Instance::Open(r_info, resource_p);
                STORAGE_EVENT_LOG("OpenResource Success! local_file_name:" << (r_info.file_path_)
                    << " " << pointer->GetDownloadBytes() << "/" << pointer->GetFileLength() << " rid_info:" << r_info.rid_info_);
                //
                filename_list.erase(r_info.file_path_);
                app_file_list.erase(GetCfgFilename(r_info.file_path_));

                // 创建Instance
                assert(pointer);
                pointer->Start();

                // 添加Instance到Storage
                AddInstanceToStorage(pointer);
                count++;
            }
            r_file.SecClose();
            if (false == bEof)
            {
                boost::system::error_code ec;
                fs::remove(fs::path(read_resinfo_file), ec);
                STORAGE_EVENT_LOG("LoadResourceInfoFromDisk!资源文件非法!" << read_resinfo_file);
            }
            CheckResourceFromDisk();
            STORAGE_EVENT_LOG("LoadResourceInfoFromDisk! resource count:" << count);

            for (std::set<Instance::p>::iterator i = instance_set_.begin(); i != instance_set_.end(); ++i)
            {
                Instance::p p_inst = *i;
                p_inst->FreeResourceHandle();
                p_inst->SetSaveMode(false);
            }

        }  // 打开ResourceInfo.dat成功

        // 删除多余文件
        string disk_store_path = space_manager_->GetStorePath();
        if (is_name_use_ppva)
        {
            STL_FOR_EACH(std::set<string>, filename_list, it)
            {
                boost::filesystem::path filepath(*it);
                if (boost::algorithm::starts_with(filepath.filename(), GetPrefixName()))
                {
                    boost::system::error_code ec;
                    fs::remove(filepath, ec);
                }
            }
        }
        else
        {
            fs::path disk_path(disk_store_path);

            if (boost::algorithm::ends_with(disk_store_path, ("FavoriteVideo\\")) || boost::algorithm::ends_with(
                disk_store_path, ("PPLiveVAShareFlv\\")) || boost::algorithm::ends_with(disk_store_path, ("cache\\ppva")))
            {
                STL_FOR_EACH(std::set<string>, filename_list, it)
                {
                    if (!boost::algorithm::iends_with(*it, ("readme.txt")))
                    {
                        boost::system::error_code ec;
                        fs::remove(fs::path(*it), ec);
                    }
                }
            }
        }

        STL_FOR_EACH(std::set<string>, app_file_list, it)
        {
            if (boost::algorithm::ends_with(*it, (".cfg")))
            {
                boost::system::error_code ec;
                fs::remove(fs::path(*it), ec);
            }
        }
    }
#endif  // #ifdef DISK_MODE

    // 返回rid对应的instance
    IInstance::p Storage::GetInstanceByRID(const RID& rid, bool is_check)
    {
        if (is_running_ == false)
            return IInstance::p();

        IInstance::p pointer;
        std::map<RID, Instance::p>::const_iterator it = rid_instance_map_.find(rid);
        if (it != rid_instance_map_.end()
#ifdef DISK_MODE
            && (is_check ? CheckInstanceResource(it->second) : true)
#endif  // #ifdef DISK_MODE
           )
        {
            pointer = it->second;
        }
        STORAGE_EVENT_LOG("RID = " << rid << " instance = " << pointer);
        return pointer;
    }

    // 返回url对应的instance
    IInstance::p Storage::GetInstanceByUrl(const string& url, bool is_check)
    {
        if (false == is_running_)
            return IInstance::p();
        IInstance::p pointer;
        std::map<string, Instance::p>::const_iterator it = url_instance_map_.find(url);
        if (it != url_instance_map_.end()
#ifdef DISK_MODE
            && (is_check ? CheckInstanceResource(it->second) : true)
#endif  // #ifdef DISK_MODE
           )
        {
            pointer = it->second;
        }
        STORAGE_EVENT_LOG("Url = " << url << " instance = " << pointer);
        return pointer;
    }

    IInstance::p Storage::GetLiveInstanceByRid(const RID& rid)
    {
        if (is_running_ == false)
        {
            return IInstance::p();
        }

        IInstance::p pointer;
        std::map<RID, LiveInstance::p>::const_iterator it = rid_to_live_instance_map_.find(rid);
        if (it != rid_to_live_instance_map_.end())
        {
            pointer = it->second;
        }

        STORAGE_EVENT_LOG("RID = " << rid << " instance = " << pointer);
        return pointer;
    }

    void Storage::OnLiveInstanceStopped(IInstance::p live_instance)
    {
        assert(live_instance);

        for(std::map<RID, LiveInstance::p>::iterator iter = rid_to_live_instance_map_.begin();
            iter != rid_to_live_instance_map_.end();
            ++iter)
        {
            if (live_instance == iter->second)
            {
                rid_to_live_instance_map_.erase(iter);
                break;
            }
        }
    }

    IInstance::p Storage::GetInstanceByFileName(const string& filename, bool is_check)
    {
        IInstance::p pointer;

        if (false == is_running_)
        {
            return pointer;
        }
        
        std::map<string, Instance::p>::const_iterator it = url_instance_map_.begin();
        for (; it != url_instance_map_.end(); ++it)
        {
            if (it->second->GetFileName() == filename)
            {
#ifdef DISK_MODE
                if(is_check ? CheckInstanceResource(it->second) : true)
                {
                    pointer = it->second;
                }
#else
                pointer = it->second;
#endif
                break;
            }
        }

        STORAGE_EVENT_LOG("filename = " << filename << " instance = " << pointer);
        return pointer;
    }

    // 添加某个instance到rid_map, url_map和inst_set中
    bool Storage::AddInstanceToStorage(Instance::p pointer)
    {
        if (!pointer)
            return false;

        RID rid = pointer->GetRID();
        if (!rid.is_empty())
        {
            assert(rid_instance_map_.find(rid) == rid_instance_map_.end());
            rid_instance_map_.insert(std::make_pair(rid, pointer));
        }
        std::vector<protocol::UrlInfo> url_s;
        pointer->GetUrls(url_s);
        for (uint32_t i = 0; i < url_s.size(); i++)
        {
            if (url_instance_map_.find(url_s[i].url_) == url_instance_map_.end())
            {
                url_instance_map_.insert(std::make_pair(url_s[i].url_, pointer));
            }
        }
        instance_set_.insert(pointer);
        return true;
    }

#ifdef DISK_MODE
    // 重命名 & SaveMode = true
    void Storage::AttachSaveModeFilenameByUrl(const string& url, const string& web_url, string qualified_filename)
    {
        if (false == is_running_)
        {
            return;
        }

        if (STORAGE_MODE_READONLY == GetStorageMode())
        {
            STORAGE_DEBUG_LOG("[DENY] Try to Attach SaveModeFilename in the Storage ReadOnlyMode");
            return;
        }

        if (false == use_disk_)
        {
            return;
        }

        string q_name = qualified_filename;
        // !
        boost::filesystem::path path(q_name);

        string file_name = path.filename();
        boost::filesystem::path file_path = path.branch_path();

        // 检查路径
        if (false == file_path.is_complete())
        {
            if (IsFileNameNeedHide(file_name))
            {
                file_path = boost::filesystem::path(space_manager_->GetHiddenSubPath());
            }
            else
            {
                file_path = boost::filesystem::path(space_manager_->store_path_);
            }
            q_name = (file_path / file_name).file_string();
        }

        url_filename_map_.insert(std::make_pair(url, file_name));
        std::map<string, Instance::p>::const_iterator it = url_instance_map_.find(url);
        // instance尚未创建
        if (it == url_instance_map_.end())
        {
            assert(!"Instance should be created now!!");
            LOG(__DEBUG, "downloadcenter", __FUNCTION__ << ":" << __LINE__ << " no such url in instance_map, url = " << url);
            return;
        }

        // instance已经创建，改名字
        Instance::p inst = it->second;
        LOG(__DEBUG, "downloadcenter", __FUNCTION__ << ":" << __LINE__ << " Set Instance to SaveMode, inst = " << inst);

        // save mode
        inst->SetSaveMode(true);
        inst->web_url_ = web_url;
        inst->qname_ = q_name;
        inst->file_name_ = file_name;

        // notify download driver
        inst->NotifyGetFileName(file_name);

        string qq_filename = q_name;
        if (false == inst->IsComplete())
        {
            qq_filename = qq_filename + tpp_extname;
        }

        if (inst->resource_name_ != qq_filename)
        {
            string full_name;
            DoVerifyName(file_name, file_path, full_name);
            if (false == inst->IsComplete())
            {
                full_name = full_name + tpp_extname;
            }
            if (inst->Rename(full_name))
            {
                url_filename_map_.erase(url);
            }
            else
            {
            }
        }
    }
#endif  // #ifdef DISK_MODE

// 获得了新的关于该url的文件名
    void Storage::AttachFilenameByUrl(const string& url, string filename)
    {
        if (false == is_running_)
        {
            return;
        }

#ifdef DISK_MODE

        if (STORAGE_MODE_READONLY == GetStorageMode())
        {
            STORAGE_DEBUG_LOG("[DENY] Try to Attach filename in the Storage ReadOnlyMode");
            return;
        }

        if (false == use_disk_)
        {
            return;
        }

        string file_name = filename;
        string file_ext = fs::path(file_name).extension();
        if (file_ext.empty() || file_ext == ".com")
        {
            network::Uri uri(url);
            string ext = fs::path(uri.getfile()).extension();
            file_name.append(ext.length() == 0 ? (".flv") : ext);
        }
        // 保存到map中
        url_filename_map_.insert(std::make_pair(url, file_name));
        std::map<string, Instance::p>::const_iterator it = url_instance_map_.find(url);
        // instance尚未创建
        if (it == url_instance_map_.end())
        {
            // STORAGE_DEBUG_LOG("instance尚未创建，保存到map中");
            // url_filename_map_.insert(make_pair(url, file_name));
            return;
        }
        // instance已经创建，改名字
        Instance::p inst = it->second;

        if (inst->IsSaveMode())
        {
            LOG(__DEBUG, "downloadcenter", __FUNCTION__ << ":" << __LINE__ << " SaveMode = true, Instance = " << inst
                << ", AttachFilenameByUrl Fail!");
            return;
        }

        // notify download driver
        inst->NotifyGetFileName(filename);

        string full_name = GetFullName(file_name);
        if (false == inst->IsComplete())
        {
            full_name = full_name + tpp_extname;
        }

        // 如果instance还没有获取文件名

        // 如果instance存在但是它的文件不在当前缓冲目录下，则不重命名
        fs::path resname_path(inst->resource_name_);
        if (resname_path.remove_filename() != fs::path(space_manager_->store_path_))
        {
            url_filename_map_.erase(url);
            return;
        }

        if (inst->resource_name_ != full_name)
        {
            boost::filesystem::path file_path(space_manager_->store_path_);
            DoVerifyName(file_name, file_path, full_name);
            if (false == inst->IsComplete())
            {
                full_name = full_name + tpp_extname;
            }
            if (inst->Rename(full_name))
            {
                url_filename_map_.erase(url);
            }
            else
            {
                // STORAGE_DEBUG_LOG("命名失败，保存到map中");
                // url_filename_map_.insert(make_pair(url, file_name));
            }
        }

#endif  // #ifdef DISK_MODE

    }

#ifdef DISK_MODE

    string Storage::GetFullName(string filename)
    {
        const static string sample("CON|PRN|AUX|NUL|COM1|COM2|COM3|COM4|COM5|COM6|COM7|COM8|COM9|LPT1|LPT2|LPT3|LPT4|LPT5|LPT6|LPT7|LPT8|LPT9");

        string file_name = filename;
        fs::path fnpath(file_name);
        string fn_ext = fnpath.extension();

        fnpath = fnpath.parent_path() / fnpath.stem();
        file_name = fnpath.filename();

        // 检查文件名是否合法
        file_name.erase(std::remove_if (file_name.begin(), file_name.end(), boost::algorithm::is_any_of("<>:\"/\\|?*")),
            file_name.end());
        file_name.erase(std::remove_if (file_name.begin(), file_name.end(), boost::algorithm::is_from_range((char) 0,
            (char) 31)), file_name.end());

        boost::algorithm::trim(file_name);

        if (file_name.length() <= 4 && sample.find(file_name) != string::npos)
            file_name += "_";

        if (file_name.length() > 200)
            file_name = file_name.substr(0, 200);

        if (IsFileExtNeedHide(fn_ext))
        {
            file_name = (fs::path(space_manager_->GetHiddenSubPath()) / file_name).file_string();
        }
        else
        {
            file_name = (fs::path(space_manager_->store_path_) / file_name).file_string();
        }

        filename = file_name + fn_ext;

        return filename;
    }

	void Storage::GetAllCompletedFiles(std::vector<std::string>& filename_vec) const
	{
		std::set<Instance::p>::const_iterator iter;
		for (iter = instance_set_.begin(); iter != instance_set_.end(); ++iter) {
            if ((*iter)->IsComplete()) {
                filename_vec.push_back((*iter)->GetFileName());
            }
		}
	}

    // 根据文件名判重，并返回最终文件名
    void Storage::DoVerifyName(string filename, boost::filesystem::path filepath, string& lastname)
    {
        // filename带".flv"
        // file_name不带".flv"
        const static string sample("CON|PRN|AUX|NUL|COM1|COM2|COM3|COM4|COM5|COM6|COM7|COM8|COM9|LPT1|LPT2|LPT3|LPT4|LPT5|LPT6|LPT7|LPT8|LPT9");

        string file_name = filename;
        fs::path fnpath(file_name);
        string fn_ext = fnpath.extension();
        // fnpath.RemoveFileExtension();
        fnpath = fnpath.parent_path() / fnpath.stem();
        file_name = fnpath.filename();

        // 检查文件名是否合法
        file_name.erase(std::remove_if (file_name.begin(), file_name.end(), boost::algorithm::is_any_of("<>:\"/\\|?*")),
            file_name.end());
        file_name.erase(std::remove_if (file_name.begin(), file_name.end(), boost::algorithm::is_from_range((char) 0,
            (char) 31)), file_name.end());

        boost::algorithm::trim(file_name);

        if (file_name.length() <= 4 && sample.find(file_name) != string::npos)
            file_name += ("_");

        if (file_name.length() > 200)
            file_name = file_name.substr(0, 200);

        file_name = (filepath / file_name).file_string();
        filename = file_name + fn_ext;


        if (true == use_disk_)
        {
            // 确定目录存在
            if (false == base::filesystem::exists_nothrow(filepath))
            {
                CreateDirectoryRecursive(filepath.file_string());
            }
        }

        // 检测文件是否存在
        int i = 0;
        while (true)
        {
            // 不存在同名的文件
            //            if (!::PathFileExists(filename.c_str()))
            if (!IsFilenameExist(filename))
            {
                // 转为tpp文件名
                string tppfilename = filename + tpp_extname;
                if (!IsFilenameExist(tppfilename))
                {
                    break;
                }
            }
            // 如果存在同名文件，自动增量名
            std::ostringstream oss;
            oss << "(" << i << ")";
            ++i;
            string change_str = oss.str();
            filename = file_name + change_str + fn_ext;
        }
        lastname = filename;
    }
#endif  // #ifdef DISK_MODE

    string Storage::FindRealName(const string& url)
    {
        std::map<string, string>::const_iterator it = url_filename_map_.find(url);
        // url没有对应的真实名字
        if (it == url_filename_map_.end())
        {
            return "";
        }
        return it->second;
    }

    // Url如果在url_map中已存在并且与RID信息不一致，则调用MergeInstance删除并重新建立新Instance
    void Storage::AttachRidByUrl(const string& url, const protocol::RidInfo& rid, int flag)
    {
        if (is_running_ == false)
            return;

#ifdef DISK_MODE
        if (STORAGE_MODE_READONLY == GetStorageMode())
        {
            STORAGE_DEBUG_LOG("[DENY] Try to Attach Rid in the Storage ReadOnlyMode");
            return;
        }
#endif  // #ifdef DISK_MODE

        STORAGE_EVENT_LOG("Storage::AttachRidByUrl, URL: " << url << ", RID: " << rid);

        // 验证rid
        if (false == IsRidInfoValid(rid))
        {
            STORAGE_EVENT_LOG("RIDInfo Invalid. RID: " << rid);
            return;
        }

        // 此处的URL一般都是主URL，如果已存在并且与RID信息不一致，删除并重新建立新Instance
        std::map<string, Instance::p>::const_iterator it = url_instance_map_.find(url);
        if (it == url_instance_map_.end())
        {
            STORAGE_ERR_LOG("Storage::AttachRidByUrl, not such url:" << url);
            return;
        }
        Instance::p inst1 = it->second;
        inst1->SetRidOriginFlag(flag);
        // 是纯下模式
        if (inst1->IsPureDownloadMode())
        {
            STORAGE_DEBUG_LOG("Storage::AttachRidByUrl, IsPureDownloadMode");
            return;
        }

        // RID已存在，不更新RID
        // ! 如何即时更新RID
        if (inst1->HasRID())
        {
            STORAGE_DEBUG_LOG("Storage::AttachRidByUrl, inst rid is not empty");
            return;
        }

        std::map<RID, Instance::p>::const_iterator iter = rid_instance_map_.find(rid.GetRID());

        // 没有重复的，不需要融合, 直接设置RidInfo
        if (iter == rid_instance_map_.end())
        {
            STORAGE_DEBUG_LOG("No such rid, no need Merge");

            if ((inst1->GetFileLength() != 0) && (inst1->GetFileLength() != rid.GetFileLength()))
            {
                // 长度检查不符，返回！
                // inst1->SetPureDownloadMode(true);
                return;
            }

            // 回头在考虑考虑！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！
            // 将rid与url_inst匹配，并为url_inst添加rid信息
            rid_instance_map_.insert(std::make_pair(rid.GetRID(), inst1));
            inst1->SetRidInfo(rid);
        }
        else
        {
#ifdef DISK_MODE
            // 文件已经不存在
            if (false == CheckInstanceResource(iter->second))
            {
                STORAGE_DEBUG_LOG("No such rid, no need Merge");

                if ((inst1->GetFileLength() != 0) && (inst1->GetFileLength() != rid.GetFileLength()))
                {
                    // 长度检查不符，返回！
                    // inst1->SetPureDownloadMode(true);
                    return;
                }

                // 回头在考虑考虑！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！！
                // 将rid与url_inst匹配，并为url_inst添加rid信息
                rid_instance_map_.insert(std::make_pair(rid.GetRID(), inst1));
                inst1->SetRidInfo(rid);
            }
            else
#endif  // #ifdef DISK_MODE
            {
                inst1->SetRidInfo(rid);

                DoMerge(iter->second, inst1);
            }
        }
    }

    // 根据mod_number和group_count获取rid_instance_map_中的rid
    void Storage::GetVodResources(std::set<RID>& rid_s, uint32_t mod_number, uint32_t group_count)
    {
        if (is_running_ == false)
            return;
        for (std::map<RID, Instance::p>::const_iterator iter = rid_instance_map_.begin(); 
             iter != rid_instance_map_.end(); 
             iter++)
        {
            Instance::p ins = iter->second;
            if (base::util::GuidMod(iter->first, group_count) == mod_number && ins->GetDownloadedBlockCount() > 0)
                rid_s.insert(iter->first);
        }
    }

    // 根据mod_number和group_count获取rid_to_live_instance_map_中的rid
    void Storage::GetLiveResources(std::set<RID>& rid_s, uint32_t mod_number, uint32_t group_count)
    {
        if (is_running_ == false)
        {
            return;
        }

        for (std::map<RID, LiveInstance::p>::const_iterator iter = rid_to_live_instance_map_.begin();
            iter != rid_to_live_instance_map_.end();
            ++iter)
        {
            if (base::util::GuidMod(iter->first, group_count) == mod_number)
            {
                rid_s.insert(iter->first);
            }
        }
    }

    // 从url对应的instance和url_map中删除某个Url
    void Storage::RemoveUrlInfo(const protocol::UrlInfo& url_info)
    {
        if (is_running_ == false)
            return;
        std::map<string, Instance::p>::const_iterator it = url_instance_map_.find(url_info.url_);
        if (it == url_instance_map_.end())
        {
            return;
        }
        it->second->RemoveUrl(url_info.url_);
        url_instance_map_.erase(url_info.url_);
    }

    void Storage::RemoveInstanceForMerge(Instance::p inst)
    {
        if (!inst)
            return;
        if (inst->resource_p_)
        {
#ifdef DISK_MODE
            space_manager_->AddToRemoveResourceList(inst->resource_p_, inst->GetDiskFileSize());
#endif  // #ifdef DISK_MODE
        }

        inst->Remove(true);  // 删除instance
        return;
    }

    // 从url和rid列表中删除某个instance，并添加到spacemanager的释放资源列表中
    void Storage::RemoveInstance(Instance::p inst, bool need_remove_file)
    {
        STORAGE_INFO_LOG("" << inst->GetResourceName() << " disk_file_size:" << inst->GetDiskFileSize());
        assert((inst->GetDDNum() == 0));

        if (!inst)
        {
            return;
        }

        STORAGE_DEBUG_LOG("\nResourceName: " << (inst->resource_name_)
            << "\n\tFileLength: " << float(inst->GetFileLength())/1024/1024 << "MB"
            << "\tRID: " << inst->GetRID()
            // << "\tValue: " << inst->GetInstanceValue()
            << "\n");

        RID rid = inst->GetRID();
        if (!rid.is_empty())
        {
            rid_instance_map_.erase(rid);
        }
        std::vector<protocol::UrlInfo> url_s;
        inst->GetUrls(url_s);
        for (uint32_t i = 0; i < url_s.size(); i++)
        {
            url_instance_map_.erase(url_s[i].url_);
            if (url_filename_map_.find(url_s[i].url_) != url_filename_map_.end())
            {
                url_filename_map_.erase(url_s[i].url_);
            }
        }

        if (inst->resource_p_)
        {
#ifdef DISK_MODE
            space_manager_->AddToRemoveResourceList(inst->resource_p_, inst->GetDiskFileSize());
#endif  // #ifdef DISK_MODE
        }

        inst->Remove(need_remove_file);  // 删除instance
        return;
    }

    // 从instance_set中找一个instance删除
    void Storage::RemoveOneInstance()
    {
        std::multimap<float, Instance::p> value_inst;
        float min_value = 10000000;
        Instance::p inst_rm, inst_minvalue;
        for (std::set<Instance::p>::iterator it = instance_set_.begin(); it != instance_set_.end(); it++)
        {
            Instance::p inst_p = *it;
            if (inst_p->CanRemove())
            {
                // inst_rid为空
                if (inst_p->GetRID().is_empty())
                {
                    RemoveInstance(inst_p, true);
                    return;
                }
                if (inst_p->GetDownloadedBlockCount() == 0)
                {
                    RemoveInstance(inst_p, true);
                    return;
                }
                if (inst_rm.get() == 0)
                {
                    inst_rm = inst_p;
                }
                // inst_rid 非空
                float tmp_value = inst_p->GetInstanceValue();
                if (tmp_value >= 0 && tmp_value < min_value)
                {
                    min_value = tmp_value;
                }
                if (false == inst_p->IsSaveMode())
                {
                    value_inst.insert(std::make_pair(tmp_value, inst_p));
                }
            }
        }
        if (value_inst.find(min_value) != value_inst.end() && inst_minvalue.get() == 0)
        {
            inst_minvalue = value_inst.find(min_value)->second;
        }
        if (inst_minvalue.get() == 0 && inst_rm.get() == 0)
        {
            STORAGE_DEBUG_LOG("inst_minvalue and inst_rm null");
            return;
        }
        RemoveInstance(inst_minvalue.get() ? inst_minvalue : inst_rm, true);
    }

    void Storage::OnTimerElapsed(framework::timer::Timer * timer)
    {
        if (is_running_ == false)
            return;

#ifdef DISK_MODE
        if (timer == &space_manager_timer_)
        {
            space_manager_->DiskSpaceMaintain();
            return;
        }
        if (timer == &res_info_timer_)
        {
            SaveResourceInfoToDisk();
            return;
        }
#endif  // #ifdef DISK_MODE

        if (timer == &memory_consumption_monitor_timer_)
        {
            const size_t MegaBytes = 1024*1024;
            MonitorMemoryConsumption(20*MegaBytes);
        }
    }

    void Storage::MonitorMemoryConsumption(size_t memory_quota_in_bytes)
    {
        MemoryConsumptionMonitor monitor(memory_quota_in_bytes);

        for(std::map<RID, LiveInstance::p>::iterator iter = rid_to_live_instance_map_.begin();
            iter != rid_to_live_instance_map_.end();
            ++iter)
        {
            monitor.Add(boost::shared_ptr<IMemoryConsumer>(new LiveInstanceMemoryConsumer(iter->second)));
        }
        
        for(std::map<RID, Instance::p>::iterator iter = rid_instance_map_.begin();
            iter != rid_instance_map_.end();
            ++iter)
        {
            monitor.Add(boost::shared_ptr<IMemoryConsumer>(new InstanceMemoryConsumer(iter->second)));
        }

        monitor.AssignQuota();
    }

#ifdef DISK_MODE
    string Storage::GetResourceDataPath(const string& default_path)
    {
        string szPath;
        if (default_path.length() != 0)
        {
            return default_path;
        }
        else if (false == base::util::GetAppDataPath(szPath))
        {
            return (fs::path(default_path) / "resconfig").file_string();
        }
        return (fs::path(szPath) / "resconfig").file_string();
    }

    string Storage::GetCfgFilename(const string& res_name)
    {
        string fileName = fs::path(res_name).filename();
        CreateDirectoryRecursive(resource_data_path_);
        fs::path cfg_file_path = fs::path(resource_data_path_) / (fileName + cfg_extname);
        return cfg_file_path.file_string();
    }

    void Storage::RemoveFileAndConfigFile(const FileResourceInfo &r_info)
    {
        boost::system::error_code ec;
        fs::remove(fs::path(r_info.file_path_), ec);

        string cfg_file_name = GetCfgFilename(r_info.file_path_);
        fs::remove(fs::path(cfg_file_name), ec);
    }

    // TODO:yujinwu:2010/11/27:和SpaceManager::GetDirFileList()功能重复
    // 应提取到一个公共函数中
    void Storage::GetDirFileList(const string& dir, std::set<string> &file_list)
    {
        // 这里不能直接clear, 因为传进来是个引用，如果clear会把原来里面的值清楚，导致BUG
        // file_list.clear();
        if (true == use_disk_)
        {
            fs::path p(dir);
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
                                file_list.insert(file_path.file_string());
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

    // 判断是否和已存在的instance的文件名重名
    bool Storage::IsFilenameExist(const string& filename)
    {
        // 这个重名判断只能局限于当前缓冲目录
        if (false == use_disk_)
        {
            return false;
        }

        fs::path name_path(filename);
        if (base::filesystem::exists_nothrow(name_path))
        {
            return true;
        }

        // 检查存放在其它目录下的instance是否重名
        const string search_name = name_path.filename();

        for (std::set<Instance::p>::const_iterator it = instance_set_.begin(); it != instance_set_.end(); ++it)
        {
            if ((*it)->resource_name_.empty())
                continue;
            fs::path exist_path((*it)->resource_name_);
            string exist_name = exist_path.filename();
            string exist_store = exist_path.parent_path().directory_string();
            // 是当前缓冲目录下的则不再检查
            if (exist_store == space_manager_->store_path_
                || exist_store == space_manager_->GetHiddenSubPath()
                || exist_store.empty())
            {
                continue;
            }
            if (exist_name == search_name)
            {
                return true;
            }
        }
        return false;
    }

#endif  // #ifdef DISK_MODE


    bool Storage::IsRidInfoValid(const protocol::RidInfo& rid_info)
    {
        // Modified by jeffrey 2010/2/28 支持ppbox发给内核的请求不带block_md5
#ifdef PEER_PC_CLIENT
        union {
            char bytes[2];
            boost::uint16_t data;
        } endian;
        endian.data = 0x0102u;
        STORAGE_DEBUG_LOG("bytes = " << (uint32_t)endian.bytes[0] << " " << (uint32_t)endian.bytes[1]);

        framework::string::Md5 hash;
        for (uint32_t i = 0; i < rid_info.GetBlockCount(); i++)
        {
            if (rid_info.block_md5_s_[i].is_empty())
            {
                STORAGE_DEBUG_LOG(" false: rid_info_.block_md5_s_[i].IsEmpty() " << rid_info);
                return false;
            }
            hash.update(rid_info.block_md5_s_[i].to_little_endian_bytes().data(), sizeof(framework::string::UUID));
        }

        hash.final();
        RID valid_rid;
        valid_rid.from_little_endian_bytes(hash.to_bytes());

        if (valid_rid != rid_info.GetRID())
        {
            STORAGE_DEBUG_LOG(" valid_rid =  " << valid_rid << ", argument_rid = " << rid_info.GetRID());
            return false;
        }
#endif
        return true;
    }

#ifdef DISK_MODE

    void Storage::RemoveExpiredInvisibleFiles()
    {
        boost::gregorian::date today(boost::gregorian::day_clock::local_day());
        boost::filesystem::path invisible_dir(space_manager_->GetHiddenSubPath());

        boost::system::error_code ec;
        boost::filesystem::directory_iterator dir_iter =
            base::filesystem::directory_iterator_nothrow(invisible_dir, ec);

        if (!ec)
        {
            for (; dir_iter != boost::filesystem::directory_iterator();)
            {
                boost::filesystem::path file_path = * dir_iter++;
                if (base::filesystem::is_regular_file_nothrow(file_path))
                {
                    std::time_t last_modified_time;
                    base::filesystem::last_write_time_nothrow(file_path, last_modified_time, ec);
                    if (!ec)
                    {
                        boost::posix_time::ptime last_write_time = boost::posix_time::from_time_t(last_modified_time);
                        boost::gregorian::date_period write_date_period(last_write_time.date(), today);
                        if (write_date_period.length().days() > 30)
                        {
                            base::filesystem::remove_nothrow(file_path);
                        }
                    }
                }
            }
        }
    }
#endif  // #ifdef DISK_MODE

    protocol::SubPieceBuffer Storage::GetSubPieceFromBlock(const protocol::SubPieceInfo& subpiece_info,
        const RID& rid, const base::AppBuffer& block_buf)
    {
        p2sp::RBIndex rb_index(rid, subpiece_info.block_index_);

        storage::Instance::p inst = boost::static_pointer_cast<storage::Instance>(GetInstanceByRID(rb_index.rid, false));
        assert(inst);
        if (!inst)
        {
            return protocol::SubPieceBuffer();
        }

        uint32_t block_offset, block_len;
        inst->GetBlockPosition(rb_index.block_index, block_offset, block_len);
        uint32_t sub_offset, sub_len;
        inst->GetSubPiecePosition(subpiece_info, sub_offset, sub_len);
        STORAGE_TEST_DEBUG("block index:" << rb_index.block_index << "--block offset, block len:<"
            << block_offset << "," << block_len << ">--sub offset, sub len:<" << sub_offset << "," << sub_len << ">"
            << "--buf len:" << block_buf.Length());
        assert(sub_offset >= block_offset);
        assert(sub_len <= block_len);
        assert(block_len == block_buf.Length());
        uint32_t start = sub_offset - block_offset;

        if (start + sub_len > block_buf.Length() || sub_offset < block_offset || sub_len > block_len || block_len
            != block_buf.Length())
        {
            return protocol::SubPieceBuffer();
        }
        else
        {
            protocol::SubPieceBuffer buf(new protocol::SubPieceContent, sub_len);
            if (buf) {
                base::util::memcpy2(buf.Data(), buf.Length(), block_buf.Data() + start, sub_len);
            }
            return buf;
        }
    }

    void Storage::UploadOneSubPiece(const RID & rid)
    {
        storage::Instance::p inst = boost::static_pointer_cast<storage::Instance>(storage::Storage::Inst()->GetInstanceByRID(rid, false));
        if (inst)
        {
            inst->UploadOneSubPiece();
        }
    }

}  // namespace storage
