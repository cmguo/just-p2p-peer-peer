//------------------------------------------------------------------------------------------
//     Copyright (c)2005-2010 PPLive Corporation.  All rights reserved.
//------------------------------------------------------------------------------------------

#ifndef P2SP_STUN_H
#define P2SP_STUN_H

#include <time.h>

namespace p2sp
{
#define Myclog clog

    // if you change this version, change in makefile too
#define STUN_VERSION "0.96"

#define STUN_MAX_STRING 256
#define STUN_MAX_UNKNOWN_ATTRIBUTES 8
#define STUN_MAX_MESSAGE_SIZE 2048

#define STUN_PORT 3478

    // define some basic types
    typedef struct { boost::uint8_t octet[16]; }  UInt128;

    /// define a structure to hold a stun address
    const boost::uint8_t  IPv4Family = 0x01;
    const boost::uint8_t  IPv6Family = 0x02;

    // define  flags
    const uint32_t ChangeIpFlag   = 0x04;
    const uint32_t ChangePortFlag = 0x02;

    // define  stun attribute
    const boost::uint16_t MappedAddress    = 0x0001;
    const boost::uint16_t ResponseAddress  = 0x0002;
    const boost::uint16_t ChangeRequest    = 0x0003;
    const boost::uint16_t SourceAddress    = 0x0004;
    const boost::uint16_t ChangedAddress   = 0x0005;
    const boost::uint16_t Username         = 0x0006;
    const boost::uint16_t Password         = 0x0007;
    const boost::uint16_t MessageIntegrity = 0x0008;
    const boost::uint16_t ErrorCode        = 0x0009;
    const boost::uint16_t UnknownAttribute = 0x000A;
    const boost::uint16_t ReflectedFrom    = 0x000B;
    const boost::uint16_t XorMappedAddress = 0x8020;
    const boost::uint16_t XorOnly          = 0x0021;
    const boost::uint16_t ServerName       = 0x8022;
    const boost::uint16_t SecondaryAddress = 0x8050;  // Non standard extention

    // define types for a stun message
    const boost::uint16_t BindRequestMsg               = 0x0001;
    const boost::uint16_t BindResponseMsg              = 0x0101;
    const boost::uint16_t BindErrorResponseMsg         = 0x0111;
    const boost::uint16_t SharedSecretRequestMsg       = 0x0002;
    const boost::uint16_t SharedSecretResponseMsg      = 0x0102;
    const boost::uint16_t SharedSecretErrorResponseMsg = 0x0112;

    typedef struct
    {
        boost::uint16_t msgType;
        boost::uint16_t msgLength;
        UInt128 id;
    } StunMsgHdr;


    typedef struct
    {
        boost::uint16_t type;
        boost::uint16_t length;
    } StunAtrHdr;

    typedef struct
    {
        boost::uint16_t port;
        uint32_t addr;
    } StunAddress4;

    typedef struct
    {
        boost::uint8_t pad;
        boost::uint8_t family;
        StunAddress4 ipv4;
    } StunAtrAddress4;

    typedef struct
    {
        uint32_t value;
    } StunAtrChangeRequest;

    typedef struct
    {
        boost::uint16_t pad;  // all 0
        boost::uint8_t errorClass;
        boost::uint8_t number;
        char reason[STUN_MAX_STRING];
        boost::uint16_t sizeReason;
    } StunAtrError;

    typedef struct
    {
        boost::uint16_t attrType[STUN_MAX_UNKNOWN_ATTRIBUTES];
        boost::uint16_t numAttributes;
    } StunAtrUnknown;

    typedef struct
    {
        char value[STUN_MAX_STRING];
        boost::uint16_t sizeValue;
    } StunAtrString;

    typedef struct
    {
        char hash[20];
    } StunAtrIntegrity;

    typedef enum
    {
        HmacUnkown = 0,
        HmacOK,
        HmacBadUserName,
        HmacUnkownUserName,
        HmacFailed,
    } StunHmacStatus;

    typedef struct
    {
        StunMsgHdr msgHdr;

        bool hasMappedAddress;
        StunAtrAddress4  mappedAddress;

        bool hasResponseAddress;
        StunAtrAddress4  responseAddress;

        bool hasChangeRequest;
        StunAtrChangeRequest changeRequest;

        bool hasSourceAddress;
        StunAtrAddress4 sourceAddress;

        bool hasChangedAddress;
        StunAtrAddress4 changedAddress;

        bool hasUsername;
        StunAtrString username;

        bool hasPassword;
        StunAtrString password;

        bool hasMessageIntegrity;
        StunAtrIntegrity messageIntegrity;

        bool hasErrorCode;
        StunAtrError errorCode;

        bool hasUnknownAttributes;
        StunAtrUnknown unknownAttributes;

        bool hasReflectedFrom;
        StunAtrAddress4 reflectedFrom;

        bool hasXorMappedAddress;
        StunAtrAddress4  xorMappedAddress;

        bool xorOnly;

        bool hasServerName;
        StunAtrString serverName;

        bool hasSecondaryAddress;
        StunAtrAddress4 secondaryAddress;
    } StunMessage;


    // Define enum with different types of NAT
    typedef enum
    {
        StunTypeUnknown = 0,
        StunTypeFailure,
        StunTypeOpen,
        StunTypeBlocked,

        StunTypeIndependentFilter,
        StunTypeDependentFilter,
        StunTypePortDependedFilter,
        StunTypeDependentMapping,

        StunTypeConeNat,
        StunTypeRestrictedNat,
        StunTypePortRestrictedNat,
        StunTypeSymNat,

        StunTypeFirewall,
    } NatType;

#ifdef WIN32
    typedef SOCKET Socket;
#else
    typedef int Socket;
#endif

#define MAX_MEDIA_RELAYS 500
#define MAX_RTP_MSG_SIZE 1500
#define MEDIA_RELAY_TIMEOUT 3*60

    typedef struct
    {
        int relayPort;       // media relay port
        Socket fd;              // media relay file descriptor
        StunAddress4 destination;  // NAT IP:port
        time_t expireTime;      // if no activity after time, close the socket
    } StunMediaRelay;

    typedef struct
    {
        StunAddress4 myAddr;
        StunAddress4 altAddr;
        Socket myFd;
        Socket altPortFd;
        Socket altIpFd;
        Socket altIpPortFd;
        bool relay;  // true if media relaying is to be done
        StunMediaRelay relays[MAX_MEDIA_RELAYS];
    } StunServerInfo;

    bool
        stunParseMessage(char* buf,
        unsigned int bufLen,
        StunMessage& message,
        bool verbose);

    void
        stunBuildReqSimple(StunMessage* msg,
        const StunAtrString& username,
        bool changePort, bool changeIp, unsigned int id = 0);

    unsigned int
        stunEncodeMessage(const StunMessage& message,
        char* buf,
        unsigned int bufLen,
        const StunAtrString& password,
        bool verbose);

    void
        stunCreateUserName(const StunAddress4& addr, StunAtrString* username);

    void
        stunGetUserNameAndPassword( const StunAddress4& dest,
        StunAtrString* username,
        StunAtrString* password);

    void
        stunCreatePassword(const StunAtrString& username, StunAtrString* password);

    int
        stunRand();

    boost::uint64_t
        stunGetSystemTimeSecs();

    /// find the IP address of a the specified stun server - return false is fails parse
    bool
        stunParseServerName(char* serverName, StunAddress4& stunServerAddr);

    bool
        stunParseHostName(char* peerName,
        uint32_t& ip,
        boost::uint16_t& portVal,
        boost::uint16_t defaultPort);

    /// return true if all is OK
    /// Create a media relay and do the STERN thing if startMediaPort is non-zero
    bool
        stunInitServer(StunServerInfo& info,
        const StunAddress4& myAddr,
        const StunAddress4& altAddr,
        int startMediaPort,
        bool verbose);

    void
        stunStopServer(StunServerInfo& info);

    /// return true if all is OK
    bool
        stunServerProcess(StunServerInfo& info, bool verbose);

    /// returns number of address found - take array or addres
    int
        stunFindLocalInterfaces(uint32_t* addresses, int maxSize);

    void
        stunTest(StunAddress4& dest, int testNum, bool verbose, StunAddress4* srcAddr = 0);

    NatType
        stunNatType(StunAddress4& dest, bool verbose,
        bool* preservePort = 0,  // if std::set, is return for if NAT preservers ports or not
        bool* hairpin = 0 ,  // if std::set, is the return for if NAT will hairpin packets
        int port = 0,  // port to use for the test, 0 to choose random port
        StunAddress4* sAddr = 0,  // NIC to use,
        Socket *pSocket1 = NULL,
        Socket *pSocket2 = NULL
       );

    /// prints a StunAddress
    std::ostream&
        operator << (std::ostream& strm, const StunAddress4& addr);

    std::ostream&
        operator << (std::ostream& strm, const UInt128&);


    bool
        stunServerProcessMsg(char* buf,
        unsigned int bufLen,
        StunAddress4& from,
        StunAddress4& myAddr,
        StunAddress4& altAddr,
        StunMessage* resp,
        StunAddress4* destination,
        StunAtrString* hmacPassword,
        bool* changePort,
        bool* changeIp,
        bool verbose);

    int
        stunOpenSocket(StunAddress4& dest,
        StunAddress4* mappedAddr,
        int port = 0,
        StunAddress4* srcAddr = 0,
        bool verbose = false);

    bool
        stunOpenSocketPair(StunAddress4& dest, StunAddress4* mappedAddr,
        int* fd1, int* fd2,
        int srcPort = 0,  StunAddress4* srcAddr = 0,
        bool verbose = false);

    int
        stunRandomPort();


}

#endif  // P2SP_STUN_H


/* = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = ==
* The Vovida Software License, Version 1.0
*
* Copyright (c) 2000 Vovida Networks, Inc.  All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions
* are met:
*
* 1. Redistributions of source code must retain the above copyright
*    notice, this std::list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright
*    notice, this std::list of conditions and the following disclaimer in
*    the documentation and/or other materials provided with the
*    distribution.
*
* 3. The names "VOCAL", "Vovida Open Communication Application Library",
*    and "Vovida Open Communication Application Library (VOCAL)" must
*    not be used to endorse or promote products derived from this
*    software without prior written permission. For written
*    permission, please contact vocal@vovida.org.
*
* 4. Products derived from this software may not be called "VOCAL", nor
*    may "VOCAL" appear in their name, without prior written
*    permission of Vovida Networks, Inc.
*
* THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND
* NON-INFRINGEMENT ARE DISCLAIMED.  IN NO EVENT SHALL VOVIDA
* NETWORKS, INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT DAMAGES
* IN EXCESS OF $1, 000, NOR FOR ANY INDIRECT, INCIDENTAL, SPECIAL,
* EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
* PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
* OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
* USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
* DAMAGE.
*
* = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = ==
*
* This software consists of voluntary contributions made by Vovida
* Networks, Inc. and many individuals on behalf of Vovida Networks,
* Inc.  For more information on Vovida Networks, Inc., please see
* <http://www.vovida.org/>.
*
*/

// Local Variables:
// mode:c++
// c-file-style:"ellemtel"
// c-file-offsets:((case-label . + ))
// indent-tabs-mode:nil
// End:

/*
http://sourceforge.net/projects/stun/
*/
