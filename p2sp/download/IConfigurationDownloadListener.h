//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _P2SP_CONFIGURATION_DOWNLOAD_LISTENER_H_
#define _P2SP_CONFIGURATION_DOWNLOAD_LISTENER_H_

#include <boost/system/error_code.hpp>

namespace p2sp
{
    class IConfigurationDownloadListener
    {
    public:
        virtual void OnDownloadFailed(const boost::system::error_code& err) = 0;
        virtual void OnDownloadSucceeded(const string& config_content) = 0;
        virtual ~IConfigurationDownloadListener() {}
    };
}

#endif  // _P2SP_CONFIGURATION_DOWNLOAD_LISTENER_H_
