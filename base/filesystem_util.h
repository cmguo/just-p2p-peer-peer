//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _FILESYSTEM_UTIL_
#define _FILESYSTEM_UTIL_

#include <boost/filesystem.hpp>

namespace base
{
    namespace filesystem
    {
        using namespace boost::filesystem;
        inline void remove_nothrow(const path& ph)
        {
            boost::system::error_code ec;
            boost::filesystem::remove(ph, ec);
        }

        inline void rename_nothrow(const path & from_path, const path & to_path, boost::system::error_code & ec)
        {
            try
            {
                boost::filesystem::rename(from_path, to_path);
            }
            catch(boost::filesystem::basic_filesystem_error<path> & e)
            {
                ec = e.code();
            }
        }

        inline bool exists_nothrow(const path & ph)
        {
            try
            {
                return boost::filesystem::exists(ph);
            }
            catch(boost::filesystem::basic_filesystem_error<path>&)
            {
                return false;
            }
        }

        inline void last_write_time_nothrow(const path& file_path, std::time_t& last_write_time, boost::system::error_code & code)
        {
            try
            {
                last_write_time = boost::filesystem::last_write_time(file_path);
                code.clear();
            }
            catch(boost::filesystem::basic_filesystem_error<path> & e)
            {
                code = e.code();
            }
        }
    }
}
#endif