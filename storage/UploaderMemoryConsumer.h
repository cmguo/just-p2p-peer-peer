//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _UPLOADER_MEMORY_CONSUMER_H
#define _UPLOADER_MEMORY_CONSUMER_H

#include "MemoryConsumer.h"
#include "../p2sp/p2p/UploadManager.h"

namespace storage
{
    //Note: ���MemoryConsumer��ʵ�������ٻ���̭�㲥�еĴ��ϴ�����
    //��ֱ���Ĵ��ϴ�������LiveInstanceMemoryConsumer�и�������̭
    class UploaderMemoryConsumer: public IMemoryConsumer
    {
    private:
        static const int BlockSizeInBytes = 2*1024*1024;

    public:
        UploaderMemoryConsumer(p2sp::UploadManager::p upload_manager)
            : upload_manager_(upload_manager)
        {
        }

        MemoryUsageDescription GetMemoryUsage() const;

        void SetMemoryQuota(const MemoryQuota & quota);
        
    private:
        p2sp::UploadManager::p upload_manager_;
    };
}

#endif  //_UPLOADER_MEMORY_CONSUMER_H
