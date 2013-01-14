//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------


#ifndef _STORAGE_C_HMAC_MD5_H_
#define _STORAGE_C_HMAC_MD5_H_

#include <framework/string/Md5.h>

namespace storage
{

    typedef union
    {
        boost::uint8_t    n[16];
        boost::uint8_t    b[16];
        boost::uint32_t    w[4];
    } SMD4, SMD5;

    static boost::uint8_t KEY_G_[] = {0x01, 0x02, 0x03, 0x04};

    class CHmacMD5
        : public framework::string::Md5
#ifdef DUMP_OBJECT
        , public count_object_allocate<CHmacMD5>
#endif
    {
#define DEFAULT_KEY_LEN 64

        // Construction
    public:
        virtual ~CHmacMD5();

    public:
        void            GetHash(SMD5 * pHash);
        void            GetHash(Guid & pHash);
    public:
        static string    HashToString(const SMD5* pHash, bool bURN = false);
        static bool        HashFromString(const char* pszHash, SMD5* pMD5);
        static bool        HashFromURN(const char* pszHash, SMD5* pMD5);

    public:
        CHmacMD5(u_char *key = KEY_G_ , boost::uint32_t keylen = sizeof(KEY_G_));

    private:
        boost::uint8_t    m_key[64];
        bool    b_reset_;

        // Operations
    public:
        virtual void    Reset();
        virtual void    Add(void const * pData, boost::uint32_t nLength);
        virtual void    Finish();

    };

}

#endif  // _STORAGE_C_HMAC_MD5_H_
