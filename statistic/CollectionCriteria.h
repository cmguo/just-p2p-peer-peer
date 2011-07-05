//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _STATISTIC_CONFIGURATION_CRITERIA_H_
#define _STATISTIC_CONFIGURATION_CRITERIA_H_

namespace statistic
{
    enum ConditionType
    {
        Unknown,
        Bufferring,
    };

    enum TargetResourceType
    {
        Any,
        Vod,
        Live
    };

    class IResourceFilter
    {
    public:
        virtual bool IsTrue(const RID& rid) const = 0;
        virtual ~IResourceFilter(){}
    };

    class ResourceFilterAny: public IResourceFilter
    {
    public:
        bool IsTrue(const RID& rid) const
        {
            return true;
        }
    };

    class ResourceFilterByRid: public IResourceFilter
    {
        std::set<RID> applicable_resource_ids_;
    public:
        ResourceFilterByRid(const std::vector<RID>& applicable_resource_ids)
        {
            for(size_t i = 0; i < applicable_resource_ids.size(); ++i)
            {
                applicable_resource_ids_.insert(applicable_resource_ids[i]);
            }
        }

        bool IsTrue(const RID& rid) const
        {
            return applicable_resource_ids_.find(rid) != applicable_resource_ids_.end();
        }
    };

    class CollectionCriteria
    {
    public:
        CollectionCriteria()
            :condition_type_(Unknown), target_resource_type_(Any)
        {

        }

        ConditionType condition_type_;
        TargetResourceType target_resource_type_;
        boost::shared_ptr<IResourceFilter> resource_filter_;

        bool IsApplicable(const RID& rid) const;
    };
}

#endif  // _STATISTIC_CONFIGURATION_CRITERIA_H_
