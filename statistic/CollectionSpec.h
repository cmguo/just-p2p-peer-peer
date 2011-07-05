//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _STATISTIC_COLLECTOR_SPEC_H_
#define _STATISTIC_COLLECTOR_SPEC_H_

namespace statistic
{
    class CollectionSpec
    {
    public:
        CollectionSpec()
            : includes_downloaders_(false), includes_download_drivers_(false), collection_length_(0)
        {
        }

        CollectionSpec(const RID& rid, size_t collection_length, bool includes_downloaders, bool includes_download_driver)
            : rid_(rid), collection_length_(collection_length), 
            includes_downloaders_(includes_downloaders), includes_download_drivers_(includes_download_driver)
        {
        }

        RID GetResourceId() const { return rid_; }
        bool IncludesDownloadDrivers() const { return includes_download_drivers_; }
        bool IncludesDownloaders() const { return includes_downloaders_; }
        size_t GetCollectionLength() const { return collection_length_; }

        void Union(const CollectionSpec& spec)
        {
            assert(rid_ == spec.rid_);
            if (rid_ != spec.rid_)
            {
                return;
            }
            
            includes_downloaders_ |= spec.includes_downloaders_;
            includes_download_drivers_ |= spec.includes_download_drivers_;
            if (spec.collection_length_ > collection_length_)
            {
                collection_length_ = spec.collection_length_;
            }
        }

    private:
        RID rid_;
        bool includes_downloaders_;
        bool includes_download_drivers_;
        size_t collection_length_;
    };
}

#endif  // _STATISTIC_COLLECTOR_SPEC_H_
