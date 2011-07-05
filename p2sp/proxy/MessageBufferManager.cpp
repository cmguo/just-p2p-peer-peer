//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "p2sp/proxy/MessageBufferManager.h"

namespace p2sp
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("msg");

    MessageBufferManager::p MessageBufferManager::inst_(new MessageBufferManager());

    boost::uint8_t* MessageBufferManager::NewBuffer(uint32_t buffer_size)
    {
        if (buffer_size == 0)
        {
            LOGX(__DEBUG, "msg", "BufferSize = 0!");
            return NULL;
        }

        boost::uint8_t* buffer = (boost::uint8_t*)malloc(buffer_size);

        if (NULL == buffer)
        {
            LOGX(__DEBUG, "msg", "BufferSize = " << buffer_size << ", protocol::SubPieceContent malloc failed!");
            return NULL;
        }

        assert(buffer_cache_map_.find(buffer) == buffer_cache_map_.end());
        LOGX(__DEBUG, "msg", "BufferSize = " << buffer_size << ", protocol::SubPieceContent: " << (void*)buffer);
        buffer_cache_map_[buffer].reset();
        return buffer;
    }

    void MessageBufferManager::DeleteBuffer(boost::uint8_t* buffer)
    {
        if (buffer != NULL)
        {
            BufferCacheMap::iterator it = buffer_cache_map_.find(buffer);
            if (it != buffer_cache_map_.end())
            {
                LOGX(__DEBUG, "msg", "protocol::SubPieceContent: " << (void*)buffer);
                free(buffer);
                buffer_cache_map_.erase(it);
            }
            else
            {
                LOGX(__DEBUG, "msg", "protocol::SubPieceContent: " << (void*)buffer << ", Not found!");
            }
        }
        else
        {
            LOGX(__DEBUG, "msg", "Invalid protocol::SubPieceContent");
        }
    }

    uint32_t MessageBufferManager::ExpireCache()
    {
        LOGX(__DEBUG, "msg", "");
        uint32_t count = 0;
        BufferCacheMap::iterator it;
        for (it = buffer_cache_map_.begin(); it != buffer_cache_map_.end();)
        {
            LOGX(__DEBUG, "msg", "protocol::SubPieceContent: " << (void*)it->first << ", ElapsedTime: " << it->second.elapsed());
            if (it->second.elapsed() > 180 * 1000)  // 3 min
            {
                ++count;
                free(it->first);
                buffer_cache_map_.erase(it++);
            }
            else
            {
                ++it;
            }
        }
        LOGX(__DEBUG, "msg", "EraseCount: " << count);
        return count;
    }

}
