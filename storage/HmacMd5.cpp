//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// HmacMd5.cpp

#include "Common.h"
#include "storage/HmacMd5.h"
#include "base/util.h"

#include <framework/string/Digest.hpp>

namespace storage
{
    void CHmacMD5::GetHash(Guid& pHash)
    {
        pHash.from_bytes(to_bytes());
    }

    void CHmacMD5::GetHash(SMD5* pHash)
    {
        Guid guid;
        GetHash(guid);
        HashFromString(guid.to_string().c_str(), pHash);
    }

    string CHmacMD5::HashToString(const SMD5* pHash, bool bURN)
    {
        char buf[255];
        sprintf(buf, bURN ? ("md5:%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x") : (
            "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x"), pHash->n[0], pHash->n[1], pHash->n[2],
            pHash->n[3], pHash->n[4], pHash->n[5], pHash->n[6], pHash->n[7], pHash->n[8], pHash->n[9], pHash->n[10],
            pHash->n[11], pHash->n[12], pHash->n[13], pHash->n[14], pHash->n[15]);
        string str(buf);
        return str;
    }

    //////////////////////////////////////////////////////////////////////
    // CMD5 parse from string

    bool CHmacMD5::HashFromString(const char* pszHash, SMD5* pMD5)
    {
        if (strlen(pszHash) < 32)
            return false;

        boost::uint8_t* pOut = (boost::uint8_t*) pMD5;

        for (int nPos = 16; nPos; nPos--, pOut++)
        {
            if (*pszHash >= '0' && *pszHash <= '9')
                *pOut = (*pszHash - '0') << 4;
            else if (*pszHash >= 'A' && *pszHash <= 'F')
                *pOut = (*pszHash - 'A' + 10) << 4;
            else if (*pszHash >= 'a' && *pszHash <= 'f')
                *pOut = (*pszHash - 'a' + 10) << 4;
            pszHash++;
            if (*pszHash >= '0' && *pszHash <= '9')
                *pOut |= (*pszHash - '0');
            else if (*pszHash >= 'A' && *pszHash <= 'F')
                *pOut |= (*pszHash - 'A' + 10);
            else if (*pszHash >= 'a' && *pszHash <= 'f')
                *pOut |= (*pszHash - 'a' + 10);
            pszHash++;
        }

        return true;
    }

    bool CHmacMD5::HashFromURN(const char* pszHash, SMD5* pMD5)
    {
        if (pszHash == NULL)
            return false;

        boost::uint32_t nLen = strlen(pszHash);

        if (nLen >= 8 + 32 && strncmp(pszHash, "urn:md5:", strlen("urn:md5:")) == 0)
        {
            return HashFromString(pszHash + 8, pMD5);
        }
        else if (nLen >= 4 + 32 && strncmp(pszHash, "md5:", strlen("md5:")) == 0)
        {
            return HashFromString(pszHash + 4, pMD5);
        }

        return false;
    }

    void CHmacMD5::Reset()
    {
        init();
        b_reset_ = true;
    }

    void CHmacMD5::Add(void const * pData, boost::uint32_t nLength)
    {
        if (nLength == 0)
            return;
        if (b_reset_) {
            update(m_key, sizeof(m_key));
            b_reset_ = false;
        }
        update((boost::uint8_t const *)pData, nLength);
    }

    void CHmacMD5::Finish()
    {
        update(m_key, sizeof(m_key));
        final();
    }

    CHmacMD5::CHmacMD5(
        u_char *key, boost::uint32_t keylen)
        : b_reset_(true)
    {
        framework::string::Md5 md5_;
        md5_.update(key, keylen);
        md5_.update(key, keylen);
        md5_.final();
        memset(m_key, 0, sizeof(m_key));
        base::util::memcpy2(m_key, sizeof(m_key), md5_.digest(), (std::min)((boost::uint32_t)sizeof(SMD5), (boost::uint32_t)DEFAULT_KEY_LEN));
    }

    CHmacMD5::~CHmacMD5()
    {
    }

}
