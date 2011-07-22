//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef DOWNLOAD_CENTER_MODULE_H
#define DOWNLOAD_CENTER_MODULE_H

#include "downloadcenter/DownloadCenterStructs.h"
#include "downloadcenter/DownloadCenterStructsInternal.h"
#include "interprocess/SharedMemory.h"

namespace p2sp
{
    class ProxyConnection;
    typedef boost::shared_ptr<ProxyConnection> ProxyConnection__p;
}

namespace downloadcenter
{

    class DownloadCenterModule
        : public boost::noncopyable
        , public boost::enable_shared_from_this<DownloadCenterModule>
#ifdef DUMP_OBJECT
        , public count_object_allocate<DownloadCenterModule>
#endif
    {
    public:

        typedef boost::shared_ptr<DownloadCenterModule> p;

        // static DownloadCenterModule::p Create();

    public:

        void Start(uint32_t flush_interval_in_millisec);

        void Stop();

        void UpdateDownloadResourceData(const DownloadResourceData& download_resource_data);

        void RemoveDownloadResourceData(const DownloadResourceData& download_resource_data);

        void OnTimerElapsed(framework::timer::Timer * timer_ptr);

        bool IsRunning() const { return is_running_; }

        void ClearAllData();

        void ProxyConnectionProcessor(p2sp::ProxyConnection__p conn);

        void FlushData();

        bool HasUrl(const string& url) const;

        bool IsUrlDownloading(const string& url) const;

    private:

        bool CreateSharedMemory();

        void CloseSharedMemory();

        uint32_t GetMaxSharedMemorySize() const { return 2 * 1024 * 1024; }

        void PullActiveProxyConnectionsData();

        boost::uint16_t GetString(const string& str_value) const;

        boost::uint16_t CreateString(const string& str_value);

        boost::uint16_t RemoveString(const string& str_value);

        boost::uint16_t NextID();

    private:

        DownloadCenterModule();

    private:

        typedef std::map<string, uint32_t> StringStoreMap;

        typedef std::map<boost::uint16_t, internal::DOWNLOAD_RESOURCE_DATA> DownloadResourceMap;

        //////////////////////////////////////////////////////////////////////////
        interprocess::SharedMemory shared_memory_;

        framework::timer::PeriodicTimer flush_timer_;

        string shared_memory_name_;

        //////////////////////////////////////////////////////////////////////////
        // Data

        StringStoreMap strings_;

        DownloadResourceMap download_resources_;

        boost::uint16_t next_id_;

        std::list<boost::uint16_t> free_id_list_;

        //////////////////////////////////////////////////////////////////////////
        // Status

        volatile bool is_running_;

        //////////////////////////////////////////////////////////////////////////
        // Instance

    private:

        static DownloadCenterModule::p inst_;

    public:

        static DownloadCenterModule::p Inst() 
        {
            if (!inst_)
            {
                inst_.reset(new DownloadCenterModule());
            }
            return inst_; 
        }
    };

}

#endif  // DOWNLOAD_CENTER_MODULE_H
