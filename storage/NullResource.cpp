//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "storage/storage_base.h"
#include "storage/IStorage.h"
#include "storage/Resource.h"

#include "storage/NullResource.h"
#include "storage/Instance.h"
#include "storage/Storage.h"


namespace storage
{
#if !DISK_MODE

    NullResource::p NullResource::CreateResource(
        boost::asio::io_service & io_svc, 
        uint32_t file_length,
        string file_name,
        boost::shared_ptr<Instance> inst_p,
        uint32_t init_size)
    {
        NullResource::p resource(new NullResource(io_svc, file_length, file_name, inst_p, init_size));
        return resource;
    }

    NullResource::NullResource(
        boost::asio::io_service & io_svc, 
        uint32_t file_length,
        string file_name,
        boost::shared_ptr<Instance> inst_p,
        uint32_t init_size)
        : Resource(io_svc, file_length, file_name, inst_p, init_size)
    {
    }

    NullResource::NullResource(
        boost::asio::io_service & io_svc, 
        boost::shared_ptr<SubPieceManager> subpiece_manager_p,
        string file_name,
        boost::shared_ptr<Instance> inst_p,
        uint32_t actual_size)
        : Resource(io_svc, subpiece_manager_p, file_name, inst_p, actual_size)
    {
    }

    void NullResource::CloseResource(bool need_remove_file)
    {
        if (instance_p_)
        {
            // MainThread::Post(boost::bind(&Instance::OnResourceCloseFinish, instance_p_, shared_from_this(), need_remove_file));
            instance_p_->OnResourceCloseFinish(shared_from_this(), need_remove_file);
            instance_p_ = Instance::p();
            return;
        }
    }

    void NullResource::Rename(const string& newname)
    {

    }

    void NullResource::FlushStore()
    {

    }

    base::AppBuffer NullResource::ReadBuffer(const uint32_t startpos, const uint32_t length)
    {
        return base::AppBuffer();
    }

    std::vector<base::AppBuffer> NullResource::ReadBufferArray(const uint32_t startpos, const uint32_t length)
    {
        return std::vector<base::AppBuffer>();
    }
    
    bool NullResource::ReadBufferArray(const uint32_t startpos, const uint32_t length, std::vector<protocol::SubPieceContent*> buffs)
    {
        return false;
    }

    bool NullResource::WriteBuffer(const uint32_t startpos, const protocol::SubPieceBuffer* buffer)
    {
        return false;
    }

    bool NullResource::WriteBufferArray(const uint32_t startpos, const std::vector<const protocol::SubPieceBuffer*>& buffer)
    {
        return false;
    }

    void NullResource::Erase(const uint32_t startpos, const uint32_t length)
    {
        return;
    }

    bool NullResource::TryRenameToNormalFile()
    {
        return true;
    }

    bool NullResource::TryRenameToTppFile()
    {
        return true;
    }

    boost::int64_t NullResource::GetLastWriteTime()
    {
        return 0;
    }

#endif  // !DISK_MODE

}