//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef STORAGE_FILE_RESOURCE_H
#define STORAGE_FILE_RESOURCE_H

namespace storage
{
#ifdef DISK_MODE

class FileResource: public Resource
#ifdef DUMP_OBJECT
    , public count_object_allocate<FileResource>
#endif
{
    public:

    typedef boost::shared_ptr<FileResource> p;
    public:
    static p CreateResource(
        boost::asio::io_service & io_svc,
        uint32_t file_length,
        string file_name,
        FILE* file_handle,
        boost::shared_ptr<Instance> inst_p,
        uint32_t init_size);

    // 根据资源信息创建资源对象实例
    static p CreateResource(
        boost::asio::io_service & io_svc,
        const FileResourceInfo &resource_info);

    ~FileResource();

    private:
    static bool OpenResourceFile(
        const FileResourceInfo &resource_info,
        FILE* &resource_file_handle,
        uint32_t &actual_size);

    protected:
    FileResource(
        boost::asio::io_service & io_svc,
        uint32_t file_length,
        string file_name,
        FILE* file_handle,
        boost::shared_ptr<Instance> inst_p,
        uint32_t init_size);

    FileResource(
        boost::asio::io_service & io_svc,
        boost::shared_ptr<SubPieceManager> subpiece_manager,
        string file_name,
        FILE* file_handle,
        boost::shared_ptr<Instance> inst_p,
        uint32_t actual_size);

    protected:
#ifdef DISK_MODE
    virtual uint32_t GetLocalFileSize();
    virtual void FreeDiskSpace(bool need_remove_file);
    virtual bool IsFileOpen();
    virtual void CloseFileHandle();
    virtual bool ReOpenFile();
#endif

    virtual void Rename(const string& newname);
    virtual void CloseResource(bool need_remove_file);

    virtual void FlushStore();
    virtual base::AppBuffer ReadBuffer(const uint32_t startpos, const uint32_t length);
    virtual std::vector<base::AppBuffer> ReadBufferArray(const uint32_t startpos, const uint32_t length);
    virtual bool ReadBufferArray(const uint32_t startpos, const uint32_t length, std::vector<protocol::SubPieceContent*> buffs);
    virtual bool WriteBufferArray(const uint32_t startpos, const std::vector<const protocol::SubPieceBuffer*>& buffer);
    virtual void Erase(const uint32_t startpos, const uint32_t length);
    virtual bool TryRenameToNormalFile();
    virtual bool TryRenameToTppFile();
    virtual boost::int64_t GetLastWriteTime();
    private:
    FILE* file_handle_;
};

#endif  // DISK_MODE

}

#endif  // STORAGE_FILE_RESOURCE_H
