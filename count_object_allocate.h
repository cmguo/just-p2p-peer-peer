//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

/*
 *  count_object_allocate.h
 *  test
 *
 *  Created by nightsuns on 9/10/10.
 *  Copyright 2010 hunan. All rights reserved.
 *
 */

#ifndef _COUNT_OBJECT_ALLOCATE_H_
#define _COUNT_OBJECT_ALLOCATE_H_

#include <map>
#include <string>
#include <typeinfo>

class object_counter
{
    public:
    object_counter();
    ~object_counter();
#ifdef DUMP_OBJECT
    static object_counter * get_counter();
#endif
    bool add_object_count(const char * object_name);
    bool delete_object_count(const char * object_name);

    void dump_all_objects();

    private:
    std::map<std::string , int> allocated_objects_;
};

template<typename T>
class count_object_allocate
{
    public:
    count_object_allocate()
    {
#ifdef DUMP_OBJECT
        object_counter::get_counter()->add_object_count(typeid(T).name());
#endif
    }

    ~count_object_allocate()
    {
#ifdef DUMP_OBJECT
        object_counter::get_counter()->delete_object_count(typeid(T).name());
#endif
    }

    count_object_allocate(const count_object_allocate<T> & coa)
    {
#ifdef DUMP_OBJECT
        object_counter::get_counter()->add_object_count(typeid(T).name());
#endif
    }
};
#endif
