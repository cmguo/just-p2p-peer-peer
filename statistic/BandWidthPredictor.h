//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#pragma once

namespace statistic
{
    class BandWidthPredictor
        : public boost::noncopyable
    {
    public:
        typedef boost::shared_ptr<BandWidthPredictor> p;

        static BandWidthPredictor::p Create();

    public:

        void Start();

        void Stop();

        bool IsRunning() const { return is_running_; }

        uint32_t GetBandWidth() const { return band_width_; }

        void UpdateTotalSpeed(uint32_t seconds, uint32_t total_speed_of_second);

    protected:

        void GenerateCoeff();

        void CalcBandWidth();

        double Pow(double a, uint32_t n);

        double Sum(double a, uint32_t n);

        BandWidthPredictor()
            : is_running_(false), band_width_(0)
        {}

    private:

        static const uint32_t STATISTIC_MINUTES = 30;
        static const uint32_t SECONDS_PER_MINUTE = 60;

    private:

        volatile bool is_running_;

        uint32_t band_width_;

        uint32_t start_position_;

        uint32_t end_position_;

        uint32_t band_width_of_minutes[STATISTIC_MINUTES];  // 60 minutes

        uint32_t start_second_;
        uint32_t current_second_;

        uint32_t last_second_;
        uint32_t last_count_;
        double band_width_double_;

    };
}
