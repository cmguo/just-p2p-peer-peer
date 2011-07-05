//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// UrlCodec.h

#ifndef _NETWORK_URLCODEC_H_
#define _NETWORK_URLCODEC_H_

namespace network
{
    class UrlCodec
#ifdef DUMP_OBJECT
        : public count_object_allocate<UrlCodec>
#endif
    {
    public:
        /**
        * @param {url} Url to be encoded.
        *
        */
        static string Encode(const string& url);
        /**
        * @param {encoded_url} Encoded Url.
        */
        static string Decode(const string& encoded_url);
    };
}

#endif  // _NETWORK_URLCODEC_H_
