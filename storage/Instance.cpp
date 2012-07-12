//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

/*******************************************************************************
*   Instance.cpp
*******************************************************************************/

#include "Common.h"

#include "p2sp/AppModule.h"
#include "storage/format/MetaFLVParser.h"
#include "storage/format/MetaMP4Parser.h"

#include "storage/storage_base.h"
#include "storage/IStorage.h"
#include "storage/SubPieceManager.h"
#include "storage/Instance.h"
#include "storage/Storage.h"
#include "storage/FileResource.h"
#include "storage/StorageThread.h"
#ifdef DISK_MODE
#include "storage/FileResourceInfo.h"
#endif

#include "statistic/DACStatisticModule.h"
#include "p2sp/tracker/TrackerModule.h"
#include "network/Uri.h"
#include "random.h"

#include <boost/algorithm/string/predicate.hpp>

namespace storage
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_instance = log4cplus::Logger::getInstance("[instance]");
#endif
    using namespace base;

    const uint32_t storage_send_speed_limit = 2048;
    const int32_t deattach_timer_timeout = 5;
    const boost::int64_t OneDayInSeconds = 24 * 60 * 60;

    Instance::Instance()
        : is_running_(true)
        , b_pure_download_mode_(false)
        , instance_state_(INSTANCE_NEED_RESOURCE)
        , local_complete_(false)
        , disk_file_size_(0)
        , is_have_rename_(false)
        , send_speed_limit_(storage_send_speed_limit)
        , send_count_(0)
        , last_push_time_(0)
        , down_mode_(DM_BY_ACCELERATE)
        , file_duration_in_sec_(0)  // 时长
        , last_write_time_(0)  // 该资源上一次写磁盘的时间
        , data_rate_(0)   // 码流率
        , cfg_timer_(global_second_timer(), 10*1000, boost::bind(&Instance::OnConfigTimerElapsed, this, &cfg_timer_))
        , traffic_timer_(global_second_timer(), 1000, boost::bind(&Instance::OnTrafficTimerElapsed, this, &traffic_timer_))
        , merge_timer_(global_250ms_timer(), 250, boost::bind(&Instance::OnMergeTimerElapsed, this, &merge_timer_))
        , deattach_timer_(global_second_timer(), deattach_timer_timeout * 1000, boost::bind(&Instance::OnDeAttachTimerElapsed, this, &deattach_timer_))
        , md5_hash_failed_(0)
        , is_push_(false)
        , filesystem_last_write_time_(0)
        , have_upload_at_least_one_subpiece_(false)
    {
        LOG4CPLUS_DEBUG_LOG(
            logger_instance, "traffic_timer_:" << (void*)&traffic_timer_ <<
            ", cfg_timer_:" << (void*)&cfg_timer_ <<
            ", merge_timer_" << (void*)&merge_timer_);
    }

    Instance::~Instance()
    {
        Stop();
    }

    Instance::p Instance::Create(const protocol::UrlInfo& url_info)
    {
        // TODO:yujinwu:需要验证url_info不为空？
        LOG4CPLUS_DEBUG_LOG(logger_instance, "Instance::Create " << url_info);
        if (url_info.url_.empty())
        {
            LOG4CPLUS_DEBUG_LOG(logger_instance, "********************************************");
        }

        Instance::p pointer(new Instance());
        assert(pointer);

        pointer->origanel_url_info_ = url_info;
        pointer->url_info_s_.insert(url_info);

        pointer->ParseFileNameFromUrl(url_info);

        if (true == pointer->IsComplete())
        {
            LOG4CPLUS_DEBUG_LOG(logger_instance, "local_complete_ = true");
            pointer->local_complete_ = true;
        }

        return pointer;
    }

#ifdef DISK_MODE
    // 根据FileResourceInfo和Resource_p创建一个新的instance
    Instance::p Instance::Open(FileResourceInfo r_f, Resource::p resource_p)
    {
        assert(resource_p);
        Instance::p pointer;
        // 解析ResourceInfo
        pointer = Instance::p(new Instance());
        assert(pointer);

        pointer->url_info_s_.insert(r_f.url_info_.begin(), r_f.url_info_.end());
        if (pointer->url_info_s_.size() > 0)
        {
            pointer->origanel_url_info_ = *(pointer->url_info_s_.begin());
        }

        pointer->last_push_time_ = r_f.last_push_time_;
        pointer->traffic_list_.assign(r_f.traffic_list_.begin(), r_f.traffic_list_.end());
        // version 3
        pointer->down_mode_ = r_f.down_mode_;
        pointer->web_url_ = r_f.web_url_;
        pointer->file_duration_in_sec_ = r_f.file_duration_in_sec_;
        pointer->last_write_time_ = r_f.last_write_time_;
        pointer->data_rate_ = r_f.data_rate_;
        pointer->file_name_ = r_f.file_name_;
        pointer->is_open_service_ = r_f.is_open_service_;
        pointer->is_push_ = r_f.is_push_;
        LOG4CPLUS_DEBUG_LOG(logger_instance, "Load File IsOpenService = " << r_f.is_open_service_ << 
            ", FileName = " << r_f.file_name_);

        assert(resource_p->subpiece_manager_);
        resource_p->subpiece_manager_->InitRidInfo(r_f.rid_info_);  // wired
        assert(!pointer->subpiece_manager_);
        pointer->subpiece_manager_ = resource_p->subpiece_manager_;

        pointer->instance_state_ = INSTANCE_APPLY_RESOURCE;
        pointer->AttachResource(resource_p, true);

        if (pointer->IsComplete())
        {
            LOG4CPLUS_DEBUG_LOG(logger_instance, "local_complete_ = true");
            pointer->local_complete_ = true;
        }

        return pointer;
    }
#endif  // #ifdef DISK_MODE

    // 根据file_length创建文件资源
    void Instance::SetFileLength(uint32_t file_length)
    {
        if (is_running_ == false)
            return;
        LOG4CPLUS_DEBUG_LOG(logger_instance, "Instance::SetFileLength" << file_length);

        if (file_length == 0)
        {
            assert(false);
            return;
        }

        protocol::RidInfo rid_info;
        rid_info.InitByFileLength(file_length);

        // 如果文件描述符为空，则创建文件资源
        bool result = SetRidInfo(rid_info);
        assert(result);
    }

    void Instance::Start()
    {
        cfg_timer_.start();
        traffic_timer_.start();
        is_running_ = true;
    }

    void Instance::Stop()
    {
        if (is_running_ == false)
            return;
        is_running_ = false;

        assert(download_driver_s_.empty());
        LOG4CPLUS_DEBUG_LOG(logger_instance, "Instance::Stop! ");
        instance_state_ = INSTANCE_CLOSING;

        ReleaseData();

        if (resource_p_)
        {
#ifdef DISK_MODE
            StorageThread::Post(boost::bind(&Resource::CloseResource, resource_p_, true));
#else
            resource_p_->CloseResource(true);
#endif  // #ifdef DISK_MODE
            resource_p_.reset();
        }
        else
        {
            OnResourceCloseFinish(resource_p_, true);
        }

    }

    void Instance::ReleaseData()
    {
        // release all
        download_driver_s_.clear();
        cfg_timer_.stop();
        merge_timer_.stop();
        traffic_timer_.stop();
        deattach_timer_.stop();
        if (subpiece_manager_)
            subpiece_manager_.reset();
        if (merge_to_instance_p_)
            merge_to_instance_p_.reset();
        url_info_s_.clear();
        traffic_list_.clear();
    }

    // 关闭本instance并通知storage，释放空间
    void Instance::Remove(bool need_remove_file)
    {
        if (is_running_ == false)
            return;
        is_running_ = false;

        LOG4CPLUS_DEBUG_LOG(logger_instance, "Instance::Remove! ");

        if (instance_state_ == INSTANCE_REMOVING)
        {
            return;
        }
        instance_state_ = INSTANCE_REMOVING;
        assert(download_driver_s_.empty());

        ReleaseData();

        Storage::Inst_Storage()->OnInstanceCloseFinish(shared_from_this());

        if (resource_p_)
        {
#ifdef DISK_MODE
            // 先关闭resource，然后调用OnResourceCloseFinish，但是如果此时instance已经销毁，则
            // 通知spacemanager释放资源空间
            StorageThread::Post(boost::bind(&Resource::CloseResource, resource_p_, need_remove_file));
#else
            resource_p_->CloseResource(need_remove_file);
#endif  // #ifdef DISK_MODE
        }
        else
        {
            // resource已被关闭，则直接调用OnResourceCloseFinish，通知storage取消instance的资源申请
            OnResourceCloseFinish(resource_p_, need_remove_file);
        }
    }

    void Instance::NotifyGetFileName(const string& file_name)
    {
        if (false == is_running_)
        {
            return;
        }

        STL_FOR_EACH(std::set<IDownloadDriver::p>, download_driver_s_, it)
        {
            (*it)->OnNoticeGetFileName(file_name);
        }
        file_name_ = file_name;
    }

    // 通知storage关闭instance，释放资源空间
    void Instance::OnResourceCloseFinish(Resource::p resource_p, bool need_remove_file)
    {
        LOG4CPLUS_DEBUG_LOG(logger_instance, "Instance::OnResourceCloseFinish! ");

        resource_p_.reset();
        url_info_s_.clear();

        if (Storage::Inst_Storage())
        {
            global_io_svc().post(boost::bind(&Storage::OnResourceCloseFinish, Storage::Inst_Storage(), shared_from_this(),
                resource_p, need_remove_file));
        }
    }

    // 文件是否完整(是否下载完毕)
    bool Instance::IsComplete()
    {
        if (is_running_ == false) return false;

        if (!subpiece_manager_)  // 文件描述为空
        {
            LOG4CPLUS_DEBUG_LOG(logger_instance, "no");
            return false;
        }
        if (subpiece_manager_->IsFull())
        {
            LOG4CPLUS_DEBUG_LOG(logger_instance, "yes");
            return true;
        }
        LOG4CPLUS_DEBUG_LOG(logger_instance, "no");
        return false;
    }

    // 将本instance合并到new_instance中，并通知download_driver，然后删除本instance
    void Instance::BeMergedTo(Instance::p new_instance)
    {
        if (is_running_ == false) return;
        merge_to_instance_p_ = new_instance;
        merge_timer_.start();
        std::set<IDownloadDriver::p>::const_iterator iter = download_driver_s_.begin();
        assert(!new_instance->GetRID().is_empty());

        while (iter != download_driver_s_.end())
        {
            LOG4CPLUS_DEBUG_LOG(logger_instance, 
                "Instance::BeMergedTo Notify!! IDownloadDriver::OnNoticeChangeResource");
            if (*iter)
            {
                global_io_svc().post(boost::bind(&IDownloadDriver::OnNoticeChangeResource, *iter, shared_from_this(), new_instance));
                // (*iter)->OnNoticeChangeResource(shared_from_this(), new_instance);
            }
            iter++;
        }
        instance_state_ = INSTANCE_BEING_MERGED;
        merging_pos_subpiece.block_index_ = 0;
        merging_pos_subpiece.subpiece_index_ = 0;

        if (!subpiece_manager_)
        {
            // MainThread::Post(boost::bind(&Storage::RemoveInstanceForMerge, Storage::Inst_Storage(), shared_from_this()));
            Storage::Inst_Storage()->RemoveInstanceForMerge(shared_from_this());
            LOG4CPLUS_DEBUG_LOG(logger_instance, " merging finish!");
            return;
        }

        MergeResourceTo();
    }

    // 合并，然后通知Storage删除本instance
    void Instance::MergeResourceTo()
    {
        assert(instance_state_ == INSTANCE_BEING_MERGED);
        bool merge_finish = false;

        uint32_t merging_count = 0;
        while (true)
        {
            //我们收集到的个别crash (1910, 0x2e4ed)中，merge_to_instance_p为空。
            //而根据我们的逻辑，这不应该发生(该指针被置空前timer已经stop了)。
            //我们没能重现，但怀疑这是由未知的内存越界导致的。这里加个assertion来帮助我们在debug build下抓到这个问题。
            assert(merge_to_instance_p_);

            // 如果当前资源中有该subpiece而merge目的资源中没有，则mergesubpiece
            if (HasSubPiece(merging_pos_subpiece) && (merge_to_instance_p_ && !merge_to_instance_p_->HasSubPiece(merging_pos_subpiece)))
            {
                MergeSubPiece(merging_pos_subpiece, merge_to_instance_p_);
                merging_count++;
            }
            // subpiece++
            if (false == subpiece_manager_->IncSubPieceInfo(merging_pos_subpiece))
            {
                merge_finish = true;
                break;
            }
        }

        LOG4CPLUS_ERROR_LOG(logger_instance, " merging_count" << merging_count);
        if ((!merge_finish) || (!download_driver_s_.empty()))
        {
            merge_timer_.start();
            return;
        }

        // merge完成
        global_io_svc().post(boost::bind(&Storage::RemoveInstanceForMerge, Storage::Inst_Storage(), shared_from_this()));
        // Storage::Inst_Storage()->RemoveInstanceForMerge(shared_from_this());
        LOG4CPLUS_ERROR_LOG(logger_instance, " merging finish!" << merging_count);
        return;
    }

    void Instance::GetUrls(std::set<protocol::UrlInfo>& url_s)
    {
        if (is_running_ == false) return;
        url_s.clear();
        url_s.insert(url_info_s_.begin(), url_info_s_.end());
        if (!url_s.empty())
        {
            LOG4CPLUS_DEBUG_LOG(logger_instance, " " << *(url_s.begin()) << " url_s.size" << url_s.size());
        }
    }

    void Instance::GetUrls(std::vector<protocol::UrlInfo>& url_s)
    {
        if (is_running_ == false) return;
        url_s.assign(url_info_s_.begin(), url_info_s_.end());

        LOG4CPLUS_DEBUG_LOG(logger_instance, " " << " url_s.size" << url_s.size());
    }

    // 如果资源描述为空(正常情况)，则根据rid_info创建资源描述符，进而创建文件资源
    bool Instance::SetRidInfo(const protocol::RidInfo& rid_info)
    {
        if (is_running_ == false) return false;

        while (true)
        {
            if (!subpiece_manager_)
            {
                subpiece_manager_ = SubPieceManager::Create(rid_info, false);
                break;
            }
            // ------------------------------------------------------------------------------
            // 已经有资源描述符
            if (b_pure_download_mode_)
            {
                LOG4CPLUS_INFO_LOG(logger_instance, 
                    "Instance::SetRidInfo block hash !"
                    << rid_info
                    << " subpiece_manager_->rid_info_"
                    << subpiece_manager_->GetRidInfo());

                return false;
            }

            if (subpiece_manager_->HasRID())
            {
                LOG4CPLUS_INFO_LOG(logger_instance, "Instance::SetRidInfo RID已存在，返回!" << rid_info << 
                    " subpiece_manager_->rid_info_" << subpiece_manager_->GetRidInfo());
                return false;
            }

            if (subpiece_manager_->GetFileLength() != rid_info.GetFileLength())
            {
                LOG4CPLUS_INFO_LOG(logger_instance, "Instance::SetRidInfo rid_info:" << rid_info << 
                    " subpiece_manager_->rid_info_:" << subpiece_manager_->GetRidInfo());
                return false;
            }

            for (uint32_t i = 0; i < subpiece_manager_->GetBlockCount(); i++)
            {
                if (!subpiece_manager_->GetRidInfo().block_md5_s_[i].is_empty())
                {
                    if (subpiece_manager_->GetRidInfo().block_md5_s_[i] != rid_info.block_md5_s_[i])
                    {
                        LOG4CPLUS_INFO_LOG(logger_instance, "Instance::SetRidInfo block hash 不同，返回！" << rid_info);
                        // 已下载block hash 不符，返回
                        return false;
                    }
                }
            }
            subpiece_manager_->InitRidInfo(rid_info);
            break;
        }

        if (subpiece_manager_)
        {
            assert(rid_info.GetBlockCount() == subpiece_manager_->GetBlockCount());
            assert(rid_info.GetBlockSize() == subpiece_manager_->GetBlockSize());
        }

        // 告诉download_driver, rid改变
        if ((subpiece_manager_ && subpiece_manager_->HasRID()) || rid_info.HasRID())
        {
            std::set<IDownloadDriver::p>::const_iterator iter = download_driver_s_.begin();
            while (iter != download_driver_s_.end())
            {
                if (*iter)
                {
                    // MainThread::Post(boost::bind(&IDownloadDriver::OnNoticeRIDChange, *iter));
                    (*iter)->OnNoticeRIDChange();
                }
                iter++;
            }
        }
        LOG4CPLUS_INFO_LOG(logger_instance, " success!" << rid_info);
        // 注意

        // 创建文件资源
        if (instance_state_ == INSTANCE_NEED_RESOURCE)
        {
            assert(!resource_p_);
            TryCreateResource();
        }
        return true;
    }

    // 根据url_info_s的第一个Url生成资源文件的文件名，然后向Storage申请资源
    // instance状态：NEED_RESOURCE --> APPLY_RESOURCE
    void Instance::TryCreateResource()
    {
        assert(url_info_s_.size()>0);
        if (instance_state_ != INSTANCE_NEED_RESOURCE)
        {
            return;
        }
        instance_state_ = INSTANCE_APPLY_RESOURCE;
        protocol::UrlInfo url_info = *(url_info_s_.begin());
        ParseFileNameFromUrl(url_info);

        LOG4CPLUS_DEBUG_LOG(logger_instance, "try create resource file :" << (resource_name_) << " length:" 
            << subpiece_manager_->GetFileLength());
        Storage::Inst_Storage()->ApplyResource(shared_from_this());

#ifdef DISK_MODE
        // Try Rename!
        if (IsSaveMode())
        {
            Storage::Inst()->AttachSaveModeFilenameByUrl(url_info.url_, web_url_, qname_);
        }
#endif
        return;
    }

    // 添加url_info(如果已存在不做操作)，并通知download_driver
    void Instance::AddUrlInfo(const std::vector<protocol::UrlInfo>& url_infos)
    {
        if (is_running_ == false) return;

        bool need_notify = false;
        for (std::vector<protocol::UrlInfo>::const_iterator it = url_infos.begin(); it != url_infos.end(); ++it)
        {
            LOG4CPLUS_DEBUG_LOG(logger_instance, "Instance::AddUrlInfo, URL: " << *it);
            if (url_info_s_.find(*it) == url_info_s_.end())
            {
                need_notify = true;
                url_info_s_.insert(*it);
            }
        }

        LOG4CPLUS_DEBUG_LOG(logger_instance, "Instance::AddUrlInfo, 添加URL之后:" << url_info_s_.size());

        if (!need_notify)
        {
            return;
        }
    }

    // 添加url(如果已存在，替换本地refer)，并通知download_driver
    void Instance::AddUrlInfo(const protocol::UrlInfo& url_info)
    {
        if (is_running_ == false)
            return;
        bool need_insert = true;

        for (std::set<protocol::UrlInfo>::iterator list_it = url_info_s_.begin(); list_it != url_info_s_.end(); ++list_it)
        {
            // 单个增加，是从CreateInstance获取的原生url信息，如果url相同，refer将被替换！
            if ((*list_it).url_ == url_info.url_)
            {
                if ((*list_it).refer_url_ == url_info.refer_url_)
                {
                    return;
                }
                // (*list_it).refer_url_ = url_info.refer_url_;
                protocol::UrlInfo info = *list_it;
                url_info_s_.erase(list_it);
                info.refer_url_ = url_info.refer_url_;
                url_info_s_.insert(info);
                need_insert = false;
                break;
            }
        }

        if (need_insert)
        {
            url_info_s_.insert(url_info);
        }
    }

    // 从url_info_s中删除某个url，并通知download_driver
    void Instance::RemoveUrl(const string& url_str)
    {
        if (is_running_ == false)
            return;
        int count = 0;
        if (url_info_s_.empty())
        {
            return;
        }
        for (std::set<protocol::UrlInfo>::iterator it = url_info_s_.begin(); it != url_info_s_.end();)
        {
            if (url_str == (*it).url_)
            {
                url_info_s_.erase(it++);
                count++;
            }
            else
                it++;
        }
        if (count == 0)  // 什么都没删
        {
            LOG4CPLUS_ERROR_LOG(logger_instance, "erase error!");
            return;
        }
    }

    // 将subpiece添加到cache
    void Instance::AsyncAddSubPiece(const protocol::SubPieceInfo& subpiece_info, const protocol::SubPieceBuffer& buffer)
    {
        if (is_running_ == false)
            return;
        if (!subpiece_manager_)
        {
            LOG4CPLUS_ERROR_LOG(logger_instance, "subpiece_manager_ is null!");
            return;
        }

        if (!subpiece_manager_->IsSubPieceValid(subpiece_info))
        {
            LOG4CPLUS_ERROR_LOG(logger_instance, "subpiece_info " << subpiece_info << " is not valid.");
            return;
        }

        // 重复的直接丢弃
        if (subpiece_manager_->HasSubPiece(subpiece_info))
        {
            LOG4CPLUS_ERROR_LOG(logger_instance, "subpiece_manager_->HasSubPiece " << subpiece_info);
            return;
        }

        if (subpiece_manager_->AddSubPiece(subpiece_info, buffer))
        {
            if (send_count_ < send_speed_limit_)
            {
                uint32_t position = subpiece_manager_->SubPieceInfoToPosition(subpiece_info);
                LOG4CPLUS_ERROR_LOG(logger_instance, "Notify OnRecvSubPiece, subpiece:" << subpiece_info 
                    << ", position = " << position);
                for (std::set<IDownloadDriver::p>::const_iterator iter = download_driver_s_.begin(); iter != download_driver_s_.end(); ++iter) {
                    (*iter)->OnRecvSubPiece(position, buffer);
                }
                ++send_count_;
            }
            else
            {
                LOG4CPLUS_ERROR_LOG(logger_instance, "send_count_ full, send_count_=" << send_count_ << 
                    ", send_speed_limit_=" << send_speed_limit_);
            }
        }
    }

    void Instance::OnWriteBlockFinish(uint32_t block_index)
    {
        if (false == is_running_)
            return;
        if (subpiece_manager_)
        {
            LOG4CPLUS_DEBUG_LOG(logger_instance, "OnWriteBlockFinish block_index:" << block_index);
            subpiece_manager_->OnWriteBlockFinish(block_index);
        }
    }

void Instance::OnReadBlockForUploadFinishWithHash(uint32_t block_index, base::AppBuffer& buf, 
        IUploadListener::p listener, MD5 hash_val)
    {
        if (false == is_running_)
            return;
        if (!subpiece_manager_)
            return;

        DebugLog("hash: OnReadBlockForUploadFinishWithHash index:%d md5:%s", block_index, hash_val.to_string().c_str());

        if (subpiece_manager_->GetRidInfo().block_md5_s_[block_index].is_empty())
        {
            subpiece_manager_->GetRidInfo().block_md5_s_[block_index] = hash_val;
        }

        if (subpiece_manager_->GetRidInfo().block_md5_s_[block_index] == hash_val)
        {
            // MD5校验成功--------------------------------
            LOG4CPLUS_DEBUG_LOG(logger_instance, "Hash ok and Upload------>block index:" << block_index);
            UpdateBlockHashTime(block_index);
            if (listener)
            {
                listener->OnAsyncGetBlockSucced(GetRID(), block_index, buf);
            }            
        }
        else
        {
            // MD5校验失败--------------------------------
            LOG4CPLUS_DEBUG_LOG(logger_instance, "Hash failed----->block index:" << block_index);
            if (listener)
            {
                listener->OnAsyncGetBlockFailed(GetRID(), block_index, (int) ERROR_GET_SUBPIECE_BLOCK_VERIFY_FAILED);
            }            
        }
    }

    void Instance::OnReadBlockForUploadFinish(uint32_t block_index, base::AppBuffer& buf, IUploadListener::p listener)
    {
        if (false == is_running_)
        {
            return;
        }

        DebugLog("hash: OnReadBlockForUploadFinish index:%d", block_index);
        if (listener)
        {
            listener->OnAsyncGetBlockSucced(GetRID(), block_index, buf);
        }
    }

    // block写入完毕，赋予MD5值，检查文件是否写入完毕，完毕则通知其他模块下载完毕
    // 读取需要上传的block，上传
    void Instance::OnPendingHashBlockFinish(uint32_t block_index, MD5 hash_val)
    {
        if (false == is_running_)
            return;
        if (!subpiece_manager_)
            return;

        LOG4CPLUS_DEBUG_LOG(logger_instance, " block_index:" << block_index << " md5:" << hash_val);

        if (subpiece_manager_->GetRidInfo().block_md5_s_[block_index].is_empty())
        {
            subpiece_manager_->GetRidInfo().block_md5_s_[block_index] = hash_val;
        }

        if (subpiece_manager_->GetRidInfo().block_md5_s_[block_index] == hash_val)
        {
            LOG4CPLUS_DEBUG_LOG(logger_instance, "Hash Block Verifed, block_index = " << block_index << 
                ", HAS_BEEN_VERIFIED");

            // 告诉download_driver，hash成功
            OnNotifyHashBlock(block_index, true);
            if (subpiece_manager_->GenerateRid())
            {
                LOG4CPLUS_DEBUG_LOG(logger_instance, " Finished: ");
                OnHashResourceFinish();
            }
        }
        else
        {
            assert(false);
            md5_hash_failed_++;
            LOG4CPLUS_DEBUG_LOG(logger_instance, " MD5CheckFailed! md5[block_index] (" << 
                subpiece_manager_->GetRidInfo().block_md5_s_[block_index] << ")  != " << hash_val);
            if (resource_p_)
            {
#ifdef DISK_MODE
                StorageThread::Post(boost::bind(&Resource::ThreadRemoveBlock, resource_p_, block_index));
#else
                resource_p_->ThreadRemoveBlock(block_index);
#endif  // #ifdef DISK_MODE
            }
            else
            {
                // 删除block，作用同上
                OnRemoveResourceBlockFinish(block_index);
            }
        }
    }

    // 从资源描述, pending_subpiece_manager和pending_get_subpiece_manager中删除block
    // 通知upload_listener获取subpiece失败，通知download_driver，makeblock失败
    void Instance::OnRemoveResourceBlockFinish(uint32_t block_index)
    {
        if (!subpiece_manager_)
        {
            return;
        }
        LOG4CPLUS_DEBUG_LOG(logger_instance, " block_index:" << block_index << "rid_info: " << 
            subpiece_manager_->GetRidInfo());
        subpiece_manager_->RemoveBlockInfo(block_index);

        OnNotifyHashBlock(block_index, false);
    }

    void Instance::UploadOneSubPiece()
    {
        if (is_open_service_)
        {
            statistic::DACStatisticModule::Inst()->SubmitP2PUploadBytes(SUB_PIECE_SIZE, is_push_);
        }
        if (traffic_list_.size() == 0 && TRAFFIC_T0 > 0)
        {
            traffic_list_.push_back(1);
            last_push_time_ = 0;
            return;
        }
        else
        {
            traffic_list_.back() = traffic_list_.back() + 1;
        }

        if (!have_upload_at_least_one_subpiece_)
        {
            statistic::DACStatisticModule::Inst()->SubmitRidUploadCount();
            have_upload_at_least_one_subpiece_ = true;
        }
    }

    // 如果某个block已被鉴定，则从文件中读取，将该block交给upload_driver，否则...
    void Instance::AsyncGetBlock(uint32_t block_index, IUploadListener::p listener)
    {
        
        if (is_running_ == false)
        {
            return;
        }

        if (!subpiece_manager_)
        {
            LOG4CPLUS_ERROR_LOG(logger_instance, "AsyncGetBlock: subpiece_manager_==null");
            return;
        }

        assert(HasRID());

        // block不是满的
        if (false == subpiece_manager_->HasFullBlock(block_index))
        {
            if (listener)
            {
                listener->OnAsyncGetBlockFailed(GetRID(), block_index, (int)ERROR_GET_SUBPIECE_BLOCK_NOT_FULL);
            }
            return;
        }

#ifdef DISK_MODE
        // block是满的，校验，然后交给UploadDriver
        if (NULL == resource_p_)
        {
            if (listener)
            {
                listener->OnAsyncGetBlockFailed(GetRID(), block_index, (int)ERROR_GET_SUBPIECE_BLOCK_NOT_FULL);
            }
        }
        else
        {
            DebugLog("hash: Upload Get Block index: %d", block_index);
            StorageThread::Post(boost::bind(&Resource::ThreadReadBlockForUpload, resource_p_, GetRID(), block_index,
                listener, CheckBlockNeedHash(block_index)));
        }
#else
        if (listener)
        {
            listener->OnAsyncGetBlockFailed(GetRID(), block_index, (int)ERROR_GET_SUBPIECE_BLOCK_NOT_FULL);
        }
#endif
        return;
    }

    // 从pending_subpiece_manager中或文件中找到某个subpiece，然后merge到merge_to_instance_p
    void Instance::MergeSubPiece(const protocol::SubPieceInfo& subpiece_info, Instance::p merge_to_instance_p)
    {
        if (!subpiece_manager_ || !merge_to_instance_p)
        {
            return;
        }
        LOG4CPLUS_DEBUG_LOG(logger_instance, "");
        assert(subpiece_manager_->HasSubPiece(subpiece_info));

        protocol::SubPieceBuffer ret_buf = subpiece_manager_->GetSubPiece(subpiece_info);
        if (ret_buf.Length() != 0)
        {
            LOG4CPLUS_DEBUG_LOG(logger_instance, "success! pending return:" << subpiece_info);
            if (merge_to_instance_p)
                merge_to_instance_p->OnMergeSubPieceSuccess(subpiece_info, ret_buf);
            return;
        }
        else
        {
#ifdef DISK_MODE
            StorageThread::Post(boost::bind(&Resource::ThreadMergeSubPieceToInstance, resource_p_, subpiece_info,
                new protocol::SubPieceContent(), merge_to_instance_p));
#endif  // #ifdef DISK_MODE
            return;
        }
    }

    // 将subpiece添加到准备写入的队列，并检查是否该写入文件，并检查block, 上传等操作, 针对merge_to_instance_p
    void Instance::OnMergeSubPieceSuccess(const protocol::SubPieceInfo& subpiece_info, protocol::SubPieceBuffer buffer)
    {
        if (is_running_ == false)
            return;
        LOG4CPLUS_DEBUG_LOG(logger_instance, "" << subpiece_info);
        AsyncAddSubPiece(subpiece_info, buffer);
    }

    bool Instance::GetSubPieceForPlay(IDownloadDriver::p dd, uint32_t start_position, std::vector<protocol::SubPieceBuffer> & buffers)
    {
        LOG4CPLUS_DEBUG_LOG(logger_instance, " start_position:" << start_position);
        if (is_running_ == false)
            return false;
        if (!subpiece_manager_)
            return false;

        not_upload_tc_.reset();

        if (start_position < GetFileLength())
        {
            protocol::SubPieceInfo start_s_info;
            if (!subpiece_manager_->PosToSubPieceInfo(start_position, start_s_info))
            {
                return false;
            }

            protocol::SubPieceInfo playing_s_info;
            protocol::SubPieceBuffer ret_buf;
            while (send_count_ < send_speed_limit_)
            {
                if (!subpiece_manager_->HasSubPieceInMem(start_s_info))
                {
                    LOG4CPLUS_DEBUG_LOG(logger_instance, "HasSubPieceInMem = False, subpiece_info = " << start_s_info);
                    break;
                }
                ret_buf = subpiece_manager_->GetSubPiece(start_s_info);
                assert(ret_buf.Data() && (ret_buf.Length() <= bytes_num_per_piece_g_) && (ret_buf.Length()>0));
                if (ret_buf.Data() && (ret_buf.Length() <= bytes_num_per_piece_g_) && (ret_buf.Length()>0))
                {
                    buffers.push_back(ret_buf);
                    ++send_count_;
                    playing_s_info = start_s_info;
                }
                else
                {
                    LOG4CPLUS_DEBUG_LOG(logger_instance, "GetSubPiece failed! " << start_s_info);
                    break;
                }

                if (!subpiece_manager_->IncSubPieceInfo(start_s_info))
                {
                    LOG4CPLUS_DEBUG_LOG(logger_instance, "End of IncSubPieceInfo " << start_s_info);
                    break;
                }
            }
#ifdef DISK_MODE
            // read from disk @herain
            if (send_count_ < send_speed_limit_ &&
                subpiece_manager_->IsSubPieceValid(start_s_info) &&
                subpiece_manager_->HasSubPiece(start_s_info))
            {
                ReadFromDisk(start_s_info);
            }
            else if (send_count_ >= send_speed_limit_)
            {
                LOG4CPLUS_ERROR_LOG(logger_instance, "send_count_ full, send_count_=" << send_count_ << 
                    ", send_speed_limit_=" << send_speed_limit_);
            }
#endif

            return !buffers.empty();
        }
        else
        {
            LOG4CPLUS_DEBUG_LOG(logger_instance, "Play to the end of file! ");
            return false;
        }
    }

    bool Instance::GetNextPieceForDownload(const protocol::PieceInfoEx &start_piece_index, protocol::PieceInfoEx& piece_for_download)
    {
        if (is_running_ == false)
            return false;

        LOG4CPLUS_DEBUG_LOG(logger_instance, "start_piece_index:" << start_piece_index);

        piece_for_download = protocol::PieceInfoEx();
        if (!subpiece_manager_)
        {
            assert(false);
            return true;
        }

        protocol::SubPieceInfo start_subpiece_info, subpiece_for_download;
        subpiece_manager_->PieceInfoExToSubPieceInfo(start_piece_index, start_subpiece_info);
        if (!subpiece_manager_->GetNextNullSubPiece(start_subpiece_info, subpiece_for_download))
        {
            LOG4CPLUS_DEBUG_LOG(logger_instance, "Failed!");
            return false;
        }
        else
        {
            subpiece_manager_->SubPieceInfoToPieceInfoEx(subpiece_for_download, piece_for_download);
            LOG4CPLUS_DEBUG_LOG(logger_instance, "Succeed! piece_for_download=" << piece_for_download);
            return true;
        }
    }

    bool Instance::GetNextPieceForDownload(uint32_t start_position, protocol::PieceInfoEx& piece_for_download)
    {
        if (is_running_ == false)
            return false;

        piece_for_download = protocol::PieceInfoEx();
        if (!subpiece_manager_)
        {
            return true;
        }

        protocol::PieceInfoEx piece_index;
        if (!subpiece_manager_->PosToPieceInfoEx(start_position, piece_index))
        {
            LOG4CPLUS_DEBUG_LOG(logger_instance, "Instance::GetNextPieceForDownload false!" << " start_position:" 
                << start_position << " piece_for_download" << piece_for_download);
            return false;
        }

        if (!GetNextPieceForDownload(piece_index, piece_for_download))
        {
            LOG4CPLUS_DEBUG_LOG(logger_instance, "Instance::GetNextPieceForDownload false!" << " start_position:" 
                << start_position << " piece_for_download" << piece_for_download);
            return false;
        }
        return true;
    }

    // 向instance加入某个download_driver
    void Instance::AttachDownloadDriver(IDownloadDriver::p download_driver)
    {
        LOG4CPLUS_INFO_LOG(logger_instance, " download_driver" << download_driver);
        if (is_running_ == false)
            return;
        assert(download_driver != 0);

        if (download_driver_s_.find(download_driver) == download_driver_s_.end())
        {
            download_driver_s_.insert(download_driver);
            send_count_ = 0;
            // notify file_name
            if (file_name_.length() != 0)
            {
                if (download_driver)
                {
                    download_driver->OnNoticeGetFileName(file_name_);
                }
            }
        }
    }

    // 从instance中删掉某个download_driver，并将内存中所有数据写入文件
    void Instance::DettachDownloadDriver(IDownloadDriver::p download_driver)
    {
        if (is_running_ == false)
            return;
        // detach
        download_driver_s_.erase(download_driver);

        if (download_driver_s_.empty())
        {
            LOG4CPLUS_DEBUG_LOG(logger_instance, "download_driver_s_.empty");
            delete_tc_.reset();
            deattach_timer_.start();

#ifndef DISK_MODE
            Storage::Inst_Storage()->RemoveInstance(shared_from_this(), false);
#endif
        }
    }

    // 根据url生成文件名，如果获取失败，则生成一串随机数表示文件名, 文件名写入original_url_info_url
    // TODO:yujinwu: url_info在origanel_url_info_存在的情况下是不起作用的
    void Instance::ParseFileNameFromUrl(const protocol::UrlInfo &url_info)
    {
        LOG4CPLUS_DEBUG_LOG(logger_instance, "url " << url_info);

        string tmp_url = url_info.url_;

        if (tmp_url != origanel_url_info_.url_ && origanel_url_info_.url_.size() != 0)
        {
            tmp_url = origanel_url_info_.url_;

            LOG4CPLUS_DEBUG_LOG(logger_instance, "Warning: use original url: " << tmp_url);
        }

        string tmp_name = Storage::Inst_Storage()->FindRealName(tmp_url);
        if (tmp_name.size() != 0)
        {
            this->resource_name_ = tmp_name;
            return;
        }

        network::Uri uri(url_info.url_);
        string urifile = uri.getfile();

        if (urifile.size() == 0)
        {
            char buffer[40] = { 0 };
            sprintf(buffer, "%08x%08x%s", Random::GetGlobal().Next(),
                Random::GetGlobal().Next(), ".flv");

            tmp_name = buffer;
        }
        else
        {
            if (IsOpenService() && !file_name_.empty())
            {
                urifile = file_name_; 
            }

            tmp_name = urifile;
        }

        this->resource_name_ = tmp_name;
    }

    // 通知Storage做相应操作，通知appmodule发AddRidUrlRequestPacket包，通知download_driver下载完毕
    void Instance::OnHashResourceFinish()
    {
        assert(subpiece_manager_->HasRID());
        assert(url_info_s_.size() == 1);
        Storage::Inst_Storage()->AddInstanceToRidMap(shared_from_this());

        STL_FOR_EACH_CONST(std::set<IDownloadDriver::p>, download_driver_s_, iter)
        {
            if (*iter)
            {
                (*iter)->OnNoticeDownloadComplete();
            }
        }
    }

    // 通知DownloadDriver makeblock成功或失败
    void Instance::OnNotifyHashBlock(uint32_t block_index, bool b_success)
    {
        if (is_running_ == false)
            return;

        LOG4CPLUS_DEBUG_LOG(logger_instance, "block_index:" << block_index << (b_success?"hash success!":"hash fail! ") 
            << subpiece_manager_->GetRidInfo());
        std::set<IDownloadDriver::p>::const_iterator iter;

        for (iter = download_driver_s_.begin(); iter != download_driver_s_.end(); ++iter)
        {
            if (*iter)
            {
                if (!b_success)
                    global_io_svc().post(boost::bind(&IDownloadDriver::OnNoticeMakeBlockFailed, *iter, block_index));
                else
                    global_io_svc().post(boost::bind(&IDownloadDriver::OnNoticeMakeBlockSucced, *iter, block_index));
            }
        }
    }

    // 将resource_p_中的instance指向本对象，并将信息写入ResourceInfo文件中(ResourceInfo.dat)
    // instance状态: APPLY_RESOURCE --> HAVE_RESOURCE
    void Instance::AttachResource(Resource::p resource_p, bool is_opening)
    {
        assert(is_running_ == true);
        assert(instance_state_ == INSTANCE_APPLY_RESOURCE);
        assert(resource_p);
        assert(!resource_p->GetInstance());

#ifdef DISK_MODE
        disk_file_size_ = resource_p->GetLocalFileSize();
#endif  // #ifdef DISK_MODE

        resource_name_ = resource_p->GetLocalFileName();
        resource_p->BindInstance(shared_from_this(), subpiece_manager_);
        resource_p_ = resource_p;
        instance_state_ = INSTANCE_HAVE_RESOURCE;

#ifdef DISK_MODE
        if (!is_opening)
        {
            // 在打开Instance的时候不需要SaveResourceInfoToDisk
            LOG4CPLUS_DEBUG_LOG(logger_instance, "add bind SaveResourceInfoToDisk");
            global_io_svc().post(boost::bind(&Storage::SaveResourceInfoToDisk, Storage::Inst_Storage()));
        }
#endif  // #ifdef DISK_MODE

        return;
    }

    // 只有没有download_driver且计时器大于10s时，才能删除
    bool Instance::CanRemove()
    {
        if (!resource_p_)
            return false;

        if (instance_state_ != INSTANCE_HAVE_RESOURCE)
        {
            return false;
        }

        if (disk_file_size_ == 0)
        {
            return false;
        }

        // 通过BHO保存下载的资源，不会被删除
        if (IsSaveMode())
        {
            return false;
        }

        if (download_driver_s_.empty())
        {
            if (delete_tc_.elapsed() > default_delay_tickcount_for_delete)
            {
                return true;
            }
        }
        return false;
    }

    // ------------------------------------------------------------
    // cfg_timer_ = 10s
    void Instance::OnConfigTimerElapsed(framework::timer::Timer * pointer)
    {
        if (is_running_ == false)
        {
            return;
        }

        assert(pointer == &cfg_timer_);

        if (!resource_p_)
        {
            if (download_driver_s_.empty() && (!subpiece_manager_ || subpiece_manager_->IsEmpty()))
            {
                if (delete_tc_.elapsed() > default_delay_tickcount_for_delete)
                {
                    LOG4CPLUS_INFO_LOG(logger_instance, 
                        "delete_tc_.GetElapsed()>default_delay_tickcount_for_delete kill myslef!" 
                        << "notice storage to remove myself!");
                    Storage::Inst_Storage()->RemoveInstance(shared_from_this(), true);
                }
            }
        }
    }

    void Instance::OnMergeTimerElapsed(framework::timer::Timer * pointer)
    {
        if (is_running_ == false)
        {
            return;
        }

        assert(pointer == &merge_timer_);

        MergeResourceTo();
    }

    // ------------------------------------------------------------
    // traffic_timer_ = 1s
    void Instance::OnTrafficTimerElapsed(framework::timer::Timer * pointer)
    {
        if (is_running_ == false)
            return;

        assert(pointer == &traffic_timer_);
        
        ++last_push_time_;
        if (last_push_time_ >= TRAFFIC_UNIT_TIME)
        {
            traffic_list_.push_back(0);
            while (traffic_list_.size() > TRAFFIC_T0)
            {
                traffic_list_.pop_front();
            }
            last_push_time_ = 0;
        }
        if (false == IsDownloading() && false == IsUploading())
        {
            FreeResourceHandle();
        }

        // 借用traffic_timer_来控制发送数据的速度
        send_count_ = 0;
        if (download_driver_s_.size())
        {
            LOG4CPLUS_DEBUG_LOG(logger_instance, "---------------OnTimerElapsed:" << download_driver_s_.size() 
                << "----------------");
            std::set<IDownloadDriver::p>::iterator iter = download_driver_s_.begin();
            for (; iter != download_driver_s_.end() && (*iter)->IsHeaderResopnsed(); ++iter)
            {
                IDownloadDriver::p dd = *iter;
                uint32_t play_position = dd->GetPlayingPosition();
                if (play_position < GetFileLength())
                {
                    protocol::SubPieceInfo play_s_info;
                    if (!subpiece_manager_->PosToSubPieceInfo(play_position, play_s_info))
                    {
                        continue;
                    }
                    if (subpiece_manager_->HasSubPieceInMem(play_s_info))
                    {
                        LOG4CPLUS_DEBUG_LOG(logger_instance, "Has SubPiece " << play_s_info << 
                            " In Memory, call OnRecvSubPiece ");
                        dd->OnRecvSubPiece(play_position, subpiece_manager_->GetSubPiece(play_s_info));
                    }
                    else if (subpiece_manager_->HasSubPiece(play_s_info))
                    {
                        LOG4CPLUS_DEBUG_LOG(logger_instance, "Has SubPiece " << play_s_info << 
                            " In Disk, call ReadFromDisk ");
#ifdef DISK_MODE
                        ReadFromDisk(play_s_info);
#endif
                    }
                    else
                    {
                        LOG4CPLUS_DEBUG_LOG(logger_instance, "Don't Has SubPiece " << play_s_info);
                    }
                }
            }
        }
    }

    void Instance::OnDeAttachTimerElapsed(framework::timer::Timer * pointer)
    {
        // herain:2011-1-11:deattach_timer_的响应函数，用于在所有download_driver都deattach后
        // 延迟5s再来存储未保存数据，防止在拖动时存储未保存的数据的过程占用存储线程
        // 造成读磁盘会延时的问题。
        assert(pointer == &deattach_timer_);

        if (resource_p_ && subpiece_manager_ && download_driver_s_.empty())
        {
            LOG4CPLUS_DEBUG_LOG(logger_instance, "OnDeAttachTimerElapsed SaveAllBlock");
            subpiece_manager_->SaveAllBlock(resource_p_);
        }
    }

#ifdef DISK_MODE
    bool Instance::GetFileResourceInfo(FileResourceInfo &r_info)
    {
        if (instance_state_ != INSTANCE_HAVE_RESOURCE)
        {
            return false;
        }
        assert(resource_p_);

        if (disk_file_size_ == 0)
        {
            return false;
        }

        r_info.url_info_.assign(url_info_s_.begin(), url_info_s_.end());
        r_info.rid_info_ = subpiece_manager_->GetRidInfo();
        r_info.file_path_ = resource_name_;
        r_info.last_push_time_ = last_push_time_;
        r_info.traffic_list_.assign(traffic_list_.begin(), traffic_list_.end());
        r_info.down_mode_ = down_mode_;
        r_info.data_rate_ = data_rate_;
        r_info.file_duration_in_sec_ = file_duration_in_sec_;
        r_info.file_name_ = file_name_;
        r_info.file_path_ = resource_p_->GetLocalFileName();
        r_info.last_write_time_ = last_write_time_;
        r_info.web_url_ = web_url_;
        r_info.is_open_service_ = is_open_service_;
        r_info.is_push_ = is_push_;

        assert(!r_info.file_path_.empty());
        return true;
    }
#endif  // #ifdef DISK_MODE

    // 资源文件改名，通知storage将文件信息写入资源信息文件
    void Instance::OnDiskFileNameChange(string file_name)
    {
        resource_name_ = file_name;
#ifdef DISK_MODE
        // MainThread::Post(boost::bind(&Storage::SaveResourceInfoToDisk, Storage::Inst_Storage()));
        Storage::Inst_Storage()->SaveResourceInfoToDisk();
#endif  // #ifdef DISK_MODE
    }

    // 用于AttatchFilenameByUrl，newname应为完整文件名
    bool Instance::Rename(const string& newname)
    {
        const string new_name = newname;
        if (new_name.size() == 0)
        {
            return false;
        }
        if (resource_p_)
        {
#ifdef DISK_MODE
            StorageThread::Post(boost::bind(&Resource::Rename, resource_p_, new_name));
            // resource_p_->Rename(new_name);
#else
            resource_p_->Rename(new_name);
#endif  // #ifdef DISK_MODE

            return true;
        }
        LOG4CPLUS_DEBUG_LOG(logger_instance, "命名失败，原因：resource_p == 0");
        return false;
    }

    void Instance::OnRenameFinish(const string newname)
    {
        resource_name_ = newname;
        is_have_rename_ = true;
        LOG4CPLUS_DEBUG_LOG(logger_instance, "resource_name_ " << (resource_name_));
#ifdef DISK_MODE
        Storage::Inst_Storage()->SaveResourceInfoToDisk();
#endif  // #ifdef DISK_MODE
    }

    float Instance::GetInstanceValue()
    {
        LOG4CPLUS_DEBUG_LOG(logger_instance, "--------------" << resource_p_->file_name_);
        uint32_t total_time = traffic_list_.size();
        float protect_value = 0.0;
        if (total_time <= TRAFFIC_PROTECT_TIME)
        {
            // 应该被保护，不该被删除
            LOG4CPLUS_DEBUG_LOG(logger_instance, "in protect period as before");
            protect_value = 65535;
        }

        // assert(total_time > 0);

        std::list<uint32_t>::iterator begin = traffic_list_.begin();
        std::list<uint32_t>::iterator end = traffic_list_.end();

        double sum = 0;
        int i = 0;
        int n = traffic_list_.size();
        LOG4CPLUS_DEBUG_LOG(logger_instance, "天数 = " << n);
        for (std::list<uint32_t>::iterator it = begin; it != end; ++it)
        {
            LOG4CPLUS_DEBUG_LOG(logger_instance, "i = " << i << " 分子 = " << pow((double)2, (double)i) << " 分母 = " 
                << (pow((double)2, (double)n)-1) << " 值 = " << *it);
            if (i != total_time-1)
            {
                LOG4CPLUS_DEBUG_LOG(logger_instance, "时间参数 = " << TRAFFIC_UNIT_TIME);
                sum += pow((double)2, (double)i) * (*it) / (pow((double)2, (double)n)-1) / TRAFFIC_UNIT_TIME;
            }
            else
            {
                LOG4CPLUS_DEBUG_LOG(logger_instance, "时间参数 = " << last_push_time_);
                sum += pow((double)2, (double)i) * (*it) / (pow((double)2, (double)n)-1) / last_push_time_;
            }

            i++;
        }
        if (resource_p_->actual_size_ == 0)
        {
            LOG4CPLUS_DEBUG_LOG(logger_instance, "Actual size == 0");
            return 0;
        }

        LOG4CPLUS_DEBUG_LOG(logger_instance, "码流1 = " << data_rate_ << " 码流2 = " << GetMetaData().VideoDataRate 
            << " 时长 = " << GetMetaData().Duration << " FileLength = " << GetMetaData().FileLength);
        // 码流
        // 将码流率由 B 转换为 kb
        float dr = data_rate_ * 8 / 1024;
        LIMIT_MIN_MAX(dr, 500, 1500);

        dr = dr / 500;

        assert(resource_p_->actual_size_ > 0);
        LOG4CPLUS_DEBUG_LOG(logger_instance, "码流系数 = " << dr << " value = " 
            << float(sum * SUB_PIECE_SIZE) / (resource_p_->actual_size_) * dr * 1024);
        return protect_value + float(sum * SUB_PIECE_SIZE) / (resource_p_->actual_size_) * dr * 1024;
    }

    bool Instance::ParseMetaData(base::AppBuffer const & buf)
    {
        boost::uint8_t *buffer = buf.Data();
        int length = buf.Length();

        if (buffer == NULL || length == 0)
            return false;

        // format
        if (true == storage::MetaMP4Parser::IsMP4((const boost::uint8_t*) buffer, length))
            GetMetaData().FileFormat = "mp4";
        else if (true == storage::MetaFLVParser::IsFLV((const boost::uint8_t*) buffer, length))
            GetMetaData().FileFormat = "flv";
        else
            GetMetaData().FileFormat = "none";

        // meta
        if (GetMetaData().FileFormat == "flv")
        {
            storage::MetaFLVParser parser;
            parser.Parse((const boost::uint8_t*) buffer, length);

            // get
            boost::any duration = parser.GetProperty("duration");
            if (duration.type() == typeid(double))
                GetMetaData().Duration = (uint32_t) (boost::any_cast<double>(duration) + 0.5);
            boost::any width = parser.GetProperty("width");
            if (width.type() == typeid(double))
                GetMetaData().Width = (uint32_t) (boost::any_cast<double>(width) + 0.5);
            boost::any height = parser.GetProperty("height");
            if (height.type() == typeid(double))
                GetMetaData().Height = (uint32_t) (boost::any_cast<double>(height) + 0.5);
            boost::any video_data_rate = parser.GetProperty("videodatarate");
            if (video_data_rate.type() == typeid(double))
                GetMetaData().VideoDataRate = (uint32_t) (boost::any_cast<double>(video_data_rate) / 8.0 + 0.5);
        }
        else if (GetMetaData().FileFormat == "mp4")
        {
            storage::MetaMP4Parser parser;
            parser.Parse((const boost::uint8_t*) buffer, length);

            GetMetaData().Duration = parser.GetDurationInSeconds();
        }

        GetMetaData().FileLength = GetFileLength();
        if (GetMetaData().Duration != 0)
            GetMetaData().VideoDataRate = (uint32_t) (GetMetaData().FileLength / (GetMetaData().Duration + 0.0) + 0.5);

        file_duration_in_sec_ = GetMetaData().Duration;
        data_rate_ = GetMetaData().VideoDataRate;

        return true;
    }

    void Instance::FreeResourceHandle()
    {
        if (resource_p_)
        {
#ifdef DISK_MODE
            StorageThread::Post(boost::bind(&Resource::CloseFileHandle, resource_p_));
#endif  // #ifdef DISK_MODE
        }
    }

    bool Instance::IsSaveMode() const
    {
        return down_mode_ == storage::DM_BY_BHOSAVE;
    }

    void Instance::SetSaveMode(bool save_mode)
    {
        down_mode_ = (save_mode ? storage::DM_BY_BHOSAVE : storage::DM_BY_ACCELERATE);
    }


    void Instance::OnFileWriteFinish()
    {
        if (false == is_running_)
        {
            return;
        }
        LOG4CPLUS_DEBUG_LOG(logger_instance, "OnFileWriteFinish!");
        STL_FOR_EACH(std::set<IDownloadDriver::p>, download_driver_s_, it)
        {
            if (*it)
            {
                (*it)->OnNoticeFileDownloadComplete();
            }
        }
    }

    void Instance::OnThreadReadSubPieceSucced(const protocol::SubPieceInfo & subpiece_info, protocol::SubPieceBuffer buff)
    {
        assert(subpiece_manager_);
        STL_FOR_EACH_CONST(std::set<IDownloadDriver::p>, download_driver_s_, iter)
        {
            (*iter)->OnRecvSubPiece(subpiece_info.GetPosition(GetBlockSize()), buff);
        }
    }

#ifdef DISK_MODE
    void Instance::ReadFromDisk(protocol::SubPieceInfo & start_s_info)
    {
        LOG4CPLUS_DEBUG_LOG(logger_instance, "ReadFromDisk" << start_s_info);
        assert(!download_driver_s_.empty());
#ifndef PEER_PC_CLIENT
        if (protocol::SubPieceContent::get_left_capacity() < 100)
        {
            LOG4CPLUS_DEBUG_LOG(logger_instance, "no more memory! left = " << 
                protocol::SubPieceContent::get_left_capacity());
        }
        else
#endif
        {
            std::vector<protocol::SubPieceContent*> buffs;
            protocol::SubPieceInfo iter_sub_piece(start_s_info);
            for (int i = 0; i < 256; ++i)
            {
                protocol::SubPieceContent* content = new protocol::SubPieceContent();
                if (!content->get_buffer())
                    break;
                buffs.push_back(content);

                if (!subpiece_manager_->SetSubPieceReading(iter_sub_piece))
                {
                    buffs.pop_back();
                    break;
                }

                if (false == subpiece_manager_->IncSubPieceInfo(iter_sub_piece))
                    break;
            }

            if (buffs.size() > 0)
            {
                StorageThread::Post(boost::bind(&Resource::ThreadReadBufferForPlay, resource_p_, start_s_info, buffs));
                LOG4CPLUS_DEBUG_LOG(logger_instance, "ReadFromDisk " << (int)buffs.size());
                LOG4CPLUS_DEBUG_LOG(logger_instance, "post ThreadReadBufferForPlay. size=" << (int)buffs.size());
            }
        }
    }
#endif

#ifdef DISK_MODE
    // 查询某个rid的校验失败的次数
    boost::int32_t Instance::GetBlockHashFailed()
    {
        return md5_hash_failed_;
    }
#endif

    void Instance::UpdateBlockHashTime(uint32_t block_index)
    {
#ifdef DISK_MODE
        boost::int64_t last_write_time_now = resource_p_->GetLastWriteTime();
        if (last_write_time_now != 0)
        {
            if (filesystem_last_write_time_ != last_write_time_now)
            {
                block_hash_time_map_.clear();
            }

            filesystem_last_write_time_ = last_write_time_now;
            block_hash_time_map_[block_index] = std::time(NULL);
            DebugLog("hash: update block %d, hash time to %ld", block_index, block_hash_time_map_[block_index]);
        }        
#endif
    }

#ifdef DISK_MODE
    bool Instance::CheckBlockNeedHash(uint32_t block_index)
    {
        boost::uint64_t last_write_time_now = resource_p_->GetLastWriteTime();
        if (last_write_time_now == 0 || filesystem_last_write_time_ != last_write_time_now
            || block_hash_time_map_.find(block_index) == block_hash_time_map_.end())
        {
            DebugLog("hash: check block %d need hash:1", block_index);
            return true;
        }
        else
        {
            bool need_hash = std::time(NULL) >  block_hash_time_map_[block_index] + OneDayInSeconds;
            DebugLog("hash: check block %d need hash:%d", block_index, need_hash);
            // 校验超过一天时间需要重新校验
            return  need_hash;
        }
    }
#endif

    std::map<uint32_t,boost::dynamic_bitset<uint32_t> > Instance::GetSubPiecesBitMap()
    {
       map<uint32_t,boost::dynamic_bitset<uint32_t> > subpieces_bitmap_;
       uint32_t total_block_count = GetBlockCount();
       for(uint32_t block_index = 0; block_index<total_block_count; block_index++)
       {
           //获取每个block中的bitset
           boost::dynamic_bitset<uint32_t> &db = subpieces_bitmap_[block_index];
           uint32_t block_subpiece_num = subpiece_manager_->GetBlockSubPieceCount(block_index);
           db.resize(block_subpiece_num);
           for(int subpiece_index = 0; subpiece_index < block_subpiece_num; subpiece_index++)
           {
               protocol::SubPieceInfo spi(block_index,subpiece_index);
               if (subpiece_manager_->HasSubPiece(spi))
               {
                   db.set(subpiece_index);
               }
           }
       }
       return subpieces_bitmap_;
    }

}
