//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// StunClient.h

#ifndef _P2SP_STUN_STUN_CLIENT_H_
#define _P2SP_STUN_STUN_CLIENT_H_

#include "p2sp/stun/udp.h"
#include "p2sp/stun/stun.h"
#include "struct/Base.h"

namespace p2sp
{
    // enum protocol::MY_STUN_NAT_TYPE
    // {
    // TYPE_ERROR = -1,
    // TYPE_FULLCONENAT = 0,
    // TYPE_IP_RESTRICTEDNAT,
    // TYPE_IP_PORT_RESTRICTEDNAT,
    // TYPE_SYMNAT,
    // TYPE_PUBLIC
    // };

    // enum protocol::MY_STUN_NAT_TYPE
    // {
    // TYPE_ERROR = -1,
    // TYPE_FULLCONENAT = ENU_NT_FULL_CONE_NAT,  // !
    // TYPE_IP_RESTRICTEDNAT,
    // TYPE_IP_PORT_RESTRICTEDNAT,
    // TYPE_SYMNAT,
    // TYPE_PUBLIC  // !
    // };

    class CStunClient
#ifdef DUMP_OBJECT
        : public count_object_allocate<CStunClient>
#endif
    {
    public:
        CStunClient();

    public:
        // 获取nat的类型

        protocol::MY_STUN_NAT_TYPE StartGetNatTpye(const string& config_path);
        void Stop();

    public:

        static uint32_t GetLocalFirstIP(void);

    private:
        protocol::MY_STUN_NAT_TYPE getNatType(char *pcServer);
        // 判断是否需要使用stun协议检测NAT信息，同时将保存的nat信息输出到参数中
        bool IsNeedToUpdateNat(protocol::MY_STUN_NAT_TYPE &snt_result, const string& config_path);
        bool GetPPLiveAppDataPath(string& pszCachePath);

    private:
        Socket m_Socket1;
        Socket m_Socket2;
        string m_strConfig;
    };

}

#endif  // _P2SP_STUN_STUN_CLIENT_H_
