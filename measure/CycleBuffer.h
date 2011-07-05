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
        CycleBuffer(uint32_t capacity);

        void Push(uint32_t val);

        uint32_t Average() const;

        uint32_t MaxValue() const;

        uint32_t MinValue() const;

        uint32_t Count() const;

        uint32_t Sum() const;

        void Clear() {data_.clear();}

    private:
        std::deque<uint32_t> data_;
        uint32_t capacity_;
    };
}

#endif  // _PEER_MEASURE_CYCLE_BUFFER_H
