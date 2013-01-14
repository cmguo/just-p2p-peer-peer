//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "p2sp/proxy/RangeInfo.h"

#include <boost/algorithm/string/erase.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/predicate.hpp>

namespace p2sp
{

    RangeInfo::RangeInfo(boost::uint32_t range_begin, boost::uint32_t range_end)
        : range_begin_(range_begin), range_end_(range_end)
    {
    }

    bool RangeInfo::IsSupportedRangeFormat(const string& range_property)
    {
        return Parse(range_property);
    }

    RangeInfo::p RangeInfo::Parse(const string& range_property)
    {
        string range_string = boost::algorithm::trim_copy(range_property);
        boost::algorithm::ierase_all(range_string, " ");
        // check
        string bytes_pattern = "bytes=";
        if (false == boost::algorithm::istarts_with(range_string, bytes_pattern)) {
            return RangeInfo::p();
        }
        if (boost::algorithm::contains(range_string, ",")) {  // not support multi range
            return RangeInfo::p();
        }
        range_string = range_string.substr(bytes_pattern.length());
        if (range_string.length() == 0 || range_string[0] == '-') {  // only support a- or a-b
            return RangeInfo::p();
        }
        // split
        std::vector<string> ranges;
        boost::algorithm::split(ranges, range_string, boost::algorithm::is_any_of("-"));
        if (ranges.size() != 2) {
            return RangeInfo::p();
        }

        boost::uint32_t range_begin;
        boost::uint32_t range_end;
        boost::system::error_code ec = framework::string::parse2(ranges[0], range_begin);
        if (ec) return RangeInfo::p();
        if (ranges[1].length() != 0) {
            ec = framework::string::parse2(ranges[1], range_end);
            if (ec) return RangeInfo::p();
        }
        else {
            range_end = RangeInfo::npos;
        }
        // check
        if (range_begin == RangeInfo::npos) {
            return RangeInfo::p();
        }
        if (range_end != RangeInfo::npos) {
            if (range_begin > range_end) {
                return RangeInfo::p();
            }
        }
        // ok
        return RangeInfo::p(new RangeInfo(range_begin, range_end));
    }

}
