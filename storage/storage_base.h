//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef STORAGE_STORAGEBASE_H
#define STORAGE_STORAGEBASE_H

#ifdef BOOST_WINDOWS_API
#pragma once
#endif

namespace storage
{

    enum{
        ERROR_UNKOWNED,
        ERROR_GET_SUBPIECE_RESOURCE_NOT_EXIST,
        ERROR_GET_SUBPIECE_OUTOFRANGE,
        ERROR_GET_SUBPIECE_NOT_FIND_SUBPIECE,
        ERROR_GET_SUBPIECE_BLOCK_NOT_BEEN_PUBLISHED,
        ERROR_GET_SUBPIECE_BLOCK_VERIFY_FAILED,
        ERROR_GET_SUBPIECE_BLOCK_NOT_FULL,
        ERROR_ADD_SUBPIECE_BLOCK_BEEN_PUBLISHED,
        ERROR_GET_SUBPIECE_VERIFY_WRONG,
        ERROR_GET_SUBPIECE_NO_FILELENGTH,
        ERROR_GET_BLOCK_NOT_FIND_BLOCK
    };
    enum
    {
        INSTANCE_NEED_RESOURCE,
        INSTANCE_APPLY_RESOURCE,
        INSTANCE_HAVE_RESOURCE,
        INSTANCE_BEING_MERGED,
        INSTANCE_REMOVING,
        INSTANCE_STOP,
        INSTANCE_CLOSING
    };
    enum
    {
        NO_VERIFIED,
        HAS_BEEN_VERIFIED,
        BEING_VERIFIED
    };
    enum ResDownMode
    {
        DM_BY_ACCELERATE = 0,    // 资源以“加速”的方式下载
        DM_BY_BHOSAVE     = 1    // 资源以“BHO保存”的方式下载
    };

    enum StorageMode
    {
        STORAGE_MODE_NORMAL   = 0,
        STORAGE_MODE_READONLY = 1,
    };

    namespace SecVerCtrl
    {
        static const uint32_t sec_version1 = 0x00000001;
        static const uint32_t sec_version2 = 0x00000002;
        static const uint32_t sec_version3 = 0x00000003;
        static const uint32_t sec_version4 = 0x00000004;
        static const uint32_t sec_version5 = 0x00000005;
        static const uint32_t sec_version6 = 0x00000006;
        static const uint32_t sec_version7 = 0x00000007;
        static const uint32_t sec_version8 = 0x00000008;
    }

#define STORAGE_LOG(a)     LOG(__DEBUG, "storage", __FUNCTION__ << " line:" << __LINE__ << " " << a)
#define STORAGE_ERR_LOG(a)  LOG(__ERROR, "storage", __FUNCTION__ << " line:" << __LINE__ << " " << a)
#define STORAGE_WARN_LOG(a)  LOG(__WARN, "storage", __FUNCTION__ << " line:" << __LINE__ << " " << a)
#define STORAGE_EVENT_LOG(a)  LOG(__EVENT, "storage", __FUNCTION__ << " line:" << __LINE__ << " " << a)
#define STORAGE_INFO_LOG(a)  LOG(__INFO, "storage", __FUNCTION__ << " line:" << __LINE__ << " " << a)
#define STORAGE_DEBUG_LOG(a)  LOG(__DEBUG, "storage", __FUNCTION__ << " line:" << __LINE__ << " " << a)
#define STORAGE_TEST_DEBUG(a) LOG(__DEBUG, "storage_test", a)

    static const string tpp_extname(".tpp");
    static const string cfg_extname(".cfg");
    static const int max_size_cfg_file_g_ = 2 * 1024 * 1024;
    static const int version_g_ = 0x03050700;
    static const int hash_len_g_ = 16;

    struct StorageConfig
    {
        boost::uint16_t bytes_num_per_subpiece_g_;
        boost::uint16_t subpiece_num_per_piece_g_;
        boost::uint16_t piece_num_per_block_g_;
        uint32_t max_block_num_g_;
    };

    static const uint32_t bytes_num_per_subpiece_g_ = SUB_PIECE_SIZE;

    // static StorageConfig storage_config;
    //    static const boost::uint16_t bytes_num_per_subpiece_g_ = boost::uint16_t(1) << 10;
    static const boost::uint16_t subpiece_num_per_piece_g_ = boost::uint16_t(1) << 7;
    static const boost::uint16_t piece_num_per_block_g_ = boost::uint16_t(1) << 4;
    static const uint32_t bytes_num_per_piece_g_ = bytes_num_per_subpiece_g_ * subpiece_num_per_piece_g_;
    static const boost::uint16_t default_subpiece_num_per_block_g_ = subpiece_num_per_piece_g_ * piece_num_per_block_g_;  // boost::uint16_t(1) << 11;
    static const uint32_t default_block_size_g_ = piece_num_per_block_g_ * subpiece_num_per_piece_g_
        * bytes_num_per_subpiece_g_;
    static const uint32_t max_block_num_g_ = 50;
    static const string default_cfg_key_g_ = "123456";
    static const uint32_t default_delay_tickcount_for_delete = 10 * 1000;

    static const uint32_t TRAFFIC_UNIT_TIME = 6 * 60 * 60;
    static const uint32_t TRAFFIC_T0 = 7;
    static const uint32_t TRAFFIC_PROTECT_TIME = 2;
}

#endif  // STORAGE_STORAGEBASE_H
