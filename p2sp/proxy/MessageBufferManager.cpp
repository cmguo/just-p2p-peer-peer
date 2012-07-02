//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "p2sp/proxy/MessageBufferManager.h"

namespace p2sp
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_msg = log4cplus::Logger::getInstance("[msg_buffer_manager]");
#endif

    MessageBufferManager::p MessageBufferManager::inst_(new MessageBufferManager());

    boost::uint8_t* MessageBufferManager::NewBuffer(uint32_t buffer_size)
    {
        if (buffer_size == 0)
        {
            LOG4CPLUS_DEBUG_LOG(logger_msg, "BufferSize = 0!");
            return NULL;
        }

        boost::uint8_t* buffer = (boost::uint8_t*)malloc(buffer_size);

        if (NULL == buffer)
        {
            LOG4CPLUS_DEBUG_LOG(logger_msg, "BufferSize = " << buffer_size << 
                ", protocol::SubPieceContent malloc failed!");
            return NULL;
        }

        assert(buffer_cache_map_.find(buffer) == buffer_cache_map_.end());
        LOG4CPLUS_DEBUG_LOG(logger_msg, "BufferSize = " << buffer_size << 
            ", protocol::SubPieceContent: " << (void*)buffer);
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
                LOG4CPLUS_DEBUG_LOG(logger_msg, "protocol::SubPieceContent: " << (void*)buffer);
                free(buffer);
                buffer_cache_map_.erase(it);
            }
            else
            {
                LOG4CPLUS_DEBUG_LOG(logger_msg, "protocol::SubPieceContent: " << (void*)buffer << ", Not found!");
            }
        }
        else
        {
            LOG4CPLUS_DEBUG_LOG(logger_msg, "Invalid protocol::SubPieceContent");
        }
    }

    uint32_t MessageBufferManager::ExpireCache()
    {
        LOG4CPLUS_DEBUG_LOG(logger_msg, "");
        uint32_t count = 0;
        BufferCacheMap::iterator it;
        for (it = buffer_cache_map_.begin(); it != buffer_cache_map_.end();)
        {
            LOG4CPLUS_DEBUG_LOG(logger_msg, "protocol::SubPieceContent: " << (void*)it->first << 
                ", ElapsedTime: " << it->second.elapsed());
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
        LOG4CPLUS_DEBUG_LOG(logger_msg, "EraseCount: " << count);
        return count;
    }

}
