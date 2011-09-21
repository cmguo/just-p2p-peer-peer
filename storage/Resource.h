//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef STORAGE_RESOURCE_H
#define STORAGE_RESOURCE_H

#ifdef BOOST_WINDOWS_API
#pragma once
#endif

/*******************************************************************************
//
//  Resource.h
//  class Resource
//  负责subpiece的写入和读出, 所有纯虚函数的实现见class FileResource
//
*******************************************************************************/

#include "storage/CfgFile.h"
#include "storage/IStorage.h"

namespace storage
{
    class Instance;
    class SubPieceManager;

    class Resource
        : public boost::noncopyable
        , public boost::enable_shared_from_this<Resource>
#ifdef DUMP_OBJECT
        , public count_object_allocate<Resource>
#endif
    {
    public:
        friend class Instance;
        typedef boost::shared_ptr<Resource> p;
        typedef boost::asio::ip::udp::endpoint REndpoint;
        virtual ~Resource() {}

    protected:
        // 构造 启动
        Resource(
            boost::asio::io_service & io_svc,
            uint32_t file_length,                 // 文件大小
            string file_name,                   // 完整文件名
            boost::shared_ptr<Instance> inst_p,
            uint32_t init_size);

        Resource(
            boost::asio::io_service & io_svc,
            boost::shared_ptr<SubPieceManager> subpiece_manager,             // 关联的文件描述
            string file_name,                                               // 完整文件名
            boost::shared_ptr<Instance> inst_p,                             // 关联的instance
            uint32_t actual_size                                              // 占用空间
           );

    public:
        // 绑定某个instance
        void BindInstance(boost::shared_ptr<Instance> instance_p, boost::shared_ptr<SubPieceManager> subpiece_manager_p);

        boost::shared_ptr<Instance> GetInstance()  { return instance_p_; }

        // 深度拷贝当前的ResourceDescriptor
        // ResourceDescriptor::p CloneResourceDescriptor();

        // 返回文件名
        virtual string GetLocalFileName();

        inline uint32_t GetActualSize() const { return actual_size_; }

#ifdef DISK_MODE
        virtual void ThreadPendingWriteBlock(uint32_t block_i, const std::map<protocol::SubPieceInfo, protocol::SubPieceBuffer>* buffer_set_p);

        void ThreadPendingWriteBlockHelper(uint32_t block_i, uint32_t startpos, uint32_t length);

        // 将一个block的subpiece写入文件，并计算该block的MD5值，然后交给Instance, 读取需要上传的block，上传
        virtual void ThreadPendingHashBlock(uint32_t block_index, std::map<protocol::SubPieceInfo, protocol::SubPieceBuffer>* buffer_set_p);

        // ThreadPendingHashBlock辅助函数, 用于在主线程中内析构buffer_set_p对象, 保证intrusive_ptr的线程安全.
        // 以Helper结尾的函数均为辅助函数，为了保证内存池和intrusive_ptr不被跨线程访问@herain
        static void ThreadPendingHashBlockHelper(
            std::map<protocol::SubPieceInfo, protocol::SubPieceBuffer>* buffer_set_p,
            boost::shared_ptr<Instance> instance_p,
            uint32_t &block_index,
            MD5 &hash_val);

        // 保存资源信息到cfg文件中
        virtual void SecSaveResourceFileInfo();

        // 从文件中读取指定的block，然后交给IUploadListener
        virtual void ThreadReadBlockForUpload(const RID& rid, const uint32_t block_index, IUploadListener::p listener,
            bool need_hash);

        // 从某个subpiece处读取不超过n个的连续subpiece，然后交给player_listener
        virtual void ThreadReadBufferForPlay(const protocol::SubPieceInfo subpiece_info, std::vector<protocol::SubPieceContent*> buffs);

        // 以Helper结尾的函数均为辅助函数，为了保证内存池和intrusive_ptr不被跨线程访问@herain
        static void ThreadReadBufferForPlayHelper(
            boost::shared_ptr<Instance> instance_p,
            boost::shared_ptr<SubPieceManager> subpiece_manager_p,
            const protocol::SubPieceInfo& subpiece_info,
            std::vector<protocol::SubPieceContent*> buffs);

        // 从文件中读取指定的subpiece，然后交给Instance merge
        virtual void ThreadMergeSubPieceToInstance(const protocol::SubPieceInfo subpiece_info, protocol::SubPieceContent* buff,
            boost::shared_ptr<Instance> merge_to_instance_p);

        // 以Helper结尾的函数均为辅助函数，为了保证内存池和intrusive_ptr不被跨线程访问@herain
        static void ThreadMergeSubPieceToInstanceHelper(
            boost::shared_ptr<Instance> merge_instance_p,
            const protocol::SubPieceInfo& subpiece_info,
            protocol::SubPieceContent* buffer);

        // 将一个subpiece写入文件，然后交由Instance从poolout_pending_subpiece_info中删除该subpiece
        virtual void ThreadSecWriteSubPiece(protocol::SubPieceInfo subpiece_info, protocol::SubPieceBuffer* buf, bool del_buf);

        void ThreadSecWriteSubPieceHelper(protocol::SubPieceInfo subpiece_info, uint32_t startpos, uint32_t length,
            protocol::SubPieceBuffer* buf, bool del_buf);

        void CheckFileDownComplete(uint32_t start_pos, uint32_t length);

        void ThreadTryRenameToNormalFile();

        // ------------------------------------------------------------------------------------
        // 派生类实现
        virtual uint32_t GetLocalFileSize() = 0;
        virtual void FreeDiskSpace(bool need_remove_file) = 0;

        virtual void CloseFileHandle() = 0;
        virtual bool IsFileOpen() = 0;
        virtual bool ReOpenFile() = 0;

        virtual boost::int64_t GetLastWriteTime() = 0;
#endif  // DISK_MODE

    public:

        unsigned char GetDownMode();

        // 改名并通知instance、删除某个block并通知instance、保存资源信息
        virtual void ThreadRemoveBlock(uint32_t index, bool hash_check_failed);

        virtual void Rename(const string& newname) = 0;
        virtual void CloseResource(bool need_remove_file) = 0;

    protected:
        virtual void FlushStore() = 0;
        virtual base::AppBuffer ReadBuffer(const uint32_t startpos, const uint32_t length) = 0;
        virtual std::vector<base::AppBuffer> ReadBufferArray(const uint32_t startpos, const uint32_t length) = 0;
        virtual bool ReadBufferArray(const uint32_t startpos, const uint32_t length, std::vector<protocol::SubPieceContent*> buffs) = 0;
        virtual bool WriteBuffer(const uint32_t startpos, const protocol::SubPieceBuffer* buffer) = 0;
        virtual bool WriteBufferArray(const uint32_t startpos, const std::vector<const protocol::SubPieceBuffer*>& buffer) = 0;
        virtual void Erase(const uint32_t startpos, const uint32_t length) = 0;
        virtual bool TryRenameToNormalFile() = 0;
        virtual bool TryRenameToTppFile() = 0;

    private:
        static void ReleaseSubPieceContent(protocol::SubPieceContent *buff){ delete buff;}
        static void ReleaseSubPieceContentArray(std::vector<protocol::SubPieceContent*> buffs)
        {
            for (std::vector<protocol::SubPieceContent*>::iterator iter = buffs.begin();
                iter != buffs.end(); ++iter)
            {
                delete *iter;
            }
        }
        static void ReleaseSubPieceBuffer(protocol::SubPieceBuffer * buf) {delete buf;}

        bool ThreadReadBufferForPlayAfterHash(protocol::SubPieceInfo const & subpiece_info, std::vector<protocol::SubPieceContent*> const & buffs);
        MD5 CalcHash(base::AppBuffer const & buff) const;

    protected:
        boost::asio::io_service & io_svc_;

        // ResourceDescriptor::p file_resource_desc_p_;    // 关联的文件描述
        boost::shared_ptr<SubPieceManager> subpiece_manager_;             // related subpiece manager
        uint32_t actual_size_;                            // 由构造函数赋值，在派生类中改变
#ifdef DISK_MODE
        bool need_saveinfo_to_disk_;                    // 决定是否将下载信息保存到cfg文件中
#endif
        string file_name_;                              // 完整文件名
        boost::shared_ptr<Instance> instance_p_;        // 关联的instance
        volatile bool is_running_;                      // 该值可能会被其他线程被改变
    };  // class Resource

}  // namespace storage

#endif  // STORAGE_RESOURCE_H
