//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef STORAGE_STORAGEBASE_H
#define STORAGE_STORAGEBASE_H

#ifdef PEER_PC_CLIENT
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
	
	enum ResDownMode
    {
        DM_BY_ACCELERATE = 0,    // ��Դ�ԡ����١��ķ�ʽ����
        DM_BY_BHOSAVE     = 1    // ��Դ�ԡ�BHO���桱�ķ�ʽ����
    };

    enum StorageMode
    {
        STORAGE_MODE_NORMAL   = 0,
        STORAGE_MODE_READONLY = 1,
    };

    namespace SecVerCtrl
    {
        static const boost::uint32_t sec_version1 = 0x00000001;
        static const boost::uint32_t sec_version2 = 0x00000002;
        static const boost::uint32_t sec_version3 = 0x00000003;
        static const boost::uint32_t sec_version4 = 0x00000004;
        static const boost::uint32_t sec_version5 = 0x00000005;
        static const boost::uint32_t sec_version6 = 0x00000006;
        static const boost::uint32_t sec_version7 = 0x00000007;
        static const boost::uint32_t sec_version8 = 0x00000008;
    }

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
        boost::uint32_t max_block_num_g_;
    };

    static const boost::uint32_t bytes_num_per_subpiece_g_ = SUB_PIECE_SIZE;

    // static StorageConfig storage_config;
    //    static const boost::uint16_t bytes_num_per_subpiece_g_ = boost::uint16_t(1) << 10;
    static const boost::uint16_t subpiece_num_per_piece_g_ = boost::uint16_t(1) << 7;
    static const boost::uint16_t piece_num_per_block_g_ = boost::uint16_t(1) << 4;
    static const boost::uint32_t bytes_num_per_piece_g_ = bytes_num_per_subpiece_g_ * subpiece_num_per_piece_g_;
    static const boost::uint16_t default_subpiece_num_per_block_g_ = subpiece_num_per_piece_g_ * piece_num_per_block_g_;  // boost::uint16_t(1) << 11;
    static const boost::uint32_t default_block_size_g_ = piece_num_per_block_g_ * subpiece_num_per_piece_g_
        * bytes_num_per_subpiece_g_;
    static const boost::uint32_t max_block_num_g_ = 50;
    static const string default_cfg_key_g_ = "123456";
    static const boost::uint32_t default_delay_tickcount_for_delete = 10 * 1000;

    static const boost::uint32_t TRAFFIC_UNIT_TIME = 6 * 60 * 60;
    static const boost::uint32_t TRAFFIC_T0 = 7;
    static const boost::uint32_t TRAFFIC_PROTECT_TIME = 2;
    static const boost::uint32_t ENCRYPT_HEADER_LENGTH = 1024;
}

#endif  // STORAGE_STORAGEBASE_H
