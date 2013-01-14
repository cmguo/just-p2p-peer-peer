//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

/*
* SharedMemory.h
*
*  Created on: 6 Jan 2010
*      Author: xuwaters
*/

#ifndef FRAMEWORK_INTERPROCESS_SHAREDMEMORY_H
#define FRAMEWORK_INTERPROCESS_SHAREDMEMORY_H

namespace boost
{
    namespace interprocess
    {
#ifndef PEER_PC_CLIENT
        class shared_memory_object;
#else
        class windows_shared_memory;
#endif
        class mapped_region;
    }
}

namespace interprocess
{

    class SharedMemory
    {
    public:
        bool Create(const string& name, boost::uint32_t size);
        bool Open(const string& name);
        void Close();
        void* GetView();
        bool IsValid() const;
        string GetName() const;

        SharedMemory();
        ~SharedMemory();

    private:
        static void Remove(const string& name);

#ifdef NO_SHARED_MEMORY
        // nothing
#else

#ifndef PEER_PC_CLIENT
        typedef boost::shared_ptr<boost::interprocess::shared_memory_object> SharedMemoryPtr;
#else
        typedef boost::shared_ptr<boost::interprocess::windows_shared_memory> SharedMemoryPtr;
#endif
        typedef boost::shared_ptr<boost::interprocess::mapped_region> MappedRegionPtr;
        //
        SharedMemoryPtr shared_memory_;
        MappedRegionPtr mapped_region_;
#endif  // NO_SHARED_MEMORY

        string name_;
        bool is_owner_;
    };

}

#endif  // FRAMEWORK_INTERPROCESS_SHAREDMEMORY_H
