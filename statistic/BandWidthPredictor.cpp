//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include <sstream>
#include "statistic/BandWidthPredictor.h"

FRAMEWORK_LOGGER_DECLARE_MODULE("bandwidth")

#define BW_DEBUG(msg) LOG(__DEBUG, "bandwidth", __FUNCTION__ << " " << msg)

namespace statistic
{
    const uint32_t MIN_SPEED_BANDWIDTH = 2*1024;

    BandWidthPredictor::p BandWidthPredictor::Create()
    {
        return BandWidthPredictor::p(new BandWidthPredictor());
    }

    void BandWidthPredictor::Start()
    {
        if (true == IsRunning())
            return;

        start_position_ = 0;
        start_second_ = (uint32_t)-1;
        end_position_ = 0;
        current_second_ = (uint32_t)-1;

        band_width_ = 0;

        last_second_ = 0;
        last_count_ = 0;
        band_width_double_ = 0.0;

        memset(band_width_of_minutes, 0, sizeof(band_width_of_minutes));

        is_running_ = true;
    }

    void BandWidthPredictor::Stop()
    {
        if (false == IsRunning())
            return;

        is_running_ = true;

    }

    void BandWidthPredictor::UpdateTotalSpeed(uint32_t seconds, uint32_t total_speed_of_second)
    {
        if (false == IsRunning())
            return;

        if (total_speed_of_second >= 2*1024)
        {
            band_width_ = (std::max)(band_width_, total_speed_of_second);
        }
        return;

        if (start_second_ == (uint32_t)-1)
        {
            start_second_ = seconds;
            start_position_ = (start_second_ / SECONDS_PER_MINUTE) % STATISTIC_MINUTES;
        }

        uint32_t curr_minutes = (seconds / SECONDS_PER_MINUTE);
        uint32_t last_minutes = (current_second_ / SECONDS_PER_MINUTE);
        if (curr_minutes - last_minutes >= SECONDS_PER_MINUTE)
        {
            memset(band_width_of_minutes, 0, sizeof(band_width_of_minutes));
        }
        else
        {
            for (uint32_t i = last_minutes + 1; i <= curr_minutes; ++i)
                band_width_of_minutes[ i % STATISTIC_MINUTES ] = 0;
        }
        current_second_ = seconds;
        end_position_ = (curr_minutes % STATISTIC_MINUTES);
        band_width_of_minutes[ end_position_ ] = (std::max)(band_width_of_minutes[end_position_], total_speed_of_second);
        // calc
        if (total_speed_of_second > MIN_SPEED_BANDWIDTH)  // 2KBps
        {
            CalcBandWidth();
        }

#ifdef NEED_LOG
        {
            std::stringstream ss;
            // copy(band_width_of_minutes, band_width_of_minutes + STATISTIC_MINUTES, ostream_iterator<uint32_t>(ss, ","));
            BW_DEBUG("bandwidth=" << band_width_ << " data=" << ss.str());
        }
#endif
    }

    void BandWidthPredictor::CalcBandWidth()
    {
        assert(start_second_ != (uint32_t)-1);
        uint32_t count = 0;
        count = (current_second_ - start_second_) / SECONDS_PER_MINUTE + 1;
        LIMIT_MAX(count, STATISTIC_MINUTES);

        assert(count > 0);
        // if (count == 1)
        // {
        //    band_width_ = band_width_of_minutes[ end_position_ ];
        // }
        // else if (count == 2)
        // {
        //    band_width_ = band_width_of_minutes[end_position_ - 1] + 3 * band_width_of_minutes[end_position_];
        //    band_width_ /= 4;
        // }
        // else if (count == 3)
        // {
        //    band_width_ = band_width_of_minutes[end_position_ - 2] + 6 * band_width_of_minutes[end_position_ - 1] + 9 * band_width_of_minutes[end_position_];
        //    band_width_ /= 16;
        // }
        // else
        // {
        //    double bw = 0, div = 0;
        //    int i, j;
        //    for (i = 0, j = end_position_; i < count; ++i, j = (STATISTIC_MINUTES + j - 1) % STATISTIC_MINUTES)
        //    {
        //        if (band_width_of_minutes[j] > MIN_SPEED_BANDWIDTH)
        //        {
        //            bw += band_width_of_minutes[j]
        //        }
        //        if (j == start_position_) break;
        //    }
        //    if (div > 0)
        //    {
        //        bw /= div;
        //        band_width_ = (uint32_t)(bw + 0.5);
        //    }
        // }

        /*
        const double c = 0.5;
        double bw = 0.0, ml = 1.0, div = 0.0;
        int i, j;
        for (i = 0, j = end_position_; i < count; ++i, j = (STATISTIC_MINUTES + j - 1)%STATISTIC_MINUTES)
        {
            if (band_width_of_minutes[j] > MIN_SPEED_BANDWIDTH)
            {
                bw += band_width_of_minutes[j] * ml;
            }
            div += ml;
            ml *= c;
            if (j == start_position_)
                break;
        }

        if (div > 0)
        {
            band_width_double_ = bw / div;
        }

        // update
        last_second_ = current_second_;
        band_width_ = (uint32_t)(band_width_double_ + 0.5);
        */

        uint32_t i, j;
        uint32_t bw = 0;
        for (i = 0, j = end_position_; i < count; ++i, j = (STATISTIC_MINUTES + j - 1)%STATISTIC_MINUTES)
        {
            if (band_width_of_minutes[j] > MIN_SPEED_BANDWIDTH && band_width_of_minutes[j] > bw)
            {
                bw = band_width_of_minutes[j];
            }
            if (j == start_position_)
                break;
        }

        band_width_ = bw;
    }

    double BandWidthPredictor::Pow(double a, uint32_t n)
    {
        double r = 1.0;
        while (n)
        {
            if (n&1) r = r * a;
            a *= a;
            n >>= 1;
        }
        return r;
    }

    double BandWidthPredictor::Sum(double a, uint32_t n)
    {
        if (a == 1.0) return n;
        double r = (1 - Pow(a, n + 1)) / (1 - a);
        return r;
    }

}
