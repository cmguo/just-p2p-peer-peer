//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

/*
CfgFile.h
*/
#ifndef STORAGE_CFGFILE_H
#define STORAGE_CFGFILE_H

#include "storage/storage_base.h"
#include "storage/HmacMd5.h"
#include "io/StdFile.h"

namespace storage
{

#ifdef DISK_MODE
    struct FileResourceInfo;
    // ----------------------------------------------------------------------------
#ifdef PEER_PC_CLIENT
    #pragma pack(push, 1)
#endif
    struct SecFileHead
    {
        int verison_;
        boost::uint64_t time_stamp_;
        MD5 sec_md5_;
        SecFileHead() :
        verison_(0), sec_md5_(), time_stamp_(0)
        {
        }
       ;
        inline static boost::uint32_t md5_offset()
        {
            return sizeof(int) + sizeof(boost::uint64_t);
        }
       ;
    };
#ifdef PEER_PC_CLIENT
#pragma pack(pop)
#endif
    // ----------------------------------------------------------------------------

    class SecFile
        : private io::StdFile
#ifdef DUMP_OBJECT
        , public count_object_allocate<SecFile>
#endif
    {
    public:
        SecFile() :
          crash_b_(true), sec_content_count_(0), only_read(false)
          {
          }
         ;
          SecFile(boost::uint8_t *key, boost::uint32_t key_len) :
          md5_(key, key_len), crash_b_(true), sec_content_count_(0), only_read(false)
          {
          }
         ;
          virtual bool SecOpen(const char* path);
          virtual bool SecCreate(const char* path);
          virtual bool SecWrite(const boost::uint8_t *buf, boost::uint32_t buflen);
          virtual boost::uint32_t SecRead(boost::uint8_t *buf, boost::uint32_t buflen);
          virtual void SecClose();

    protected:
        virtual bool SetCrashFlag()
        {
            return crash_b_;
        }
        virtual boost::uint32_t GetContentSize();
        virtual bool IsCrash()
        {
            return crash_b_;
        }

        boost::uint32_t HeadVersion()
        {
            return head_.verison_;
        }
        static boost::uint32_t Version1()
        {
            return SecVerCtrl::sec_version1;
        }
        static boost::uint32_t Version2()
        {
            return SecVerCtrl::sec_version2;
        }
        static boost::uint32_t Version3()
        {
            return SecVerCtrl::sec_version3;
        }
        static boost::uint32_t Version4()
        {
            return SecVerCtrl::sec_version4;
        }
        static boost::uint32_t Version5()
        {
            return SecVerCtrl::sec_version5;
        }
        static boost::uint32_t Version6()
        {
            return SecVerCtrl::sec_version6;
        }
        static boost::uint32_t Version7()
        {
            return SecVerCtrl::sec_version7;
        }
        static boost::uint32_t Version8()
        {
            return SecVerCtrl::sec_version8;
        }
        static boost::uint32_t VersionNow()
        {
            return Version8();
        }

    private:
        bool DoSign();
        bool DoVerify();
        bool CheckVersion(boost::uint32_t ver)
        {
            return ver == Version1() || ver == Version2() || ver == Version3() || ver == Version4() || ver == Version5() || ver == Version6() || ver == Version7() || ver == Version8();
        }

    private:
        CHmacMD5 md5_;
        static const boost::uint32_t max_sec_file_size = 32 * 1024 * 1024;  // 32M
        bool crash_b_;
        boost::uint32_t sec_content_count_;
        SecFileHead head_;
        bool only_read;
    };

    class CfgFile: protected SecFile
    {
    public:
        CfgFile() :
          SecFile((boost::uint8_t*) default_cfg_key_g_.c_str(), default_cfg_key_g_.size()), resource_file_size_(0),
              cfg_head_len_(0)
          {
          }
          CfgFile(boost::uint8_t *key, boost::uint16_t key_len) :
          SecFile(key, key_len), resource_file_size_(0), cfg_head_len_(0)
          {
          }
          virtual bool SecOpen(const string& resource_file_path);
          virtual bool SecCreate(const string &respurce_file_name, boost::uint32_t respurce_file_size);
          virtual void SecClose();
          bool AddContent(base::AppBuffer const & buf);
          base::AppBuffer GetContent();
    public:
        boost::uint32_t resource_file_size_;
        string resource_file_name_;
    private:
        int cfg_head_len_;
    };

    class ResourceInfoListFile: public SecFile
    {
    public:
        ResourceInfoListFile() :
          SecFile()
          {
          }
          ResourceInfoListFile(boost::uint8_t *key, boost::uint16_t key_len) :
          SecFile(key, key_len)
          {
          }
          void AddResourceInfo(const std::vector<FileResourceInfo> &r_info_vec);
          bool GetResourceInfo(FileResourceInfo &r_info, bool & bEof);
    private:
    };

#endif  // DISK_MODE

}

#endif  // STORAGE_CFGFILE_H
