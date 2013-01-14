//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _PEER_UTIL_H
#define _PEER_UTIL_H

#include <framework/system/BytesOrder.h>
#include <network/Uri.h>
#include "random.h"

#if PEER_PC_CLIENT

// #include <shlwapi.h>
// #pragma comment(lib. "shlwapi.lib")
#include <Iphlpapi.h>
#pragma comment(lib, "Iphlpapi.lib")
#else
#include "framework/network/Interface.h"
#endif

namespace base
{
    namespace util
    {
        inline boost::int32_t GuidMod(const Guid& guid, boost::int32_t mod)
        {
            boost::uint64_t buf[2];
            memcpy(&buf, &guid.data(), sizeof(guid.data()));
            buf[1] = framework::system::BytesOrder::little_endian_to_host_longlong(buf[1]);
            return static_cast<boost::int32_t> (buf[1] % mod);
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

        inline void memcpy2(void *dest, boost::int32_t numberOfElements, const void *src, boost::int32_t count)
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

        inline bool is_private_address(boost::int32_t addr)
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

        inline void DoCrash(boost::int32_t percentage)
        {
            if (Random::GetGlobal().Next(100) < percentage)
            {
                char *bad_ptr = 0;
                *bad_ptr = 0;
            }
        }

        inline boost::int32_t GetLocalFirstIP()
        {
#if PEER_PC_CLIENT
            // 使用 ip helper函数
            boost::int32_t nip = 0;
            PMIB_IPADDRTABLE pIPAddrTable;
            boost::int32_t dwSize = 0 , dwRetVal;

            pIPAddrTable = (MIB_IPADDRTABLE*) malloc(sizeof(MIB_IPADDRTABLE));

            // Make an initial call to GetIpAddrTable to get the
            // necessary size into the dwSize variable
            if (GetIpAddrTable(pIPAddrTable, &dwSize, 0) == ERROR_INSUFFICIENT_BUFFER)
            {
                free(pIPAddrTable);
                pIPAddrTable = (MIB_IPADDRTABLE *) malloc (dwSize);
            }

            // Make a second call to GetIpAddrTable to get the
            // actual data we want
            if ((dwRetVal = GetIpAddrTable(pIPAddrTable, &dwSize, 0)) == NO_ERROR)
            {
                for (boost::int32_t i = 0; i < pIPAddrTable->dwNumEntries; i++)
                {
                    if (pIPAddrTable->table[i].dwAddr != inet_addr("127.0.0.1") && pIPAddrTable->table[i].dwAddr != 0)
                    {
                        nip = pIPAddrTable->table[i].dwAddr;
                        break;
                    }
                }

            }

            free(pIPAddrTable);

            return nip;
#else
        std::vector<framework::network::Interface> interfaces;
        if (::framework::network::enum_interface(interfaces))
            return 0;

        boost::int32_t local_ip = 0;
        for (boost::int32_t i = 0; i < interfaces.size(); ++i) {
            framework::network::Interface const & inf = interfaces[i];
            if (string(inf.name) != "lo" && inf.up && inf.addr.is_v4()) {
                return inf.addr.to_v4().to_ulong();
            }
        }

        return 0;
#endif
        }
    }
}

#endif