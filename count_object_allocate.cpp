//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

/*
 *  count_object_allocate.cpp
 *  test
 *
 *  Created by nightsuns on 9/10/10.
 *  Copyright 2010 hunan. All rights reserved.
 *
 */
#include "Common.h"
#include "count_object_allocate.h"

object_counter g_count;

object_counter::object_counter()
{
}

object_counter::~object_counter()
{
}

#ifdef DUMP_OBJECT
object_counter * object_counter::get_counter()
{
    static object_counter g_count;
    return &g_count;
}
#endif

bool object_counter::add_object_count(const char * object_name)
{
    std::map<std::string , int>::iterator iter = this->allocated_objects_.find(object_name);
    if (iter == this->allocated_objects_.end()) {
    // new
    this->allocated_objects_.insert(std::make_pair(object_name , 0));
    iter = this->allocated_objects_.find(object_name);
    }

    iter->second++;

    return true;
}

bool object_counter::delete_object_count(const char * object_name)
{
    std::map<std::string , int>::iterator iter = this->allocated_objects_.find(object_name);
    if (iter == this->allocated_objects_.end()) {
    // new
    return false;
    }

    iter->second--;

    return true;
}

void object_counter::dump_all_objects()
{
    printf("begin dump objects:\r\n");
    for (std::map<std::string , int>::iterator i = this->allocated_objects_.begin(); i != this->allocated_objects_.end(); ++i) {
    printf("%s\t\t: %d\r\n" , i->first.c_str() , i->second);
    }
    printf("end dump objects:\r\n");
}
