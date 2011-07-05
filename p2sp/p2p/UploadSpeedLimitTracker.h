//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// UploadSpeedLimitTracker.h

#ifndef _P2SP_P2P_UPLOAD_SPEED_LIMIT_TRACKER_H_
#define _P2SP_P2P_UPLOAD_SPEED_LIMIT_TRACKER_H_

namespace p2sp
{
    // 跟踪在不限速的情况下的最大上载速度
    class UploadSpeedLimitTracker
    {
    public:
        UploadSpeedLimitTracker();

        void Reset(boost::uint32_t max_unlimited_upload_speed_in_record);

        boost::uint32_t GetMaxUnlimitedUploadSpeedInRecord() const;
        
        boost::uint32_t GetMaxUploadSpeedForControl() const
        {
            return std::max(GetMaxUnlimitedUploadSpeedInRecord(), max_upload_speed_since_last_reset_);
        }

        void ReportUploadSpeed(boost::uint32_t current_upload_speed);

        void SetUploadWithoutLimit(bool upload_without_limit);

        void UpdateMaxUnlimitedUploadSpeedInRecordIfAppropriate();

    private:
        bool ShouldUpdateUnlimitedUploadSpeedInRecord() const;

        void DoReset(boost::uint32_t max_unlimited_upload_speed_in_record);

        boost::uint32_t AccumulativeUnlimitedUploadingTicks() const
        {
            return accumulative_unlimited_uploading_ticks_before_tickcount_reset_ + 
                   ticks_since_last_changed_to_unlimited_upload_.elapsed();
        }

    private:
        static const int RecordUpdateFrequencyInMinutes = 10;

        // 由于limited/unlimited状态经常切换,我们需要记录累计unlimited upload时间。
        // 每次更新max_unlimited_upload_speed_in_record_之后，这个累计记录均被重置
        // 累计时间由两部分组成:
        // 1. TickCounter记录的是最近一段连续unlimited upload时间，
        framework::timer::TickCounter ticks_since_last_changed_to_unlimited_upload_;
        // 2. 下面这个成员记录tickCount被重置之前的所有的累计时间（每次DoReset都会重置它）。
        boost::uint32_t accumulative_unlimited_uploading_ticks_before_tickcount_reset_;

        bool upload_without_limit_;

        boost::uint32_t max_upload_speed_since_last_reset_;
        boost::uint32_t max_unlimited_upload_speed_in_record_;
    };
}
#endif  // _P2SP_P2P_UPLOAD_SPEED_LIMIT_TRACKER_H_
