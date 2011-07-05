//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef FRAMEWORK_IO_STDFILE_H
#define FRAMEWORK_IO_STDFILE_H

#ifdef BOOST_WINDOWS_API
#include "io/AccessControl.h"
#endif  // BOOST_WINDOWS_API

#include <stdarg.h>

namespace io
{

    class StdFile: private boost::noncopyable
    {
    public:
        StdFile() :
          m_handle(NULL)
          {
          }
          ~StdFile()
          {
              Close();
          }

          FILE* GetHandle() const
          {
              return m_handle;
          }

          bool IsOpen() const
          {
              return m_handle != NULL;
          }

          void Close();

          bool IsEOF() const;

          bool Open(const char* path, const char* mode = "r");

          int ReadByte()
          {
              return fgetc(m_handle);
          }

          uint32_t Read(void* buf, uint32_t size);

          bool ReadLine(char* buf, int size);

          bool ReadLine(string& line);

#ifdef _WIN
          bool ReadLine(wchar_t* buf, int size);
          bool ReadLine(wstring& line);
#endif

          uint32_t Write(const void* data, uint32_t size);

          bool Write(const char* str);

          int WriteV(const char* format, va_list argptr);

          int WriteF(const char* format, ...);

          template<typename StructT>
          bool WriteStruct(const StructT& buffer)
          {
              uint32_t size = Write(&buffer, sizeof(StructT));
              return size == sizeof(StructT);
          }

          template<typename StructT>
          bool ReadStruct(StructT& buffer)
          {
              uint32_t size = Read(&buffer, sizeof(StructT));
              return size == sizeof(StructT);
          }

          bool Seek(long offset, int origin = SEEK_SET)
          {
              return 0 == fseek(m_handle, offset, origin);
          }

          bool Flush();

          bool IsFailed();

    protected:
        FILE* m_handle;
    };

    class StdFileReader: public StdFile
    {
    public:
        bool OpenBinary(const char* path);

        bool OpenText(const char* path);
    };

    class StdFileWriter: public StdFile
    {
    public:
        StdFileWriter()
        {
        }
        ~StdFileWriter();

        void Close();

        bool OpenBinary(const char* path);

        bool OpenText(const char* path);

        static bool WriteBinary(const char* path, const void* data, uint32_t size);
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

    inline bool StdFile::IsFailed()
    {
        assert(IsOpen());
        return ferror(m_handle) != 0;
    }
    inline int StdFile::WriteV(const char* format, va_list argptr)
    {
        assert(format != NULL);
        assert(IsOpen());
        if (!IsOpen())
        {
            return 0;
        }
        return vfprintf(m_handle, format, argptr);
    }
    inline bool StdFile::Write(const char* str)
    {
        assert(str != NULL);
        assert(IsOpen());
        if (!IsOpen())
        {
            return false;
        }
        return EOF != fputs(str, m_handle);
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
    inline bool StdFile::IsEOF() const
    {
        return 0 != feof(m_handle);
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
#ifdef _WIN
            framework::io::AccessControl::AddAccessRights(path, TEXT("Everyone"), GENERIC_ALL);
#endif
            return true;
        }
        return false;
    }

    inline bool StdFile::ReadLine(char* buf, int size)
    {
        assert(buf != NULL && size > 0);
        assert(IsOpen());
        if (!IsOpen())
            return false;
        if (fgets(buf, size, m_handle) != NULL)
            return true;
        return false;
    }

    inline bool StdFile::ReadLine(string& line)
    {
        assert(IsOpen());
        if (!IsOpen())
            return false;
        const uint32_t max_size = 1025;
        line.clear();
        while (true)
        {
            char str[max_size] = { 0 };
            if (fgets(str, max_size, m_handle) == NULL)
            {
                return false;
            }
            string tmp_line = str;
            line += tmp_line;
            if (tmp_line.size() < 1024 || (tmp_line.size() == 1024 && tmp_line[tmp_line.size() - 1] == '\n'))
            {
                break;
            }
        }
        return true;
    }
#ifdef _WIN
    inline bool StdFile::ReadLine(wchar_t* buf, int size)
    {
        assert(buf != NULL && size > 0);
        assert(IsOpen());
        if (!IsOpen())
            return false;
        if (fgetws(buf, size, m_handle) != NULL)
            return true;
        if (IsFailed())
        {
            UTIL_DEBUG("fgets failed ");
        }
        return false;
    }

    inline bool StdFile::ReadLine(wstring& line)
    {
        assert(IsOpen());
        if (!IsOpen())
            return false;
        const uint32_t max_size = 1024;
        wchar_t str[max_size + 1] =
        {   0};
        if (fgetws(str, max_size, m_handle) == NULL)
        {
            if (IsFailed())
            {
                UTIL_DEBUG("fgets failed");
            }
            return false;
        }
        line = str;
        return true;
    }
#endif

    inline int StdFile::WriteF(const char* format, ...)
    {
        assert(format != NULL);
        va_list(args);
        va_start(args, format);
        int count = WriteV(format, args);
        va_end(args);
        return count;
    }

    inline bool StdFileReader::OpenBinary(const char* path)
    {
        return this->Open(path, ("rb"));
    }

    inline bool StdFileReader::OpenText(const char* path)
    {
        return this->Open(path, ("r"));
    }

    inline bool StdFileWriter::OpenBinary(const char* path)
    {
        return this->Open(path, ("wb"));
    }

    inline bool StdFileWriter::OpenText(const char* path)
    {
        return this->Open(path, ("w"));
    }

    inline StdFileWriter::~StdFileWriter()
    {
        Close();
    }

    inline void StdFileWriter::Close()
    {
        if (IsOpen())
        {
            Flush();
            StdFile::Close();
        }
    }

    inline bool StdFileWriter::WriteBinary(const char* path, const void* data, uint32_t size)
    {
        StdFileWriter file;
        if (!file.OpenBinary(path))
            return false;
        if (file.Write(data, size) != size)
            return false;
        file.Flush();
        return true;
    }

}// io

#endif  // FRAMEWORK_IO_STDFILE_H
