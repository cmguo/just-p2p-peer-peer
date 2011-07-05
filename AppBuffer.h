//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _BUFFER_H_
#define _BUFFER_H_

#include "storage/Buffer.h"

typedef boost::uint8_t byte;

struct AppBuffer
{
    public:
    enum { BUF_NORMAL, BUF_SUBPIECE };
    private:
    boost::shared_array<byte> data_;
    storage::SubPieceBuffer::pointer subpiece_;

    size_t length_;
    size_t offset_;
    size_t type_;

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
            length_ = buffer.length_;
            offset_ = buffer.offset_;
            type_ = buffer.type_;
        }
        return *this;
    }

    explicit AppBuffer(const storage::Buffer& buffer)
    {
        length_ = buffer.Length();
        offset_ = buffer.Offset();
        subpiece_ = buffer.GetSubPieceBuffer();
        type_ = BUF_SUBPIECE;
    }

    explicit AppBuffer(size_t length) :
        data_(new byte[length]), length_(length), offset_(0), type_(BUF_NORMAL)
    {
    }

    explicit AppBuffer(const std::string& str)
        : data_(new byte[str.length()]), length_(str.length()), offset_(0), type_(BUF_NORMAL)
    {
        memcpy(data_.get(), str.c_str(), length_);
    }

    AppBuffer(const char* data, size_t length) :
        data_(new byte[length]), length_(length), offset_(0), type_(BUF_NORMAL)
    {
        memcpy(data_.get(), data, length);
    }

    AppBuffer(const byte* data, size_t length) :
        data_(new byte[length]), length_(length), offset_(0), type_(BUF_NORMAL)
    {
        memcpy(data_.get(), data, length);
    }

    AppBuffer(boost::shared_array<byte> data, size_t length) :
        data_(new byte[length]), length_(length), offset_(0), type_(BUF_NORMAL)
    {
        memcpy(data_.get(), data.get(), length);
    }

    inline byte* Data() const
    {
        if (BUF_NORMAL == type_) {
            return &data_[offset_];
        }
        else if (BUF_SUBPIECE == type_) {
            return *subpiece_;
        }
        return NULL;
    }

    inline byte* Data(size_t offset) const
    {
        byte* base_ptr = Data();
        if (base_ptr != NULL) {
            return base_ptr + offset;
        }
        return NULL;
    }

    inline size_t Length() const
    {
        return length_;
    }

    size_t Type() const
    {
        return type_;
    }

    size_t Offset() const
    {
        return offset_;
    }

    void Data(boost::shared_array<byte> data)
    {
        data_ = data;
        type_ = BUF_NORMAL;
    }

    void Length(size_t length)
    {
        length_ = length;
    }

    void Offset(size_t offset)
    {
        offset_ = offset;
    }

    AppBuffer SubBuffer(size_t offset) const
    {
        AppBuffer buffer;
        if (offset < length_)
        {
            buffer.offset_ = offset_ + offset;
            buffer.length_ = length_ - offset;
            buffer.data_ = data_;
            buffer.subpiece_ = subpiece_;
            buffer.type_ = type_;
        }
        return buffer;
    }

    AppBuffer SubBuffer(size_t offset, size_t length) const
    {
        AppBuffer buffer;
        if (offset + length <= length_)
        {
            buffer.offset_ = offset_ + offset;
            buffer.length_ = length;
            buffer.data_ = data_;
            buffer.subpiece_ = subpiece_;
            buffer.type_ = type_;
        }
        return buffer;
    }

    void Malloc(size_t length)
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

struct TempBuffer
{
    AppBuffer buffer_;
    byte* data_;
    size_t length_;

    TempBuffer(const AppBuffer& buffer, byte* data, size_t length) :
        buffer_(buffer), data_(data), length_(length)
    {
        assert(buffer);
        assert(data >= buffer.Data());
        assert(data < buffer.Data() + buffer.Length());
    }

    TempBuffer(const AppBuffer& buffer, size_t offset, size_t length) :
        buffer_(buffer), data_(buffer_.Data() + offset), length_(length)
    {
        assert(buffer);
        assert(offset >= 0);
        assert(offset < buffer_.Length());
    }

    TempBuffer(const AppBuffer& buffer, byte* data) :
        buffer_(buffer), data_(data)
    {
        assert(buffer);
        assert(data >= buffer.Data());
        assert(data < buffer.Data() + buffer.Length());
        length_ = buffer.Length() - (data - buffer.Data());
    }

    TempBuffer(const AppBuffer& buffer, size_t offset) :
        buffer_(buffer), data_(buffer_.Data() + offset), length_(buffer_.Length() - offset)
    {
        assert(buffer);
        assert(offset >= 0);
        assert(offset < buffer_.Length());
    }

    inline byte* Get() const
    {
        return data_;
    }
    inline size_t GetLength() const
    {
        return length_;
    }

    inline bool operator == (const TempBuffer& buffer)
    {
        return buffer_ == buffer.buffer_ && Get() == buffer.Get() && GetLength() == buffer.GetLength();
    }

    inline AppBuffer ToBuffer()
    {
        return AppBuffer(Get(), GetLength());
    }
};

inline bool operator <(const TempBuffer& b1, const TempBuffer& b2)
{
    if (b1.buffer_ == b2.buffer_)
    {
        if (b1.Get() != b2.Get())
            return b1.Get() < b2.Get();
        else
            return b1.GetLength() < b2.GetLength();
    }
    else
    {
        return b1.buffer_ < b2.buffer_;
    }
}

#endif  // _BUFFER_H_
