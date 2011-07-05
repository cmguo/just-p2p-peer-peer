//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef STORAGE_STATISTIC_TOOLS_H
#define STORAGE_STATISTIC_TOOLS_H

#ifdef BOOST_WINDOWS_API
#pragma once
#endif

/************************************************************************
 //
 // Filename: StatisticTools.h
 // Comment:  一些做统计用的函数或结构
 // Date:     2008-9-2
 //
 ************************************************************************/

namespace storage
{
    // 统计命中率
struct HitRate
{
    boost::uint64_t from_mem;
    boost::uint64_t from_disk;
    HitRate() :
        from_mem(0), from_disk(0)
    {
    }
    float GetHitRate()
    {
        float r = (from_mem + from_disk) == 0 ? 0 : float(from_mem) / (from_mem + from_disk) * 100;
        return r;
    }
};

/*
struct MemoryPercentage
{
    float mem_percent_;

    MemoryPercentage() :
        mem_percent_(0.0)
    {
    }
};*/


}  // namespace storage

#endif  // STORAGE_STATISTIC_TOOLS_H
