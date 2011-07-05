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
#include <ShlObj.h>
#include <atlbase.h>

// #include <shlwapi.h>
// #pragma comment(lib. "shlwapi.lib")
#include <Iphlpapi.h>
#pragma comment(lib, "Iphlpapi.lib")
#endif

#define STUN_INFO(s) LOG(__INFO, "stun", s)
FRAMEWORK_LOGGER_DECLARE_MODULE("stun");

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
    protocol::MY_STUN_NAT_TYPE snt_tpye = protocol::TYPE_ERROR;

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
        return snt_tpye;
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

            return snt_tpye;
        }

        bool presPort = false;
        bool hairpin = false;

        NatType stype = stunNatType(stunServerAddr, verbose, &presPort, &hairpin, srcPort, &sAddr[nic], &m_Socket1,
            &m_Socket2);

        STUN_INFO("getNatType server:" << string(pcServer, 5));
        DebugLog("getNatType type:%d", (int)stype);
        switch (stype)
        {
        case StunTypeFailure:
            snt_tpye = protocol::TYPE_ERROR;
            STUN_INFO("type=StunTypeFailure");
            break;
        case StunTypeUnknown:
            snt_tpye = protocol::TYPE_ERROR;
            STUN_INFO("type=StunTypeUnknown");
            break;
        case StunTypeOpen:
            snt_tpye = protocol::TYPE_PUBLIC;
            STUN_INFO("type=StunTypeOpen");
            break;
        case StunTypeFirewall:
            snt_tpye = protocol::TYPE_ERROR;
            STUN_INFO("type=StunTypeFirewall");
            break;
        case StunTypeBlocked:
            snt_tpye = protocol::TYPE_ERROR;
            STUN_INFO("type=StunTypeBlocked");
            break;

        case StunTypeConeNat:
#ifdef BOOST_WINDOWS_API
            WritePrivateProfileStringA(("PPVA"), ("NATTYPE"), ("protocol::TYPE_FULLCONENAT"), m_strConfig.c_str());
#endif
            snt_tpye = protocol::TYPE_FULLCONENAT;
            STUN_INFO("type=StunTypeConeNat");
            break;

        case StunTypeRestrictedNat:
#ifdef BOOST_WINDOWS_API
            WritePrivateProfileStringA(("PPVA"), ("NATTYPE"), ("protocol::TYPE_IP_RESTRICTEDNAT"), m_strConfig.c_str());
#endif
            snt_tpye = protocol::TYPE_IP_RESTRICTEDNAT;
            STUN_INFO("type=StunTypeRestrictedNat");
            break;

        case StunTypePortRestrictedNat:
#ifdef BOOST_WINDOWS_API
            WritePrivateProfileStringA(("PPVA"), ("NATTYPE"), ("protocol::TYPE_IP_PORT_RESTRICTEDNAT"), m_strConfig.c_str());
#endif
            snt_tpye = protocol::TYPE_IP_PORT_RESTRICTEDNAT;
            STUN_INFO("type=StunTypePortRestrictedNat");
            break;

        case StunTypeSymNat:
#ifdef BOOST_WINDOWS_API
            WritePrivateProfileStringA(("PPVA"), ("NATTYPE"), ("protocol::TYPE_SYMNAT"), m_strConfig.c_str());
#endif
            snt_tpye = protocol::TYPE_SYMNAT;
            STUN_INFO("type=StunTypeSymNat");
            break;

        default:
#ifdef BOOST_WINDOWS_API
            WritePrivateProfileStringA(("PPVA"), ("NATTYPE"), ("Unkown NAT type"), m_strConfig.c_str());
#endif
            snt_tpye = protocol::TYPE_ERROR;
            STUN_INFO("type=TYPE_ERROR");
            break;
        }

    }  // end of for loop


    return snt_tpye;
}
/*
bool CStunClient::GetPPLiveAppDataPath(string& pszCachePath)
{
#ifdef BOOST_WINDOWS_API
    // 通过注册表获取
    bool bGetFromReg = FALSE;
    CRegKey key;
    if (key.Open(HKEY_LOCAL_MACHINE, ("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders"), KEY_READ|KEY_QUERY_VALUE) == ERROR_SUCCESS)
    {
        uint32_t dwCount = MAX_PATH;
        LONG lRes = key.QueryValue(pszCachePath.c_str(), ("Common AppData"), &dwCount);
        if (lRes == ERROR_SUCCESS)
        {
            bGetFromReg = TRUE;
        }
        key.Close();
    }

    if (!bGetFromReg)
    {
        // 通过SHELL获取
        HRESULT hr = SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, SHGFP_TYPE_DEFAULT, pszCachePath);
        if (FAILED(hr))
        {
            // 通过环境变量获取"ALLUSERSPROFILE"
            boost::uint32_t dwRet = GetEnvironmentVariable(("ALLUSERSPROFILE"), pszCachePath, MAX_PATH);
            if (dwRet)
            {
                PathAppend(pszCachePath, ("Application Data"));
            }
            else
            {
                // 通过环境变量获取"ProgramFiles"
                dwRet = GetEnvironmentVariable(("ProgramFiles"), pszCachePath, MAX_PATH);
            }

            if (dwRet == 0)
            return FALSE;
        }
    }

    // 得到完整的CACHE路径
    PathAppend(pszCachePath, ("PPLiveVA"));

    int retval = PathFileExists (pszCachePath);
    if (retval != 1)
    {
        _wmkdir(pszCachePath);
    }
    return TRUE;
#else
    return false;
#endif
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

protocol::MY_STUN_NAT_TYPE CStunClient::StartGetNatTpye(const string& config_path)
{

    protocol::MY_STUN_NAT_TYPE snt_ret = protocol::TYPE_ERROR;

    if (IsNeedToUpdateNat(snt_ret, config_path))
    {
        // 默认的stunserver服务器地址
        string strStunServer = "211.152.45.105";

        // 从设置的多个服务器中随机选择一个
        int ServerCounts = sizeof(g_strStunServer) / sizeof(string);
        srand((unsigned) time(NULL));
        if (ServerCounts > 0)
        {
            strStunServer = g_strStunServer[rand() % ServerCounts];
        }

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
    uint32_t dwSize = 0 , dwRetVal;

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
