//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#pragma once

#include <framework/process/Process.h>

namespace statistic
{
  inline boost::uint32_t GetCurrentProcessID()
  {
#ifdef PEER_PC_CLIENT
    return ::GetCurrentProcessId();
#else
    static int pid = framework::this_process::id();
    return pid;
#endif
  }

  inline string CreateStatisticModuleSharedMemoryName(boost::uint32_t process_id)
  {
    std::stringstream is;
    is << "PPVIDEO_" << process_id;
    return is.str();
  }

  inline string CreateP2PDownloaderModuleSharedMemoryName(boost::uint32_t process_id, const RID& rid)
  {
    std::stringstream is;
    is << "P2PDOWNLOADER_" << process_id << "_" << rid;
    return is.str();
  }

  inline string CreateDownloadDriverModuleSharedMemoryName(boost::uint32_t process_id, boost::uint32_t download_driver_id)
  {
    std::stringstream is;
    is << "DOWNLOADDRIVER_" << process_id << "_" << download_driver_id;
    return is.str();
  }

  inline string CreateLiveDownloadDriverModuleSharedMemoryName(boost::uint32_t process_id, boost::uint32_t download_driver_id)
  {
      std::stringstream is;
      is << "LIVEDOWNLOADDRIVER_" << process_id << "_" << download_driver_id;
      return is.str();
  }

  inline string CreateDownloadCenterModuleSharedMemoryName(boost::uint32_t process_id)
  {
    std::stringstream is;
    is << "PPVIDEO_DOWNLOAD_CENTER_" << process_id;
    return is.str();
  }

  inline string CreateUploadShareMemoryName(boost::uint32_t process_id)
  {
      std::stringstream is;
      is << "UPLOAD_" << process_id;
      return is.str();
  }

  inline boost::uint32_t GetTickCountInMilliSecond()
  {
      return framework::timer::TickCounter::tick_count();
  }

  inline boost::uint32_t GetTickCountInSecond()
  {
    return GetTickCountInMilliSecond() / 1000;
  }
}
