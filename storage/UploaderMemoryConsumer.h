//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _UPLOADER_MEMORY_CONSUMER_H
#define _UPLOADER_MEMORY_CONSUMER_H

#include "MemoryConsumer.h"
#include "p2sp/p2p/UploadModule.h"

namespace storage
{
    //Note: 这个MemoryConsumer其实仅仅跟踪或淘汰点播中的待上传数据
    //而直播的待上传数据在LiveInstanceMemoryConsumer中跟踪与淘汰
    class UploaderMemoryConsumer: public IMemoryConsumer
    {
    private:
        static const int BlockSizeInBytes = 2*1024*1024;

    public:
        UploaderMemoryConsumer(boost::shared_ptr<p2sp::UploadModule> upload_module)
            : upload_module_(upload_module)
        {
        }

        MemoryUsageDescription GetMemoryUsage() const;

        void SetMemoryQuota(const MemoryQuota & quota);
        
    private:
        boost::shared_ptr<p2sp::UploadModule> upload_module_;
    };
}

#endif  //_UPLOADER_MEMORY_CONSUMER_H
