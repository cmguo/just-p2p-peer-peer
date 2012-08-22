//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// StunClient.cpp
#include "Common.h"
#include "StunClient.h"

#include <framework/network/Interface.h>

#include <boost/filesystem/path.hpp>

#include <cassert>
#include <cstring>
#include <iostream>
#include <cstdlib>

#ifdef WIN32
#include <time.h>

#else
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <netinet/in.h>
#include <errno.h>
#endif

#ifdef BOOST_WINDOWS_API
#include   <shlwapi.h>
#pragma   comment(lib, "shlwapi.lib")
#include <shlobj.h>
#ifndef __MINGW32__
#include <atlbase.h>
#endif

// #include <shlwapi.h>
// #pragma comment(lib. "shlwapi.lib")
#include <iphlpapi.h>
#pragma comment(lib, "Iphlpapi.lib")
#endif

#ifdef LOG_ENABLE
static log4cplus::Logger logger_stun = log4cplus::Logger::getInstance("[stun_client]");
#endif

namespace p2sp
{

#define MAX_NIC 3

CStunClient::CStunClient() :
    m_Socket1(-1), m_Socket2(-1)
{

}

void CStunClient::Stop()
{
    if (m_Socket1 != -1)
    {
        closesocket(m_Socket1);
        m_Socket1 = -1;
    }

    if (m_Socket2 != -1)
    {
        closesocket(m_Socket2);
        m_Socket2 = -1;
    }

}

protocol::MY_STUN_NAT_TYPE CStunClient::getNatType(char *pcServer)
{
    protocol::MY_STUN_NAT_TYPE snt_type = protocol::TYPE_ERROR;

    initNetwork();

    bool verbose = false;

    StunAddress4 stunServerAddr;
    stunServerAddr.addr = 0;

    int srcPort = 0;
    StunAddress4 sAddr[MAX_NIC];

    int numNic = 0;

    for (int i = 0; i < MAX_NIC; i++)
    {
        sAddr[i].addr = 0;
        sAddr[i].port = 0;

    }

    if (stunParseServerName(pcServer, stunServerAddr) != true)
    {
        return snt_type;
    }
    if (srcPort == 0)
    {
        //srcPort = stunRandomPort();
        srcPort = 25041;
    }

    if (numNic == 0)
    {
        numNic = 1;
    }

    for (int nic = 0; nic < numNic; nic++)
    {
        sAddr[nic].port = srcPort;
        if (stunServerAddr.addr == 0)
        {
            return snt_type;
        }

        bool presPort = false;
        bool hairpin = false;

        NatType stype = stunNatType(stunServerAddr, verbose, &presPort, &hairpin, srcPort, &sAddr[nic], &m_Socket1,
            &m_Socket2);

        LOG4CPLUS_INFO_LOG(logger_stun, "getNatType server:" << string(pcServer, 5));
        DebugLog("getNatType type:%d", (int)stype);
        switch (stype)
        {
        case StunTypeFailure:
            snt_type = protocol::TYPE_ERROR;
            LOG4CPLUS_INFO_LOG(logger_stun, "type=StunTypeFailure");
            break;
        case StunTypeUnknown:
            snt_type = protocol::TYPE_ERROR;
            LOG4CPLUS_INFO_LOG(logger_stun, "type=StunTypeUnknown");
            break;
        case StunTypeOpen:
            snt_type = protocol::TYPE_PUBLIC;
            LOG4CPLUS_INFO_LOG(logger_stun, "type=StunTypeOpen");
            break;
        case StunTypeFirewall:
            snt_type = protocol::TYPE_ERROR;
            LOG4CPLUS_INFO_LOG(logger_stun, "type=StunTypeFirewall");
            break;
        case StunTypeBlocked:
            snt_type = protocol::TYPE_ERROR;
            LOG4CPLUS_INFO_LOG(logger_stun, "type=StunTypeBlocked");
            break;

        case StunTypeConeNat:
#ifdef BOOST_WINDOWS_API
            WritePrivateProfileStringA(("PPVA"), ("NATTYPE"), ("protocol::TYPE_FULLCONENAT"), m_strConfig.c_str());
#endif
            snt_type = protocol::TYPE_FULLCONENAT;
            LOG4CPLUS_INFO_LOG(logger_stun, "type=StunTypeConeNat");
            break;

        case StunTypeRestrictedNat:
#ifdef BOOST_WINDOWS_API
            WritePrivateProfileStringA(("PPVA"), ("NATTYPE"), ("protocol::TYPE_IP_RESTRICTEDNAT"), m_strConfig.c_str());
#endif
            snt_type = protocol::TYPE_IP_RESTRICTEDNAT;
            LOG4CPLUS_INFO_LOG(logger_stun, "type=StunTypeRestrictedNat");
            break;

        case StunTypePortRestrictedNat:
#ifdef BOOST_WINDOWS_API
            WritePrivateProfileStringA(("PPVA"), ("NATTYPE"), ("protocol::TYPE_IP_PORT_RESTRICTEDNAT"), m_strConfig.c_str());
#endif
            snt_type = protocol::TYPE_IP_PORT_RESTRICTEDNAT;
            LOG4CPLUS_INFO_LOG(logger_stun, "type=StunTypePortRestrictedNat");
            break;

        case StunTypeSymNat:
#ifdef BOOST_WINDOWS_API
            WritePrivateProfileStringA(("PPVA"), ("NATTYPE"), ("protocol::TYPE_SYMNAT"), m_strConfig.c_str());
#endif
            snt_type = protocol::TYPE_SYMNAT;
            LOG4CPLUS_INFO_LOG(logger_stun, "type=StunTypeSymNat");
            break;

        default:
#ifdef BOOST_WINDOWS_API
            WritePrivateProfileStringA(("PPVA"), ("NATTYPE"), ("Unkown NAT type"), m_strConfig.c_str());
#endif
            snt_type = protocol::TYPE_ERROR;
            LOG4CPLUS_INFO_LOG(logger_stun, "type=TYPE_ERROR");
            break;
        }

    }  // end of for loop

    return snt_type;
}

/*
// 判断是否需要使用stun协议检测NAT信息，同时将保存的nat信息输出到snt_result
// 需要更新NAT信息则返回true；否则返回false
需要更新NAT的条件为：
1.保存的nattype为error
2.获取的本地ip地址与保存的地址不同
3.保存信息已经过期(超过3天)
*/
bool CStunClient::IsNeedToUpdateNat(protocol::MY_STUN_NAT_TYPE &snt_result, const string& config_path)
{
    if (config_path.length() == 0)
    {
        return false;
    }

#ifdef BOOST_WINDOWS_API
    boost::filesystem::path filepath(config_path);
    filepath /= ("ppvaconfig.ini");
    string filename = filepath.file_string();

    m_strConfig = filename;

    int LastTime = 0;
    uint32_t LastLocalIP = 0;

    snt_result = (protocol::MY_STUN_NAT_TYPE)GetPrivateProfileIntA(("PPVA"), ("NTYPE"), protocol::TYPE_ERROR, m_strConfig.c_str());
    if (snt_result < protocol::TYPE_FULLCONENAT || snt_result > protocol::TYPE_PUBLIC)
    {
        return true;
    }

    LastLocalIP = GetPrivateProfileIntA(("PPVA"), ("NIP"), -1, m_strConfig.c_str());
    if (LastLocalIP != GetLocalFirstIP())
    {
        return true;
    }

    LastTime = GetPrivateProfileIntA(("PPVA"), ("NTIME"), -1, m_strConfig.c_str());
    SYSTEMTIME st;
    GetLocalTime(&st);
    // 如果日期相差三天，则判断已经过期
    if (abs(st.wDay - LastTime) >= 3)
    {
        return true;
    }
    return false;
#else
    return true;
#endif

}

string g_strStunServer[] = { "stun.ekiga.net", "stun.fwdnet.net", "stun.ideasip.com", "stun01.sipphone.com",
    "stun.xten.com", "stunserver.org", "stun.sipgate.net", "211.152.45.105" };

protocol::MY_STUN_NAT_TYPE CStunClient::StartGetNatType(const string& config_path)
{

    protocol::MY_STUN_NAT_TYPE snt_ret = protocol::TYPE_ERROR;

    if (IsNeedToUpdateNat(snt_ret, config_path))
    {
        // 从设置的多个服务器中随机选择一个
        int serverCounts = sizeof(g_strStunServer) / sizeof(string);
        assert(serverCounts > 0);
        srand((unsigned) time(NULL));
        string strStunServer = g_strStunServer[rand() % serverCounts];

        snt_ret = getNatType((char*) strStunServer.c_str());
#ifdef BOOST_WINDOWS_API
        TCHAR str[64];
        // 保存获取到的NAT信息
#ifdef UNICODE
        wsprintf(str, L"%d", snt_ret);
#else
        sprintf(str, "%d", snt_ret);
#endif
        WritePrivateProfileStringA(("PPVA"), ("NTYPE"), str, m_strConfig.c_str());

        SYSTEMTIME st;
        GetLocalTime(&st);
        // 保存当天的时间
#ifdef UNICODE
        wsprintf(str, L"%d", st.wDay);
#else
        sprintf(str, "%d", st.wDay);
#endif
        WritePrivateProfileStringA(("PPVA"), ("NTIME"), str, m_strConfig.c_str());

        // 保存获取的本地IP
#ifdef UNICODE
        wsprintf(str, L"%u", GetLocalFirstIP());
#else
        sprintf(str, "%u", GetLocalFirstIP());
#endif
        WritePrivateProfileStringA(("PPVA"), ("NIP"), str, m_strConfig.c_str());
#else
        // write to config
#endif
    }

    return snt_ret;
}

#ifdef BOOST_WINDOWS_API

uint32_t CStunClient::GetLocalFirstIP(void)
{
    // 使用 ip helper函数
    boost::uint32_t nip = 0;
    PMIB_IPADDRTABLE pIPAddrTable;
    DWORD dwSize = 0 , dwRetVal;

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
        for (uint32_t i = 0; i < pIPAddrTable->dwNumEntries; i++)
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
}

#else

#include "framework/network/Interface.h"

uint32_t CStunClient::GetLocalFirstIP()
{
    std::vector<framework::network::Interface> interfaces;
    if (::framework::network::enum_interface(interfaces))
        return 0;

    uint32_t local_ip = 0;
    for (uint32_t i = 0; i < interfaces.size(); ++i) {
        framework::network::Interface const & inf = interfaces[i];
        if (string(inf.name) != "lo" && inf.up && inf.addr.is_v4()) {
            return inf.addr.to_v4().to_ulong();
        }
    }

    return 0;
}

#endif

}
