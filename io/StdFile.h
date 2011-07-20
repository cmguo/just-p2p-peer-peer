//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef FRAMEWORK_IO_STDFILE_H
#define FRAMEWORK_IO_STDFILE_H

#include <stdarg.h>

namespace io
{
    class StdFile: private boost::noncopyable
    {
    public:
        StdFile()
            : m_handle(NULL)
        {

        }

        ~StdFile()
        {
            Close();
        }

        bool IsOpen() const
        {
            return m_handle != NULL;
        }

        void Close();

        bool Open(const char* path, const char* mode = "r");

        uint32_t Read(void* buf, uint32_t size);

        uint32_t Write(const void* data, uint32_t size);

        bool Seek(long offset, int origin = SEEK_SET)
        {
            return 0 == fseek(m_handle, offset, origin);
        }

        bool Flush();

    protected:
        FILE* m_handle;
    };

    inline bool StdFile::Flush()
    {
        assert(IsOpen());
        if (!IsOpen())
        {
            return false;
        }
        return EOF != fflush(m_handle);
    }

    inline uint32_t StdFile::Write(const void* data, uint32_t size)
    {
        assert(data != NULL && size > 0);
        assert(IsOpen());
        if (!IsOpen())
        {
            return 0;
        }
        return fwrite(data, size, 1, m_handle) * size;
    }

    inline uint32_t StdFile::Read(void* buf, uint32_t size)
    {
        assert(buf != NULL && size > 0);
        assert(IsOpen());
        if (!IsOpen())
        {
            return 0;
        }
        return fread(buf, 1, size, m_handle);
    }

    inline void StdFile::Close()
    {
        if (IsOpen())
        {
            fclose(m_handle);
            m_handle = NULL;
        }
    }

    inline bool StdFile::Open(const char* path, const char* mode)
    {
        assert(!IsOpen());
        m_handle = fopen(path, mode);
        if (IsOpen())
        {
            return true;
        }
        return false;
    }
}// io

#endif  // FRAMEWORK_IO_STDFILE_H
