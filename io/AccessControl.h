//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef FRAMEWORK_IO_ACCESS_CONTROL_H
#define FRAMEWORK_IO_ACCESS_CONTROL_H

#include <sddl.h>

#define myheapalloc(x) (HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, x))
#define myheapfree(x) (HeapFree(GetProcessHeap(), 0, x))

namespace io
{
    typedef BOOL (WINAPI *SetSecurityDescriptorControlFnPtr)(
        IN PSECURITY_DESCRIPTOR pSecurityDescriptor,
        IN SECURITY_DESCRIPTOR_CONTROL ControlBitsOfInterest,
        IN SECURITY_DESCRIPTOR_CONTROL ControlBitsToSet);

    typedef BOOL (WINAPI *AddAccessAllowedAceExFnPtr)(
        PACL pAcl, DWORD dwAceRevision, DWORD AceFlags, DWORD AccessMask, PSID pSid);

    class AccessControl
    {
    public:

        static HANDLE CreateAccountFile(const TCHAR* lpszFileName, const TCHAR *lpszAccountName);

        static HANDLE CreateAllAccessFile(const TCHAR* lpszFileName, DWORD dwcreation,
            DWORD dwdesiredaccess, DWORD dwshare);

        static HANDLE OpenAllAccessFile(const TCHAR* lpszFileName, DWORD dwcreation,
            DWORD dwdesiredaccess, DWORD dwshare);

        static BOOL CreateAllAccessDir(const TCHAR* lpszDirPath, bool& add_succ);

        static BOOL CreateMyDACL(SECURITY_ATTRIBUTES * pSA);

        static BOOL AddAccessRights(const TCHAR *lpszFileName, const TCHAR *lpszAccountName, DWORD dwAccessMask);

    private:
    };
}

#endif  // FRAMEWORK_IO_ACCESS_CONTROL_H
