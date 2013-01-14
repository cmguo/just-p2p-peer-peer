//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// HttpTargetsManager.cpp

#include "Common.h"
#include "HttpTargetsManager.h"

namespace p2sp
{
    HttpTargetsManager::HttpTargetsManager()
        : trying_http_targets_(true)
    {
    }

    void HttpTargetsManager::SetHttpTargets(const std::vector<IHTTPControlTarget::p> & http_targets)
    {
        assert(http_targets_.empty());
        http_targets_ = http_targets;
        current_http_target_index_ = 0;
    }

    void HttpTargetsManager::UpdateBestHttpTarget()
    {
        if (current_http_target_index_ == 0 ||
            (boost::int32_t)http_targets_[current_http_target_index_]->GetCurrentDownloadSpeed() > best_http_target_speed_)
        {
            best_http_target_index_ = current_http_target_index_;
            best_http_target_speed_ = (boost::int32_t)http_targets_[current_http_target_index_]->GetCurrentDownloadSpeed();
        }
    }

    void HttpTargetsManager::MoveNextHttpTarget()
    {
        assert(!http_targets_.empty());
        assert(trying_http_targets_);
        assert(current_http_target_index_ != http_targets_.size() - 1);

        UpdateBestHttpTarget();
        ++current_http_target_index_;
    }

    void HttpTargetsManager::MoveBestHttpTarget()
    {
        if (!trying_http_targets_)
        {
            return;
        }

        assert(!http_targets_.empty());
        assert(current_http_target_index_ == http_targets_.size() - 1);

        UpdateBestHttpTarget();
        trying_http_targets_ = false;
        current_http_target_index_ = best_http_target_index_;
    }

    bool HttpTargetsManager::HaveNextHttpTarget() const
    {
        if (http_targets_.empty())
        {
            return false;
        }

        return trying_http_targets_ && current_http_target_index_ != http_targets_.size() - 1;
    }

    IHTTPControlTarget::p HttpTargetsManager::GetCurrentHttpTarget() const
    {
        if (!http_targets_.empty())
        {
            return http_targets_[current_http_target_index_];
        }
        else
        {
            return IHTTPControlTarget::p();
        }  
    }
}