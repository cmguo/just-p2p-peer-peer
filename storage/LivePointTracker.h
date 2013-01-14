//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _LIVE_POINT_TRACKER_H
#define _LIVE_POINT_TRACKER_H

#include "LivePosition.h"

namespace storage
{
    class LivePointTracker
    {
    public:
        LivePointTracker(boost::uint16_t live_interval)
            : live_point_start_(0, 0), ticks_since_live_point_last_set_(false)
        {
            assert(live_interval > 0);
            live_interval_ = live_interval == 0 ? 1 : live_interval;
        };

        boost::uint16_t GetLiveInterval() const { return live_interval_; }

        const LivePosition GetCurrentLivePoint() const
        {
            return GetCurrentLivePoint(true);
        }

        void SetCurrentLivePoint(const LivePosition & live_point);

    private:
        const LivePosition GetCurrentLivePoint(bool adjustment_needed) const;

        boost::uint16_t live_interval_;
        LivePosition live_point_start_;
        framework::timer::TickCounter ticks_since_live_point_last_set_;
    };

    inline const LivePosition LivePointTracker::GetCurrentLivePoint(bool adjustment_needed) const
    {
        boost::uint32_t current_live_point_block_id = live_point_start_.GetBlockId();
        if (ticks_since_live_point_last_set_.running())
        {
            assert(live_interval_ > 0);

            boost::uint32_t ticks_ellapsed = ticks_since_live_point_last_set_.elapsed();
            if (adjustment_needed)
            {
                ticks_ellapsed += 500*live_interval_;
            }

            boost::uint32_t incremental_blocks = ticks_ellapsed / (live_interval_*1000);
            current_live_point_block_id += live_interval_*incremental_blocks;
        }

        return LivePosition(current_live_point_block_id, live_point_start_.GetSubPieceIndex());
    }

    inline void LivePointTracker::SetCurrentLivePoint(const LivePosition & live_point)
    {
        assert(live_point.GetSubPieceIndex() == 0);

        if (GetCurrentLivePoint(false) < live_point)
        {
            //start() actually reset the tick counter as well
            ticks_since_live_point_last_set_.start();
            live_point_start_ = live_point;
        }
    }
}

#endif  //_LIVE_POINT_TRACKER_H
