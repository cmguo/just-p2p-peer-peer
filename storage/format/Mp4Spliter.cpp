//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

/***
*  Mp4Spliter.cpp
*/
#include "Common.h"

#include "Mp4Spliter.h"
#include <base/util.h>

//////////////////////////////////////////////////////////////////////////
//

unsigned int Mp4Spliter::Mp4HeadLength(
                                       base::AppBuffer const & mp4_head)
{
    atom_t ftyp_atom, moov_atom, mdat_atom, tmp_atom;
    unsigned char* p = mp4_head.Data();
    unsigned char* p_end = mp4_head.Data() + mp4_head.Length();
    unsigned int head_length = 0;
    unsigned int i = 0;
    while (p < p_end && i < 2)
    {
        unsigned int offset = p - mp4_head.Data();
        p = atom_read_header(p, &tmp_atom);

        if (atom_is(&tmp_atom, "ftyp"))
        {
            ftyp_atom = tmp_atom;
            // head_length += ftyp_atom.size_;
            ++i;
        }
        else if (atom_is(&tmp_atom, "moov"))
        {
            moov_atom = tmp_atom;
            head_length = offset + moov_atom.size_ + 8;
            ++i;
        }
        else if (atom_is(&tmp_atom, "mdat"))
        {
            mdat_atom = tmp_atom;
        }
        p = atom_skip(p, &tmp_atom);
    }
    return head_length;
}

base::AppBuffer Mp4Spliter::Mp4HeadParse(
                                   base::AppBuffer const & mp4_head,
                                   boost::uint32_t & offset)
{
    atom_t ftyp_atom, moov_atom, mdat_atom, tmp_atom;
    unsigned char* p = mp4_head.Data();
    unsigned char* p_beg = p;
    unsigned char* p_end = mp4_head.Data() + mp4_head.Length();
    base::AppBuffer moov_data, ftyp_data;

    int used_size = 0;
    while (p < p_end)
    {
        p = atom_read_header(p, &tmp_atom);

        if (atom_is(&tmp_atom, "ftyp"))
        {
            if (tmp_atom.size_ > mp4_head.Length() - used_size)
            {
                return base::AppBuffer();
            }

            ftyp_atom = tmp_atom;
            ftyp_data.Malloc(ftyp_atom.size_);
            base::util::memcpy2(ftyp_data.Data(), ftyp_atom.size_ , ftyp_atom.start_, ftyp_atom.size_);
            // memcpy_s(ftyp_data.Data(), ftyp_data.Length(), ftyp_atom.start_, ftyp_atom.size_);
        }
        else if (atom_is(&tmp_atom, "moov"))
        {
            if (tmp_atom.size_ > mp4_head.Length() - used_size)
            {
                return base::AppBuffer();
            }

            moov_atom = tmp_atom;
            moov_data.Malloc(moov_atom.size_);
            base::util::memcpy2(moov_data.Data(), moov_atom.size_ , moov_atom.start_, moov_atom.size_);
            // memcpy_s(moov_data.Data(), moov_data.Length(), moov_atom.start_, moov_atom.size_);
        }
        else if (atom_is(&tmp_atom, "mdat"))
        {
            mdat_atom = tmp_atom;
        }

        used_size += tmp_atom.size_;
        p = atom_skip(p, &tmp_atom);
    }

    if (!moov_data.Data()) return base::AppBuffer();

    unsigned int mdat_tmp_start = (ftyp_data.Data() ? ftyp_atom.size_ : 0) + moov_atom.size_;
    boost::shared_ptr<moov_t> moov(new moov_t);

    /* build index */
    moov_init(moov.get());
    if (-1 == moov_parse(moov.get(), moov_data.Data() + ATOM_PREAMBLE_SIZE, moov_atom.size_ - ATOM_PREAMBLE_SIZE))
    {
        moov_exit(moov.get());
        return base::AppBuffer();
    }

    // keyframe

    long moov_time_scale;
    unsigned int video_trak_id;
    std::vector<Mp4KeyFrame> mp4_video_keyframe;

    moov_time_scale = mvhd_get_time_scale(moov->mvhd_);
    for (unsigned int i = 0; i != moov->tracks_; ++i)
    {
        trak_t* trak = &moov->traks_[i];
        stbl_t* stbl = &trak->mdia_.minf_.stbl_;
        unsigned char* hdlr = trak->mdia_.hdlr_;
        long trak_time_scale = mdhd_get_time_scale(trak->mdia_.mdhd_);

        if (memcmp(hdlr + 8, "vide", 4) == 0)  // video trak
        {
            video_trak_id = i;
            if (stbl->stss_)
            {
                unsigned int kf_num = stss_get_entries(stbl->stss_);
                for (unsigned int jj = 0; jj < kf_num; ++jj)
                {
                    Mp4KeyFrame mkfe;
                    mkfe.sam_no = stss_get_sample(stbl->stss_, jj);
                    mkfe.start_time = stts_get_time(stbl->stts_, mkfe.sam_no -1) / trak_time_scale;
                    mkfe.offset = trak->samples_[mkfe.sam_no-1].pos_;
                    mp4_video_keyframe.push_back(mkfe);
                }
            }
            else
            {
                unsigned int kf_num = trak->samples_size_;
                for (unsigned int jj = 0; jj < kf_num; ++jj)
                {
                    Mp4KeyFrame mkfe;
                    mkfe.sam_no = jj + 1;
                    mkfe.start_time = stts_get_time(stbl->stts_, jj) / trak_time_scale;
                    mkfe.offset = trak->samples_[jj].pos_;
                    mp4_video_keyframe.push_back(mkfe);
                }
            }
        }
    }  // for

    unsigned int drag_sam_no;
    float drag_time;
    std::vector<Mp4KeyFrame>::iterator it = mp4_video_keyframe.begin();
    for (; it != mp4_video_keyframe.end(); ++it)
    {
        if (it->offset >= offset)
        {
            drag_sam_no = it->sam_no;
            drag_time = static_cast<float>(it->start_time);
            offset = it->offset;
            break;
        }
    }
    if (it == mp4_video_keyframe.end())
    {
        // return protocol::SubPieceContent();
        drag_sam_no = mp4_video_keyframe.back().sam_no;
        drag_time = static_cast<float>(mp4_video_keyframe.back().start_time);
        offset = mp4_video_keyframe.back().offset;
    }

    unsigned int end_sam_no = mp4_video_keyframe.back().sam_no;
    SNC_UI64 mdat_atom_start = mdat_atom.start_ - p_beg;
    MoovSeekRet moov_ret_val = moov_seek_ex(moov_data.Data(), moov_atom.size_, drag_time, 0.0,
        &mdat_atom_start, (SNC_UI64*)&mdat_atom.size_, mdat_tmp_start - mdat_atom_start,
        true, drag_sam_no, end_sam_no);
    //     {
    //         FILE* fp = fopen("log.log", "a+");
    //         fprintf(fp, "drag_time: %.2f, drag_sam_no: %u, end_sam_no: %u, start:%u, size: %u\n",
    //             drag_time, drag_sam_no, end_sam_no, mdat_atom_start, mdat_atom.size_);
    //         fclose(fp);
    //     }
    if (!moov_ret_val.pmoov)
    {
        moov_exit(moov.get());

        return base::AppBuffer();
    }

    offset = mdat_atom_start + ATOM_PREAMBLE_SIZE;

    int new_head_length = ftyp_atom.size_ + moov_ret_val.moov_size + ATOM_PREAMBLE_SIZE;
    base::AppBuffer new_head(ftyp_atom.size_ + moov_ret_val.moov_size + ATOM_PREAMBLE_SIZE);
    unsigned char* p_nh = new_head.Data();

    // ftyp
    if (ftyp_data.Data())
    {
        base::util::memcpy2(p_nh, new_head_length, ftyp_data.Data(), ftyp_atom.size_);
        p_nh += ftyp_atom.size_;
        new_head_length -= ftyp_atom.size_;
    }

    // moov
    base::util::memcpy2(p_nh, new_head_length , moov_ret_val.pmoov, moov_ret_val.moov_size);
    p_nh += moov_ret_val.moov_size;
    new_head_length -= moov_ret_val.moov_size;

    // mdat
    unsigned char mdat_bytes[ATOM_PREAMBLE_SIZE];
    atom_write_header(mdat_bytes, &mdat_atom);
    base::util::memcpy2(p_nh, new_head_length , mdat_bytes, ATOM_PREAMBLE_SIZE);
    p_nh += ATOM_PREAMBLE_SIZE;
    new_head_length -= ATOM_PREAMBLE_SIZE;

    moov_exit(moov.get());
    free(moov_ret_val.pmoov);
    return new_head;
}
/*
base::AppBuffer Mp4Spliter::Mp4GetHead(HANDLE file_handle)
{
#ifdef PEER_PC_CLIENT
    if (INVALID_HANDLE_VALUE == file_handle)
    {
        return base::AppBuffer();
    }
    atom_t tmp_atom, mdat_atom;
    char buf[ATOM_PREAMBLE_SIZE];
    boost::uint32_t readlen;
    bool have_mdat = false, have_ftyp = false, have_moov = false;
    base::AppBuffer ftyp_buf, moov_buf, mdat_buf;

    ::SetFilePointer(file_handle, 0, 0, FILE_BEGIN);
    while (::ReadFile(file_handle, buf, ATOM_PREAMBLE_SIZE, &readlen, NULL))
    {
        if (0 == readlen)
        {
            break;
        }
        tmp_atom.size_ = read_int32(buf);
        memcpy(tmp_atom.type_, &buf[4], 4);
        if (atom_is(&tmp_atom, "ftyp"))
        {
            ftyp_buf.Malloc(tmp_atom.size_);
            memcpy(ftyp_buf.Data(), buf, ATOM_PREAMBLE_SIZE);  // header
            ::ReadFile(file_handle, ftyp_buf.Data() + ATOM_PREAMBLE_SIZE,
                tmp_atom.size_ - ATOM_PREAMBLE_SIZE, &readlen, NULL);
            have_ftyp = true;
        }
        else if (atom_is(&tmp_atom, "moov"))
        {
            moov_buf.Malloc(tmp_atom.size_);
            memcpy(moov_buf.Data(), buf, ATOM_PREAMBLE_SIZE);  // header
            ::ReadFile(file_handle, moov_buf.Data() + ATOM_PREAMBLE_SIZE,
                tmp_atom.size_ - ATOM_PREAMBLE_SIZE, &readlen, NULL);
            have_moov = true;
        }
        else if (atom_is(&tmp_atom, "mdat"))
        {
            mdat_atom = tmp_atom;
            mdat_buf.Malloc(ATOM_PREAMBLE_SIZE);
            memcpy(mdat_buf.Data(), buf, ATOM_PREAMBLE_SIZE);
            have_mdat = true;
            ::SetFilePointer(file_handle, tmp_atom.size_ - ATOM_PREAMBLE_SIZE, 0, FILE_CURRENT);
        }
        else
        {
            ::SetFilePointer(file_handle, tmp_atom.size_ - ATOM_PREAMBLE_SIZE, 0, FILE_CURRENT);
        }
    }
    if (false == have_mdat || false == have_ftyp || false == have_moov)
    {
        return base::AppBuffer();
    }

    unsigned int head_size = ftyp_buf.Length() + moov_buf.Length() + mdat_buf.Length();
    base::AppBuffer head_buf(head_size);
    unsigned char *p = head_buf.Data();
    memcpy(p, ftyp_buf.Data(), ftyp_buf.Length()); p += ftyp_buf.Length();
    memcpy(p, moov_buf.Data(), moov_buf.Length()); p += moov_buf.Length();
    memcpy(p, mdat_buf.Data(), mdat_buf.Length()); p += mdat_buf.Length();
    ::SetFilePointer(file_handle, 0, 0, FILE_BEGIN);
    return head_buf;
#endif
    return base::AppBuffer();
}
*/
int Mp4Spliter::moov_seek(unsigned char* moov_data,
                          SNC_UI64 moov_size,
                          float start_time,
                          float end_time,
                          SNC_UI64* mdat_start,
                          SNC_UI64* mdat_size,
                          SNC_UI64 offset,
                          bool is_key, SNC_UI32 start_key_no, SNC_UI32 end_key_no)
{
    struct moov_t* moov = (moov_t*)malloc(sizeof(struct moov_t));
    if (0 == moov) return -1;
    moov_init(moov);
    if (-1 == moov_parse(moov, moov_data + ATOM_PREAMBLE_SIZE, moov_size - ATOM_PREAMBLE_SIZE))
    {
        moov_exit(moov);
        free(moov);
        return -1;
    }

    if (start_time == 0 && end_time == 0)
    {
        moov_shift_offsets(moov, offset);
        moov_exit(moov);
        free(moov);
        return 0;
    }


    {
        SNC_UI64 skip_from_start = (SNC_UI64)-1;
        SNC_UI64 end_offset = 0;
        unsigned int i;
        unsigned int pass;

        unsigned int trak_sample_start[MAX_TRACKS];
        unsigned int trak_sample_end[MAX_TRACKS];

        SNC_UI64 moov_duration = 0;
        unsigned int stss_trak_id;
        long moov_time_scale = mvhd_get_time_scale(moov->mvhd_);
        unsigned int start = (unsigned int)(start_time * moov_time_scale);
        unsigned int end = (unsigned int)(end_time * moov_time_scale);

        SNC_UI64 sam_base_pos = 0;

        // for every trak, convert seconds to sample (time-to-sample).
        // adjust sample to keyframe


        // fixed:
        // first pass we get the new aligned times for traks with an stss present
        // second pass is for traks without an stss
        for (pass = 0; pass != 2; ++pass)
        {
            for (i = 0; i != moov->tracks_; ++i)
            {
                struct trak_t* trak = &moov->traks_[i];
                struct stbl_t* stbl = &trak->mdia_.minf_.stbl_;
                long trak_time_scale = mdhd_get_time_scale(trak->mdia_.mdhd_);
                float moov_to_trak_time = (float)trak_time_scale / (float)moov_time_scale;
                float trak_to_moov_time = (float)moov_time_scale / (float)trak_time_scale;

                // 1st pass: stss present, 2nd pass: no stss present
                if (pass == 0 && !stbl->stss_)
                    continue;
                if (pass == 1 && stbl->stss_)
                    continue;

                if (stbl->stss_) stss_trak_id = i;

                // ignore empty track
                if (mdhd_get_duration(trak->mdia_.mdhd_) == 0)
                    continue;

                // get start
                if (start == 0)
                {
                    trak_sample_start[i] = start;
                }
                else
                {
                    if (is_key && stbl->stss_) start = start_key_no - 1;
                    else {
                        start = stts_get_sample(stbl->stts_, (unsigned int)(start * moov_to_trak_time));
                        start = stbl_get_nearest_keyframe(stbl, start + 1) - 1;
                    }
                    trak_sample_start[i] = start;
                    start = (unsigned int)(stts_get_time(stbl->stts_, start) * trak_to_moov_time);
                }

                // get end
                if (end == 0)
                {
                    trak_sample_end[i] = trak->samples_size_;
                }
                else
                {
                    end = stts_get_sample(stbl->stts_, (unsigned int)(end * moov_to_trak_time));
                    if (end >= trak->samples_size_)
                    {
                        end = trak->samples_size_;
                    }
                    else
                    {
                        if (is_key && stbl->stss_) end = end_key_no - 1;
                        else end = stbl_get_nearest_keyframe(stbl, end + 1) - 1;
                    }
                    trak_sample_end[i] = end;
                    end = (unsigned int)(stts_get_time(stbl->stts_, end) * trak_to_moov_time);
                }
            }
        }

        if (end && start >= end)
        {
            moov_exit(moov);
            free(moov);
            return -1;
        }
        /*
        for (i = 0; i != moov->tracks_; ++i)
        {
        struct trak_t* trak = &moov->traks_[i];
        struct stbl_t* stbl = &trak->mdia_.minf_.stbl_;

        unsigned int start_sample = trak_sample_start[i];
        unsigned int tmp_start_sample = start_sample;

        if (i != stss_trak_id) {
        struct trak_t* stss_trak = &moov->traks_[stss_trak_id];
        unsigned int stss_start_sample = trak_sample_start[stss_trak_id];

        while (trak->samples_[tmp_start_sample].pos_ <
        stss_trak->samples_[stss_start_sample].pos_) {
        ++tmp_start_sample;
        if (tmp_start_sample >= trak->samples_size_) {
        tmp_start_sample = start_sample;
        break;
        }
        }

        trak_sample_start[i] = tmp_start_sample;
        }

        if (i != stss_trak_id) {
        struct trak_t* stss_trak = &moov->traks_[stss_trak_id];
        unsigned int stss_end_sample = trak_sample_end[stss_trak_id];

        unsigned int end_sam = trak_sample_end[i];
        unsigned int tmp_end_sam = trak_sample_end[i];

        while (trak->samples_[tmp_end_sam].pos_ >
        stss_trak->samples_[stss_end_sample].pos_) {
        --tmp_end_sam;
        if (tmp_end_sam <= trak_sample_start[i]) {
        break;
        }
        }
        trak_sample_end[i] = tmp_end_sam;
        }
        }
        */

        if (moov->traks_[0].samples_size_ == 0 || moov->traks_[1].samples_size_ == 0)
        {
            // 如果没有，那么就出错，退出
            moov_exit(moov);
            free(moov);
            return -1;
        }

        sam_base_pos = moov->traks_[0].samples_[0].pos_;
        if (moov->traks_[1].samples_[0].pos_ < sam_base_pos) {
            sam_base_pos = moov->traks_[1].samples_[0].pos_;
        }


        for (i = 0; i != moov->tracks_; ++i)
        {
            struct trak_t* trak = &moov->traks_[i];
            struct stbl_t* stbl = &trak->mdia_.minf_.stbl_;

            unsigned int start_sample = trak_sample_start[i];
            unsigned int end_sample = trak_sample_end[i];
            //            long trak_time_scale = mdhd_get_time_scale(trak->mdia_.mdhd_);

            // ignore empty track
            if (mdhd_get_duration(trak->mdia_.mdhd_) == 0)
                continue;

            if (-1 == trak_write_index(trak, start_sample, end_sample))
            {
                moov_exit(moov);
                free(moov);
                return -1;
            }

            {
                // if (i == stss_trak_id)
                {
                    //                          SNC_UI64 skip =
                    //                               trak->samples_[start_sample].pos_ - trak->samples_[0].pos_;
                    SNC_UI64 skip =
                        trak->samples_[start_sample].pos_ - sam_base_pos;
                    if (skip < skip_from_start)
                        skip_from_start = skip;

                    if (end_sample != trak->samples_size_)
                    {
                        SNC_UI64 end_pos = trak->samples_[end_sample].pos_;
                        if (end_pos > end_offset)
                            end_offset = end_pos;
                    }
                }
            }

            {
                // fixup trak (duration)
                SNC_UI64 trak_duration = stts_get_duration(stbl->stts_);
                long trak_time_scale = mdhd_get_time_scale(trak->mdia_.mdhd_);
                float trak_to_moov_time = (float)moov_time_scale / (float)trak_time_scale;
                SNC_UI64 duration = (SNC_UI64)((SNC_UI64)trak_duration * trak_to_moov_time);
                mdhd_set_duration(trak->mdia_.mdhd_, trak_duration);
                tkhd_set_duration(trak->tkhd_, duration);

                if (duration > moov_duration) moov_duration = duration;
            }
        }
        mvhd_set_duration(moov->mvhd_, moov_duration);
        offset -= skip_from_start;
        moov_shift_offsets(moov, offset);

        *mdat_start += skip_from_start;
        if (end_offset != 0)
        {
            *mdat_size = end_offset;
            *mdat_size -= *mdat_start;
        }
        else
            *mdat_size -= skip_from_start;
    }

    moov_exit(moov);
    free(moov);

    return 0;
}

MoovSeekRet Mp4Spliter::moov_seek_ex(unsigned char* moov_data,
                                     SNC_UI64 moov_size,
                                     float start_time,
                                     float end_time,
                                     SNC_UI64* mdat_start,
                                     SNC_UI64* mdat_size,
                                     SNC_UI64 offset,
                                     bool is_key, SNC_UI32 start_key_no, SNC_UI32 end_key_no)
{
    MoovSeekRet ret_val;
    memset(&ret_val, 0, sizeof(MoovSeekRet));
    if (-1 == moov_seek(moov_data, moov_size, start_time, end_time, mdat_start,
        mdat_size, offset, is_key, start_key_no, end_key_no))
    {
        return ret_val;
    }

    ret_val = rm_freeatom(moov_data, moov_size);

    return ret_val;
}

//////////////////////////////////////////////////////////////////////////
// ??????????????ж?????Free Box

unsigned char* Mp4Spliter::stbl_rmfree(unsigned char* p_new_buf, unsigned char* p_old_buf,
                                       unsigned int old_size, unsigned int *p_rmsize  , bool & has_error)
{
    struct atom_t leaf_atom;
    unsigned char* buffer = p_old_buf;
    unsigned int rm_size = 0;

    unsigned int new_buf_lenght = old_size;
    int readed = 0;

    while (buffer < p_old_buf + old_size)
    {
        buffer = atom_read_header(buffer, &leaf_atom);
        // atom_print(&leaf_atom);
        if (atom_is(&leaf_atom, "free"))
        {
            rm_size += leaf_atom.size_;
        }
        else
        {
            // 判断是否越界
            if( readed + leaf_atom.size_ > new_buf_lenght ) {
                has_error = true;
                return p_new_buf;
            }

            base::util::memcpy2(p_new_buf, new_buf_lenght - readed  , leaf_atom.start_, leaf_atom.size_);
            p_new_buf += leaf_atom.size_;

            readed += leaf_atom.size_;
        }

        buffer = atom_skip(buffer, &leaf_atom);
    }
    
    has_error = false;
    (*p_rmsize) += rm_size;
    return p_new_buf;
}

unsigned char* Mp4Spliter::minf_rmfree(unsigned char* p_new_buf, unsigned char* p_old_buf,
                                       unsigned int old_size, unsigned int *p_rmsize , bool & has_error)
{
    struct atom_t leaf_atom;
    unsigned char* buffer = p_old_buf;
    unsigned int rm_size = 0;
    unsigned int new_buf_lenght = old_size;

    int readed = 0;

    while (buffer < p_old_buf + old_size)
    {
        buffer = atom_read_header(buffer, &leaf_atom);
        // atom_print(&leaf_atom);
        if (atom_is(&leaf_atom, "stbl"))
        {
            unsigned char* p_stbl = p_new_buf;
            unsigned char* old_p_new_buf = p_new_buf;
            bool error = false;

            p_new_buf += ATOM_PREAMBLE_SIZE;
            // 判断是否越界
            if( readed + 4 + 4 > new_buf_lenght ) {
                has_error = true;
                return p_new_buf;
            }

            base::util::memcpy2(p_stbl + 4,  new_buf_lenght - readed - 4 , leaf_atom.type_, 4);
            p_new_buf = stbl_rmfree(p_new_buf, leaf_atom.start_ + ATOM_PREAMBLE_SIZE,
                leaf_atom.size_-ATOM_PREAMBLE_SIZE, &rm_size , error);
            if( error ) {
                has_error = true;
                return p_new_buf;
            }

            write_int32(p_stbl, (leaf_atom.size_ - rm_size));

            readed += p_new_buf - old_p_new_buf;
        }
        else if (atom_is(&leaf_atom, "free"))
        {
            rm_size += leaf_atom.size_;
        }
        else
        {
            // 判断是否越界
            if( readed + leaf_atom.size_ > new_buf_lenght ) {
                has_error = true;
                return p_new_buf;
            }

            base::util::memcpy2(p_new_buf, new_buf_lenght - readed  , leaf_atom.start_, leaf_atom.size_);
            p_new_buf += leaf_atom.size_;

            readed += leaf_atom.size_;
        }
        buffer = atom_skip(buffer, &leaf_atom);
    }

    has_error = false;
    (*p_rmsize) += rm_size;
    return p_new_buf;
}

unsigned char* Mp4Spliter::mdia_rmfree(unsigned char* p_new_buf, unsigned char* p_old_buf,
                                       unsigned int old_size, unsigned int *p_rmsize  , bool & has_error)
{
    struct atom_t leaf_atom;
    unsigned char* buffer = p_old_buf;
    unsigned int rm_size = 0;

    unsigned int new_buf_lenght = old_size;
    int readed = 0;

    while (buffer < p_old_buf + old_size)
    {
        buffer = atom_read_header(buffer, &leaf_atom);
        // atom_print(&leaf_atom);
        if (atom_is(&leaf_atom, "minf"))
        {
            unsigned char* old_p_new_buf = p_new_buf;
            bool error = false;

            unsigned char* p_minf = p_new_buf;
            p_new_buf += ATOM_PREAMBLE_SIZE;
            // 判断是否越界
            if( readed + 4 + 4 > new_buf_lenght ) {
                has_error = true;
                return p_new_buf;
            }

            base::util::memcpy2(p_minf + 4, new_buf_lenght - readed - 4 , leaf_atom.type_, 4);
            p_new_buf = minf_rmfree(p_new_buf, leaf_atom.start_ + ATOM_PREAMBLE_SIZE,
                leaf_atom.size_-ATOM_PREAMBLE_SIZE, &rm_size , error);
            if( error ) {
                has_error = true;
                return p_new_buf;
            }

            write_int32(p_minf, (leaf_atom.size_ - rm_size));

            readed += p_new_buf - old_p_new_buf;
        }
        else if (atom_is(&leaf_atom, "free"))
        {
            rm_size += leaf_atom.size_;
        }
        else
        {
            // 判断是否越界
            if( readed + leaf_atom.size_ > new_buf_lenght ) {
                has_error = true;
                return p_new_buf;
            }

            base::util::memcpy2(p_new_buf, new_buf_lenght - readed  , leaf_atom.start_, leaf_atom.size_);
            p_new_buf += leaf_atom.size_;

            readed += leaf_atom.size_;
        }
        buffer = atom_skip(buffer, &leaf_atom);
    }
    (*p_rmsize) += rm_size;
    return p_new_buf;
}

unsigned char* Mp4Spliter::trak_rmfree(unsigned char* p_new_buf, unsigned char* p_old_buf,
                                       unsigned int old_size, unsigned int *p_rmsize  , bool & has_error)
{
    struct atom_t leaf_atom;
    unsigned char* buffer = p_old_buf;
    unsigned int rm_size = 0;

    unsigned int new_buf_lenght = old_size;
    int readed = 0;

    while (buffer < p_old_buf + old_size)
    {
        buffer = atom_read_header(buffer, &leaf_atom);
        // atom_print(&leaf_atom);
        if (atom_is(&leaf_atom, "mdia"))
        {
            unsigned char* old_p_new_buf = p_new_buf;
            bool error = false;

            unsigned char* p_mdia = p_new_buf;
            p_new_buf += ATOM_PREAMBLE_SIZE;
            // 判断是否越界
            if( readed + 4 + 4 > new_buf_lenght ) {
                has_error = true;
                return p_new_buf;
            }

            base::util::memcpy2(p_mdia + 4, new_buf_lenght - readed - 4 , leaf_atom.type_, 4);
            p_new_buf = mdia_rmfree(p_new_buf, leaf_atom.start_ + ATOM_PREAMBLE_SIZE,
                leaf_atom.size_-ATOM_PREAMBLE_SIZE, &rm_size , error);
            if( error ) {
                has_error = true;
                return p_new_buf;
            }

            write_int32(p_mdia, (leaf_atom.size_ - rm_size));

            readed += p_new_buf - old_p_new_buf;
        }
        else if (atom_is(&leaf_atom, "free"))
        {
            rm_size += leaf_atom.size_;
        }
        else
        {
            // 判断是否越界
            if( readed + leaf_atom.size_ > new_buf_lenght ) {
                has_error = true;
                return p_new_buf;
            }

            base::util::memcpy2(p_new_buf, new_buf_lenght - readed  , leaf_atom.start_, leaf_atom.size_);
            p_new_buf += leaf_atom.size_;

            readed += leaf_atom.size_;
        }
        buffer = atom_skip(buffer, &leaf_atom);
    }
    (*p_rmsize) += rm_size;
    return p_new_buf;
}

unsigned char* Mp4Spliter::moov_rmfree(unsigned char* p_new_buf, unsigned char* p_old_buf,
                                       unsigned int old_size, unsigned int *p_rmsize , bool & has_error)
{
    struct atom_t leaf_atom;
    unsigned char* buffer = p_old_buf;
    unsigned int rm_size = 0;
    unsigned char* old_p_new_buf = p_new_buf;

    unsigned int new_buf_lenght = old_size;
    int readed = 0;

    int used_size = 0;
    while (buffer < p_old_buf + old_size)
    {
        buffer = atom_read_header(buffer, &leaf_atom);
        // atom_print(&leaf_atom);
        if (atom_is(&leaf_atom, "trak"))
        {
            unsigned char* old_p_new_buf1 = p_new_buf;
            bool error = false;

            unsigned char* p_trak = p_new_buf;
            unsigned int rm_trak_size = 0;
            p_new_buf += ATOM_PREAMBLE_SIZE;
            // 判断是否越界
            if( readed + 4 + 4 > new_buf_lenght ) {
                has_error = true;
                return p_new_buf;
            }

            base::util::memcpy2(p_trak + 4, new_buf_lenght - readed - 4 , leaf_atom.type_, 4);
            p_new_buf = trak_rmfree(p_new_buf, leaf_atom.start_ + ATOM_PREAMBLE_SIZE,
                leaf_atom.size_-ATOM_PREAMBLE_SIZE, &rm_trak_size , error);
            write_int32(p_trak, (leaf_atom.size_ - rm_trak_size));
            rm_size += rm_trak_size;
            if( error ) {
                has_error = true;
                return p_new_buf;
            }

            readed += p_new_buf - old_p_new_buf1;
        }
        else if (atom_is(&leaf_atom, "free"))
        {
            rm_size += leaf_atom.size_;
        }
        else
        {
            if (leaf_atom.size_ > old_size - used_size)
            {
                has_error = true;
                return old_p_new_buf;
            }

            base::util::memcpy2(p_new_buf, new_buf_lenght - readed  , leaf_atom.start_, leaf_atom.size_);
            p_new_buf += leaf_atom.size_;
        }

        used_size += leaf_atom.size_;
        buffer = atom_skip(buffer, &leaf_atom);
    }
    (*p_rmsize) += rm_size;
    return p_new_buf;
}

MoovSeekRet Mp4Spliter::rm_freeatom(unsigned char* moov_data, SNC_UI64 moov_size)
{
    unsigned char* p_old_buf = moov_data;
    unsigned char* new_moov_buf = (unsigned char*)malloc(moov_size);
    unsigned char* p_new_buf = new_moov_buf;
    unsigned int new_moov_size = 0;
    unsigned int rm_size = 0;
    MoovSeekRet ret_val;
    ret_val.pmoov = 0;
    bool has_error = false;

    memset(&ret_val, 0, sizeof(MoovSeekRet));
    base::util::memcpy2(new_moov_buf + 4 , moov_size - 4, "moov", 4);
    p_new_buf += ATOM_PREAMBLE_SIZE;
    p_new_buf = moov_rmfree(p_new_buf, p_old_buf + ATOM_PREAMBLE_SIZE,
        moov_size - ATOM_PREAMBLE_SIZE, &rm_size , has_error);
    if (has_error)
    {
        free(new_moov_buf);
        return ret_val;
    }

    write_int32(new_moov_buf, (moov_size - rm_size));
    new_moov_size = (unsigned int)(p_new_buf - new_moov_buf);

    {
        struct moov_t* moov = (moov_t*)malloc(sizeof(struct moov_t));
        moov_init(moov);
        if (-1 == moov_parse(moov, new_moov_buf + ATOM_PREAMBLE_SIZE, new_moov_size - ATOM_PREAMBLE_SIZE))
        {
            // 原来有内存泄漏
            free(new_moov_buf);
            moov_exit(moov);
            free(moov);
            return ret_val;
        }

        int rm_size_int = (int)rm_size;
        moov_shift_offsets(moov, -rm_size_int);

        moov_exit(moov);
        free(moov);
    }

    // memcpy(moov_data, new_moov_buf, new_moov_size);
    // (*moov_size) = new_moov_size;
    // free(new_moov_buf);
    ret_val.pmoov = new_moov_buf;
    ret_val.moov_size = new_moov_size;
    return ret_val;
}

//////////////////////////////////////////////////////////////////////////
// parse

int Mp4Spliter::stbl_parse(struct stbl_t* stbl, unsigned char* buffer, unsigned int size , int left_size)
{
    struct atom_t leaf_atom;
    unsigned char* buffer_start = buffer;
    stbl->stss_ = 0;
    stbl->ctts_ = 0;

    stbl->start_ = buffer;
    stbl->new_ = (unsigned char*)calloc(1, size);
    if (0 == stbl->new_) return -1;
    stbl->newp_ = stbl->new_;
    stbl->newp_size_ = size;
    //    stbl->size_ = size;

    int used_size = 0;
    while (buffer < buffer_start + size)
    {
        buffer = atom_read_header(buffer, &leaf_atom);

        if (leaf_atom.size_ < ATOM_PREAMBLE_SIZE) return -1;

        // atom_print(&leaf_atom);

        if (atom_is(&leaf_atom, "stts"))
        {
            stbl->stts_ = buffer;
        }
        else if (atom_is(&leaf_atom, "stss"))
        {
            stbl->stss_ = buffer;
        }
        else if (atom_is(&leaf_atom, "stsc"))
        {
            stbl->stsc_ = buffer;
        }
        else if (atom_is(&leaf_atom, "stsz"))
        {
            stbl->stsz_ = buffer;
        }
        else if (atom_is(&leaf_atom, "stco"))
        {
            stbl->stco_ = buffer;
        }
        else if (atom_is(&leaf_atom, "co64"))
        {
            // printf("TODO: co64");
        }
        else if (atom_is(&leaf_atom, "ctts"))
        {
            stbl->ctts_ = buffer;
        }
        else
        {
            if ((!leaf_atom.type_[0] || !leaf_atom.type_[0] || !leaf_atom.type_[0] || !leaf_atom.type_[0])
                && leaf_atom.size_ > 10485760)
                return -1;

            // 长度不够了
            if (left_size - used_size < leaf_atom.size_)
                return -1;

            // copy unknown/unused atoms directly (e.g. stsd)
            base::util::memcpy2(stbl->newp_, left_size - used_size, buffer - ATOM_PREAMBLE_SIZE, leaf_atom.size_);
            stbl->newp_ += leaf_atom.size_;
        }

        used_size += leaf_atom.size_;
        buffer = atom_skip(buffer, &leaf_atom);
    }

    return 0;
}

int Mp4Spliter::minf_parse(struct minf_t* minf, unsigned char* buffer, unsigned int size , int left_size)
{
    struct atom_t leaf_atom;
    unsigned char* buffer_start = buffer;

    minf->start_ = buffer;

    int used_size = 0;
    while (buffer < buffer_start + size)
    {
        buffer = atom_read_header(buffer, &leaf_atom);

        if (leaf_atom.size_ < ATOM_PREAMBLE_SIZE) return -1;

        used_size += ATOM_PREAMBLE_SIZE;
        // atom_print(&leaf_atom);

        if (atom_is(&leaf_atom, "stbl"))
        {
            if (-1 == stbl_parse(&minf->stbl_, buffer, leaf_atom.size_ - ATOM_PREAMBLE_SIZE , left_size - used_size))
            {
                return -1;
            }
        }

        used_size += (leaf_atom.size_ - ATOM_PREAMBLE_SIZE);
        buffer = atom_skip(buffer, &leaf_atom);
    }

    return 0;
}

int Mp4Spliter::mdia_parse(struct mdia_t* mdia, unsigned char* buffer, unsigned int size, int left_size)
{
    struct atom_t leaf_atom;
    unsigned char* buffer_start = buffer;

    mdia->start_ = buffer;

    int used_size = 0;
    while (buffer < buffer_start + size)
    {
        buffer = atom_read_header(buffer, &leaf_atom);

        if (leaf_atom.size_ < ATOM_PREAMBLE_SIZE) return -1;
        used_size += ATOM_PREAMBLE_SIZE;

        // atom_print(&leaf_atom);

        if (atom_is(&leaf_atom, "mdhd"))
        {
            mdia->mdhd_ = buffer;
        }
        else if (atom_is(&leaf_atom, "minf"))
        {
            if (-1 == minf_parse(&mdia->minf_, buffer, leaf_atom.size_ - ATOM_PREAMBLE_SIZE , left_size - used_size))
            {
                return -1;
            }
        }
        else if (atom_is(&leaf_atom, "hdlr"))
        {
            mdia->hdlr_ = buffer;
        }

        used_size += (leaf_atom.size_ - ATOM_PREAMBLE_SIZE);
        buffer = atom_skip(buffer, &leaf_atom);
    }

    return 0;
}

void Mp4Spliter::trak_init(struct trak_t* trak)
{
    trak->chunks_ = 0;
    trak->samples_ = 0;
    trak->samples_size_ = 0;
}

void Mp4Spliter::trak_exit(struct trak_t* trak)
{
    if (trak->chunks_)
        free(trak->chunks_);
    if (trak->samples_)
        free(trak->samples_);

    if (trak->mdia_.minf_.stbl_.new_)
    {
        free(trak->mdia_.minf_.stbl_.new_);
        trak->mdia_.minf_.stbl_.new_ = 0;
        trak->mdia_.minf_.stbl_.newp_ = 0;
        trak->mdia_.minf_.stbl_.newp_size_ = 0;
    }
}

int Mp4Spliter::trak_parse(struct trak_t* trak, unsigned char* buffer, unsigned int size , int left_size)
{
    struct atom_t leaf_atom;
    unsigned char* buffer_start = buffer;

    trak->start_ = buffer;

    int used_size = 0;
    while (buffer < buffer_start + size)
    {
        buffer = atom_read_header(buffer, &leaf_atom);

        if (leaf_atom.size_ < ATOM_PREAMBLE_SIZE) return -1;

        used_size += ATOM_PREAMBLE_SIZE;
        // atom_print(&leaf_atom);

        if (atom_is(&leaf_atom, "tkhd"))
        {
            trak->tkhd_ = buffer;
        }
        else if (atom_is(&leaf_atom, "mdia"))
        {
            if (-1 == mdia_parse(&trak->mdia_, buffer, leaf_atom.size_ - ATOM_PREAMBLE_SIZE , left_size - used_size))
            {
                return -1;
            }
        }

        used_size += (leaf_atom.size_ - ATOM_PREAMBLE_SIZE);
        buffer = atom_skip(buffer, &leaf_atom);
    }

    return 0;
}

void Mp4Spliter::moov_init(struct moov_t* moov)
{
    moov->tracks_ = 0;
}

void Mp4Spliter::moov_exit(struct moov_t* moov)
{
    unsigned int i;
    for (i = 0; i != moov->tracks_; ++i)
    {
        trak_exit(&moov->traks_[i]);
    }
}

int Mp4Spliter::trak_build_index(struct trak_t* trak)
{
    const unsigned char* stco = trak->mdia_.minf_.stbl_.stco_;

    trak->chunks_size_ = stco_get_entries(stco);
    trak->chunks_ = (struct chunks_t*)malloc(trak->chunks_size_ * sizeof(struct chunks_t));
    if (0 == trak->chunks_) return -1;

    {
        unsigned int i;
        for (i = 0; i != trak->chunks_size_; ++i)
        {
            trak->chunks_[i].pos_ = stco_get_offset(stco, i);
        }
    }

    // process chunkmap:
    {
        const unsigned char* stsc = trak->mdia_.minf_.stbl_.stsc_;
        unsigned int last = trak->chunks_size_;
        unsigned int i = stsc_get_entries(stsc);
        while (i > 0)
        {
            struct stsc_table_t stsc_table;
            unsigned int j;

            --i;

            stsc_get_table(stsc, i, &stsc_table);
            for (j = stsc_table.chunk_; j < last; j++)
            {
                trak->chunks_[j].id_ = stsc_table.id_;
                trak->chunks_[j].size_ = stsc_table.samples_;
            }
            last = stsc_table.chunk_;
        }
    }

    // calc pts of chunks:
    {
        const unsigned char* stsz = trak->mdia_.minf_.stbl_.stsz_;
        unsigned int sample_size = stsz_get_sample_size(stsz);
        unsigned int s = 0;
        {
            unsigned int j;
            for (j = 0; j < trak->chunks_size_; j++)
            {
                trak->chunks_[j].sample_ = s;
                s += trak->chunks_[j].size_;
            }
        }

        if (sample_size == 0)
        {
            trak->samples_size_ = stsz_get_entries(stsz);
        }
        else
        {
            trak->samples_size_ = s;
        }

        trak->samples_ = (samples_t*)malloc(trak->samples_size_ * sizeof(struct samples_t));
        if (0 == trak->samples_) return -1;

        if (sample_size == 0)
        {
            unsigned int i;
            for (i = 0; i != trak->samples_size_; ++i)
            {
                trak->samples_[i].size_ = stsz_get_size(stsz, i);
            }
        }
        else
        {
            unsigned int i;
            for (i = 0; i != trak->samples_size_; ++i)
            {
                trak->samples_[i].size_ = sample_size;
            }
        }
    }

    // calc pts:
    {
        const unsigned char* stts = trak->mdia_.minf_.stbl_.stts_;
        unsigned int s = 0;
        unsigned int pts = 0;
        unsigned int entries = stts_get_entries(stts);
        unsigned int j;
        for (j = 0; j < entries; j++)
        {
            unsigned int i;
            unsigned int sample_count;
            unsigned int sample_duration;
            stts_get_sample_count_and_duration(stts, j,
                &sample_count, &sample_duration);
            for (i = 0; i < sample_count; i++)
            {
                if (s >= trak->samples_size_) break;

                trak->samples_[s].pts_ = pts;
                ++s;
                pts += sample_duration;
            }
        }
    }

    // calc composition times:
    {
        const unsigned char* ctts = trak->mdia_.minf_.stbl_.ctts_;
        if (ctts)
        {
            unsigned int s = 0;
            unsigned int entries = ctts_get_entries(ctts);
            unsigned int j;
            for (j = 0; j < entries; j++)
            {
                unsigned int i;
                unsigned int sample_count;
                unsigned int sample_offset;
                ctts_get_sample_count_and_offset(ctts, j, &sample_count, &sample_offset);
                for (i = 0; i < sample_count; i++)
                {
                    if (s >= trak->samples_size_) break;

                    trak->samples_[s].cto_ = sample_offset;
                    ++s;
                }
            }
        }
    }

    // calc sample offsets
    {
        unsigned int s = 0;
        unsigned int j;
        for (j = 0; j != trak->chunks_size_; j++)
        {
            SNC_UI64 pos = trak->chunks_[j].pos_;
            unsigned int i;
            for (i = 0; i != trak->chunks_[j].size_; i++)
            {
                if (s >= trak->samples_size_) break;

                trak->samples_[s].pos_ = pos;
                pos += trak->samples_[s].size_;
                ++s;
            }
        }
    }

    return 0;
}

int Mp4Spliter::trak_write_index(struct trak_t* trak, unsigned int start, unsigned int end)
{
    // write samples [start, end>

    unsigned int stts_off, ctts_off, stsc_off, stco_off, stss_off, stsz_off;
    unsigned char* newp = trak->mdia_.minf_.stbl_.newp_;
    unsigned char* tmp_new = trak->mdia_.minf_.stbl_.new_;

    unsigned char * max_newp = newp + trak->mdia_.minf_.stbl_.newp_size_;

    // stts = [entries * [sample_count, sample_duration]
    {
        unsigned char* stts_atom_start = newp;
        unsigned char* stts = trak->mdia_.minf_.stbl_.stts_;

        unsigned int entries = 0;
        //    struct stts_table_t const* table = (struct stts_table_t*)(stts + 8);
        unsigned int s;

        stts_off = newp + ATOM_PREAMBLE_SIZE - tmp_new;
        // copy header
        // atom + version + flags

        // nightsuns: 判断是否越界
        if( max_newp < ATOM_PREAMBLE_SIZE + 4 + newp ) {
            return -1;
        }

        base::util::memcpy2(newp, max_newp - newp , stts - ATOM_PREAMBLE_SIZE, ATOM_PREAMBLE_SIZE + 4);
        newp += ATOM_PREAMBLE_SIZE + 4;
        newp += 4;  // Number Of Entries

        for (s = start; s != end; ++s)
        {
            unsigned int sample_count = 1;
            unsigned int sample_duration =
                trak->samples_[s + 1].pts_ - trak->samples_[s].pts_;
            while (s != end - 1)
            {
                if ((trak->samples_[s + 1].pts_ - trak->samples_[s].pts_) != sample_duration)
                    break;
                ++sample_count;
                ++s;
            }
            // write entry

            // nightsuns: 判断是否越界
            if( max_newp < 4 + newp ) {
                return -1;
            }
            write_int32(newp, sample_count);
            newp += 4;

            // nightsuns: 判断是否越界
            if( max_newp < 4 + newp ) {
                return -1;
            }
            write_int32(newp, sample_duration);
            newp += 4;

            ++entries;
        }
        stts = trak->mdia_.minf_.stbl_.stts_ = stts_atom_start + ATOM_PREAMBLE_SIZE;
        write_int32(stts_atom_start + ATOM_PREAMBLE_SIZE + 4, entries);
        write_int32(stts_atom_start, newp - stts_atom_start);
        //    printf("Atom(%c%c%c%c,%d)\n", stts_atom_start[4], stts_atom_start[5],
        //            stts_atom_start[6], stts_atom_start[7], read_int32(stts_atom_start));
        if (stts_get_samples(stts_atom_start + ATOM_PREAMBLE_SIZE) != end - start)
        {
            //      printf("ERROR: stts_get_samples=%d, should be %d\n",
            //             stts_get_samples(stts_atom_start + ATOM_PREAMBLE_SIZE), end - start);
        }
    }

    // ctts = [entries * [sample_count, sample_offset]
    {
        unsigned char* ctts_atom_start = newp;
        unsigned char* ctts = trak->mdia_.minf_.stbl_.ctts_;
        if (ctts)
        {
            unsigned int entries = 0;
            //      struct ctts_table_t const* table = (struct ctts_table_t*)(ctts + 8);
            unsigned int s;

            ctts_off = newp + ATOM_PREAMBLE_SIZE - tmp_new;
            // copy header
            // atom + version + flags
            
            // nightsuns: 判断是否越界
            if( max_newp < ATOM_PREAMBLE_SIZE + 4 + newp ) {
                return -1;
            }

            base::util::memcpy2(newp, max_newp - newp , ctts - ATOM_PREAMBLE_SIZE, ATOM_PREAMBLE_SIZE + 4);
            newp += ATOM_PREAMBLE_SIZE + 4;
            newp += 4;  // Number Of Entries

            for (s = start; s != end; ++s)
            {
                unsigned int sample_count = 1;
                unsigned int sample_offset = trak->samples_[s].cto_;
                while (s != end - 1)
                {
                    if (trak->samples_[s + 1].cto_ != sample_offset)
                        break;
                    ++sample_count;
                    ++s;
                }
                // write entry

                // nightsuns: 判断是否越界
                if( max_newp < 4 + newp ) {
                    return -1;
                }
                write_int32(newp, sample_count);
                newp += 4;

                // nightsuns: 判断是否越界
                if( max_newp < 4 + newp ) {
                    return -1;
                }
                write_int32(newp, sample_offset);
                newp += 4;

                ++entries;
            }
            trak->mdia_.minf_.stbl_.ctts_ = ctts_atom_start + ATOM_PREAMBLE_SIZE;
            write_int32(ctts_atom_start + ATOM_PREAMBLE_SIZE + 4, entries);
            write_int32(ctts_atom_start, newp - ctts_atom_start);
            //      printf("Atom(%c%c%c%c,%d)\n", ctts_atom_start[4], ctts_atom_start[5],
            //              ctts_atom_start[6], ctts_atom_start[7], read_int32(ctts_atom_start));
            if (ctts_get_samples(ctts_atom_start + ATOM_PREAMBLE_SIZE) != end - start)
            {
                //        printf("ERROR: ctts_get_samples=%d, should be %d\n",
                //               ctts_get_samples(ctts_atom_start + ATOM_PREAMBLE_SIZE), end - start);
            }
        }
    }

    // process chunkmap:
    {
        unsigned char* stsc_atom_start = newp;
        unsigned char* stsc = trak->mdia_.minf_.stbl_.stsc_;
        //    struct stsc_table_t const* stsc_table = (struct stsc_table_t*)(stsc + 8);
        unsigned int i;

        stsc_off = newp + ATOM_PREAMBLE_SIZE - tmp_new;
        // copy header
        // atom + version + flags

        // nightsuns: 判断是否越界
        if( max_newp < ATOM_PREAMBLE_SIZE + 4 + newp ) {
            return -1;
        }
        base::util::memcpy2(newp, max_newp - newp , stsc - ATOM_PREAMBLE_SIZE, ATOM_PREAMBLE_SIZE + 4);
        newp += ATOM_PREAMBLE_SIZE + 4;
        newp += 4;  // Number Of Entries

        for (i = 0; i != trak->chunks_size_; ++i)
        {
            if (trak->chunks_[i].sample_ + trak->chunks_[i].size_ > start)
                break;
        }

        {
            unsigned int stsc_entries = 0;
            unsigned int chunk_start = i;
            unsigned int last_i;
            unsigned int chunk_end;
            // problem.mp4: reported by Jin-seok Lee. Second track contains no samples
            if (trak->chunks_size_ != 0)
            {
                unsigned int samples =
                    trak->chunks_[i].sample_ + trak->chunks_[i].size_ - start;
                unsigned int id = trak->chunks_[i].id_;

                // write entry [chunk, samples, id]

                // nightsuns: 判断是否越界
                if( max_newp < 4 + newp ) {
                    return -1;
                }
                write_int32(newp, 1);
                newp += 4;

                // nightsuns: 判断是否越界
                if( max_newp < 4 + newp ) {
                    return -1;
                }
                write_int32(newp, samples);
                newp += 4;

                // nightsuns: 判断是否越界
                if( max_newp < 4 + newp ) {
                    return -1;
                }
                write_int32(newp, id);
                newp += 4;
                ++stsc_entries;
                last_i = i;
                if (i != trak->chunks_size_)
                {
                    for (i += 1; i != trak->chunks_size_; ++i)
                    {
                        if (trak->chunks_[i].sample_ >= end)
                            break;

                        if (trak->chunks_[i].size_ != samples)
                        {
                            samples = trak->chunks_[i].size_;
                            id = trak->chunks_[i].id_;

                            // nightsuns: 判断是否越界
                            if( max_newp < 4 + newp ) {
                                return -1;
                            }
                            write_int32(newp, i - chunk_start + 1);
                            newp += 4;

                            // nightsuns: 判断是否越界
                            if( max_newp < 4 + newp ) {
                                return -1;
                            }
                            write_int32(newp, samples);
                            newp += 4;

                            // nightsuns: 判断是否越界
                            if( max_newp < 4 + newp ) {
                                return -1;
                            }
                            write_int32(newp, id);
                            newp += 4;
                            ++stsc_entries;
                            last_i = i;
                        }
                    }

                    if (i == trak->chunks_size_)
                    {
                        unsigned int total_sams = 0;
                        unsigned int tmp_i = 0;
                        for (; tmp_i < trak->chunks_size_; ++tmp_i)
                        {
                            total_sams += trak->chunks_[tmp_i].size_;
                        }
                        if (total_sams > end)
                        {
                            unsigned int dif = total_sams - end;
                            if ((i - 1) == last_i)
                            {
                                // 这里可以不用管
                                newp -= 12;
                                // write_int32(newp, i - chunk_start + 1);
                                newp += 4;
                                write_int32(newp, samples - dif);
                                newp += 4;
                                // write_int32(newp, id);
                                newp += 4;
                            }
                            else
                            {
                                // nightsuns: 判断是否越界
                                if( max_newp < 4 + newp ) {
                                    return -1;
                                }
                                write_int32(newp, i - chunk_start + 1 -1);
                                newp += 4;

                                // nightsuns: 判断是否越界
                                if( max_newp < 4 + newp ) {
                                    return -1;
                                }
                                write_int32(newp, samples - dif);
                                newp += 4;

                                // nightsuns: 判断是否越界
                                if( max_newp < 4 + newp ) {
                                    return -1;
                                }
                                write_int32(newp, id);
                                newp += 4;
                                ++stsc_entries;
                                // i++;
                            }
                        }
                    }
                    else    // i != trak->chunks_size_
                    {
                        if (trak->chunks_[i].sample_ > end)
                        {
                            unsigned int dif = trak->chunks_[i].sample_ - end;
                            if ((i - 1) == last_i)
                            {
                                // 不用管
                                newp -= 12;
                                // write_int32(newp, i - chunk_start + 1);
                                newp += 4;
                                write_int32(newp, samples - dif);
                                newp += 4;
                                // write_int32(newp, id);
                                newp += 4;
                            }
                            else
                            {
                                // nightsuns: 判断是否越界
                                if( max_newp < 4 + newp ) {
                                    return -1;
                                }
                                write_int32(newp, i - chunk_start + 1 -1);
                                newp += 4;

                                // nightsuns: 判断是否越界
                                if( max_newp < 4 + newp ) {
                                    return -1;
                                }
                                write_int32(newp, samples - dif);
                                newp += 4;

                                // nightsuns: 判断是否越界
                                if( max_newp < 4 + newp ) {
                                    return -1;
                                }
                                write_int32(newp, id);
                                newp += 4;
                                ++stsc_entries;
                                // i++;
                            }
                        }
                    }
                }
            }
            chunk_end = i;
            trak->mdia_.minf_.stbl_.stsc_ = stsc_atom_start + ATOM_PREAMBLE_SIZE;
            write_int32(stsc_atom_start + ATOM_PREAMBLE_SIZE + 4, stsc_entries);
            write_int32(stsc_atom_start, newp - stsc_atom_start);
            //      printf("Atom(%c%c%c%c,%d)\n", stsc_atom_start[4], stsc_atom_start[5],
            //              stsc_atom_start[6], stsc_atom_start[7], read_int32(stsc_atom_start));

            {
                unsigned char* stco_atom_start = newp;
                unsigned char const* stco = trak->mdia_.minf_.stbl_.stco_;
                unsigned int entries = chunk_end;
                SNC_UI32 const* stco_table = (SNC_UI32*)(stco + 8);

                stco_off = newp + ATOM_PREAMBLE_SIZE - tmp_new;
                // copy header
                // atom + version + flags

                // nightsuns: 判断是否越界
                if( max_newp < ATOM_PREAMBLE_SIZE + 4 + newp ) {
                    return -1;
                }
                base::util::memcpy2(newp, max_newp - newp , stco - ATOM_PREAMBLE_SIZE, ATOM_PREAMBLE_SIZE + 4);
                newp += ATOM_PREAMBLE_SIZE + 4;
                newp += 4;  // Number Of Entries

                for (i = chunk_start; i != entries; ++i)
                {
                    unsigned int tmp_csize = read_int32(&stco_table[i]);
                    // nightsuns: 判断是否越界
                    if( max_newp < 4 + newp ) {
                        return -1;
                    }
                    write_int32(newp, tmp_csize);
                    newp += 4;
                }
                trak->mdia_.minf_.stbl_.stco_ = stco_atom_start + ATOM_PREAMBLE_SIZE;
                write_int32(stco_atom_start + ATOM_PREAMBLE_SIZE + 4, entries - chunk_start);
                write_int32(stco_atom_start, newp - stco_atom_start);
                //        printf("Atom(%c%c%c%c,%d)\n", stco_atom_start[4], stco_atom_start[5],
                //                stco_atom_start[6], stco_atom_start[7], read_int32(stco_atom_start));

                // patch first chunk with correct sample offset
                write_int32(stco_atom_start + ATOM_PREAMBLE_SIZE + 8,
                    (SNC_UI32)trak->samples_[start].pos_);
            }
        }
    }

    // process sync samples:
    if (trak->mdia_.minf_.stbl_.stss_)
    {
        unsigned char* stss_atom_start = newp;
        unsigned char* stss = trak->mdia_.minf_.stbl_.stss_;
        unsigned int entries = read_int32(stss + 4);
        SNC_UI32 const* table = (SNC_UI32*)(stss + 8);
        unsigned int stss_start;
        unsigned int i;

        stss_off = newp + ATOM_PREAMBLE_SIZE - tmp_new;
        // copy header
        // atom + version + flags

        // nightsuns: 判断是否越界
        if( max_newp < ATOM_PREAMBLE_SIZE + 4 + newp ) {
            return -1;
        }
        base::util::memcpy2(newp, max_newp - newp , stss - ATOM_PREAMBLE_SIZE, ATOM_PREAMBLE_SIZE + 4);
        newp += ATOM_PREAMBLE_SIZE + 4;
        newp += 4;  // Number Of Entries

        for (i = 0; i != entries; ++i)
        {
            if (read_int32(&table[i]) >= start + 1)
                break;
        }
        stss_start = i;
        for (; i != entries; ++i)
        {
            unsigned int sync_sample = read_int32(&table[i]);
            if (sync_sample >= end + 1)
                break;

            // nightsuns: 判断是否越界
            if( max_newp < 4 + newp ) {
                return -1;
            }
            write_int32(newp, sync_sample - start);
            newp += 4;
        }
        trak->mdia_.minf_.stbl_.stss_ = stss_atom_start + ATOM_PREAMBLE_SIZE;
        write_int32(stss_atom_start + ATOM_PREAMBLE_SIZE + 4, i - stss_start);
        write_int32(stss_atom_start, newp - stss_atom_start);
        //    printf("Atom(%c%c%c%c,%d)\n", stss_atom_start[4], stss_atom_start[5],
        //            stss_atom_start[6], stss_atom_start[7], read_int32(stss_atom_start));
    }

    // process sample sizes
    {
        unsigned char* stsz_atom_start = newp;
        unsigned char* stsz = trak->mdia_.minf_.stbl_.stsz_;

        stsz_off = newp + ATOM_PREAMBLE_SIZE - tmp_new;
        // copy header
        // atom + version + flags, sample_size, number_of_etries

        // nightsuns: 判断是否越界
        if( max_newp < ATOM_PREAMBLE_SIZE + 12 + newp ) {
            return -1;
        }

        base::util::memcpy2(newp, max_newp - newp , stsz - ATOM_PREAMBLE_SIZE, ATOM_PREAMBLE_SIZE + 12);
        newp += ATOM_PREAMBLE_SIZE + 12;

        if (stsz_get_sample_size(stsz) == 0)
        {
            SNC_UI32 const* table = (SNC_UI32*)(stsz + 12);
            // nightsuns: 判断是否越界
            if( max_newp < ((end - start) * sizeof(SNC_UI32) )+ newp ) {
                return -1;
            }

            base::util::memcpy2(newp, max_newp - newp , &table[start], (end - start) * sizeof(SNC_UI32));
            newp += (end - start) * sizeof(SNC_UI32);
            write_int32(stsz_atom_start + ATOM_PREAMBLE_SIZE + 8, end - start);
        }
        trak->mdia_.minf_.stbl_.stsz_ = stsz_atom_start + ATOM_PREAMBLE_SIZE;
        write_int32(stsz_atom_start, newp - stsz_atom_start);
    }

    trak->mdia_.minf_.stbl_.newp_ = newp;

    // copy newly generated stbl over old one
    {
        struct stbl_t* stbl = &trak->mdia_.minf_.stbl_;

        unsigned int old_stbl_size = read_int32(stbl->start_ - ATOM_PREAMBLE_SIZE);
        unsigned int new_stbl_size = stbl->newp_ - stbl->new_ + ATOM_PREAMBLE_SIZE;

        if (new_stbl_size > old_stbl_size)
        {
            return -1;
        }

        base::util::memcpy2(stbl->start_, old_stbl_size , stbl->new_, new_stbl_size - ATOM_PREAMBLE_SIZE);
        //    write_int32(stbl->start_ - ATOM_PREAMBLE_SIZE, new_stbl_size);

        // relocate stbl pointers
        if (stbl->stts_)
            stbl->stts_ += stbl->start_ - stbl->new_;
        // stbl->stts_ = stbl->start_ + stts_off;
        if (stbl->stss_)
            stbl->stss_ += stbl->start_ - stbl->new_;
        // stbl->stss_ = stbl->start_ + stss_off;
        if (stbl->stsc_)
            stbl->stsc_ += stbl->start_ - stbl->new_;
        // stbl->stsc_ = stbl->start_ + stsc_off;
        if (stbl->stsz_)
            stbl->stsz_ += stbl->start_ - stbl->new_;
        // stbl->stsz_ = stbl->start_ + stsz_off;
        if (stbl->stco_)
            stbl->stco_ += stbl->start_ - stbl->new_;
        // stbl->stco_ = stbl->start_ + stco_off;
        if (stbl->ctts_)
            stbl->ctts_ += stbl->start_ - stbl->new_;
        // stbl->ctts_ = stbl->start_ + ctts_off;

        free(trak->mdia_.minf_.stbl_.new_);
        trak->mdia_.minf_.stbl_.new_ = 0;
        trak->mdia_.minf_.stbl_.newp_ = 0;
        trak->mdia_.minf_.stbl_.newp_size_ = 0;

        // add free atom for left over
        if (new_stbl_size < old_stbl_size - ATOM_PREAMBLE_SIZE)
        {
            unsigned int free_size = old_stbl_size - new_stbl_size;
            unsigned char* free_atom = stbl->start_ + new_stbl_size - ATOM_PREAMBLE_SIZE;
            write_int32(free_atom, free_size);
            free_atom[4] = 'f';
            free_atom[5] = 'r';
            free_atom[6] = 'e';
            free_atom[7] = 'e';
            {
                const char free_bytes[8] =
                {
                    'j', 'u', 's', 't', 'f', 'r', 'e', 'e'
                };
                SNC_UI32 padding_index;
                for (padding_index = ATOM_PREAMBLE_SIZE; padding_index != free_size; ++padding_index)
                {
                    free_atom[padding_index] = free_bytes[padding_index % 8];
                }
            }
        }
    }

    return 0;
}

int Mp4Spliter::moov_parse(struct moov_t* moov, unsigned char* buffer, SNC_UI64 size)
{
    struct atom_t leaf_atom;
    unsigned char* buffer_start = buffer;

    moov->start_ = buffer;

    int used_size = 0;
    while (buffer < buffer_start + size)
    {
        buffer = atom_read_header(buffer, &leaf_atom);

        used_size += ATOM_PREAMBLE_SIZE;
        if (leaf_atom.size_ < ATOM_PREAMBLE_SIZE) return -1;

        // atom_print(&leaf_atom);

        if (atom_is(&leaf_atom, "cmov"))
        {
            return -1;
        }
        else if (atom_is(&leaf_atom, "mvhd"))
        {
            moov->mvhd_ = buffer;
        }
        else if (atom_is(&leaf_atom, "trak"))
        {
            if (moov->tracks_ < MAX_TRACKS)
            {
                struct trak_t* trak = &moov->traks_[moov->tracks_];
                trak_init(trak);
                if (-1 == trak_parse(trak, buffer, leaf_atom.size_ - ATOM_PREAMBLE_SIZE , size - used_size))
                {
                    return -1;
                }
                ++moov->tracks_;
            }
            else
            {
                // clayton.mp4 has a third track with one sample that lasts the whole
                // clip. Assuming the first two tracks are the audio and video track,
                // we patch the remaining tracks to 'free' atoms.
                unsigned char* p = buffer - 4;
                p[0] = 'f';
                p[1] = 'r';
                p[2] = 'e';
                p[3] = 'e';
            }
        }

        used_size += (leaf_atom.size_ - ATOM_PREAMBLE_SIZE);
        buffer = atom_skip(buffer, &leaf_atom);
    }

    // build the indexing tables
    {
        unsigned int i;
        for (i = 0; i != moov->tracks_; ++i)
        {
            if (-1 == trak_build_index(&moov->traks_[i]))
            {
                return -1;
            }
        }
    }

    return 0;
}

int Mp4Spliter::stco_shift_offsets(unsigned char* stco, int offset)
{
    unsigned int entries = read_int32(stco + 4);
    unsigned int* table = (unsigned int*)(stco + 8);
    unsigned int i;
    int err_code = 0;
    for (i = 0; i != entries; ++i) {
        int last_pos = read_int32(&table[i]);
        if ((last_pos + offset) >= 0) {
            write_int32(&table[i], (SNC_UI32)(last_pos + offset));
        }
        else {
            write_int32(&table[i], 0);
            err_code = -1;
        }
    }
    return err_code;
}

int Mp4Spliter::trak_shift_offsets(struct trak_t* trak, SNC_I64 offset)
{
    unsigned char* stco = trak->mdia_.minf_.stbl_.stco_;
    return stco_shift_offsets(stco, (SNC_I32)offset);
}

void Mp4Spliter::moov_shift_offsets(struct moov_t* moov, SNC_I64 offset)
{
    unsigned int i;
    for (i = 0; i != moov->tracks_; ++i)
    {
        trak_shift_offsets(&moov->traks_[i], offset);
    }
}

long Mp4Spliter::mvhd_get_time_scale(unsigned char* mvhd)
{
    int version = read_char(mvhd);
    unsigned char* p = mvhd + (version == 0 ? 12 : 20);
    return read_int32(p);
}

SNC_UI64 Mp4Spliter::mvhd_get_duration(unsigned char* mvhd)
{
    int version = read_char(mvhd);
    if (version == 0) {
        unsigned char* p = mvhd + 16;
        return read_int32(p);
    }
    else {  // version == 1
        unsigned char* p = mvhd + 24;
        return read_int64(p);
    }
}

void Mp4Spliter::mvhd_set_duration(unsigned char* mvhd, SNC_UI64 duration)
{
    int version = read_char(mvhd);
    if (version == 0)
    {
        write_int32(mvhd + 16, (SNC_UI32)duration);
    }
    else
    {
        write_int64(mvhd + 24, duration);
    }
}

long Mp4Spliter::mdhd_get_time_scale(unsigned char* mdhd)
{
    int version = read_char(mdhd);
    unsigned char* p = mdhd + (version == 0 ? 12 : 20);

    return read_int32(p);
}

SNC_UI64 Mp4Spliter::mdhd_get_duration(unsigned char* mdhd)
{
    int version = read_char(mdhd);
    if (version == 0)
    {
        return read_int32(mdhd + 16);
    }
    else
    {
        return read_int64(mdhd + 24);
    }
}

void Mp4Spliter::mdhd_set_duration(unsigned char* mdhd, SNC_UI64 duration)
{
    int version = read_char(mdhd);
    if (version == 0)
    {
        write_int32(mdhd + 16, (SNC_UI32)duration);
    }
    else
    {
        write_int64(mdhd + 24, duration);
    }
}

void Mp4Spliter::tkhd_set_duration(unsigned char* tkhd, SNC_UI64 duration)
{
    int version = read_char(tkhd);
    if (version == 0)
    {
        write_int32(tkhd + 20, (SNC_UI32)duration);
    }
    else
    {
        write_int64(tkhd + 28, duration);
    }
}

SNC_UI32 Mp4Spliter::tkhd_get_width(unsigned char* tkhd)
{
    int version = read_char(tkhd);
    if (version == 0)
    {
        return read_int16(tkhd + 76);
    }
    else
    {
        return read_int16(tkhd + 88);
    }
}

SNC_UI32 Mp4Spliter::tkhd_get_height(unsigned char* tkhd)
{
    int version = read_char(tkhd);
    if (version == 0)
    {
        return read_int16(tkhd + 80);
    }
    else
    {
        return read_int16(tkhd + 92);
    }
}

unsigned int Mp4Spliter::stss_get_entries(unsigned char const* stss)
{
    return read_int32(stss + 4);
}

long Mp4Spliter::stss_get_sample(unsigned char const* stss, unsigned int idx)
{
    unsigned char const* p = stss + 8 + idx * 4;
    return read_int32(p);
}

unsigned int Mp4Spliter::stss_get_nearest_keyframe(unsigned char const* stss, unsigned int sample)
{
    // scan the sync samples to find the key frame that precedes the sample number
    unsigned int i;
    unsigned int entries = stss_get_entries(stss);
    unsigned int table_sample = 0;
    for (i = 0; i != entries; ++i)
    {
        table_sample = stss_get_sample(stss, i);
        if (table_sample >= sample)
            break;
    }
    if (table_sample == sample)
        return table_sample;
    else
        return stss_get_sample(stss, i - 1);
}

unsigned int Mp4Spliter::stbl_get_nearest_keyframe(struct stbl_t const* stbl, unsigned int sample)
{
    // If the sync atom is not present, all samples are implicit sync samples.
    if (!stbl->stss_)
        return sample;

    return stss_get_nearest_keyframe(stbl->stss_, sample);
}

//////////////////////////////////////////////////////////////////////////
// ??????stbl????????

unsigned int Mp4Spliter::stts_get_entries(unsigned char const* stts)
{
    return read_int32(stts + 4);
}

void Mp4Spliter::stts_get_sample_count_and_duration(unsigned char const* stts,
                                                    unsigned int idx, unsigned int* sample_count, unsigned int* sample_duration)
{
    unsigned char const* table = stts + 8 + idx * 8;
    *sample_count = read_int32(table);
    *sample_duration = read_int32(table + 4);
}

unsigned int Mp4Spliter::ctts_get_entries(unsigned char const* ctts)
{
    return read_int32(ctts + 4);
}

void Mp4Spliter::ctts_get_sample_count_and_offset(unsigned char const* ctts,
                                                  unsigned int idx, unsigned int* sample_count, unsigned int* sample_offset)
{
    unsigned char const* table = ctts + 8 + idx * 8;
    *sample_count = read_int32(table);
    *sample_offset = read_int32(table + 4);
}

unsigned int Mp4Spliter::ctts_get_samples(unsigned char const* ctts)
{
    unsigned int samples = 0;
    unsigned int entries = ctts_get_entries(ctts);
    unsigned int i;
    for (i = 0; i != entries; ++i)
    {
        unsigned int sample_count;
        unsigned int sample_offset;
        ctts_get_sample_count_and_offset(ctts, i, &sample_count, &sample_offset);
        samples += sample_count;
    }

    return samples;
}

unsigned int Mp4Spliter::stsc_get_entries(unsigned char const* stsc)
{
    return read_int32(stsc + 4);
}

void Mp4Spliter::stsc_get_table(unsigned char const* stsc, unsigned int i, struct stsc_table_t *stsc_table)
{
    struct stsc_table_t* table = (struct stsc_table_t*)(stsc + 8);
    stsc_table->chunk_ = read_int32(&table[i].chunk_) - 1;
    stsc_table->samples_ = read_int32(&table[i].samples_);
    stsc_table->id_ = read_int32(&table[i].id_);
}

unsigned int Mp4Spliter::stsc_get_chunk(unsigned char* stsc, unsigned int sample)
{
    unsigned int entries = read_int32(stsc + 4);
    struct stsc_table_t* table = (struct stsc_table_t*)(stsc + 8);

    if (entries == 0)
    {
        return 0;
    }
    else
        //  if (entries == 1)
        //  {
        //    unsigned int table_samples = read_int32(&table[0].samples_);
        //    unsigned int chunk = (sample + 1) / table_samples;
        //    return chunk - 1;
        //  }
        //  else
    {
        unsigned int total = 0;
        unsigned int chunk1 = 1;
        unsigned int chunk1samples = 0;
        unsigned int chunk2entry = 0;
        unsigned int chunk, chunk_sample;

        do
        {
            unsigned int range_samples;
            unsigned int chunk2 = read_int32(&table[chunk2entry].chunk_);
            chunk = chunk2 - chunk1;
            range_samples = chunk * chunk1samples;

            if (sample < total + range_samples)
                break;

            chunk1samples = read_int32(&table[chunk2entry].samples_);
            chunk1 = chunk2;

            if (chunk2entry < entries)
            {
                chunk2entry++;
                total += range_samples;
            }
        } while (chunk2entry < entries);

        if (chunk1samples)
        {
            unsigned int sample_in_chunk = (sample - total) % chunk1samples;
            if (sample_in_chunk != 0)
            {
                //        printf("ERROR: sample must be chunk aligned: %d\n", sample_in_chunk);
            }
            chunk = (sample - total) / chunk1samples + chunk1;
        }
        else
            chunk = 1;

        chunk_sample = total + (chunk - chunk1) * chunk1samples;

        return chunk;
    }
}

unsigned int Mp4Spliter::stsc_get_samples(unsigned char* stsc)
{
    unsigned int entries = read_int32(stsc + 4);
    struct stsc_table_t* table = (struct stsc_table_t*)(stsc + 8);
    unsigned int samples = 0;
    unsigned int i;
    for (i = 0; i != entries; ++i)
    {
        samples += read_int32(&table[i].samples_);
    }
    return samples;
}

unsigned int Mp4Spliter::stco_get_entries(unsigned char const* stco)
{
    return read_int32(stco + 4);
}

SNC_UI32 Mp4Spliter::stco_get_offset(unsigned char const* stco, int idx)
{
    SNC_UI32 const* table = (SNC_UI32 const*)(stco + 8);
    return read_int32(&table[idx]);
}

unsigned int Mp4Spliter::stsz_get_sample_size(unsigned char const* stsz)
{
    return read_int32(stsz + 4);
}

unsigned int Mp4Spliter::stsz_get_entries(unsigned char const* stsz)
{
    return read_int32(stsz + 8);
}

unsigned int Mp4Spliter::stsz_get_size(unsigned char const* stsz, unsigned int idx)
{
    SNC_UI32 const* table = (SNC_UI32 const*)(stsz + 12);
    return read_int32(&table[idx]);
}

SNC_UI64 Mp4Spliter::stts_get_duration(unsigned char const* stts)
{
    long duration = 0;
    unsigned int entries = stts_get_entries(stts);
    unsigned int i;
    for (i = 0; i != entries; ++i)
    {
        unsigned int sample_count;
        unsigned int sample_duration;
        stts_get_sample_count_and_duration(stts, i,
            &sample_count, &sample_duration);
        duration += sample_duration * sample_count;
    }

    return duration;
}

unsigned int Mp4Spliter::stts_get_samples(unsigned char const* stts)
{
    unsigned int samples = 0;
    unsigned int entries = stts_get_entries(stts);
    unsigned int i;
    for (i = 0; i != entries; ++i)
    {
        unsigned int sample_count;
        unsigned int sample_duration;
        stts_get_sample_count_and_duration(stts, i,
            &sample_count, &sample_duration);
        samples += sample_count;
    }

    return samples;
}

unsigned int Mp4Spliter::stts_get_sample(unsigned char const* stts, unsigned int time)
{
    unsigned int stts_index = 0;
    unsigned int stts_count;

    unsigned int ret = 0;
    unsigned int time_count = 0;

    unsigned int entries = stts_get_entries(stts);
    for (; stts_index != entries; ++stts_index)
    {
        unsigned int sample_count;
        unsigned int sample_duration;
        stts_get_sample_count_and_duration(stts, stts_index,
            &sample_count, &sample_duration);
        if (time_count + sample_duration * sample_count >= time)
        {
            stts_count = (time - time_count) / sample_duration;
            time_count += stts_count * sample_duration;
            ret += stts_count;
            break;
        }
        else
        {
            time_count += sample_duration * sample_count;
            ret += sample_count;
            //      stts_index++;
        }
        //    if (stts_index >= table_.size())
        //      break;
    }
    //  *time = time_count;
    return ret;
}

unsigned int Mp4Spliter::stts_get_time(unsigned char const* stts, unsigned int sample)
{
    unsigned int ret = 0;
    unsigned int stts_index = 0;
    unsigned int sample_count = 0;

    for (;;)
    {
        unsigned int table_sample_count;
        unsigned int table_sample_duration;
        stts_get_sample_count_and_duration(stts, stts_index,
            &table_sample_count, &table_sample_duration);

        if (sample_count + table_sample_count > sample)
        {
            unsigned int stts_count = (sample - sample_count);
            ret += stts_count * table_sample_duration;
            break;
        }
        else
        {
            sample_count += table_sample_count;
            ret += table_sample_count * table_sample_duration;
            stts_index++;
        }
    }
    return ret;
}

//////////////////////////////////////////////////////////////////////////
// ??????atom????

inline unsigned int Mp4Spliter::atom_header_size(unsigned char* atom_bytes)
{
    return (atom_bytes[0] << 24) +
        (atom_bytes[1] << 16) +
        (atom_bytes[2] << 8) +
        (atom_bytes[3]);
}

inline unsigned char* Mp4Spliter::atom_read_header(unsigned char* buffer, struct atom_t* atom)
{
    atom->start_ = buffer;
    base::util::memcpy2(&atom->type_[0], 4 , &buffer[4], 4);  // nightsuns: 这个地址不需要检测
    atom->size_ = atom_header_size(buffer);
    atom->end_ = atom->start_ + atom->size_;

    return buffer + ATOM_PREAMBLE_SIZE;
}

void Mp4Spliter::atom_write_header(unsigned char* outbuffer, struct atom_t* atom)
{
    int i;
    write_int32(outbuffer, atom->size_);
    for (i = 0; i != 4; ++i)
        write_char(outbuffer + 4 + i, atom->type_[i]);
}

inline unsigned char* Mp4Spliter::atom_skip(unsigned char* buffer, struct atom_t const* atom)
{
    return atom->end_;
}

inline int Mp4Spliter::atom_is(struct atom_t const* atom, const char* type)
{
    return (atom->type_[0] == type[0] &&
        atom->type_[1] == type[1] &&
        atom->type_[2] == type[2] &&
        atom->type_[3] == type[3])
       ;
}

//////////////////////////////////////////////////////////////////////////
// ????????????

inline int Mp4Spliter::read_char(unsigned char const* buffer)
{
    return buffer[0];
}

inline unsigned short Mp4Spliter::read_int16(void const* buffer)
{
    unsigned char* p = (unsigned char*)buffer;
    return (p[0] << 8) + p[1];
}

inline unsigned int Mp4Spliter::read_int32(void const* buffer)
{
    unsigned char* p = (unsigned char*)buffer;
    return (p[0] << 24) + (p[1] << 16) + (p[2] << 8) + p[3];
}

inline SNC_UI64 Mp4Spliter::read_int64(void const* buffer)
{
    unsigned char* p = (unsigned char*)buffer;
    return ((SNC_UI64)(read_int32(p)) << 32) + read_int32(p + 4);
}

inline void Mp4Spliter::write_char(unsigned char* outbuffer, int value)
{
    outbuffer[0] = (unsigned char)(value);
}

inline void Mp4Spliter::write_int32(void* outbuffer, SNC_UI32 value)
{
    unsigned char* p = (unsigned char*)outbuffer;
    p[0] = (unsigned char)((value >> 24) & 0xff);
    p[1] = (unsigned char)((value >> 16) & 0xff);
    p[2] = (unsigned char)((value >> 8) & 0xff);
    p[3] = (unsigned char)((value >> 0) & 0xff);
}

inline void Mp4Spliter::write_int64(void* outbuffer, SNC_UI64 value)
{
    unsigned char* p = (unsigned char*)outbuffer;
    write_int32(p + 0, (SNC_UI32)(value >> 32));
    write_int32(p + 4, (SNC_UI32)(value >>  0));
}
