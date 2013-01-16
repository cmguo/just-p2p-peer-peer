//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef STORAGE_STORAGE_H
#define STORAGE_STORAGE_H

/*******************************************************************************
*
* Filename: Storage.h
* Comment:  class Storage, 负责管理instance，该类是单例的
*
********************************************************************************/


#include "storage/IStorage.h"
#include "storage/CfgFile.h"
#include "storage/Resource.h"
#include "storage/Instance.h"
#include "storage/LiveInstance.h"
#include "storage/SpaceManager.h"
#include "storage/Performance.h"

#include <boost/filesystem/path.hpp>

namespace storage
{

    class Storage:
        public boost::noncopyable,
        public boost::enable_shared_from_this<Storage>,
        public ILiveInstanceStoppedListener,
        public IStorage
#ifdef DUMP_OBJECT
        , public count_object_allocate<Storage>
#endif
    {
    public:
        typedef boost::shared_ptr<Storage> p;

    public:
        static IStorage::p Inst();

        static Storage::p CreateInst(
            boost::asio::io_service & io_svc);

        static Storage::p Inst_Storage();

        virtual void Start(
            bool bUseDisk,                  // 是否使用磁盘, 如果是TRUE, 则使用磁盘; 如果是FALSE, 则纯内存
            boost::uint64_t ullDiskLimit,   // 使用磁盘上限
            string DiskPathName,            // 磁盘使用路径
            string ConfigPath,
            boost::uint32_t storage_mode);  // storage mode

        virtual void Stop();

    private:
        Storage(
            boost::asio::io_service & io_svc);

        static Storage::p inst_;

    public:
        // 根据url_info从map中找到或创建一个instance
        virtual IInstance::p CreateInstance(const protocol::UrlInfo& url_info, bool is_force = false);

        virtual IInstance::p CreateInstance(const protocol::UrlInfo& url_info, const protocol::RidInfo& rid_info, bool is_force = false);

        virtual IInstance::p CreateLiveInstance(const RID& rid, boost::uint16_t live_interval_in_seconds);

        virtual IInstance::p GetLiveInstanceByRid(const RID& rid);

        // 返回rid对应的instance
        virtual IInstance::p GetInstanceByRID(const RID& rid, bool is_check);
        // 返回url对应的instance
        virtual IInstance::p GetInstanceByUrl(const string& url, bool is_check);

        virtual IInstance::p GetInstanceByFileName(const string & filename, bool is_check);

        // Url如果在url_map中已存在并且与RID信息不一致，则调用MergeInstance删除并重新建立新Instance
        virtual void AttachRidByUrl(const string& url, const protocol::RidInfo& rid);

        // 根据mod_number和group_count获取rid_inst_map中的rid
        virtual void GetVodResources(std::set<RID>& rid_s, boost::uint32_t mod_number, boost::uint32_t group_count);
        virtual void GetLiveResources(std::set<RID>& rid_s, boost::uint32_t mod_number, boost::uint32_t group_count);

        // 从url对应的instance和url_map中删除某个Url
        virtual void RemoveUrlInfo(const protocol::UrlInfo& url_info);

        // 获得了新的关于该url的文件名
        virtual void AttachFilenameByUrl(const string& url, string filename);

        virtual void OnLiveInstanceStopped(IInstance::p live_instance);

#ifdef DISK_MODE
        virtual void AttachSaveModeFilenameByUrl(const string& url, const string& web_url, string qualified_filename);

        // 获得总共使用了的磁盘空间
        virtual boost::int64_t GetUsedDiskSpace()
        {
            return space_manager_->curr_resource_files_total_size_;
        }
        virtual boost::int64_t GetFreeSize() const
        {
            return space_manager_->GetFreeSize();
        }
        virtual boost::int64_t GetStoreSize() const
        {
            return space_manager_->GetStoreSize();
        }

        bool CreateHiddenDir(const string& dirpath);

        bool IsFileNameNeedHide(const string& filename);

        bool IsFileExtNeedHide(const string& fileext);

#endif  // #ifdef DISK_MODE

    public:

        // static
        static bool IsRidInfoValid(const protocol::RidInfo& rid_info);

    private:
        void MonitorMemoryConsumption(size_t memory_quota_in_bytes);

#ifdef DISK_MODE
        // 装载磁盘资源信息，如果资源信息丢失，则删除所有文件
        virtual void LoadResourceInfoFromDisk();

        // 删除资源文件和对应的cfg配置文件
        void RemoveFileAndConfigFile(const FileResourceInfo &r_info);

        void LoadResInfoOnReadMode();

        // 读取并检查磁盘资源信息
        virtual void CheckResourceFromDisk();

        virtual bool CheckInstanceResource(Instance::p inst);

        virtual bool CreateDirectoryRecursive(const string& directory);
#endif  // #ifdef DISK_MODE

    public:

        void OnTimerElapsed(framework::timer::Timer * pointer);


        // 从url和rid列表中删除某个instance，并添加到spacemanager的释放资源列表中
        virtual void RemoveInstance(Instance::p inst, bool need_remove_file);

        // 删除某个Instance

        // 删除某个Instance
        virtual void RemoveInstanceForMerge(Instance::p inst);

        // 从instance_set中找一个instance删除，会调用RemoveInstance
        virtual void RemoveOneInstance();

        // storage被通知instance关闭，storage取消instance的资源申请，释放资源空间
        virtual void OnInstanceCloseFinish(Instance::p instance_p);
        virtual void OnResourceCloseFinish(Instance::p instance_p, Resource::p resource_p, bool need_remove_file);

        // instance向spacemanager申请资源
        void ApplyResource(Instance::p resource_inst);

        // resource创建成功，绑定resource和instance, 将信息写入ResourceInfo文件中
        void OnCreateResourceSuccess(Resource::p resource_p, Instance::p instance_p);

        // 将instance添加到rid_map，如果和map中的rid重复，则将map中的inst合并到pointer
        virtual void AddInstanceToRidMap(Instance::p pointer);

        // 通过url-filename map中查找获取到的文件名
        string FindRealName(const string& url);

        boost::uint32_t LocalRidCount() { return rid_instance_map_.size();}

#ifdef DISK_MODE
        // 遍历instance_set_, 将Instace信息保存到Resourceinfo，同时保存原资源信息到bak
        virtual void SaveResourceInfoToDisk();

        // 根据文件名判重，并返回最终文件名
        void DoVerifyName(string filename, boost::filesystem::path filepath, string& lastname, bool is_openservice);

        string GetFullName(string filename, bool is_openservice);

		void GetAllCompletedFiles(std::vector<std::string>& filename_vec) const;

        virtual SpaceManager::p GetSpaceManager()
        {
            return space_manager_;
        }

        string ResourceDataPath()
        {
            return resource_data_path_;
        }

        string GetCfgFilename(const string& res_name);

        void GetDirFileList(const string& dir, std::set<string> &file_list);
        // 判断是否和已存在的instance的文件名重名
        bool IsFilenameExist(const string& filename);

        StorageMode GetStorageMode() const
        {
            return storage_mode_;
        }

        string GetResourceDataPath() const
        {
            return resource_data_path_;
        }
#endif  // DISK_MODE

        protocol::SubPieceBuffer GetSubPieceFromBlock(const protocol::SubPieceInfo& subpiece_info,
            const RID& rid, const base::AppBuffer& block_buf);

        void UploadOneSubPiece(const RID & rid,bool isTcp = false);

    protected:
        virtual void DoMerge(Instance::p instance_old, Instance::p instance_new);
        // 将instance_p2融合到instance_p1
        virtual void MergeInstance(Instance::p instance_p1, Instance::p instance_p2);
        // 添加某个instance到rid_map, url_map和inst_set中
        virtual bool AddInstanceToStorage(Instance::p pointer);

    private:
        void RemoveExpiredInvisibleFiles();

    private:
        boost::asio::io_service & io_svc_;

        volatile bool is_running_;
        bool use_disk_;

        std::set<Instance::p> instance_set_;  // inst_set
        std::map<RID, Instance::p> rid_instance_map_;  // rid_map
        std::map<string, Instance::p> url_instance_map_;  // url_map
        std::map<string, string> url_filename_map_;  // url-filename
        std::map<RID, LiveInstance::p> rid_to_live_instance_map_;

        framework::timer::PeriodicTimer memory_consumption_monitor_timer_; //5s

#ifdef DISK_MODE
        string resourceinfo_file_;  // "ResourceInfo.dat"
        string resourceinfo_bak_file_;  // "ResourceInfo.dat.bak"
        string resource_data_path_;
        StorageMode storage_mode_;
        framework::timer::PeriodicTimer space_manager_timer_;  // 5s
        framework::timer::PeriodicTimer res_info_timer_;
        SpaceManager::p space_manager_;
#endif  // #ifdef DISK_MODE

    };

}// storage

#endif  // STORAGE_STORAGE_H

