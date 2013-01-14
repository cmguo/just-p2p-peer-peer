//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------


#include "Common.h"
#include "measure/CycleBuffer.h"
#include <numeric>
using namespace std;

namespace measure
{
    CycleBuffer::CycleBuffer(boost::uint32_t capacity)
    {
        capacity_ = capacity;
    }


    void CycleBuffer::Push(boost::uint32_t val)
    {
        if (data_.size() == capacity_)
        {
            data_.pop_front();
        }

        data_.push_back(val);
    }

    boost::uint32_t CycleBuffer::Average() const
    {
        if (data_.empty())
        {
            return 0;
        }

        return std::accumulate(data_.begin(), data_.end(), 0) / data_.size();
    }

    boost::uint32_t CycleBuffer::MaxValue() const
    {
        if (data_.empty())
        {
            return 0;
        }

        return *std::max_element(data_.begin(), data_.end());
    }

    boost::uint32_t CycleBuffer::MinValue() const
    {
        if (data_.empty())
        {
            return 0;
        }

        return *std::min_element(data_.begin(), data_.end());
    }

    boost::uint32_t CycleBuffer::Count() const
    {
        return data_.size();
    }

    boost::uint32_t CycleBuffer::Sum() const
    {
        if (data_.empty())
        {
            return 0;
        }

        return std::accumulate(data_.begin(), data_.end(), 0);
    }
}
