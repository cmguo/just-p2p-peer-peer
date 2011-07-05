//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _COUNT_CPU_TIME_H_
#define _COUNT_CPU_TIME_H_

#include <map>
using namespace std;

class count_cpu_time
{
    public:
    count_cpu_time(string function_name);
    ~count_cpu_time();
    private:
    framework::timer::TickCounter tick_count;
    string function_name_;
};

class cpu_time_record
{
    public:
    void add_record(uint32_t time_elapse, string function_name);
#ifdef COUNT_CPU_TIME
    static cpu_time_record * get_record();
#endif
    void dump();

#ifdef COUNT_CPU_TIME
    static cpu_time_record g_cpu_time_record_;
#endif

    private:
    multimap<uint32_t, string> cpu_record_;
};

#endif