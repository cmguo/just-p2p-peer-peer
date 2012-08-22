//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// MetaMp4Parser.h

#ifndef _P2SP_FORMAT_MP4_SPLITTER_H_
#define _P2SP_FORMAT_MP4_SPLITTER_H_

#ifdef WIN32
#include <windows.h>
typedef boost::uint64_t SNC_UI64;
typedef boost::int64_t SNC_I64;
#define UINT64_MAX 0xffffffffffffffffULL
#else
#include <stdint.h>
#include <stdbool.h>
typedef uint64_t SNC_UI64;
typedef int64_t SNC_I64;
#endif

#define MAX_TRACKS 2
#define ATOM_PREAMBLE_SIZE 8

typedef unsigned int SNC_UI32;
typedef int SNC_I32;

using protocol::SubPieceContent;

struct atom_t
{
    unsigned char type_[4];
    unsigned int size_;
    unsigned char* start_;
    unsigned char* end_;
};

struct stts_table_t
{
    SNC_UI32 sample_count_;
    SNC_UI32 sample_duration_;
};

struct ctts_table_t
{
    SNC_UI32 sample_count_;
    SNC_UI32 sample_offset_;
};

struct stsc_table_t
{
    SNC_UI32 chunk_;
    SNC_UI32 samples_;
    SNC_UI32 id_;
};

struct stbl_t
{
    unsigned char* start_;
    unsigned char* stts_;     // decoding time-to-sample
    unsigned char* stss_;     // sync sample
    unsigned char* stsc_;     // sample-to-chunk
    unsigned char* stsz_;     // sample size
    unsigned char* stco_;     // chunk offset
    unsigned char* ctts_;     // composition time-to-sample

    unsigned char* new_;      // the newly generated stbl
    unsigned char* newp_;     // the newly generated stbl
    unsigned int newp_size_; // 长度
};

struct minf_t
{
    unsigned char* start_;
    struct stbl_t stbl_;
};

struct mdia_t
{
    unsigned char* start_;
    unsigned char* mdhd_;
    unsigned char* hdlr_;
    struct minf_t minf_;
    //  hdlr hdlr_;
};

struct chunks_t
{
    unsigned int sample_;   // number of the first sample in the chunk
    unsigned int size_;     // number of samples in the chunk
    int id_;                // for multiple codecs mode - not used
    SNC_UI64 pos_;          // start boost::uint8_t position of chunk
};

struct samples_t
{
    unsigned int pts_;      // decoding/presentation time
    unsigned int size_;     // size in bytes
    SNC_UI64 pos_;          // boost::uint8_t offset
    unsigned int cto_;      // composition time offset
};

struct trak_t
{
    unsigned char* start_;
    unsigned char* tkhd_;
    struct mdia_t mdia_;

    /* temporary indices */
    unsigned int chunks_size_;
    struct chunks_t* chunks_;

    unsigned int samples_size_;
    struct samples_t* samples_;
};

struct moov_t
{
    unsigned char* start_;
    unsigned int tracks_;
    unsigned char* mvhd_;
    struct trak_t traks_[MAX_TRACKS];
};

struct Mp4KeyFrame
{
    SNC_UI32 sam_no;
    double start_time;
    SNC_UI64 offset;
};

typedef struct MoovSeekRet_ST
{
    unsigned char* pmoov;
    unsigned int moov_size;
} MoovSeekRet;

class Mp4Spliter
    : private boost::noncopyable
{
    public:
    //////////////////////////////////////////////////////////////////////////
    // ???

  /**
   * ????????????????????Mp4?????????С
   */
  static unsigned int Mp4HeadLength(base::AppBuffer const & mp4_head);

    /***
     *  ????Mp4???????????ó?????μ?Mp4???
     *  <offset>: ??????????????????(?????????????????)
     *  <mp4_head>: Mp4????
     */
    static base::AppBuffer Mp4HeadParse(base::AppBuffer const & mp4_head,  uint32_t & offset);

    /***
     *  ????FileHandle?????????Mp4?????????????????
     */
    // static base::AppBuffer Mp4GetHead(HANDLE file_handle);

    private:
    static int moov_seek(unsigned char* moov_data, SNC_UI64 moov_size,
        float start_time, float end_time,
        SNC_UI64* mdat_start, SNC_UI64* mdat_size, SNC_UI64 offset,
        bool is_key, SNC_UI32 start_key_no, SNC_UI32 end_key_no);

    static MoovSeekRet moov_seek_ex(unsigned char* moov_data, SNC_UI64 moov_size,
        float start_time, float end_time,
        SNC_UI64* mdat_start, SNC_UI64* mdat_size, SNC_UI64 offset,
        bool is_key, SNC_UI32 start_key_no, SNC_UI32 end_key_no);

    //////////////////////////////////////////////////////////////////////////
    // ??????????????ж?????Free Box

    static unsigned char* stbl_rmfree(unsigned char* p_new_buf, unsigned char* p_old_buf,
        unsigned int old_size, unsigned int *p_rmsize , bool & has_error);

    static unsigned char* minf_rmfree(unsigned char* p_new_buf, unsigned char* p_old_buf,
        unsigned int old_size, unsigned int *p_rmsize , bool & has_error);

    static unsigned char* mdia_rmfree(unsigned char* p_new_buf, unsigned char* p_old_buf,
        unsigned int old_size, unsigned int *p_rmsize , bool & has_error);

    static unsigned char* trak_rmfree(unsigned char* p_new_buf, unsigned char* p_old_buf,
        unsigned int old_size, unsigned int *p_rmsize , bool & has_error);

    static unsigned char* moov_rmfree(unsigned char* p_new_buf, unsigned char* p_old_buf,
        unsigned int old_size, unsigned int *p_rmsize , bool & has_error);

    static MoovSeekRet rm_freeatom(unsigned char* moov_data, SNC_UI64 moov_size);

    //////////////////////////////////////////////////////////////////////////
    // parse

    static int stbl_parse(struct stbl_t* stbl, unsigned char* buffer, unsigned int size, int left_size);

    static int minf_parse(struct minf_t* minf, unsigned char* buffer, unsigned int size , int left_size);

    static int mdia_parse(struct mdia_t* mdia, unsigned char* buffer, unsigned int size , int left_size);

    static void trak_init(struct trak_t* trak);

    static void trak_exit(struct trak_t* trak);

    static int trak_parse(struct trak_t* trak, unsigned char* buffer, unsigned int size , int left_size);

    static void moov_init(struct moov_t* moov);

    static void moov_exit(struct moov_t* moov);

    static int trak_build_index(struct trak_t* trak);

    static int trak_write_index(struct trak_t* trak, unsigned int start, unsigned int end);

    static int moov_parse(struct moov_t* moov, unsigned char* buffer, SNC_UI64 size);

    static int stco_shift_offsets(unsigned char* stco, int offset);

    static int trak_shift_offsets(struct trak_t* trak, SNC_I64 offset);

    static void moov_shift_offsets(struct moov_t* moov, SNC_I64 offset);

    static long mvhd_get_time_scale(unsigned char* mvhd);

    static SNC_UI64 mvhd_get_duration(unsigned char* mvhd);

    static void mvhd_set_duration(unsigned char* mvhd, SNC_UI64 duration);

    static long mdhd_get_time_scale(unsigned char* mdhd);

    static SNC_UI64 mdhd_get_duration(unsigned char* mdhd);

    static void mdhd_set_duration(unsigned char* mdhd, SNC_UI64 duration);

    static void tkhd_set_duration(unsigned char* tkhd, SNC_UI64 duration);

    static SNC_UI32 tkhd_get_width(unsigned char* tkhd);

    static SNC_UI32 tkhd_get_height(unsigned char* tkhd);

    //////////////////////////////////////////////////////////////////////////
    // ??????stbl????????

    static unsigned int stss_get_entries(unsigned char const* stss);

    static long stss_get_sample(unsigned char const* stss, unsigned int idx);

    static unsigned int stss_get_nearest_keyframe(unsigned char const* stss, unsigned int sample);

    static unsigned int stbl_get_nearest_keyframe(struct stbl_t const* stbl, unsigned int sample);

    static unsigned int stts_get_entries(unsigned char const* stts);

    static void stts_get_sample_count_and_duration(unsigned char const* stts,
        unsigned int idx, unsigned int* sample_count, unsigned int* sample_duration);

    static unsigned int ctts_get_entries(unsigned char const* ctts);

    static void ctts_get_sample_count_and_offset(unsigned char const* ctts,
        unsigned int idx, unsigned int* sample_count, unsigned int* sample_offset);

    static unsigned int ctts_get_samples(unsigned char const* ctts);

    static unsigned int stsc_get_entries(unsigned char const* stsc);

    static void stsc_get_table(unsigned char const* stsc, unsigned int i, struct stsc_table_t *stsc_table);

    static unsigned int stsc_get_chunk(unsigned char* stsc, unsigned int sample);

    static unsigned int stsc_get_samples(unsigned char* stsc);

    static unsigned int stco_get_entries(unsigned char const* stco);

    static SNC_UI32 stco_get_offset(unsigned char const* stco, int idx);

    static unsigned int stsz_get_sample_size(unsigned char const* stsz);

    static unsigned int stsz_get_entries(unsigned char const* stsz);

    static unsigned int stsz_get_size(unsigned char const* stsz, unsigned int idx);

    static SNC_UI64 stts_get_duration(unsigned char const* stts);

    static unsigned int stts_get_samples(unsigned char const* stts);

    static unsigned int stts_get_sample(unsigned char const* stts, unsigned int time);

    static unsigned int stts_get_time(unsigned char const* stts, unsigned int sample);

    //////////////////////////////////////////////////////////////////////////
    // ??????atom????

    static unsigned int atom_header_size(unsigned char* atom_bytes);

    static unsigned char* atom_read_header(unsigned char* buffer, struct atom_t* atom);

    static void atom_write_header(unsigned char* outbuffer, struct atom_t* atom);

    static unsigned char* atom_skip(unsigned char* buffer, struct atom_t const* atom);

    static int atom_is(struct atom_t const* atom, const char* type);

    //////////////////////////////////////////////////////////////////////////
    // ????????????

    static int read_char(unsigned char const* buffer);

    static unsigned short read_int16(void const* buffer);

    static unsigned int read_int32(void const* buffer);

    static SNC_UI64 read_int64(void const* buffer);

    static void write_char(unsigned char* outbuffer, int value);

    static void write_int32(void* outbuffer, SNC_UI32 value);

    static void write_int64(void* outbuffer, SNC_UI64 value);
};

#endif  // _P2SP_FORMAT_MP4_SPLITTER_H_
