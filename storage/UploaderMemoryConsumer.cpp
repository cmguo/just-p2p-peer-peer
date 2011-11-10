//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "UploaderMemoryConsumer.h"

namespace storage
{
    MemoryUsageDescription UploaderMemoryConsumer::GetMemoryUsage() const
    {
        MemoryUsageDescription memory_usage;
        if (!upload_module_)
        {
            return memory_usage;
        }

        size_t cache_size = upload_module_->GetCurrentCacheSize();
        if (cache_size <= 0)
        {
            return memory_usage;
        }

        size_t minimum_size = std::min<size_t>(cache_size, 5) * BlockSizeInBytes;
        size_t desirable_size = cache_size * BlockSizeInBytes;

        return MemoryUsageDescription(minimum_size, desirable_size, MemoryUsageDescription::LOW);
    }

    void UploaderMemoryConsumer::SetMemoryQuota(const MemoryQuota & quota)
    {
        if (upload_module_)
        {
            size_t assigned_cache_size = (quota.quota + BlockSizeInBytes - 1)/BlockSizeInBytes;
            upload_module_->SetCurrentCacheSize(assigned_cache_size);
        }
    }
}
