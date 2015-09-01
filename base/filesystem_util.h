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
            catch(boost::filesystem::filesystem_error & e)
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
            catch(boost::filesystem::filesystem_error&)
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
            catch(boost::filesystem::filesystem_error & e)
            {
                code = e.code();
            }
        }

        inline bool is_regular_file_nothrow(const path & file_path)
        {
            try
            {
                return boost::filesystem::is_regular_file(file_path);
            }
            catch (boost::filesystem::filesystem_error &)
            {
                return false;
            }
        }

        inline boost::filesystem::directory_iterator directory_iterator_nothrow(
            const boost::filesystem::path & file_path,
            boost::system::error_code & ec)
        {
            boost::filesystem::directory_iterator directory_iter;
            try
            {
                directory_iter = boost::filesystem::directory_iterator(file_path, ec);
            }
            catch (boost::filesystem::filesystem_error & e)
            {
                ec = e.code();
            }

            return directory_iter;
        }
    }
}
#endif
