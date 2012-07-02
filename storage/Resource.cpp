//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "storage_base.h"
#include "storage/IStorage.h"
#include "storage/Resource.h"
#include "storage/Instance.h"
#include "storage/Storage.h"
#include "storage/SpaceManager.h"
#include "storage/StorageThread.h"
#include "p2sp/bootstrap/BootStrapGeneralConfig.h"

namespace storage
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_resource = log4cplus::Logger::getInstance("[resource]");
#endif
    using namespace base;

    // TODO:yujinwu:2010/11/27:两个构造函数差异这么大，设计上要重新考虑，
    // 重点是要减小类的状态的复杂性
    Resource::Resource(
        boost::asio::io_service & io_svc,
        uint32_t file_length,
        string file_name,
        boost::shared_ptr<Instance> inst_p,
        uint32_t init_size)
        : io_svc_(io_svc)
        , actual_size_(init_size)
        , instance_p_(inst_p)
        , is_running_(false)
    {
#ifdef DISK_MODE
        need_saveinfo_to_disk_ = false;
#endif  // #ifdef DISK_MODE

        if (instance_p_)
        {
            is_running_ = true;
        }
        // file_resource_desc_p_ = ResourceDescriptor::Create(file_length, false);
        file_name_ = file_name;
    }

    Resource::Resource(
        boost::asio::io_service & io_svc,
        boost::shared_ptr<SubPieceManager> subpiece_manager,
        string file_name,
        boost::shared_ptr<Instance> inst_p,
        uint32_t actual_size)
        : io_svc_(io_svc)
        , actual_size_(actual_size)
        , instance_p_(inst_p)
        , is_running_(false)
    {
#ifdef DISK_MODE
        need_saveinfo_to_disk_ = (false);
#endif  // #ifdef DISK_MODE

        if (instance_p_)
        {
            is_running_ = true;
        }
        subpiece_manager_ = subpiece_manager;
        assert(subpiece_manager_);
        file_name_ = file_name;
    }

    void Resource::BindInstance(boost::shared_ptr<Instance> instance_p, boost::shared_ptr<SubPieceManager> subpiece_manager_p)
    {
        assert(is_running_ == false);
        assert(!instance_p_);
        instance_p_ = instance_p;
        subpiece_manager_ = subpiece_manager_p;
        if (instance_p_)
            is_running_ = true;
        else
            is_running_ = false;
    }

#ifdef DISK_MODE

    void Resource::SecSaveResourceFileInfo()
    {
        if (is_running_ == false)
        {
            LOG4CPLUS_DEBUG_LOG(logger_resource, __FUNCTION__ << ":" << __LINE__ << " is_running = false");
            return;
        }

        if (STORAGE_MODE_READONLY == Storage::Inst_Storage()->GetStorageMode())
        {
            return;
        }

        if (!need_saveinfo_to_disk_)
        {
            LOG4CPLUS_DEBUG_LOG(logger_resource, __FUNCTION__ << ":" << __LINE__ << " need_saveinfo_to_disk_ = false");
            return;
        }

        LOG4CPLUS_DEBUG_LOG(logger_resource, __FUNCTION__ << ":" << __LINE__ << " FileName = " << 
            (file_name_) << " FileLength:"
            << subpiece_manager_->GetFileLength() << "bytes! " << subpiece_manager_->GetDownloadBytes() << "bytes!");

        FlushStore();

        base::AppBuffer buf;
        if (!subpiece_manager_->ToBuffer(buf) || buf.Length() == 0)
        {
            return;
        }

        CfgFile cfg_file;
        if (!cfg_file.SecCreate(file_name_, subpiece_manager_->GetFileLength()))
        {
            LOG4CPLUS_DEBUG_LOG(logger_resource, __FUNCTION__ << "FileLength " << subpiece_manager_->GetFileLength());
            return;
        }

        if (!cfg_file.AddContent(buf))
        {
            cfg_file.SecClose();
            return;
        }
        cfg_file.SecClose();
        need_saveinfo_to_disk_ = false;
    }
#endif  // #ifdef DISK_MODE

    string Resource::GetLocalFileName()
    {
        return file_name_;
    }

#ifdef DISK_MODE

    void Resource::ThreadPendingWriteBlock(uint32_t block_i, const std::map<protocol::SubPieceInfo, protocol::SubPieceBuffer>* buffer_set_p)
    {
        if (false == is_running_)
            return;

        LOG4CPLUS_DEBUG_LOG(logger_resource, "Enter!");
        const std::map<protocol::SubPieceInfo, protocol::SubPieceBuffer>& block_pending_buffer_set = *buffer_set_p;
        if (block_pending_buffer_set.empty())
            return;

        protocol::SubPieceInfo first_subpiece = block_pending_buffer_set.begin()->first;
        uint32_t startoffset, length;
        subpiece_manager_->GetSubPiecePosition(first_subpiece, startoffset, length);

        length = 0;
        std::vector<const protocol::SubPieceBuffer*> buffers;
        for (std::map<protocol::SubPieceInfo, protocol::SubPieceBuffer>::const_iterator cit = block_pending_buffer_set.begin();
            cit != block_pending_buffer_set.end(); ++cit)
        {
            buffers.push_back(&(cit->second));
            length += cit->second.Length();
        }

        if (!WriteBufferArray(startoffset, buffers))
            return;

        io_svc_.post(boost::bind(&Resource::ThreadPendingWriteBlockHelper, shared_from_this(),
            block_i, startoffset, length));
    }

    void Resource::ThreadPendingWriteBlockHelper(
        uint32_t block_i,
        uint32_t startpos,
        uint32_t length)
    {
        if (instance_p_)
        {
            instance_p_->OnWriteBlockFinish(block_i);
            CheckFileDownComplete(startpos, length);
        }
        else
        {
            assert(false);
        }
    }

    void Resource::ThreadPendingHashBlock(uint32_t block_index,
        std::map<protocol::SubPieceInfo, protocol::SubPieceBuffer>* buffer_set_p)
    {
        if (is_running_ == false)
        {
            return;
        }

        if (false == IsFileOpen() && false == ReOpenFile())
            return;

        LOG4CPLUS_DEBUG_LOG(logger_resource, "Enter!");
        std::map<protocol::SubPieceInfo, protocol::SubPieceBuffer> & buffer_set = *buffer_set_p;
        assert(!buffer_set.empty());

        framework::string::Md5 md5;
        if (buffer_set.size() == subpiece_manager_->GetBlockSubPieceCount(block_index))
        {
            for (std::map<protocol::SubPieceInfo, protocol::SubPieceBuffer>::const_iterator it = buffer_set.begin();
                it != buffer_set.end(); ++it)
            {
                md5.update(it->second.Data(), it->second.Length());
            }

            md5.final();

            if (p2sp::BootStrapGeneralConfig::Inst()->WriteBlockWhenVerified())
            {
                MD5 actual_hash;
                actual_hash.from_bytes(md5.to_bytes());

                if (actual_hash == subpiece_manager_->GetRidInfo().block_md5_s_[block_index])
                {
                    ThreadPendingWriteBlock(block_index, buffer_set_p);
                }
            }
            else
            {
                ThreadPendingWriteBlock(block_index, buffer_set_p);
            }
        }
        else
        {
            if (!p2sp::BootStrapGeneralConfig::Inst()->WriteBlockWhenFull())
            {
                // 只有满2M才会写磁盘，所以这里代码应该不会走进来
                assert(false);
                std::map<protocol::SubPieceInfo, protocol::SubPieceBuffer>::iterator it = buffer_set.begin();
                for (; it != buffer_set.end(); ++it)
                {
                    ThreadSecWriteSubPiece(it->first, &(it->second), false);
                }

                if (subpiece_manager_->HasFullBlock(block_index))
                {
                    uint32_t offset, length;
                    subpiece_manager_->GetBlockPosition(block_index, offset, length);

                    std::vector<AppBuffer> buffs = ReadBufferArray(offset, length);
                    for (boost::uint32_t i = 0; i < buffs.size(); ++i)
                    {
                        assert(buffs[i].Data() && (buffs[i].Length() <= bytes_num_per_subpiece_g_ && buffs[i].Length()>0));
                        md5.update(buffs[i].Data(), buffs[i].Length());
                    }
                    md5.final();
                }
            }
        }

        MD5 hash_val;
        hash_val.from_bytes(md5.to_bytes());

        LOG4CPLUS_DEBUG_LOG(logger_resource, "Block:" << block_index << " Hash:" << hash_val);

        assert(instance_p_);
        io_svc_.post(boost::bind(&Resource::ThreadPendingHashBlockHelper, buffer_set_p, instance_p_, block_index, hash_val));
    }

    void Resource::ThreadPendingHashBlockHelper(
        std::map<protocol::SubPieceInfo, protocol::SubPieceBuffer>* buffer_set_p,
        Instance::p instance_p,
        uint32_t &block_index,
        MD5 &hash_val)
    {
        delete buffer_set_p;
        instance_p->OnPendingHashBlockFinish(block_index, hash_val);
    }

    void Resource::ThreadReadBufferForPlay(const protocol::SubPieceInfo subpiece_info, std::vector<protocol::SubPieceContent*> buffs)
    {
        if (false == is_running_)
        {
            io_svc_.post(boost::bind(&Resource::ReleaseSubPieceContentArray, buffs));
            return;
        }

        LOG4CPLUS_DEBUG_LOG(logger_resource, "Enter!");
        if (false == IsFileOpen() && false == ReOpenFile())
        {
            io_svc_.post(boost::bind(&Resource::ReleaseSubPieceContentArray, buffs));
            return;
        }

        protocol::SubPieceInfo tmp_subpiece_info = subpiece_info;
        bool tmp_is_max = false;
        for (int i = 0; i < buffs.size(); ++i)
        {
            if (tmp_subpiece_info == subpiece_manager_->GetMaxSubPieceInfo())
            {
                tmp_is_max = true;
                break;
            }
            if (false == subpiece_manager_->IncSubPieceInfo(tmp_subpiece_info))
            {
                // assert(false);
                tmp_is_max = true;
                break;
            }
        }

        uint32_t length;
        if (tmp_is_max)
        {
            uint32_t last_subpiece_off, last_subpiece_len;
            subpiece_manager_->GetSubPiecePosition(tmp_subpiece_info, last_subpiece_off, last_subpiece_len);
            length = (buffs.size() - 1) * bytes_num_per_subpiece_g_ + last_subpiece_len;
        }
        else
        {
            length = buffs.size() * bytes_num_per_subpiece_g_;
        }

        uint32_t start_offset = subpiece_manager_->SubPieceInfoToPosition(subpiece_info);
        LOG4CPLUS_ERROR_LOG(logger_resource, "start:!" << subpiece_info << ", continue_len=" << buffs.size());
        LOG4CPLUS_DEBUG_LOG(logger_resource, "(start offset: " << start_offset << ", Length: " << length << ")");

        if (ReadBufferArray(start_offset, length, buffs))
        {
            LOG4CPLUS_DEBUG_LOG(logger_resource, "ReadBufferArray succed!");
            io_svc_.post(boost::bind(&Resource::ThreadReadBufferForPlayHelper,
                instance_p_, subpiece_manager_, subpiece_info, buffs));
        }
        else
        {
            LOG4CPLUS_DEBUG_LOG(logger_resource, "ReadBufferArray failed!");
            io_svc_.post(boost::bind(&Resource::ReleaseSubPieceContentArray, buffs));
            assert(false);
        }
    }

    void Resource::ThreadReadBufferForPlayHelper(
        Instance::p instance_p,
        SubPieceManager::p subpiece_manager_p,
        const protocol::SubPieceInfo& subpiece_info,
        std::vector<protocol::SubPieceContent*> buffs)
    {
        protocol::SubPieceInfo tmp_subpiece_info(subpiece_info);
        for (uint16_t i = 0; i < buffs.size(); ++i)
        {
            subpiece_manager_p->LoadSubPiece(tmp_subpiece_info, protocol::SubPieceContent::pointer(buffs[i]));
            if (!subpiece_manager_p->IncSubPieceInfo(tmp_subpiece_info))
                break;
        }

        protocol::SubPieceBuffer buff = subpiece_manager_p->GetSubPiece(subpiece_info);
        if (buff.IsValid(SUB_PIECE_SIZE))
        {
            LOG4CPLUS_DEBUG_LOG(logger_resource, "ThreadReadBufferForPlayHelper " << subpiece_info);
            instance_p->OnThreadReadSubPieceSucced(subpiece_info, buff);
        }
    }

    void Resource::ThreadMergeSubPieceToInstance(const protocol::SubPieceInfo subpiece_info, protocol::SubPieceContent* buff,
        boost::shared_ptr<Instance> merge_to_instance_p)
    {
        LOG4CPLUS_DEBUG_LOG(logger_resource, "Enter!");
        if (!buff || !buff->get_buffer())
        {
            LOG4CPLUS_ERROR_LOG(logger_resource, "buff is Invalid!");
            assert(false);
            return;
        }

        if (is_running_ == false)
        {
            io_svc_.post(boost::bind(&Resource::ReleaseSubPieceContent, buff));
            return;
        }

        if (!merge_to_instance_p)
        {
            LOG4CPLUS_ERROR_LOG(logger_resource, "merge_to_instance_p NULL!");
            io_svc_.post(boost::bind(&Resource::ReleaseSubPieceContent, buff));
            return;
        }

        if (!subpiece_manager_->HasSubPiece(subpiece_info))
        {
            LOG4CPLUS_ERROR_LOG(logger_resource, "merge subpiece " << subpiece_info << " failed! subpiece not exist.");
            io_svc_.post(boost::bind(&Resource::ReleaseSubPieceContent, buff));
            return;
        }

        if (false == IsFileOpen() && false == ReOpenFile())
        {
            io_svc_.post(boost::bind(&Resource::ReleaseSubPieceContent, buff));
            return;
        }

        uint32_t startoffset, length;
        subpiece_manager_->GetSubPiecePosition(subpiece_info, startoffset, length);
        std::vector<protocol::SubPieceContent*> buffs;
        buffs.push_back(buff);
        if (ReadBufferArray(startoffset, length, buffs))
        {
            io_svc_.post(boost::bind(&Resource::ThreadMergeSubPieceToInstanceHelper, merge_to_instance_p, subpiece_info, buffs[0]));
            LOG4CPLUS_ERROR_LOG(logger_resource, "merge subpiece from resource! " << subpiece_info);
        }
        else
        {
            io_svc_.post(boost::bind(&Resource::ReleaseSubPieceContent, buff));
        }
    }

    void Resource::ThreadMergeSubPieceToInstanceHelper(
        Instance::p merge_instance_p,
        const protocol::SubPieceInfo& subpiece_info,
        protocol::SubPieceContent* buffer)
    {
        merge_instance_p->OnMergeSubPieceSuccess(subpiece_info, protocol::SubPieceBuffer(buffer));
    }

    void Resource::ThreadReadBlockForUpload(const RID& rid, const uint32_t block_index, IUploadListener::p listener,
        bool need_hash)
    {
        if (false == is_running_)
        {
            return;
        }

        LOG4CPLUS_DEBUG_LOG(logger_resource, "Enter!");
        DebugLog("hash: ThreadReadBlockForUpload index:%d, need_hash:%d", block_index, need_hash);
#ifdef BOOST_WINDOWS_API
        std::ostringstream oss;
        oss << "read block" << block_index << " " << need_hash << std::endl;
        OutputDebugString(oss.str().c_str());
#endif

        assert(!rid.is_empty());
        bool if_file_exist = true;
        if (false == IsFileOpen() && false == ReOpenFile())
        {
            if_file_exist = false;
        }

        if (!subpiece_manager_->HasFullBlock(block_index) || false == if_file_exist)
        {
            LOG4CPLUS_DEBUG_LOG(logger_resource, "Do not has full block---->block index:" << block_index);
            if (listener)
            {
                io_svc_.post(boost::bind(&IUploadListener::OnAsyncGetBlockFailed, listener, rid, block_index,
                    (int)ERROR_GET_SUBPIECE_NOT_FIND_SUBPIECE));
            }
            return;
        }

        uint32_t startoffset, length;
        subpiece_manager_->GetBlockPosition(block_index, startoffset, length);
        base::AppBuffer buff = ReadBuffer(startoffset, length);
        CloseFileHandle();

        if (need_hash)
        {
            framework::string::Md5 md5;
            uint32_t subpiece_len = 0;
            uint32_t offset = 0;
            while (offset < buff.Length())
            {
                if (offset + bytes_num_per_piece_g_ <= buff.Length())
                    subpiece_len = bytes_num_per_piece_g_;
                else
                    subpiece_len = buff.Length() - offset;
                md5.update(buff.Data() + offset, subpiece_len);
                offset += subpiece_len;
            }

            md5.final();

            MD5 hash_val;
            hash_val.from_bytes(md5.to_bytes());

            if (instance_p_)
            {
                io_svc_.post(boost::bind(&Instance::OnReadBlockForUploadFinishWithHash, instance_p_, block_index,
                    buff, listener, hash_val));
            }
        }
        else
        {
            if (instance_p_)
            {
                io_svc_.post(boost::bind(&Instance::OnReadBlockForUploadFinish, instance_p_, block_index, buff, listener));
            }
        }
        
        return;
    }

    void Resource::ThreadSecWriteSubPiece(protocol::SubPieceInfo subpiece_info, protocol::SubPieceBuffer* buf, bool del_buf)
    {
        if (is_running_ == false)
        {
            if (del_buf) io_svc_.post(boost::bind(&Resource::ReleaseSubPieceBuffer, buf));
            return;
        }
        if (false == IsFileOpen() && false == ReOpenFile())
        {
            if (del_buf) io_svc_.post(boost::bind(&Resource::ReleaseSubPieceBuffer, buf));
            return;
        }

        LOG4CPLUS_DEBUG_LOG(logger_resource, "Enter!" << subpiece_info);
        assert(subpiece_manager_->HasSubPieceInMem(subpiece_info));

        uint32_t startoffset, length;
        subpiece_manager_->GetSubPiecePosition(subpiece_info, startoffset, length);

        if (!WriteBuffer(startoffset, buf))
        {
            if (del_buf) io_svc_.post(boost::bind(&Resource::ReleaseSubPieceBuffer, buf));
            return;
        }

        assert(instance_p_);
        io_svc_.post(boost::bind(&Resource::ThreadSecWriteSubPieceHelper, shared_from_this(),
            subpiece_info, startoffset, length, buf, del_buf));
    }

    void Resource::ThreadSecWriteSubPieceHelper(
        protocol::SubPieceInfo subpiece_info,
        uint32_t startpos,
        uint32_t length,
        protocol::SubPieceBuffer* buf,
        bool del_buf)
    {
        if (del_buf) delete buf;
        // 如果这里的instance_p_为空，是因为在存磁盘的过程中Instance被磁盘淘汰了。
        if (instance_p_)
        {
            instance_p_->OnWriteSubPieceFinish(subpiece_info);
            CheckFileDownComplete(startpos, length);
        }
    }

    void Resource::CheckFileDownComplete(uint32_t start_pos, uint32_t length)
    {
        // TODO(nightsuns): 这里采用临时的解决方案
        boost::shared_ptr<SubPieceManager> temp_subpiece_manager = subpiece_manager_;

        if (NULL == temp_subpiece_manager)
        {
            return;
        }

        // check write end of file
        LOG4CPLUS_DEBUG_LOG(logger_resource, "start_pos + length = " << (start_pos + length) << ", filesize = " 
            << subpiece_manager_->GetFileLength());

        if (start_pos + length >= temp_subpiece_manager->GetFileLength())
        {
            boost::shared_ptr<Instance> temp_instance_p = instance_p_;

            // post write end of file
            if (temp_instance_p)
            {
                io_svc_.post(boost::bind(&Instance::OnFileWriteFinish, temp_instance_p));
            }
        }

        if (temp_subpiece_manager->IsFullFile())
        {
            StorageThread::Post(boost::bind(&Resource::ThreadTryRenameToNormalFile, shared_from_this()));
        }
    }

    void Resource::ThreadTryRenameToNormalFile()
    {
        if (TryRenameToNormalFile())
        {
            if (instance_p_)
            {
                io_svc_.post(boost::bind(&Instance::OnDiskFileNameChange, instance_p_, file_name_));
            }
        }
        need_saveinfo_to_disk_ = false;
    }

#endif  // #ifdef DISK_MODE

    void Resource::ThreadRemoveBlock(uint32_t index)
    {
        if (is_running_ == false)
        {
            return;
        }
        bool if_file_exist = true;
#ifdef DISK_MODE
        if (false == IsFileOpen() && false == ReOpenFile())
        {
            if_file_exist = false;
        }
#endif  // #ifdef DISK_MODE
        uint32_t offset = 0;
        uint32_t length = 0;
        if (TryRenameToTppFile() && true == if_file_exist)
        {
            if (instance_p_)
            {
                io_svc_.post(boost::bind(&Instance::OnDiskFileNameChange, instance_p_, file_name_));
            }
        }

        subpiece_manager_->GetBlockPosition(index, offset, length);

#ifdef DISK_MODE
        Erase(offset, length);
#endif  // #ifdef DISK_MODE

        assert(instance_p_);
        io_svc_.post(boost::bind(&Instance::OnRemoveResourceBlockFinish, instance_p_, index));

#ifdef DISK_MODE
        SecSaveResourceFileInfo();
#endif  // #ifdef DISK_MODE
    }

    unsigned char Resource::GetDownMode()
    {
        if (instance_p_)
        {
            return instance_p_->GetResDownMode();
        }
        else
        {
            return DM_BY_ACCELERATE;
        }
    }
}
