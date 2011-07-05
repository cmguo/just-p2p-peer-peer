//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _MEMORY_USAGE_DESCRIPTION_H
#define _MEMORY_USAGE_DESCRIPTION_H

namespace storage
{
    class MemoryUsageDescription
    {
    public:
        enum MemoryUsagePriority
        {
            NONE = 0,
            LOW = 10,
            NORMAL = 20,
            HIGH = 30
        };

        MemoryUsageDescription()
            : minimum_size_(0), desirable_size_(0), priority_(NONE)
        {
        }

        MemoryUsageDescription(size_t minimum_size, size_t desirable_size, MemoryUsagePriority priority)
        {
            minimum_size_ = minimum_size;
            desirable_size_ = desirable_size;
            priority_ = priority;
        }

        size_t GetMinimumSize() const { return minimum_size_; }
        size_t GetDesirableSize() const { return desirable_size_; }
        MemoryUsagePriority GetPriority() const { return priority_; }
    private:
        //all sizes are in bytes
        size_t minimum_size_;
        size_t desirable_size_;

        MemoryUsagePriority priority_;
    };
}

#endif  //_MEMORY_USAGE_DESCRIPTION_H
