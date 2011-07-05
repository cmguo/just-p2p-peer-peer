//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "storage/CfgFile.h"
#include "storage/Storage.h"
#include "random.h"

#include <boost/scoped_array.hpp>

#ifdef BOOST_WINDOWS_API
#include <io.h>
#else
#include <sys/stat.h>
#endif

#ifdef DISK_MODE
#include "storage/FileResourceInfo.h"
#include "base/wsconvert.h"
#include "base/util.h"

namespace storage
{
    FRAMEWORK_LOGGER_DECLARE_MODULE("storage");
    bool SecFile::SecCreate(const char* path)
    {
        crash_b_ = false;
        only_read = false;
        sec_content_count_ = 0;
        md5_.Reset();
        if (!StdFile::Open(path, ("wb")))
        {
            crash_b_ = true;
            return false;
        }

        head_.verison_ = VersionNow();
        boost::uint64_t st = Random::GetGlobal().Next();
        st = st << 32;
        st += Random::GetGlobal().Next();

        head_.time_stamp_ = st;

        if (!SecWrite((boost::uint8_t*) &head_, sizeof(head_)))
        {
            crash_b_ = true;
            return false;
        }

        return true;
    }

    bool SecFile::SecOpen(const char* path)
    {
        crash_b_ = true;
        md5_.Reset();
        sec_content_count_ = 0;
        if (!StdFile::Open(path, ("rb")))
        {
            return false;
        }
#ifdef BOOST_WINDOWS_API
        uint32_t low_size = _filelength(_fileno(m_handle));
        if (low_size == INVALID_FILE_SIZE)
        {
            return false;
        }
#else
        struct stat status;
        int result = fstat(fileno(m_handle), &status);
        uint32_t low_size = status.st_size;
        if (result != 0)
        {
            return false;
        }
#endif  // BOOST_WINDOWS_API
        if (low_size > max_sec_file_size)
        {
            return false;
        }

        crash_b_ = false;
        if (!DoVerify())
        {
            crash_b_ = true;
            Close();
            return false;
        }
        only_read = true;
        sec_content_count_ = low_size;
        crash_b_ = false;
        return true;
    }

    bool SecFile::DoVerify()
    {
        assert(!(crash_b_));
        char content[1024 * 64];

        if (!IsOpen())
        {
            return false;
        }
        uint32_t readnum = Read(&head_, sizeof(SecFileHead));
        if (readnum != sizeof(SecFileHead))
        {
            return false;
        }

        if (false == CheckVersion(head_.verison_))
        {
            STORAGE_ERR_LOG("CheckVersion failed!");
            return false;
        }

        // 2011.1.5 modified by jeffrey：采用写入校验的DoSign函数中的算法来进行读取的时候校验
        // 与2.0.1.584兼容，与1554不兼容
        MD5 file_hash = head_.sec_md5_;
        head_.sec_md5_.clear();

        md5_.Add((char*)&head_, sizeof(head_));

        while (readnum = Read(content, sizeof(content)))
        {
            md5_.Add(content, readnum);
        }

        md5_.Finish();

        SMD5 hash;
        md5_.GetHash(&hash);

        if (memcmp((void*)&hash, (void*)&(file_hash), sizeof(hash)))
        {
            return false;
        }

        Seek(sizeof(head_), SEEK_SET);
        return true;
    }

    bool SecFile::DoSign()
    {
        assert(!only_read);
        assert(!crash_b_);
        assert(head_.sec_md5_.is_empty());

        md5_.Finish();

        SMD5 hash;
        md5_.GetHash(&hash);

        Seek(SecFileHead::md5_offset(), SEEK_SET);

        uint32_t wlen = Write((char*) &hash, sizeof(MD5));

        if (wlen != sizeof(MD5))
        {
            return false;
        }
        return true;
    }

    void SecFile::SecClose()
    {
        if (!IsOpen())
            return;

        StdFile::Flush();
        if (!crash_b_)
        {
#ifdef BOOST_WINDOWS_API
            uint32_t low_size = _filelength(_fileno(m_handle));
#else
            struct stat status;
            fstat(fileno(m_handle), &status);
            uint32_t low_size = status.st_size;
#endif
            assert(low_size == sec_content_count_);
            if (!only_read)
            {
                if (!DoSign())
                {
                    STORAGE_ERR_LOG("SecFile::SecClose error: DoSign()");
                }
            }
        }
        Close();
        return;
    }

    bool SecFile::SecWrite(const boost::uint8_t *buf, uint32_t buflen)
    {
        if (crash_b_)
        {
            assert(false);
            return false;
        }
        assert(!only_read);
        if (sec_content_count_ + buflen >= max_sec_file_size)
        {
            return false;
        }
        uint32_t w_len = Write(buf, buflen);
        {
            if (w_len != buflen)
            {
                crash_b_ = true;
                assert(false);
                return false;
            }
        }
        md5_.Add(buf, buflen);
        sec_content_count_ += buflen;
        return true;
    }

    uint32_t SecFile::SecRead(boost::uint8_t *buf, uint32_t buflen)
    {
        if (crash_b_)
        {
            return 0;
        }
        assert(only_read);

        return Read(buf, buflen);
    }

    uint32_t SecFile::GetContentSize()
    {
        return sec_content_count_ - sizeof(SecFileHead);
    }

    // -------------------------------------------------------------------------
    // class CfgFile

    bool CfgFile::SecOpen(const string &resource_file_path)
    {
        string cfg_file_path;
        cfg_file_path = Storage::Inst_Storage()->GetCfgFilename(resource_file_path);

        if (!SecFile::SecOpen(cfg_file_path.c_str()))
        {
            return false;
        }
        uint32_t readnum = SecRead((boost::uint8_t*) &resource_file_size_, sizeof(resource_file_size_));
        cfg_head_len_ += sizeof(resource_file_size_);
        if (readnum != sizeof(resource_file_size_))
        {
            SecClose();
            return false;
        }
        int file_name_len = 0;
        readnum = SecRead((boost::uint8_t*) &file_name_len, sizeof(file_name_len));
        cfg_head_len_ += sizeof(file_name_len);
        if (readnum != sizeof(file_name_len))
        {
            SecClose();
            return false;
        }
        assert(file_name_len <= 256*8);
        if (HeadVersion() < Version6())
        {
            boost::scoped_array<wchar_t> file_name_array(new wchar_t[file_name_len/sizeof(wchar_t)]);
            wchar_t *file_name = file_name_array.get();
            readnum = SecRead((boost::uint8_t*) file_name, file_name_len);
            if (readnum != file_name_len)
            {
                SecClose();
                return false;
            }
            std::wstring wresource_file_name;
            wresource_file_name.assign(file_name, file_name_len/sizeof(wchar_t));
            resource_file_name_ = base::ws2s(wresource_file_name);
        }
        else if (HeadVersion() == Version6())
        {
            // Version6版本的文件是错误的，返回失败，直接删除
            return false;
        }
        else
        {
            boost::scoped_array<char> file_name_array(new char[file_name_len]);
            char *file_name = file_name_array.get();
            readnum = SecRead((boost::uint8_t*) file_name, file_name_len);
            if (readnum != file_name_len)
            {
                SecClose();
                return false;
            }
            resource_file_name_.assign(file_name, file_name_len);
        }

        if (resource_file_name_ != resource_file_path)
        {
            STORAGE_ERR_LOG("resource_file_name_ != resource_file_path");
            STORAGE_ERR_LOG("resource_file_name_ = " << resource_file_name_);
            STORAGE_ERR_LOG("resource_file_path = " << resource_file_path);
            SecClose();
            return false;
        }
        cfg_head_len_ += file_name_len;
        return true;
    }

    bool CfgFile::SecCreate(const string &resource_file_path, uint32_t resource_file_size)
    {
        string cfg_file_path = Storage::Inst_Storage()->GetCfgFilename(resource_file_path);

        if (!SecFile::SecCreate(cfg_file_path.c_str()))
        {
            return false;
        }
        assert(cfg_file_path.size() <= 1024);

        if (!SecWrite((boost::uint8_t*) &resource_file_size, sizeof(resource_file_size)))
        {
            SecClose();
            return false;
        }

        // uint32_t file_name_len = resource_file_path.size() * 2;
        uint32_t file_name_len = resource_file_path.size();
        if (!SecWrite((boost::uint8_t*) &file_name_len, sizeof(file_name_len)))
        {
            SecClose();
            return false;
        }
        if (!SecWrite((boost::uint8_t*) resource_file_path.c_str(), file_name_len))
        {
            SecClose();
            return false;
        }
        resource_file_size_ = resource_file_size;
        resource_file_name_ = resource_file_path;
        return true;
    }

    void CfgFile::SecClose()
    {
        resource_file_size_ = 0;
        resource_file_name_.clear();
        cfg_head_len_ = 0;
        SecFile::SecClose();
        return;
    }

    bool CfgFile::AddContent(base::AppBuffer const & buf)
    {
        uint32_t len = buf.Length();
        if (!SecFile::SecWrite((boost::uint8_t*)&len, sizeof(len)))
        {
            assert(false);
            return false;
        }

        if (!SecWrite(buf.Data(), buf.Length()))
        {
            assert(false);
            return false;
        }
        return true;
    }

    base::AppBuffer CfgFile::GetContent()
    {
        if (IsCrash())
        {
            base::AppBuffer buf(0);
            return buf;
        }
        uint32_t content_len = GetContentSize() - cfg_head_len_;
        uint32_t readbuflen = 0;
        uint32_t readnum = SecRead((boost::uint8_t*) (&readbuflen), sizeof(readbuflen));

        assert(readnum == sizeof(readbuflen));

        assert(readbuflen == content_len-sizeof(readbuflen));

        base::AppBuffer content_buf(readbuflen);
        readnum = SecRead((boost::uint8_t*) (content_buf.Data()), content_buf.Length());
        assert(readnum == content_buf.Length());
        return content_buf;
    }

    // ------------------------------------------------------------------------------
    // class ResourceInfoListFile

    void ResourceInfoListFile::AddResourceInfo(const std::vector<FileResourceInfo> &r_info_vec)
    {
        for (size_t i = 0; i < r_info_vec.size(); ++i)
        {
            base::AppBuffer buf = r_info_vec[i].ToBuffer();
            STORAGE_DEBUG_LOG("buf length:" << buf.Length());
            SecWrite(buf.Data(), buf.Length());
        }
    }

    bool ResourceInfoListFile::GetResourceInfo(FileResourceInfo &r_info, bool & bEof)
    {
        static uint32_t item_max_len = 10 * 1024;
        uint32_t item_len = 0;
        uint32_t r_len = SecRead((boost::uint8_t*) &item_len, 4);
        bEof = false;
        if (r_len != 4)
        {
            // 已经读到文件尾部
            bEof = true;
            return false;
        }

        // herain:2010-12-31:如果item_len小于4，那么分配的缓冲区也将小于4字节
        // 这样在向缓冲区内写入4字节数据后将造成堆被破坏，产生不确定的行为
        if (item_len > item_max_len || item_len < 4)
        {
            return false;
        }

        base::AppBuffer in_buf(item_len);
        base::util::memcpy2((void*)in_buf.Data(), in_buf.Length(), (void*)(&item_len), sizeof(int));
        r_len = SecRead(in_buf.Data() + 4, item_len - 4);
        if (r_len != item_len - 4)
        {
            return false;
        }
        return r_info.Parse(in_buf, HeadVersion());
    }

}// storage

#endif
