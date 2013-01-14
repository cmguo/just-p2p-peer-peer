//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// CycleBuffer.h

#ifndef _PEER_MEASURE_CYCLE_BUFFER_H
#define _PEER_MEASURE_CYCLE_BUFFER_H

namespace measure
{
    enum CycleMode
    {
        CYCLE_NONE      = 0x0,
        CYCLE_MIN_VAL   = 0x1,
        CYCLE_MAX_VAL   = 0x2,
    };

    class CycleBuffer
#ifdef DUMP_OBJECT
        : public count_object_allocate<CycleBuffer>
#endif
    {
    public:
        CycleBuffer(boost::uint32_t capacity);

        void Push(boost::uint32_t val);

        boost::uint32_t Average() const;

        boost::uint32_t MaxValue() const;

        boost::uint32_t MinValue() const;

        boost::uint32_t Count() const;

        boost::uint32_t Sum() const;

        void Clear() {data_.clear();}

    private:
        std::deque<boost::uint32_t> data_;
        boost::uint32_t capacity_;
    };
}

#endif  // _PEER_MEASURE_CYCLE_BUFFER_H
