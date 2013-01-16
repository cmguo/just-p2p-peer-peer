//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef STORAGE_FILE_RESOURCE_H
#define STORAGE_FILE_RESOURCE_H

namespace storage
{
#ifdef DISK_MODE

struct EncryptHeader
{
    char id_[4];            //pptv
    boost::uint32_t version_;    //版本号
    unsigned char Reserved_[1024 - 4 - 4];

    EncryptHeader()
    {
        base::util::memcpy2(id_, 4, "pptv", 4);
        version_ = VersionNow();
        memset(Reserved_, 0, sizeof(Reserved_));
    }

    boost::uint32_t VersionNow()
    {
        return 0x00000001;
    }

    bool IsValidEncryptHeader()
    {
        return (0 == memcmp(id_, "pptv", sizeof(id_)) && version_ == VersionNow());
    }
};

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
        boost::uint32_t file_length,
        string file_name,
        FILE* file_handle,
        boost::shared_ptr<Instance> inst_p,
        boost::uint32_t init_size,
        bool need_encrypt);

    // 根据资源信息创建资源对象实例
    static p CreateResource(
        boost::asio::io_service & io_svc,
        const FileResourceInfo &resource_info);

    ~FileResource();

    private:
    static bool OpenResourceFile(
        const FileResourceInfo &resource_info,
        FILE* &resource_file_handle,
        boost::uint32_t &actual_size,
        bool &has_encrypted);

    void BufEncrypt(unsigned char * buf_to_encrypted, boost::uint32_t length);
    void BufDecrypt(unsigned char * buf_to_decrypt, boost::uint32_t length);

    protected:
    FileResource(
        boost::asio::io_service & io_svc,
        boost::uint32_t file_length,
        string file_name,
        FILE* file_handle,
        boost::shared_ptr<Instance> inst_p,
        boost::uint32_t init_size,
        bool need_encrypt);

    FileResource(
        boost::asio::io_service & io_svc,
        boost::shared_ptr<SubPieceManager> subpiece_manager,
        string file_name,
        FILE* file_handle,
        boost::shared_ptr<Instance> inst_p,
        boost::uint32_t actual_size,
        bool has_encrypted);

    protected:
#ifdef DISK_MODE
    virtual boost::uint32_t GetLocalFileSize();
    virtual void FreeDiskSpace(bool need_remove_file);
    virtual bool IsFileOpen();
    virtual void CloseFileHandle();
    virtual bool ReOpenFile();
#endif

    virtual void Rename(const string& newname);
    virtual void CloseResource(bool need_remove_file);

    virtual void FlushStore();
    virtual base::AppBuffer ReadBuffer(const boost::uint32_t startpos, const boost::uint32_t length);
    virtual std::vector<base::AppBuffer> ReadBufferArray(const boost::uint32_t startpos, const boost::uint32_t length);
    virtual bool ReadBufferArray(const boost::uint32_t startpos, const boost::uint32_t length, std::vector<protocol::SubPieceContent*> buffs);
    virtual bool WriteBufferArray(const boost::uint32_t startpos, const std::vector<const protocol::SubPieceBuffer*>& buffer);
    virtual void Erase(const boost::uint32_t startpos, const boost::uint32_t length);
    virtual bool TryRenameToNormalFile();
    virtual bool TryRenameToTppFile();
    virtual boost::int64_t GetLastWriteTime();
    private:
    FILE* file_handle_;
};

#endif  // DISK_MODE

}

#endif  // STORAGE_FILE_RESOURCE_H
