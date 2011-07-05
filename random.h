//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef FRAMEWORK_UTIL_RANDOM_H
#define FRAMEWORK_UTIL_RANDOM_H

#include <time.h>

class Random
{
    public:
    static Random& GetGlobal()
    {
        static Random random;
        return random;
    }
    public:
    Random();
    explicit Random(unsigned int seed);

    void SetSeed(unsigned int seed);

    int Next();
    int Next(int maxValue);
    unsigned char NextByte();
    unsigned short NextWord();
    unsigned long NextDWord();

    std::string GenerateRandomString(size_t size);
    std::string RandomString(size_t size);
};

inline Random::Random()
{
    SetSeed(framework::timer::TickCounter::tick_count());
}

inline Random::Random(unsigned int seed)
{
    SetSeed(seed);
}

inline void Random::SetSeed(unsigned int seed)
{
    ::srand(seed);
}

inline int Random::Next()
{
    return ::rand();
}

inline int Random::Next(int maxValue)
{
    assert(maxValue > 0);
    return Next() % maxValue;
}

inline unsigned char Random::NextByte()
{
    return Next(0x100);
}

inline unsigned short Random::NextWord()
{
    return Next(0x10000);
}

inline unsigned long Random::NextDWord()
{
    return ((NextWord() << 16) | NextWord());
}

inline std::string Random::GenerateRandomString(size_t size)
{
    assert(size < SHRT_MAX);
    std::string str(size, '0');
    for (size_t i = 0; i < size; ++i)
    {
        str[i] = NextByte();
    }
    return str;
}

inline std::string Random::RandomString(size_t size)
{
    assert(size < SHRT_MAX);
    return GenerateRandomString(Next(size));
}

#endif  // FRAMEWORK_UTIL_RANDOM_H
