//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _PEER_COMMON_H_
#define _PEER_COMMON_H_

//
// ignore VC compiler warnings on using "unsecured" version of CRT functions; the corresponding
// "secured" version does not exist on GCC so we have to use the "unsecured" version
// an example is "fopen"
#define _CRT_SECURE_NO_WARNINGS

// ignore VC compiler warnings on using unsecured STL functions
// an example is "std::copy"
#define D_SCL_SECURE_NO_WARNINGS

// StdAfx.h
#include <boost/asio.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include <boost/bind.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/function.hpp>

#ifdef BOOST_WINDOWS_API
#ifdef PEER_PC_CLIENT
#define NEED_TO_POST_MESSAGE
#endif
#endif

#include <framework/Framework.h>
#include <framework/string/Uuid.h>
#include <framework/string/Format.h>
#include <framework/string/FormatStl.h>
#include <framework/string/Parse.h>
#include <framework/string/ParseStl.h>
#include <framework/timer/TickCounter.h>
#include <framework/timer/Timer.h>
#include <framework/timer/TimerQueue.h>
#include <framework/configure/Config.h>

#include <util/Util.h>

#include <algorithm>

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

#if (defined _DEBUG || defined DEBUG)
#include <log4cplus/logger.h>
#include <log4cplus/fileappender.h>
#include <log4cplus/configurator.h>
#include <log4cplus/loggingmacros.h>
#define LOG_ENABLE
using namespace log4cplus;
#endif

#define  STRINGIFY(x) #x
#define  TOSTRING(x) STRINGIFY(x)

#if (defined _DEBUG || defined DEBUG)
#define LOG4CPLUS_DEBUG_LOG(logger,msg) LOG4CPLUS_DEBUG(logger,msg)
#define LOG4CPLUS_INFO_LOG(logger,msg) LOG4CPLUS_INFO(logger,msg)
#define LOG4CPLUS_WARN_LOG(logger,msg) LOG4CPLUS_WARN(logger,msg)
#define LOG4CPLUS_ERROR_LOG(logger,msg) LOG4CPLUS_ERROR(logger,msg)
#else
#define LOG4CPLUS_DEBUG_LOG(logger,msg)
#define LOG4CPLUS_INFO_LOG(logger,msg)
#define LOG4CPLUS_WARN_LOG(logger,msg)
#define LOG4CPLUS_ERROR_LOG(logger,msg)
#endif

void DebugLog(const char* format, ...);

#  if CONSOLE_LOG_DISABLE
#    define COUT(msg)
#  else
#include <iostream>
#    define COUT(msg) std::cout << __FILE__":"TOSTRING(__LINE__)" " << msg << std::endl
#  endif

typedef framework::string::Uuid RID;
typedef framework::string::Uuid MD5;
typedef framework::string::Uuid Guid;

using std::string;
using std::map;
using std::vector;
using boost::uint8_t;
using boost::uint16_t;
using boost::uint32_t;
using boost::int32_t;

// herain:2011-1-4:内核向播放器推送数据的默认限速值
const int32_t DEFAULT_CLIENT_SEND_SPEED_LIMIT = 2048;
const int32_t DEFAULT_SEND_SPEED_LIMIT = 512;

const int32_t MAX_HTTP_DOWNLOADER_COUNT = 100;
const int32_t MAX_P2P_DOWNLOADER_COUNT = 100;

#include "macro.h"
#include "protocol/Protocol.h"
#include "base/AppBuffer.h"
#include "base/filesystem_util.h"

extern framework::timer::TimerQueue & global_second_timer();
extern framework::timer::TimerQueue & global_250ms_timer();

#endif