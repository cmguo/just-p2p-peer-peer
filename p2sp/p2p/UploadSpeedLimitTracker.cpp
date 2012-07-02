//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "p2sp/p2p/UploadSpeedLimitTracker.h"
#include "p2sp/p2p/P2SPConfigs.h"

namespace p2sp
{
#ifdef LOG_ENABLE
    static log4cplus::Logger logger_upload_limit_tracker = log4cplus::Logger::getInstance("[upload_limit_tracker]");
#endif

    UploadSpeedLimitTracker::UploadSpeedLimitTracker()
    {
        DoReset(0);
    }

    void UploadSpeedLimitTracker::Reset(boost::uint32_t max_unlimited_upload_speed_in_record)
    {
        DoReset(max_unlimited_upload_speed_in_record);

        LOG4CPLUS_DEBUG_LOG(logger_upload_limit_tracker, "max_unlimited_upload_speed_in_record_ reset:" << 
            max_unlimited_upload_speed_in_record_);
    }

    void UploadSpeedLimitTracker::DoReset(boost::uint32_t max_unlimited_upload_speed_in_record)
    {
        max_unlimited_upload_speed_in_record_ = max_unlimited_upload_speed_in_record;
        max_upload_speed_since_last_reset_ = 0;
        upload_without_limit_ = false;
        ticks_since_last_changed_to_unlimited_upload_.reset();
        accumulative_unlimited_uploading_ticks_before_tickcount_reset_ = 0;
    }

    void UploadSpeedLimitTracker::SetUploadWithoutLimit(bool upload_without_limit)
    {
        // 由于limited/unlimited状态经常切换,我们需要记录累计unlimited upload时间。
        if (upload_without_limit_ != upload_without_limit)
        {
            UpdateMaxUnlimitedUploadSpeedInRecordIfAppropriate();

            if (upload_without_limit_)
            {
                accumulative_unlimited_uploading_ticks_before_tickcount_reset_ += ticks_since_last_changed_to_unlimited_upload_.elapsed();
            }

            ticks_since_last_changed_to_unlimited_upload_.reset();

            upload_without_limit_ = upload_without_limit;
            LOG4CPLUS_DEBUG_LOG(logger_upload_limit_tracker, "upload_without_limit_ updated:" << upload_without_limit_);
        }
    }

    boost::uint32_t UploadSpeedLimitTracker::GetMaxUnlimitedUploadSpeedInRecord() const
    {
        return std::max(max_unlimited_upload_speed_in_record_, P2SPConfigs::UPLOAD_MIN_UPLOAD_BANDWIDTH);
    }

    void UploadSpeedLimitTracker::ReportUploadSpeed(boost::uint32_t current_upload_speed)
    {
        if (current_upload_speed > max_upload_speed_since_last_reset_)
        {
            max_upload_speed_since_last_reset_ = current_upload_speed;
            LOG4CPLUS_DEBUG_LOG(logger_upload_limit_tracker, "max_upload_speed_since_last_reset_ updated:" << 
                max_upload_speed_since_last_reset_);
        }

        UpdateMaxUnlimitedUploadSpeedInRecordIfAppropriate();
    }

    bool UploadSpeedLimitTracker::ShouldUpdateUnlimitedUploadSpeedInRecord() const
    {
        return upload_without_limit_ && 
            AccumulativeUnlimitedUploadingTicks() > RecordUpdateFrequencyInMinutes * 60 * 1000 &&
            max_upload_speed_since_last_reset_ > P2SPConfigs::UPLOAD_MIN_UPLOAD_BANDWIDTH;
    }

    void UploadSpeedLimitTracker::UpdateMaxUnlimitedUploadSpeedInRecordIfAppropriate()
    {
        if (ShouldUpdateUnlimitedUploadSpeedInRecord())
        {
            //两种不同混淆比例的目的是为了让记录能缓慢地降下去，而较快速地上升。
            double recorded_speed_ratio = 0.9;
            if (max_upload_speed_since_last_reset_ > max_unlimited_upload_speed_in_record_)
            {
                recorded_speed_ratio = 0.5;
            }

            double new_speed_ratio = 1.0 - recorded_speed_ratio;

            max_unlimited_upload_speed_in_record_ = static_cast<boost::uint32_t>(
                    recorded_speed_ratio*max_unlimited_upload_speed_in_record_ + 
                    new_speed_ratio*max_upload_speed_since_last_reset_);

            LOG4CPLUS_DEBUG_LOG(logger_upload_limit_tracker, "max_unlimited_upload_speed_in_record_ updated:" << 
                max_unlimited_upload_speed_in_record_);

            DoReset(max_unlimited_upload_speed_in_record_);
        }
    }
}