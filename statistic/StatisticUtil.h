//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#pragma once

namespace statistic
{
  inline uint32_t GetCurrentProcessID()
  {
#ifdef BOOST_WINDOWS_API
    return ::GetCurrentProcessId();
#else
    static pid_t pid = ::getpid();
    return pid;
#endif
  }

  inline string CreateStatisticModuleSharedMemoryName(uint32_t process_id)
  {
    std::stringstream is;
    is << "PPVIDEO_" << process_id;
    return is.str();
  }

  inline string CreateP2PDownloaderModuleSharedMemoryName(uint32_t process_id, const RID& rid)
  {
    std::stringstream is;
    is << "P2PDOWNLOADER_" << process_id << "_" << rid;
    return is.str();
  }

  inline string CreateDownloadDriverModuleSharedMemoryName(uint32_t process_id, uint32_t download_driver_id)
  {
    std::stringstream is;
    is << "DOWNLOADDRIVER_" << process_id << "_" << download_driver_id;
    return is.str();
  }

  inline string CreateLiveDownloadDriverModuleSharedMemoryName(uint32_t process_id, uint32_t download_driver_id)
  {
      std::stringstream is;
      is << "LIVEDOWNLOADDRIVER_" << process_id << "_" << download_driver_id;
      return is.str();
  }

  inline string CreateDownloadCenterModuleSharedMemoryName(uint32_t process_id)
  {
    std::stringstream is;
    is << "PPVIDEO_DOWNLOAD_CENTER_" << process_id;
    return is.str();
  }

  inline string CreateUploadShareMemoryName(uint32_t process_id)
  {
      std::stringstream is;
      is << "UPLOAD_" << process_id;
      return is.str();
  }

  inline uint32_t GetTickCountInMilliSecond()
  {
      return framework::timer::TickCounter::tick_count();
  }

  inline uint32_t GetTickCountInSecond()
  {
    return GetTickCountInMilliSecond() / 1000;
  }
}
