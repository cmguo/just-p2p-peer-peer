//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifdef BOOST_WINDOWS_API

#include "Common.h"
#include "io/AccessControl.h"
#pragma  comment(lib, "shlwapi.lib")
#include <shlwapi.h>

namespace io
{
    HANDLE AccessControl::CreateAccountFile(const TCHAR* lpszFileName, const TCHAR *lpszAccountName)
    {
        SECURITY_ATTRIBUTES sa;
        SECURITY_DESCRIPTOR sd;
        BYTE aclBuffer[1024];
        PACL pacl = (PACL) & aclBuffer;
        BYTE sidBuffer[100];
        PSID psid = (PSID) & sidBuffer;
        DWORD sidBufferSize = 100;
        TCHAR domainBuffer[80];
        DWORD domainBufferSize = 80;
        SID_NAME_USE snu;

        ::InitializeSecurityDescriptor(&sd, SECURITY_DESCRIPTOR_REVISION);
        ::InitializeAcl(pacl, 1024, ACL_REVISION);
        ::LookupAccountName(0, lpszAccountName, psid, &sidBufferSize, domainBuffer, &domainBufferSize, &snu);
        ::AddAccessAllowedAce(pacl, ACL_REVISION, GENERIC_ALL, psid);
        ::SetSecurityDescriptorDacl(&sd, TRUE, pacl, FALSE);
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = FALSE;
        sa.lpSecurityDescriptor = &sd;

        return ::CreateFile(lpszFileName, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, NULL);
    }

    HANDLE AccessControl::CreateAllAccessFile(const TCHAR* lpszFileName, DWORD dwcreation, DWORD dwdesiredaccess,
        DWORD dwshare)
    {
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = FALSE;
        if (::PathFileExists(lpszFileName))
        {
            if (FALSE == ::DeleteFile(lpszFileName))
            {
                return INVALID_HANDLE_VALUE;
            }
        }
        CreateMyDACL(&sa);
        HANDLE hFile = ::CreateFile(lpszFileName, dwdesiredaccess, dwshare, &sa, dwcreation, FILE_ATTRIBUTE_NORMAL
            | FILE_FLAG_WRITE_THROUGH, NULL);
        if (hFile == INVALID_HANDLE_VALUE)
        {
            // LOG(__DEBUG, "bug", __FUNCTION__ << " FileName = " << w2b(tstring(lpszFileName)) << " Fail!! Erro = " << GetLastError());
        }
        else
        {
            // LOG(__DEBUG, "bug", __FUNCTION__ << " FileName = " << w2b(tstring(lpszFileName)) << " OK!!");
        }
        return hFile;
    }

    HANDLE AccessControl::OpenAllAccessFile(const TCHAR* lpszFileName, DWORD dwcreation, DWORD dwdesiredaccess,
        DWORD dwshare)
    {
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = FALSE;
        HANDLE ret = ::CreateFile(lpszFileName, dwdesiredaccess, dwshare, NULL, dwcreation, FILE_ATTRIBUTE_NORMAL
            | FILE_FLAG_WRITE_THROUGH, NULL);
        if (AddAccessRights(lpszFileName, TEXT("Everyone"), GENERIC_ALL))
        {
            return ret;
        }
        return INVALID_HANDLE_VALUE;
    }

    BOOL AccessControl::CreateAllAccessDir(const TCHAR* lpszDirPath, bool& add_succ)
    {
        if (NULL == lpszDirPath)
        {
            return FALSE;
        }
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = FALSE;
        if (::PathFileExists(lpszDirPath))
        {
            add_succ = AddAccessRights(lpszDirPath, TEXT("Everyone"), GENERIC_ALL);
            return TRUE;
        }
        CreateMyDACL(&sa);
        return ::CreateDirectory(lpszDirPath, &sa);
    }

    BOOL AccessControl::CreateMyDACL(SECURITY_ATTRIBUTES * pSA)
    {
        // Define the SDDL for the DACL. This example sets
        // the following access:
        //     Built-in guests are denied all access.
        //     Anonymous logon is denied all access.
        //     Authenticated users are allowed
        //     read/write/execute access.
        //     Administrators are allowed full control.
        // Modify these values as needed to generate the proper
        // DACL for your application.
        TCHAR * szSD = TEXT("D:")  // Discretionary ACL
            // TEXT("(A;OICI;GA;;;BG)")     // Deny access to
            // built-in guests
            // TEXT("(D;OICI;GA;;;AN)")     // Deny access to
            // anonymous logon
            // TEXT("(A;OICI;GRGWGX;;;AU)")  // Allow
            // TEXT("(A;OICI;GA;;;AU)")
            // TEXT("(A;OICI;GA;;;BU)")  // users
            TEXT("(A;OICI;GA;;;WD)")  // everyone
            // read/write/execute
            // to authenticated
            // users
            // TEXT("(A;OICI;GA;;;BA)")    // Allow full control
           ;
        // to administrators

        if (NULL == pSA)
            return FALSE;

        return ConvertStringSecurityDescriptorToSecurityDescriptor(szSD, SDDL_REVISION_1, &(pSA->lpSecurityDescriptor),
            NULL);
    }

    BOOL AccessControl::AddAccessRights(const TCHAR *lpszFileName, const TCHAR *lpszAccountName, DWORD dwAccessMask)
    {
        SID_NAME_USE snuType;
        TCHAR * szDomain = NULL;
        DWORD cbDomain = 0;
        LPVOID pUserSID = NULL;
        DWORD cbUserSID = 0;
        PSECURITY_DESCRIPTOR pFileSD = NULL;
        DWORD cbFileSD = 0;
        SECURITY_DESCRIPTOR newSD;
        PACL pACL = NULL;
        BOOL fDaclPresent;
        BOOL fDaclDefaulted;
        ACL_SIZE_INFORMATION AclInfo;
        PACL pNewACL = NULL;
        DWORD cbNewACL = 0;
        LPVOID pTempAce = NULL;
        boost::uint32_t CurrentAceIndex = 0;
        boost::uint32_t newAceIndex = 0;
        BOOL fResult = FALSE;
        BOOL fAPISuccess;
        SECURITY_INFORMATION secInfo = DACL_SECURITY_INFORMATION;
        SetSecurityDescriptorControlFnPtr _SetSecurityDescriptorControl = NULL;
        AddAccessAllowedAceExFnPtr _AddAccessAllowedAceEx = NULL;
        __try
        {
            //
            //
            fAPISuccess = LookupAccountName(NULL, lpszAccountName,
                pUserSID, &cbUserSID, szDomain, &cbDomain, &snuType);
            if (fAPISuccess)
                __leave;
            else if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            {
                //                    _tprintf(TEXT("LookupAccountName() failed. Error %d\n"),
                //                        GetLastError());
                __leave;
            }
            pUserSID = myheapalloc(cbUserSID);
            if (!pUserSID)
            {
                //                    _tprintf(TEXT("HeapAlloc() failed. Error %d\n"), GetLastError());
                __leave;
            }
            szDomain = (TCHAR *) myheapalloc(cbDomain * sizeof(TCHAR));
            if (!szDomain)
            {
                //                    _tprintf(TEXT("HeapAlloc() failed. Error %d\n"), GetLastError());
                __leave;
            }
            fAPISuccess = LookupAccountName(NULL, lpszAccountName,
                pUserSID, &cbUserSID, szDomain, &cbDomain, &snuType);
            if (!fAPISuccess)
            {
                //                    _tprintf(TEXT("LookupAccountName() failed. Error %d\n"),
                //    GetLastError());
                __leave;
            }
            //
            fAPISuccess = GetFileSecurity(lpszFileName,
                secInfo, pFileSD, 0, &cbFileSD);
            if (fAPISuccess)
                __leave;
            else if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
            {
                //                    _tprintf(TEXT("GetFileSecurity() failed. Error %d\n"),
                // GetLastError());
                __leave;
            }
            pFileSD = myheapalloc(cbFileSD);
            if (!pFileSD)
            {
                //                    _tprintf(TEXT("HeapAlloc() failed. Error %d\n"), GetLastError());
                __leave;
            }
            fAPISuccess = GetFileSecurity(lpszFileName,
                secInfo, pFileSD, cbFileSD, &cbFileSD);
            if (!fAPISuccess)
            {
                //                    _tprintf(TEXT("GetFileSecurity() failed. Error %d\n"),
                // GetLastError());
                __leave;
            }
            //
            //
            if (!InitializeSecurityDescriptor(&newSD,
                SECURITY_DESCRIPTOR_REVISION))
            {
                // _tprintf(TEXT("InitializeSecurityDescriptor() failed.")
                //    TEXT("Error %d\n"), GetLastError());
                __leave;
            }
            //
            //
            if (!GetSecurityDescriptorDacl(pFileSD, &fDaclPresent, &pACL,
                &fDaclDefaulted))
            {
                // _tprintf(TEXT("GetSecurityDescriptorDacl() failed. Error %d\n"),
                //    GetLastError());
                __leave;
            }
            //
            AclInfo.AceCount = 0;  // Assume NULL DACL.
            AclInfo.AclBytesFree = 0;
            AclInfo.AclBytesInUse = sizeof(ACL);
            if (pACL == NULL)
                fDaclPresent = FALSE;
            if (fDaclPresent)
            {
                if (!GetAclInformation(pACL, &AclInfo,
                    sizeof(ACL_SIZE_INFORMATION), AclSizeInformation))
                {
                    // _tprintf(TEXT("GetAclInformation() failed. Error %d\n"),
                    //    GetLastError());
                    __leave;
                }
            }
            //
            cbNewACL = AclInfo.AclBytesInUse + sizeof(ACCESS_ALLOWED_ACE)
                + GetLengthSid(pUserSID) - sizeof(DWORD);
            //
            //
            pNewACL = (PACL) myheapalloc(cbNewACL);
            if (!pNewACL)
            {
                //                    // _tprintf(TEXT("HeapAlloc() failed. Error %d\n"), GetLastError());
                __leave;
            }
            //
            //
            if (!InitializeAcl(pNewACL, cbNewACL, ACL_REVISION2))
            {
                //                    _tprintf(TEXT("InitializeAcl() failed. Error %d\n"),
                // GetLastError());
                __leave;
            }
            //
            //
            newAceIndex = 0;
            if (fDaclPresent && AclInfo.AceCount)
            {
                for (CurrentAceIndex = 0;
                    CurrentAceIndex < AclInfo.AceCount;
                    CurrentAceIndex++)
                {
                    //
                    //
                    if (!GetAce(pACL, CurrentAceIndex, &pTempAce))
                    {
                        //                            _tprintf(TEXT("GetAce() failed. Error %d\n"),
                        //    GetLastError());
                        __leave;
                    }
                    //
                    //
                    if (((ACCESS_ALLOWED_ACE *)pTempAce)->Header.AceFlags
                        & INHERITED_ACE)
                        break;
                    //
                    //
                    if (EqualSid(pUserSID,
                        &(((ACCESS_ALLOWED_ACE *)pTempAce)->SidStart)))
                        continue;
                    //
                    //
                    if (!AddAce(pNewACL, ACL_REVISION, MAXDWORD, pTempAce,
                        ((PACE_HEADER) pTempAce)->AceSize))
                    {
                        // _tprintf(TEXT("AddAce() failed. Error %d\n"),
                        //    GetLastError());
                        __leave;
                    }
                    newAceIndex++;
                }
            }
            //
            //
            _AddAccessAllowedAceEx = (AddAccessAllowedAceExFnPtr)
                GetProcAddress(GetModuleHandle(TEXT("advapi32.dll")),
                "AddAccessAllowedAceEx");
            if (_AddAccessAllowedAceEx)
            {
                if (!_AddAccessAllowedAceEx(pNewACL, ACL_REVISION2,
                    CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE ,
                    dwAccessMask, pUserSID))
                {
                    // _tprintf(TEXT("AddAccessAllowedAceEx() failed. Error %d\n"),
                    // GetLastError());
                    __leave;
                }
            }
            else
            {
                if (!AddAccessAllowedAce(pNewACL, ACL_REVISION2,
                    dwAccessMask, pUserSID))
                {
                    // _tprintf(TEXT("AddAccessAllowedAce() failed. Error %d\n"),
                    // GetLastError());
                    __leave;
                }
            }
            //
            //
            if (fDaclPresent && AclInfo.AceCount)
            {
                for (;
                    CurrentAceIndex < AclInfo.AceCount;
                    CurrentAceIndex++)
                {
                    //
                    //
                    if (!GetAce(pACL, CurrentAceIndex, &pTempAce))
                    {
                        // _tprintf(TEXT("GetAce() failed. Error %d\n"),
                        // GetLastError());
                        __leave;
                    }
                    //
                    //
                    if (!AddAce(pNewACL, ACL_REVISION, MAXDWORD, pTempAce,
                        ((PACE_HEADER) pTempAce)->AceSize))
                    {
                        // _tprintf(TEXT("AddAce() failed. Error %d\n"),
                        // GetLastError());
                        __leave;
                    }
                }
            }
            //
            //
            if (!SetSecurityDescriptorDacl(&newSD, TRUE, pNewACL,
                FALSE))
            {
                // _tprintf(TEXT("SetSecurityDescriptorDacl() failed. Error %d\n"),
                // GetLastError());
                __leave;
            }
            //
            //
            _SetSecurityDescriptorControl = (SetSecurityDescriptorControlFnPtr)
                GetProcAddress(GetModuleHandle(TEXT("advapi32.dll")),
                "SetSecurityDescriptorControl");
            if (_SetSecurityDescriptorControl)
            {
                SECURITY_DESCRIPTOR_CONTROL controlBitsOfInterest = 0;
                SECURITY_DESCRIPTOR_CONTROL controlBitsToSet = 0;
                SECURITY_DESCRIPTOR_CONTROL oldControlBits = 0;
                DWORD dwRevision = 0;
                if (!GetSecurityDescriptorControl(pFileSD, &oldControlBits,
                    &dwRevision))
                {
                    // _tprintf(TEXT("GetSecurityDescriptorControl() failed.")
                    // TEXT("Error %d\n"), GetLastError());
                    __leave;
                }
                if (oldControlBits & SE_DACL_AUTO_INHERITED)
                {
                    controlBitsOfInterest =
                        SE_DACL_AUTO_INHERIT_REQ |
                        SE_DACL_AUTO_INHERITED;
                    controlBitsToSet = controlBitsOfInterest;
                }
                else if (oldControlBits & SE_DACL_PROTECTED)
                {
                    controlBitsOfInterest = SE_DACL_PROTECTED;
                    controlBitsToSet = controlBitsOfInterest;
                }
                if (controlBitsOfInterest)
                {
                    if (!_SetSecurityDescriptorControl(&newSD,
                        controlBitsOfInterest,
                        controlBitsToSet))
                    {
                        // _tprintf(TEXT("SetSecurityDescriptorControl() failed.")
                        // TEXT("Error %d\n"), GetLastError());
                        __leave;
                    }
                }
            }
            //
            //
            if (!SetFileSecurity(lpszFileName, secInfo,
                &newSD))
            {
                // _tprintf(TEXT("SetFileSecurity() failed. Error %d\n"),
                // GetLastError());
                __leave;
            }
            fResult = TRUE;
        }__finally
        {
            //
            //
            //
            if (pUserSID) myheapfree(pUserSID);
            if (szDomain) myheapfree(szDomain);
            if (pFileSD) myheapfree(pFileSD);
            if (pNewACL) myheapfree(pNewACL);
        }
        return fResult;
    }
}

#endif  // BOOST_WINDOWS_API
