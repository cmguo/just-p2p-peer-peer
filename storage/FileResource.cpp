//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "storage/storage_base.h"
#include "storage/IStorage.h"
#include "storage/Resource.h"

#include "storage/FileResource.h"
#include "storage/Instance.h"
#include "storage/Storage.h"

#include <boost/filesystem/operations.hpp>
#include "FileResourceInfo.h"

#include "base/util.h"

namespace storage
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("storage");
    using namespace base;
#ifdef DISK_MODE

    FileResource::p FileResource::CreateResource(
        boost::asio::io_service & io_svc,
        uint32_t file_length,
        string file_name,
        FILE* file_handle,
        boost::shared_ptr<Instance> inst_p,
        uint32_t init_size)
    {
        assert(file_handle != NULL);
        FileResource::p pointer(new FileResource(
            io_svc,
            file_length,
            file_name,
            file_handle,
            inst_p,
            init_size));

        return pointer;
    }

    FileResource::p FileResource::CreateResource(
        boost::asio::io_service & io_svc,
        const FileResourceInfo &resource_info)
    {
        FILE* resource_file_handle = NULL;
        uint32_t actual_size;

        if (false == OpenResourceFile(resource_info, resource_file_handle, actual_size))
        {
            return FileResource::p();
        }

        assert(resource_file_handle);

        bool is_valid = false;
        SubPieceManager::p subpiece_manager;
        if (false == resource_info.IsTempFile())
        {
            if (resource_info.CheckFileSize(actual_size))
            {
                // 若资源已经下载完毕，资源文件的实际大小必须和资源信息符合
                subpiece_manager = SubPieceManager::Create(actual_size, true);
                is_valid = true;
            }
        }
        else
        {
            CfgFile cfg_file;
            if (cfg_file.SecOpen(resource_info.file_path_))
            {
                assert(cfg_file.resource_file_name_ == resource_info.file_path_);

                base::AppBuffer r_mapbuf = cfg_file.GetContent();

                if (r_mapbuf.Length() != 0)
                {
                    protocol::RidInfo rid_info;
                    rid_info.InitByFileLength(cfg_file.resource_file_size_);
                    cfg_file.SecClose();
                    subpiece_manager = SubPieceManager::Parse(r_mapbuf, rid_info);
                    if (subpiece_manager)
                    {
                        is_valid = true;
                    }
                    else
                    {
                        base::filesystem::remove_nothrow(Storage::Inst_Storage()->GetCfgFilename(resource_info.file_path_));
                        LOG(__DEBUG, "", "Cfg File Parse Error, delete Cfg File.");
                    }
                }
                else
                {
                    cfg_file.SecClose();
                }

            }
            else
            {
                base::filesystem::remove_nothrow(Storage::Inst_Storage()->GetCfgFilename(resource_info.file_path_));
                LOG(__DEBUG, "", "Cfg File Open Error, delete Cfg File.");
            }
        }

        if (!is_valid)
        {
            fclose(resource_file_handle);
            return FileResource::p();
        }

        assert(subpiece_manager);
        FileResource::p resource_p(new FileResource(
            io_svc,
            subpiece_manager,
            resource_info.file_path_,
            resource_file_handle,
            Instance::p(),
            actual_size));

        return resource_p;
    }

    FileResource::FileResource(
        boost::asio::io_service & io_svc,
        uint32_t file_length,
        string file_name,
        FILE* file_handle,
        boost::shared_ptr<Instance> inst_p,
        uint32_t init_size)
        : Resource(io_svc, file_length, file_name, inst_p, init_size)
        , file_handle_(file_handle)
    {
        need_saveinfo_to_disk_ = (true);
    }

    FileResource::FileResource(
        boost::asio::io_service & io_svc,
        boost::shared_ptr<SubPieceManager> subpiece_manager,
        string file_name,
        FILE* file_handle,
        boost::shared_ptr<Instance> inst_p,
        uint32_t actual_size)
        : Resource(io_svc, subpiece_manager, file_name, inst_p, actual_size)
        , file_handle_(file_handle)
    {
        need_saveinfo_to_disk_ = (true);
    }

    FileResource::~FileResource()
    {
    }

    bool FileResource::OpenResourceFile(
        const FileResourceInfo &resource_info,
        FILE* &resource_file_handle,
        uint32_t &actual_size)
    {
        namespace fs = boost::filesystem;

        resource_file_handle = NULL;

        if (false == base::filesystem::exists_nothrow(fs::path(resource_info.file_path_)))
        {
            STORAGE_DEBUG_LOG("FileNotExists: resource_info.file_path_ = "
                << resource_info.file_path_);
            return false;
        }

        // open existing
        resource_file_handle = fopen(resource_info.file_path_.c_str(), "r+b");
        if (NULL == resource_file_handle)
        {
            return false;
        }

        try {
            uint32_t fsize = fs::file_size(fs::path(resource_info.file_path_));
            actual_size = fsize;
        }
        catch(const fs::filesystem_error&) {
            fclose(resource_file_handle);
            return false;
        }

        return true;
    }

    std::vector<AppBuffer> FileResource::ReadBufferArray(const uint32_t startpos, const uint32_t length)
    {
        if (NULL == file_handle_)
        {
            if (false == ReOpenFile())
            {
                return std::vector<AppBuffer>();
            }
        }

        assert(file_handle_ != NULL);
        assert(startpos + length <= subpiece_manager_->GetFileLength());
        if (startpos + length <= subpiece_manager_->GetFileLength())
        {
            std::vector<AppBuffer> buffs;

            fseek(file_handle_, startpos, SEEK_SET);
            boost::uint32_t subpiece_cnt = (length % SUB_PIECE_SIZE) ?
                (length/SUB_PIECE_SIZE + 1) : (length/SUB_PIECE_SIZE);
            boost::uint32_t last_subpiece_length = (length % SUB_PIECE_SIZE) ?
                (length % SUB_PIECE_SIZE) : SUB_PIECE_SIZE;
            boost::uint32_t last_subpiece_index = subpiece_cnt - 1;

            uint32_t readlen = 0, subpiece_len = SUB_PIECE_SIZE;
            for (boost::uint32_t i = 0; i < subpiece_cnt; ++i)
            {
                if (i == last_subpiece_index)
                    subpiece_len = last_subpiece_length;

                buffs.push_back(AppBuffer(subpiece_len));
                readlen = fread(buffs[i].Data(), 1, subpiece_len, file_handle_);
                if (readlen != subpiece_len)
                {
                    assert(false);
                    return buffs;
                }
            }
            return buffs;
        }
        else
        {
            return std::vector<AppBuffer>();
        }
    }

    bool FileResource::ReadBufferArray(const uint32_t startpos, const uint32_t length, std::vector<protocol::SubPieceContent*> buffs)
    {
        if (NULL == file_handle_)
        {
            if (false == ReOpenFile())
            {
                return false;
            }
        }

        assert(file_handle_ != NULL);
        assert(startpos + length <= subpiece_manager_->GetFileLength());
        if (startpos + length <= subpiece_manager_->GetFileLength())
        {
            fseek(file_handle_, startpos, SEEK_SET);
            boost::uint32_t subpiece_cnt = (length % SUB_PIECE_SIZE) ?
                (length/SUB_PIECE_SIZE + 1) : (length/SUB_PIECE_SIZE);
            boost::uint32_t last_subpiece_length = (length % SUB_PIECE_SIZE) ?
                (length % SUB_PIECE_SIZE) : SUB_PIECE_SIZE;
            boost::uint32_t last_subpiece_index = subpiece_cnt - 1;

            uint32_t readlen = 0, subpiece_len = SUB_PIECE_SIZE;
            for (boost::uint32_t i = 0; i < subpiece_cnt; ++i)
            {
                if (i == last_subpiece_index)
                    subpiece_len = last_subpiece_length;

                if (buffs[i] && buffs[i]->get_buffer())
                {
                    readlen = fread(buffs[i]->get_buffer(), 1, subpiece_len, file_handle_);
                    if (readlen != subpiece_len)
                    {
                        assert(false);
                        return false;
                    }
                }
                else
                {
                    STORAGE_ERR_LOG("buffs[" << i << "] is invalid");
                    assert(false);
                    return false;
                }
            }
            return true;
        }
        else
        {
            return false;
        }
    }

    base::AppBuffer FileResource::ReadBuffer(const uint32_t startpos, const uint32_t length)
    {
        if (NULL == file_handle_)
        {
            if (false == ReOpenFile())
            {
                return base::AppBuffer();
            }
        }

        assert(file_handle_ != NULL);
        assert(startpos + length <= subpiece_manager_->GetFileLength());
        if (startpos + length <= subpiece_manager_->GetFileLength())
        {
            base::AppBuffer buff(length);
            fseek(file_handle_, startpos, SEEK_SET);
            uint32_t readlen = fread(buff.Data(), 1, length, file_handle_);
            if (readlen != length)
            {
                assert(false);
                STORAGE_ERR_LOG(" ReadFile Failed! ReadLength = " << readlen);
                return base::AppBuffer();
            }
            return buff;
        }
        else
        {
            return base::AppBuffer();
        }
    }

    bool FileResource::WriteBufferArray(const uint32_t startpos, const std::vector<const protocol::SubPieceBuffer*>& buffers)
    {
        if (NULL == file_handle_)
        {
            if (false == ReOpenFile())
            {
                return false;
            }
        }

        boost::uint64_t total_bufsize = 0;
        for (std::vector<const protocol::SubPieceBuffer*>::const_iterator cit = buffers.begin();
            cit != buffers.end(); ++cit)
        {
            total_bufsize += (*cit)->Length();
        }

        // extend file
        if (startpos + total_bufsize > actual_size_)
        {
            boost::uint64_t block_size = 2*1024*1024;
            boost::uint64_t delta_size = ((startpos + total_bufsize - actual_size_ + block_size - 1) / block_size) * block_size;
            boost::uint64_t new_size = std::min(actual_size_ + delta_size, (boost::uint64_t)subpiece_manager_->GetFileLength());

            if (new_size > actual_size_)
            {
                fseek(file_handle_, new_size - 1, SEEK_SET);
                if (0 == fwrite("", 1, 1, file_handle_)) {
                    return false;
                }
                Storage::Inst_Storage()->GetSpaceManager()->OnAllocDiskSpace(new_size - actual_size_, instance_p_->GetResDownMode());
                actual_size_ = new_size;
            }
            else
            {
                assert(false);
                return false;
            }
        }

        fseek(file_handle_, startpos, SEEK_SET);
        base::AppBuffer total_buf(total_bufsize);
        unsigned char *p_buf = total_buf.Data();
        for (std::vector<const protocol::SubPieceBuffer*>::const_iterator cit = buffers.begin();
            cit != buffers.end(); ++cit)
        {
            base::util::memcpy2(p_buf, total_buf.Length() - (p_buf - total_buf.Data()), (*cit)->Data(), (*cit)->Length());
            p_buf += (*cit)->Length();
        }
        uint32_t write_len = fwrite(total_buf.Data(), sizeof(boost::uint8_t), total_buf.Length(), file_handle_);

        if (write_len != total_bufsize)
        {
            LOG(__ERROR, "bug", __FUNCTION__ << ":" << __LINE__ << " WriteFile Failed!! WriteLength = " << write_len << " BufferLength = " << total_bufsize);
            LOG(__ERROR, "bug", "WriteFile Error: " <<  errno);
            return false;
        }
        need_saveinfo_to_disk_ = true;
        return true;
    }

    bool FileResource::WriteBuffer(const uint32_t startpos, const protocol::SubPieceBuffer* buffer)
    {
        if (NULL == file_handle_)
        {
            if (false == ReOpenFile())
            {
                return false;
            }
        }
        assert(file_handle_ != NULL);
        assert(startpos + buffer->Length() <= subpiece_manager_->GetFileLength());

        if (startpos + buffer->Length() > actual_size_)
        {
            /* extend file */
            uint32_t block_size = 2 * 1024 * 1024;
            uint32_t delta_size = ((startpos + buffer->Length() - actual_size_ + block_size - 1) / block_size) * block_size;
            uint32_t new_size = std::min((uint32_t)(actual_size_ + delta_size), subpiece_manager_->GetFileLength());

            if (new_size > actual_size_)
            {
                fseek(file_handle_, new_size - 1, SEEK_SET);
                if (0 == fwrite("", 1, 1, file_handle_)) {
                    return false;
                }
                Storage::Inst_Storage()->GetSpaceManager()->OnAllocDiskSpace(new_size - actual_size_, instance_p_->GetResDownMode());
                actual_size_ = new_size;
            }
            else
            {
                assert(false);
                return false;
            }
        }

        fseek(file_handle_, startpos, SEEK_SET);
        uint32_t write_len = fwrite(buffer->Data(), 1, buffer->Length(), file_handle_);

        if (write_len != buffer->Length())
        {
            LOG(__ERROR, "bug", __FUNCTION__ << ":" << __LINE__ << " WriteFile Failed!! WriteLength = " << write_len << " BufferLength = " << buffer->Length());
            LOG(__ERROR, "bug", "WriteFile Error: " << errno);
            return false;
        }
        assert(write_len == buffer->Length());
        need_saveinfo_to_disk_ = true;
        return true;
    }

    void FileResource::Erase(const uint32_t startpos, const uint32_t length)
    {
        need_saveinfo_to_disk_ = true;
        return;
    }

    void FileResource::FlushStore()
    {
        if (NULL == file_handle_)
        {
            if (false == ReOpenFile())
            {
                return;
            }
        }
        if (NULL == file_handle_)
        {
            STORAGE_ERR_LOG("file handle invalid");
            return;
        }
        fflush(file_handle_);
    }

    void FileResource::CloseResource(bool need_remove_file)
    {
        if (is_running_ == false)
            return;
        need_saveinfo_to_disk_ = true;
        SecSaveResourceFileInfo();
        is_running_ = false;
        if (NULL != file_handle_)
        {
            fclose(file_handle_);
            file_handle_ = NULL;
        }

        if (subpiece_manager_)
        {
            subpiece_manager_.reset();
        }

        if (instance_p_)
        {
            io_svc_.post(boost::bind(&Instance::OnResourceCloseFinish,
                instance_p_, shared_from_this(), need_remove_file));
            instance_p_.reset();
            return;
        }
    }

    void FileResource::FreeDiskSpace(bool need_remove_file)
    {
        namespace fs = boost::filesystem;

        assert(is_running_ == false);
        assert(file_handle_ == NULL);

        if (false == need_remove_file)
        {
            io_svc_.post(boost::bind(&SpaceManager::OnFreeDiskSpaceFinish, Storage::Inst_Storage()->GetSpaceManager(),
                actual_size_, shared_from_this()));
            return;
        }

        STORAGE_DEBUG_LOG("DeleteFile: " << (file_name_));
        try
        {
            fs::remove(fs::path(file_name_));
        }
        catch(const fs::filesystem_error&)
        {
            io_svc_.post(boost::bind(&SpaceManager::OnFreeDiskSpaceFinish, Storage::Inst_Storage()->GetSpaceManager(),
                0, shared_from_this()));
            return;
        }

        string cfg_file_name = Storage::Inst_Storage()->GetCfgFilename(file_name_);
        try
        {
            fs::remove(fs::path(cfg_file_name));
        }
        catch(const fs::filesystem_error&)
        {
        }

        io_svc_.post(boost::bind(&SpaceManager::OnFreeDiskSpaceFinish, Storage::Inst_Storage()->GetSpaceManager(),
            actual_size_, shared_from_this()));
        return;
    }

    uint32_t FileResource::GetLocalFileSize()
    {
        assert(is_running_ == false);
        return actual_size_;
    }
   ;

    bool FileResource::TryRenameToNormalFile()
    {
        namespace fs = boost::filesystem;
        if (NULL == file_handle_)
        {
            if (false == ReOpenFile())
            {
                return false;
            }
        }
        assert(NULL != file_handle_);

        int pos = file_name_.rfind(tpp_extname);

        if (pos == string::npos)
        {
            return false;
        }

        assert(pos == file_name_.size()-4);
        string new_file_name(file_name_.c_str(), pos);
        FlushStore();
        fclose(file_handle_);
        file_handle_ = NULL;

        string cfg_file = Storage::Inst_Storage()->GetCfgFilename(file_name_);
        boost::system::error_code ec;
        base::filesystem::rename_nothrow(fs::path(file_name_), fs::path(new_file_name), ec);
        STORAGE_DEBUG_LOG("old filename:" << (file_name_) << " new_file_name: " << (new_file_name));
        file_name_ = new_file_name;

        base::filesystem::remove_nothrow(fs::path(cfg_file));

        file_handle_ = fopen(file_name_.c_str(), "r+b");

        assert(file_handle_ != NULL);
        return true;
    }

    bool FileResource::TryRenameToTppFile()
    {
        namespace fs = boost::filesystem;
        if (NULL == file_handle_)
        {
            if (false == ReOpenFile())
            {
                return false;
            }
        }
        assert(file_handle_ != NULL);

        int pos = file_name_.rfind(tpp_extname);

        if (pos != string::npos)
        {
            assert(pos == file_name_.size()-4);
            return false;
        }

        string new_file_name = file_name_;
        new_file_name.append(tpp_extname);
        fclose(file_handle_);
        boost::system::error_code ec;
        base::filesystem::rename_nothrow(fs::path(file_name_), fs::path(new_file_name), ec);
        STORAGE_DEBUG_LOG("old filename:" << (file_name_) << "  new_file_name: " << (new_file_name));

        file_name_ = new_file_name;
        file_handle_ = fopen(file_name_.c_str(), "r+b");
        assert(file_handle_ != NULL);
        return true;
    }

    void FileResource::Rename(const string& newname)
    {
        namespace fs = boost::filesystem;

        if (NULL == file_handle_)
        {
            if (false == ReOpenFile())
            {
                return;
            }
        }
        assert(NULL != file_handle_);
        FlushStore();

        if (0 != fclose(file_handle_))
        {
            return;
        }

        do
        {
            STORAGE_DEBUG_LOG("Rename From: " << (file_name_) << " To: " << (newname));
            try {
                fs::rename(fs::path(file_name_), fs::path(newname));
            }
            catch(const fs::filesystem_error&) {
                break;
            }

            if (false == instance_p_->IsComplete())
            {
                string cfg_file = Storage::Inst_Storage()->GetCfgFilename(file_name_);
                try {
                    fs::remove(fs::path(cfg_file));
                }
                catch(const fs::filesystem_error&) {
                    // ignore
                }
                LOG(__DEBUG, "downloadcenter", __FUNCTION__ << " cfg_file = '" << (cfg_file) << "' file_name = '" << (file_name_) << "' new_name = '" << (newname) << "'");
                file_name_ = newname;
                SecSaveResourceFileInfo();
            }
            else
            {
                file_name_ = newname;
            }
        } while (false);

        file_handle_ = fopen(file_name_.c_str(), "r+b");
        io_svc_.post(boost::bind(&Instance::OnRenameFinish, instance_p_, file_name_));
        assert(file_handle_ != NULL);
        return;
    }

    void FileResource::CloseFileHandle()
    {
        if (NULL == file_handle_)
        {
            return;
        }

        fclose(file_handle_);
        file_handle_ = NULL;
    }

    bool FileResource::ReOpenFile()
    {
        if (NULL != file_handle_)
        {
            return true;
        }

        file_handle_ = fopen(file_name_.c_str(), "r+b");
        if (NULL == file_handle_)
        {
            return false;
        }
        return true;
    }

    inline bool FileResource::IsFileOpen()
    {
        return NULL != file_handle_;
    }

#endif  // #ifdef DISK_MODE

}

