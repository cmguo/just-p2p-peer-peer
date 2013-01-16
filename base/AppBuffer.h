//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _BUFFER_H_
#define _BUFFER_H_

#include "base/util.h"
#include "struct/SubPieceBuffer.h"
#ifdef DUMP_OBJECT
#include "count_object_allocate.h"
#endif

typedef boost::uint8_t byte;

namespace base
{
    struct AppBuffer
#ifdef DUMP_OBJECT
        : public count_object_allocate<AppBuffer>
#endif
    {
    public:
        enum { BUF_NORMAL, BUF_SUBPIECE, BUF_LIVESUBPIECE };
    private:
        boost::shared_array<byte> data_;
        protocol::SubPieceContent::pointer subpiece_;
        protocol::LiveSubPieceContent::pointer live_subpiece_;

        boost::uint32_t length_;
        boost::uint32_t offset_;       // 有效数据的起始偏移
        boost::uint32_t type_;

    public:
        AppBuffer() :
            length_(0), offset_(0), type_(BUF_NORMAL)
        {
        }

        AppBuffer(const AppBuffer& buffer)
        {
            if (this != &buffer)
            {
                data_ = buffer.data_;
                subpiece_ = buffer.subpiece_;
                live_subpiece_ = buffer.live_subpiece_;
                length_ = buffer.length_;
                offset_ = buffer.offset_;
                type_ = buffer.type_;
            }
        }

        AppBuffer& operator = (const AppBuffer& buffer)
        {
            if (this != &buffer)
            {
                data_ = buffer.data_;
                subpiece_ = buffer.subpiece_;
                live_subpiece_ = buffer.live_subpiece_;
                length_ = buffer.length_;
                offset_ = buffer.offset_;
                type_ = buffer.type_;
            }
            return *this;
        }

        explicit AppBuffer(const protocol::SubPieceBuffer& buffer)
        {
            length_ = buffer.Length();
            offset_ = buffer.Offset();
            subpiece_ = buffer.GetSubPieceBuffer();
            type_ = BUF_SUBPIECE;
        }

        explicit AppBuffer(const protocol::LiveSubPieceBuffer& buffer)
        {
            length_ = buffer.Length();
            offset_ = buffer.Offset();
            live_subpiece_ = buffer.GetSubPieceBuffer();
            type_ = BUF_LIVESUBPIECE;
        }

        explicit AppBuffer(boost::uint32_t length) :
            data_(new byte[length]), length_(length), offset_(0), type_(BUF_NORMAL)
        {
        }

        explicit AppBuffer(boost::uint32_t length, int val) :
        data_(new byte[length]), length_(length), offset_(0), type_(BUF_NORMAL)
        {
            memset((void*)data_.get(), val, length);
        }

        explicit AppBuffer(const string& str)
            : data_(new byte[str.length()]), length_(str.length()), offset_(0), type_(BUF_NORMAL)
        {
            base::util::memcpy2(data_.get(), length_, str.c_str(), length_);
        }

        AppBuffer(const char* data, boost::uint32_t length) :
            data_(new byte[length]), length_(length), offset_(0), type_(BUF_NORMAL)
        {
            base::util::memcpy2(data_.get(), length, data, length);
        }

        AppBuffer(const byte* data, boost::uint32_t length) :
            data_(new byte[length]), length_(length), offset_(0), type_(BUF_NORMAL)
        {
            base::util::memcpy2(data_.get(), length, data, length);
        }

        AppBuffer(boost::shared_array<byte> data, boost::uint32_t length) :
            data_(new byte[length]), length_(length), offset_(0), type_(BUF_NORMAL)
        {
            base::util::memcpy2(data_.get(), length, data.get(), length);
        }

        bool Add(const byte* data, boost::uint32_t length)
        {
            if (offset_ + length > length_)
            {
                return false;
            }
            base::util::memcpy2(data_.get() + offset_, length_ - offset_, data, length);
            return true;
        }

        bool Extend(boost::uint32_t length)
        {
            if (type_ == BUF_NORMAL)
            {
                boost::uint32_t new_length = length + length_;
                AppBuffer new_buf(new_length);
                new_buf.Add(&data_[0], length_);
                new_buf.Offset(offset_);
                *this = new_buf;
                return true;
            }
            else
            {
                assert(false);
                return false;
            }
        }

        inline byte* Data() const
        {
            if (BUF_NORMAL == type_) {
                return &data_[offset_];
            }
            else if (BUF_SUBPIECE == type_) {
                return *subpiece_;
            }
            else if (BUF_LIVESUBPIECE == type_)
            {
                return *live_subpiece_;
            }
            return NULL;
        }

        inline byte* Data(boost::uint32_t offset) const
        {
            byte* base_ptr = Data();
            if (base_ptr != NULL) {
                return base_ptr + offset;
            }
            return NULL;
        }

        inline boost::uint32_t Length() const
        {
            return length_;
        }

        boost::uint32_t Type() const
        {
            return type_;
        }

        boost::uint32_t Offset() const
        {
            return offset_;
        }

        void Data(boost::shared_array<byte> data)
        {
            data_ = data;
            type_ = BUF_NORMAL;
        }

        void Length(boost::uint32_t length)
        {
            length_ = length;
        }

        void Offset(boost::uint32_t offset)
        {
            offset_ = offset;
        }

        AppBuffer SubBuffer(boost::uint32_t offset) const
        {
            AppBuffer buffer;
            if (offset < length_)
            {
                buffer.offset_ = offset_ + offset;
                buffer.length_ = length_ - offset;
                buffer.data_ = data_;
                buffer.subpiece_ = subpiece_;
                buffer.live_subpiece_ = live_subpiece_;
                buffer.type_ = type_;
            }
            return buffer;
        }

        AppBuffer SubBuffer(boost::uint32_t offset, boost::uint32_t length) const
        {
            AppBuffer buffer;
            if (offset + length <= length_)
            {
                buffer.offset_ = offset_ + offset;
                buffer.length_ = length;
                buffer.data_ = data_;
                buffer.subpiece_ = subpiece_;
                buffer.live_subpiece_ = live_subpiece_;
                buffer.type_ = type_;
            }
            return buffer;
        }

        void Malloc(boost::uint32_t length)
        {
            length_ = length;
            data_ = boost::shared_array<byte>(new byte[length_]);
            offset_ = 0;
            type_ = BUF_NORMAL;
        }

        AppBuffer Clone() const
        {
            return AppBuffer(Data(), length_);
        }

        void Clear(byte padding = 0)
        {
            if (length_ > 0)
            {
                memset(Data(), padding, length_);
            }
        }

        bool operator !() const
        {
            return length_ == 0;
        }

        operator bool() const
        {
            return length_ != 0;
        }

        bool operator == (const AppBuffer& buffer) const
        {
            return data_.get() == buffer.data_.get() && length_ == buffer.length_
                && offset_ == buffer.offset_ && type_ == buffer.type_;
        }

        bool operator <(const AppBuffer& b2) const
        {
            if (type_ != b2.type_)
            {
                return type_ < b2.type_;
            }
            else if (data_.get() != b2.data_.get())
            {
                return data_.get() < b2.data_.get();
            }
            else if (offset_ != b2.offset_)
            {
                return offset_ < b2.offset_;
            }
            else
            {
                return length_ < b2.length_;
            }
        }
    };
}
#endif  // _BUFFER_H_
