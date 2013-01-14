//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// RangeInfo.h

#ifndef _P2SP_PROXY_RANGE_INFO_H_
#define _P2SP_PROXY_RANGE_INFO_H_

namespace p2sp
{
    class RangeInfo
        : public boost::noncopyable
#ifdef DUMP_OBJECT
        , public count_object_allocate<RangeInfo>
#endif
    {
    public:
        typedef boost::shared_ptr<RangeInfo> p;

    public:
        static const boost::uint32_t npos = static_cast<boost::uint32_t>(-1);

        static bool IsSupportedRangeFormat(const string& range_property);

        static RangeInfo::p Parse(const string& range_property);

        RangeInfo(boost::uint32_t range_begin, boost::uint32_t range_end);

        void SetRangeBegin(boost::uint32_t range_begin) { range_begin_ = range_begin; }
        boost::uint32_t GetRangeBegin() const { return range_begin_; }

        void SetRangeEnd(boost::uint32_t range_end) { range_end_ = range_end; }
        boost::uint32_t GetRangeEnd() const { return range_end_; }

    private:
        boost::uint32_t range_begin_;
        boost::uint32_t range_end_;
    };
}

#endif  // _P2SP_PROXY_RANGE_INFO_H_
