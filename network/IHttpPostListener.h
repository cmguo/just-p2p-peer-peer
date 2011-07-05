//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef _NETWORK_IHTTP_POST_LISTENER_H_
#define _NETWORK_IHTTP_POST_LISTENER_H_

namespace network
{
    class IHttpPostListener
    {
    public:
        virtual void OnPostResult(const boost::system::error_code& err)=0;
        virtual ~IHttpPostListener(){}
    };
}

#endif  // _NETWORK_IHTTP_POST_LISTENER_H_
