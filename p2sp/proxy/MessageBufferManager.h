//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// MessageBufferManager.h

#ifndef _P2SP_PROXY_MESSAGE_BUFFER_MANAGER_H_
#define _P2SP_PROXY_MESSAGE_BUFFER_MANAGER_H_

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/sequenced_index.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>

namespace p2sp
{
    class MessageBufferManager
        : public boost::noncopyable
        , public boost::enable_shared_from_this<MessageBufferManager>
#ifdef DUMP_OBJECT
        , public count_object_allocate<MessageBufferManager>
#endif
    {
    public:
        typedef boost::shared_ptr<MessageBufferManager> p;

        static p Inst() { return inst_; }

        boost::uint8_t* NewBuffer(boost::uint32_t buffer_size);

        void DeleteBuffer(boost::uint8_t* buffer);

        boost::uint32_t ExpireCache();

        template <typename StructType>
        StructType* NewStruct()
        {
            return (StructType*)NewBuffer(sizeof(StructType));
        }

    protected:
        MessageBufferManager(){}

    private:
        static MessageBufferManager::p inst_;

        typedef std::map<boost::uint8_t*, framework::timer::TickCounter> BufferCacheMap;

        BufferCacheMap buffer_cache_map_;
    };
}

#endif  // _P2SP_PROXY_MESSAGE_BUFFER_MANAGER_H_
