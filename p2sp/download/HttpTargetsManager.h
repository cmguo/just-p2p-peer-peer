//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// HttpTargetsManager.h

#ifndef _P2SP_DOWNLOAD_HTTPTARGETS_MANAGER_H_
#define _P2SP_DOWNLOAD_HTTPTARGETS_MANAGER_H_

#include "p2sp/download/SwitchControllerInterface.h"

namespace p2sp
{
    class HttpTargetsManager
    {
    public:
        HttpTargetsManager();
        void SetHttpTargets(const std::vector<IHTTPControlTarget::p> & http_targets);
        bool HaveNextHttpTarget() const;
        void MoveNextHttpTarget();
        void MoveBestHttpTarget();
        //当前正在尝试的http target，如果全部尝试完毕，返回速度最好的http target
        IHTTPControlTarget::p GetCurrentHttpTarget() const;
        boost::uint32_t size() const {return http_targets_.size();}

    private:
        void UpdateBestHttpTarget();
        
    private:
        std::vector<IHTTPControlTarget::p> http_targets_;
        //当前正在尝试的http target，如果全部尝试完毕，指向速度最好的http target
        boost::int32_t current_http_target_index_;
        boost::int32_t best_http_target_index_;
        boost::int32_t best_http_target_speed_;
        bool trying_http_targets_;
    };
}

#endif