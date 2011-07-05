//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"
#include "statistic/CollectionCriteria.h"
#include "storage/Storage.h"

namespace statistic
{
    bool CollectionCriteria::IsApplicable(const RID& rid) const
    {
        if (!resource_filter_->IsTrue(rid))
        {
            return false;
        }

        switch (target_resource_type_)
        {
        case Live:
            return storage::Storage::Inst()->GetLiveInstanceByRid(rid);
        case Vod:
            return storage::Storage::Inst()->GetInstanceByRID(rid);
        case Any:
            return true;
        default:
            assert(false);
            return false;
        }
    }
}
