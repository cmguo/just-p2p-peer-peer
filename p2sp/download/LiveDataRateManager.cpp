#include "Common.h"
#include "LiveDataRateManager.h"

namespace p2sp
{
    void LiveDataRateManager::Start(const vector<RID>& rids, const vector<boost::uint32_t>& data_rate_s)
    {
        assert(rids.size() > 0);
        assert(rids.size() == data_rate_s.size());
        rid_s_ = rids;
        current_data_rate_pos_ = 0;
        data_rate_s_ = data_rate_s;
        timer_.start();
    }

    RID LiveDataRateManager::GetCurrentRID()
    {
        assert(current_data_rate_pos_ < rid_s_.size());
        return rid_s_[current_data_rate_pos_];
    }

    // TODO: 跳转中心拿到的播放点与PMS最前的播放点的差距是否能超过 16 + 4 * current_data_rate_pos_
    bool LiveDataRateManager::SwitchToHigherDataRateIfNeeded(uint32_t rest_time_in_seconds)
    {
        boost::uint32_t last_pos = current_data_rate_pos_;
        if (timer_.elapsed() > 30*1000 && rest_time_in_seconds > 16 + 4 * current_data_rate_pos_)
        {
            current_data_rate_pos_++;
            if (current_data_rate_pos_ > rid_s_.size() - 1)
            {
                current_data_rate_pos_ = rid_s_.size() - 1;
            }
        }

        if (last_pos != current_data_rate_pos_)
        {
            timer_.reset();
            return true;
        }

        return false;
    }

    bool LiveDataRateManager::SwitchToLowerDataRateIfNeeded(uint32_t rest_time_in_seconds)
    {
        boost::uint32_t last_pos = current_data_rate_pos_;
        if (timer_.elapsed() > 20*1000 && rest_time_in_seconds < 8 + 4 * current_data_rate_pos_)
        {
            if (current_data_rate_pos_ != 0)
            {
                current_data_rate_pos_--;
            }
        }

        if (last_pos != current_data_rate_pos_)
        {
            timer_.reset();
            return true;
        }

        return false;
    }

    boost::uint32_t LiveDataRateManager::GetCurrentDataRatePos() const
    {
        return current_data_rate_pos_;
    }

    boost::uint32_t LiveDataRateManager::GetCurrentDefaultDataRate() const
    {
        assert(current_data_rate_pos_ < data_rate_s_.size());
        return data_rate_s_[current_data_rate_pos_];
    }

    const vector<boost::uint32_t>& LiveDataRateManager::GetDataRates() const
    {
        return data_rate_s_;
    }

    const vector<RID>& LiveDataRateManager::GetRids() const
    {
        return rid_s_;
    }
}