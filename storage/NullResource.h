//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef STORAGE_NULL_RESOURCE_H
#define STORAGE_NULL_RESOURCE_H

namespace storage
{

#if !DISK_MODE

    class NullResource : public Resource
#ifdef DUMP_OBJECT
        , public count_object_allocate<NullResource>
#endif
    {
    public:
        typedef boost::shared_ptr<NullResource> p;
        static p CreateResource(
            boost::asio::io_service & io_svc, 
            uint32_t file_length,
            string file_name,
            boost::shared_ptr<Instance> inst_p,
            uint32_t init_size);

    protected:
        NullResource(
            boost::asio::io_service & io_svc, 
            uint32_t file_length,
            string file_name,
            boost::shared_ptr<Instance> inst_p,
            uint32_t init_size);

        NullResource(
            boost::asio::io_service & io_svc, 
            boost::shared_ptr<SubPieceManager> subpiece_manager_p,
            string file_name,
            boost::shared_ptr<Instance> inst_p,
            uint32_t actual_size);

    public:
        virtual void CloseResource(bool need_remove_file);

    protected:
        virtual void Rename(const string& newname);
        virtual void FlushStore();
        virtual base::AppBuffer ReadBuffer(const uint32_t startpos, const uint32_t length);
        virtual std::vector<base::AppBuffer> ReadBufferArray(const uint32_t startpos, const uint32_t length);
        virtual bool ReadBufferArray(const uint32_t startpos, const uint32_t length, std::vector<protocol::SubPieceContent*> buffs);
        virtual bool WriteBuffer(const uint32_t startpos, const protocol::SubPieceBuffer* buffer);
        virtual bool WriteBufferArray(const uint32_t startpos, const std::vector<const protocol::SubPieceBuffer*>& buffer);
        virtual void Erase(const uint32_t startpos, const uint32_t length);
        virtual bool TryRenameToNormalFile();
        virtual bool TryRenameToTppFile();

    private:

    };

#endif  // !DISK_MODE

}

#endif  // STORAGE_NULL_RESOURCE_H
