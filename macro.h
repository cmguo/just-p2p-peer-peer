//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------


#ifndef FRAMEWORK_UTIL_MACRO_H
#define FRAMEWORK_UTIL_MACRO_H

#ifndef SIZEOF_ARRAY
#define SIZEOF_ARRAY(ar)        (sizeof(ar)/sizeof((ar)[0]))
#endif  // !defined(SIZEOF_ARRAY)


#define FillZero(obj, type) memset(&(obj), 0, sizeof(type))

#define GetMemoryValue(buf, type) (*reinterpret_cast<const type*>(buf))
#define SetMemoryValue(buf, val, type) do { *reinterpret_cast<type*>(buf) = (val); } while (false)

#define LimitMin(val, minVal) do { if ((val) < (minVal)) (val) = (minVal) } while (false)

#define LimitMax(val, maxVal) do { if ((val) > (maxVal)) (val) = (maxVal) } while (false)

#define LimitMinMax(val, minVal, maxVal) \
    do { \
    if ((val) > (maxVal)) (val) = (maxVal); \
    if ((val) < (minVal)) (val) = (minVal); \
    } while (false)



#define FILL_ZERO(obj) memset(&(obj), 0, sizeof(obj))


#define READ_MEMORY(buf, type) (*reinterpret_cast<const type*>(buf))

#define WRITE_MEMORY(buf, val, type) do { *reinterpret_cast<type*>(buf) = (val); } while (false)

#define LIMIT_MIN(val, minVal) do { if ((val) < (minVal)) (val) = (minVal); assert((val) >= (minVal)); } while (false)

#define LIMIT_MAX(val, maxVal) do { if ((val) > (maxVal)) (val) = (maxVal); assert((val) <= (maxVal)); } while (false)

#define LIMIT_MIN_MAX(val, minVal, maxVal) \
    do { \
    if ((val) > (maxVal)) (val) = (maxVal); \
    if ((val) < (minVal)) (val) = (minVal); \
    assert((val) >= (minVal)); \
    assert((val) <= (maxVal)); \
    } while (false)


#define MAKELONGLONG(low, high)        (ULONGLONG) ((((ULONGLONG)high) << 32) | ((DWORD)low))



#define STL_FOR_EACH(containerType, container, iter)    \
    for (containerType::iterator iter = (container).begin(); (iter) != (container).end(); ++(iter))

#define STL_FOR_EACH_CONST(containerType, container, iter)    \
    for (containerType::const_iterator iter = (container).begin(); (iter) != (container).end(); ++(iter))

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(x)    \
    if (NULL != x)            \
    {                        \
    x->Release();        \
    x = NULL;            \
    }

#endif

#ifndef SAFE_DELETE
#define SAFE_DELETE(x)    \
    if (NULL != x)            \
    {                        \
    delete x;            \
    x = NULL;            \
    }
#endif

#ifndef SAFE_ARRAYDELETE
#define SAFE_ARRAYDELETE(x)    \
    if (NULL != x)                \
    {                            \
    delete[] x;                \
    x = NULL;                \
    }
#endif


#define LOWPART_LONGLONG        ((ULONGLONG) 0x00000000FFFFFFFF)
#define HIGHPART_LONGLONG        ((ULONGLONG) 0xFFFFFFFF00000000)

#define LO_DWORD(x)                ((DWORD) ((x) & LOWPART_LONGLONG))
#define HI_DWORD(x)                ((DWORD) (((x) & HIGHPART_LONGLONG) >> 32))

#endif  // FRAMEWORK_UTIL_MACRO_H
