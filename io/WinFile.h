//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef FRAMEWOKR_IO_WINFILE_H
#define FRAMEWOKR_IO_WINFILE_H

#ifdef _WIN

#pragma once

#include <share.h>
#include <winbase.h>

#include "AccessControl.h"

namespace framework
{
    namespace io
    {
        class WinFile: private boost::noncopyable
        {
        public:
            WinFile() :
            m_handle(INVALID_HANDLE_VALUE), m_pos(0)
            {
            }
            ~WinFile()
            {
                Close();
            }

            HANDLE GetHandle() const
            {
                return m_handle;
            }

            bool IsOpen() const
            {
                return m_handle != INVALID_HANDLE_VALUE;
            }

            void Close();

            bool Open(const TCHAR* path, DWORD dwdesiredaccess, DWORD dwshare);

            bool Create(const TCHAR* path, DWORD dwdesiredaccess, DWORD dwshare);

            uint32_t FileLength();

            /// ??????
            uint32_t Read(void* buf, uint32_t size);

            /// д?????????
            uint32_t Write(const void* data, uint32_t size);

            /// д????????
            template<typename StructT>
            bool WriteStruct(const StructT& buffer)
            {
                uint32_t size = Write(&buffer, sizeof(StructT));
                return size == sizeof(StructT);
            }

            /// ??????????
            template<typename StructT>
            bool ReadStruct(StructT& buffer)
            {
                uint32_t size = Read(&buffer, sizeof(StructT));
                return size == sizeof(StructT);
            }

            bool Seek(long offset, int origin = SEEK_SET)
            {
                long real_off = ::SetFilePointer(m_handle, offset, NULL, origin);
                if (real_off == offset)
                {
                    m_pos += offset;
                    return true;
                }
                return false;
            }

            /// ????????
            bool Flush();

        protected:
            /// ??????
            HANDLE m_handle;
            LONG m_pos;
        private:
        };

        inline bool WinFile::Flush()
        {
            assert(IsOpen());
            if (!IsOpen())
            {
                return false;
            }
            return ::FlushFileBuffers(m_handle);
        }

        inline uint32_t WinFile::FileLength()
        {
            assert(IsOpen());
            if (!IsOpen())
            {
                return 0;
            }
            // DWORD size file
            return ::GetFileSize(m_handle, NULL);
        }

        inline uint32_t WinFile::Write(const void* data, uint32_t size)
        {
            assert(data != NULL && size > 0);
            assert(IsOpen());
            if (!IsOpen())
            {
                return 0;
            }
            ::SetFilePointer(m_handle, m_pos + size, NULL, FILE_BEGIN);
            if (FALSE == ::SetEndOfFile(m_handle))
            {
                return 0;
            }
            ::SetFilePointer(m_handle, m_pos, NULL, FILE_BEGIN);
            DWORD write_len = 0;
            ::WriteFile(m_handle, data, size, &write_len, NULL);
            m_pos += write_len;
            return write_len;
        }

        inline uint32_t WinFile::Read(void* buf, uint32_t size)
        {
            assert(buf != NULL && size > 0);
            assert(IsOpen());
            if (!IsOpen())
            {
                return 0;
            }
            ::SetFilePointer(m_handle, m_pos, NULL, FILE_BEGIN);
            DWORD readlen = 0;
            ::ReadFile(m_handle, buf, size, &readlen, NULL);
            m_pos += readlen;
            return readlen;
        }

        inline void WinFile::Close()
        {
            if (IsOpen())
            {
                ::CloseHandle(m_handle);
                m_handle = INVALID_HANDLE_VALUE;
                m_pos = 0;
            }
        }

        inline bool WinFile::Open(const TCHAR* path, DWORD dwdesiredaccess, DWORD dwshare)
        {
            assert(!IsOpen());
            m_handle = AccessControl::OpenAllAccessFile(const_cast<TCHAR*> (path), OPEN_EXISTING, dwdesiredaccess, dwshare);
            if (IsOpen())
            return true;
            return false;
        }

        inline bool WinFile::Create(const TCHAR* path, DWORD dwdesiredaccess, DWORD dwshare)
        {
            assert(!IsOpen());
            m_handle = AccessControl::CreateAllAccessFile(const_cast<TCHAR*> (path), CREATE_NEW, dwdesiredaccess, dwshare);
            if (IsOpen())
            return true;
            return false;
        }
    }
}
#endif  // _WIN
#endif  // FRAMEWOKR_IO_WINFILE_H
