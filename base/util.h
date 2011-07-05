//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _PEER_UTIL_H
#define _PEER_UTIL_H

#include <framework/system/BytesOrder.h>
#include <network/Uri.h>

namespace base
{
    namespace util
    {
        inline uint32_t GuidMod(const Guid& guid, uint32_t mod)
        {
            boost::uint64_t buf[2];
            memcpy(&buf, &guid.data(), sizeof(guid.data()));
            buf[1] = framework::system::BytesOrder::little_endian_to_host_longlong(buf[1]);
            return static_cast<uint32_t> (buf[1] % mod);
        }

        inline string GetSegno(const network::Uri & uri)
        {
            string segno = uri.getparameter("segno");
            if (segno == "")
            {
                // 现有的目录形式
                string strpath = uri.getpath();
                segno = strpath.substr(1, strpath.find_last_of('/')-1);
            }

            if (segno.empty())
            {
                segno = "0";
            }

            return segno;
        }

        inline void memcpy2(void *dest, uint32_t numberOfElements, const void *src, uint32_t count)
        {
            if (count == 0)
            {
                return;
            }

            char * bad_ptr = 0;
 
            // 默认拷贝的目的缓冲区和拷贝长度不超过1G
            if (count > 0x3FFFFFFF || numberOfElements > 0x3FFFFFFF)
            {
                assert(false);
                *bad_ptr = 0;
                return;
            }

            // 参数不合法时crash掉
            if (dest == NULL || src == NULL || numberOfElements < count)
            {
                assert(false);
                *bad_ptr = 0;
                return;
            }

            memcpy(dest, src, count);
        }

        bool GetAppDataPath(string & pszCachePath);

        inline bool is_private_address(boost::uint32_t addr)
        {
            boost::asio::ip::address_v4 address(addr);
            boost::asio::ip::address_v4::bytes_type address_bytes = address.to_bytes();

            if (address_bytes[0] == 10)
            {
                return true;
            }
            else if (address_bytes[0] == 172)
            {
                return (address_bytes[1] >= 16) && (address_bytes[1] <= 31);
            }
            else if (address_bytes[0] == 192)
            {
                return (address_bytes[1] == 168);
            }
            else if (address_bytes[0] == 169)
            {
                return (address_bytes[1] == 254);
            }
            else
            {
                return false;
            }
        }

        inline bool is_private_address(const char * addr)
        {
            boost::system::error_code ec;
            boost::asio::ip::address_v4 address(boost::asio::ip::address_v4::from_string(addr, ec));
            if (ec)
            {
                return false;
            }
            else
            {
                return is_private_address(address.to_ulong());
            }
        }
    }
}

#endif