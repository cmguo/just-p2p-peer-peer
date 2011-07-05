//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

// ProxyScript.h

#ifndef _P2SP_PROXY_PROXY_SCRIPT_H_
#define _P2SP_PROXY_PROXY_SCRIPT_H_

namespace p2sp
{
    const char PROXY_SCRIPT_TEXT[] =
        "function FindProxyForURL(url,host){\r\n"
//        "            return \"DIRECT\";\r\n"
        "if(host == \"localhost\" || host == \"127.0.0.1\")\r\n"
        "    {\r\n"
        "        return \"DIRECT\";\r\n"
        "    }\r\n"
        "    var private_re = /^http:\\/\\/((lisbon)|(0\\.0\\.0\\.0)|(127\\.\\d+\\.\\d+\\.\\d+)|(10\\.\\d+\\.\\d+\\.\\d+)|(172\\.(1[6789]|2[0-9]|3[01])\\.\\d+\\.\\d+)|(169\\.254\\.\\d+\\.\\d+)|(192\\.168\\.\\d+\\.\\d+)|(22[3-9]\\.\\d+\\.\\d+\\.\\d+)|(2[3-5][0-9]\\.\\d+\\.\\d+\\.\\d+))+/ig\r\n"
        "   var dg = /(dog.xnimg.cn)|(dog.rrimg.com)/ig;\r\n"
        "        if(url.match(dg) || url.match(private_re) || url.substring(0,7) != \"http://\")\r\n"
        "        {\r\n"
        "            return \"DIRECT\";\r\n"
        "        }\r\n"
//        "   var pattern = /\\/[^\\/&]+\\.flvx?(\\?|$)/ig;\r\n"
// 不支持ie5:   "   var pattern = /\\/[^\\/&]+\\.flv[x1234]?(\\?|$)/ig;\r\n"

//        "   var pattern = /[^\\/&]+(\\.flv|\\.mp4|\\.f4v|\\.mp3|\\.wma)+(\\?|$)/ig;\r\n"
        "   var pattern = /^(http\\:\\/\\/[a-zA-Z0-9-\\.]*(\\:[0-9]+)?)?(\\/[^\\/\\?]+)*((\\/[^\\/\\?]+(\\.flv|\\.hlv|\\.mp4|\\.f4v|\\.mp3|\\.wma)(\\?|$)))/ig; \r\n"
        "    var yp0 = /\\/get_video\\?\\w+/ig;\r\n"
        "   var yp1 = /\\/videoplayback\\?\\w+/ig;\r\n"

//        "   var pattern = /\\/[^\\/&]+\\.flv[x1234]?(\\?|$)/ig;\r\n"

        "    if(url.match(pattern))\r\n"
        "    {\r\n"
        "        return \"PROXY localhost:%u\";\r\n"
        "    }\r\n"
        "    else if(url.match(yp0) || url.match(yp1))\r\n"
        "    {\r\n"
        "        return \"PROXY localhost:%u\";\r\n"
        "    }\r\n"
        "    else\r\n"
        "    {\r\n"
        "        return \"DIRECT\";\r\n"
        "    }\r\n"
        "}";

    // const char PROXY_SCRIPT_TEXT[] =
    //    "function FindProxyForURL(url, host)\r\n"
    //    "{\r\n"
    //    "    var reg1=/\\.flv$/;\r\n"
    //    "   var reg2=/\\.flvx$/;\r\n"
    //    "    if(reg1.test(url) || reg2.test(url))\r\n"
    //    "        return \"PROXY localhost:%u\"\r\n"
    //    "    else\r\n"
    //    "        return \"DIRECT\";\r\n"
    //    "}";

    // const char PROXY_SCRIPT_TEXT[] =
    //    "function FindProxyForURL(url, host)\r\n"
    //    "{\r\n"
    //    "    var reg1=/\\.flv/;\r\n"
    //    "   var reg2=/\\.flvx/;\r\n"
    //    "    if(reg1.test(url) || reg2.test(url))\r\n"
    //    "        return \"PROXY localhost:%u\"\r\n"
    //    "    else\r\n"
    //    "        return \"DIRECT\";\r\n"
    //    "}";

}

#endif  // _P2SP_PROXY_PROXY_SCRIPT_H_
