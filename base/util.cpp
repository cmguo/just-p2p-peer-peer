#include "Common.h"

#include "util.h"

#ifdef BOOST_WINDOWS_API
#include <atlbase.h>
#include <Shlobj.h>
#include <direct.h>
#endif


namespace base
{
    namespace util
    {
        bool GetAppDataPath(string & pszCachePath)
        {
#ifdef BOOST_WINDOWS_API
            char szCachePathT[MAX_PATH] = {0};
            // 通过注册表获取
            BOOL    bGetFromReg = FALSE;
            CRegKey key;

            if (key.Open(HKEY_LOCAL_MACHINE, _T("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders"), KEY_READ|KEY_QUERY_VALUE) == ERROR_SUCCESS)
            {
                DWORD    dwCount = MAX_PATH;
                LONG lRes = key.QueryValue(szCachePathT, _T("Common AppData"), &dwCount);
                if (lRes == ERROR_SUCCESS)
                {
                    bGetFromReg = TRUE;
                }
                key.Close();
            }

            if (!bGetFromReg)
            {
                // 通过SHELL获取
                HRESULT hr = SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, SHGFP_TYPE_DEFAULT, szCachePathT);
                if (FAILED(hr))
                {
                    // 通过环境变量获取"ALLUSERSPROFILE"
                    DWORD  dwRet = GetEnvironmentVariable(_T("ALLUSERSPROFILE"), szCachePathT, MAX_PATH);
                    if (dwRet)
                    {
                        PathAppend(szCachePathT, _T("Application Data"));
                    }
                    else
                    {    // 通过环境变量获取"ProgramFiles"
                        dwRet = GetEnvironmentVariable(_T("ProgramFiles"), szCachePathT, MAX_PATH);
                    }

                    if (dwRet == 0)
                        return FALSE;
                }
            }

            // 得到完整的CACHE路径
            PathAppend(szCachePathT, _T("PPLiveVA"));

            int retval = PathFileExists (szCachePathT);
            if (retval != 1)
            {
                _mkdir(szCachePathT);
            }
            pszCachePath.assign(szCachePathT);
            return TRUE;
#else
            return false;
#endif
        }
    }
}