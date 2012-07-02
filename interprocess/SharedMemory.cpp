//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

/*
* SharedMemory.cpp
*
*  Created on: 6 Jan 2010
*      Author: xuwaters
*/

#include "Common.h"
#include "interprocess/SharedMemory.h"

#ifdef NO_SHARED_MEMORY
// nothing
#else
namespace bi = boost::interprocess;
#endif  // NO_SHARED_MEMORY

namespace interprocess
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_share_memory = log4cplus::Logger::getInstance("[share_memory]");
#endif

    SharedMemory::SharedMemory()
    {
        is_owner_ = false;
    }

    SharedMemory::~SharedMemory()
    {
        Close();
    }

    bool SharedMemory::Create(const string& name, uint32_t size)
    {
        Close();
        is_owner_ = true;
        name_ = name;
#ifdef NO_SHARED_MEMORY
        return true;
#else
        try
        {
#ifndef BOOST_WINDOWS_API
            bi::shared_memory_object::remove(name_.c_str());
            shared_memory_ = SharedMemoryPtr(new bi::shared_memory_object(bi::create_only, name_.c_str(), bi::read_write));
            shared_memory_->truncate(size);
#else
            shared_memory_ = SharedMemoryPtr(new bi::windows_shared_memory(bi::create_only, name.c_str(), bi::read_write, size));
#endif

            // region
            mapped_region_ = MappedRegionPtr(new bi::mapped_region(*shared_memory_, bi::read_write));
            return true;
        }
        catch(const bi::interprocess_exception&)
        {
            Close();
            return false;
        }
#endif  // NO_SHARED_MEMORY
    }

    bool SharedMemory::Open(const string& name)
    {
        is_owner_ = false;
#ifdef NO_SHARED_MEMORY
        return true;
#else
        try
        {
            is_owner_ = false;
#ifndef BOOST_WINDOWS_API
            shared_memory_ = SharedMemoryPtr(new bi::shared_memory_object(bi::open_only, name.c_str(), bi::read_only));
#else
            shared_memory_ = SharedMemoryPtr(new bi::windows_shared_memory(bi::open_only, name.c_str(), bi::read_only));
#endif

            // region
            mapped_region_ = MappedRegionPtr(new bi::mapped_region(*shared_memory_, bi::read_only));
            return true;
        }
        catch(const bi::interprocess_exception&)
        {
            Close();
            return false;
        }
#endif  // NO_SHARED_MEMORY
    }

    void SharedMemory::Close()
    {
#ifdef NO_SHARED_MEMORY
        // nothing
#else
        shared_memory_.reset();
        mapped_region_.reset();
#endif  // NO_SHARED_MEMORY
        if (true == is_owner_)
        {
            Remove(name_);
        }
        name_.clear();
    }

    void* SharedMemory::GetView()
    {
#ifdef NO_SHARED_MEMORY
        // nothing
#else
        if (shared_memory_ && mapped_region_)
        {
            return mapped_region_->get_address();
        }
#endif  // NO_SHARED_MEMORY
        return NULL;
    }

    bool SharedMemory::IsValid() const
    {
#ifdef NO_SHARED_MEMORY
        return false;
#else
        return shared_memory_ && mapped_region_ && NULL != mapped_region_->get_address();
#endif  // NO_SHARED_MEMORY
    }

    string SharedMemory::GetName() const
    {
        return name_;
    }

    void SharedMemory::Remove(const string& name)
    {
#ifdef NO_SHARED_MEMORY
        // nothing
#else
        bool result = true;
#ifndef BOOST_WINDOWS_API
        result = bi::shared_memory_object::remove(name.c_str());
        (void)result;
#endif
        LOG4CPLUS_DEBUG_LOG(logger_share_memory, "name = " << name << ", Result = " << result);
#endif  // NO_SHARED_MEMORY
    }

}

