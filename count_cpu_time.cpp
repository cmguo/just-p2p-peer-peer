//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#include "Common.h"

#include "count_cpu_time.h"

#ifdef COUNT_CPU_TIME
cpu_time_record g_cpu_time_record_;
#endif

count_cpu_time::count_cpu_time(string function_name)
{
    tick_count.start();
    function_name_ = function_name;
}

count_cpu_time::~count_cpu_time()
{
    uint32_t time_elapse = tick_count.elapsed();
    tick_count.stop();
#ifdef COUNT_CPU_TIME
    if (time_elapse > 0)
    {
        cpu_time_record::get_record()->add_record(time_elapse, function_name_);
    }
#endif
}

#ifdef COUNT_CPU_TIME
cpu_time_record * cpu_time_record::get_record()
{
    static cpu_time_record g_cpu_time_record_;
    return &g_cpu_time_record_;
}
#endif

void cpu_time_record::add_record(uint32_t time_elapse, string function_name)
{
    cpu_record_.insert(make_pair(time_elapse, function_name));
}

void cpu_time_record::dump()
{
    uint32_t i = 0;
    printf("--------------dump cpu recode begin--------------\n");
    for (multimap<uint32_t, string>::iterator iter = cpu_record_.begin();
        iter != cpu_record_.end(); ++iter)
    {
        i++;
        string function_name = iter->second;
        uint32_t time_elapse = iter->first;
        printf("%s used %d ms cpu\n", function_name.c_str(), time_elapse);
    }
    printf("-------------- dump cpu recode end --------------\n");
}