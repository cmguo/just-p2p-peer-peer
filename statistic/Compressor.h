//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _STATISTIC_ICOMPRESSOR_H_
#define _STATISTIC_ICOMPRESSOR_H_

namespace statistic
{
    class ICompressor
    {
    public:
        virtual bool Compress(uint8_t* raw_data, int raw_data_size, std::ostream& result) = 0;
        virtual ~ICompressor(){}
    };
}

#endif  // _STATISTIC_ICOMPRESSOR_H_
