/*

  This file is a part of JRTPLIB
  Copyright (c) 1999-2007 Jori Liesenborgs

  Contact: jori.liesenborgs@gmail.com

  This library was developed at the "Expertisecentrum Digitale Media"
  (http://www.edm.uhasselt.be), a research center of the Hasselt University
  (http://www.uhasselt.be). The library is based upon work done for 
  my thesis at the School for Knowledge Technology (Belgium/The Netherlands).

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.

*/

/**
 * \file rtpudpv6transmitter.h
 */

#ifndef RTPUDPV6TRANSMITTER_H

#define RTPUDPV6TRANSMITTER_H

#include "rtpconfig.h"

#ifdef RTP_SUPPORT_IPV6

#include "rtptransmitter.h"
#include "rtpipv6destination.h"
#include "rtphashtable.h"
#include "rtpkeyhashtable.h"
#if ! (defined(WIN32) || defined(_WIN32_WCE))
	#include <netinet/in.h>
#endif // WIN32
#include <string.h>
#include <list>

#ifdef RTP_SUPPORT_THREAD
	#include <jmutex.h>
#endif // RTP_SUPPORT_THREAD

#define RTPUDPV6TRANS_HASHSIZE										8317
#define RTPUDPV6TRANS_DEFAULTPORTBASE								5000

#define RTPUDPV6TRANS_RTPRECEIVEBUFFER							32768
#define RTPUDPV6TRANS_RTCPRECEIVEBUFFER							32768
#define RTPUDPV6TRANS_RTPTRANSMITBUFFER							32768
#define RTPUDPV6TRANS_RTCPTRANSMITBUFFER						32768

/** Parameters for the UDP over IPv6 transmitter. */
class RTPUDPv6TransmissionParams : public RTPTransmissionParams
{
public:
	RTPUDPv6TransmissionParams():RTPTransmissionParams(RTPTransmitter::IPv6UDPProto)	{ portbase = RTPUDPV6TRANS_DEFAULTPORTBASE; for (int i = 0 ; i < 16 ; i++) bindIP.s6_addr[i] = 0; multicastTTL = 1; mcastifidx = 0; rtpsendbuf = RTPUDPV6TRANS_RTPTRANSMITBUFFER; rtprecvbuf= RTPUDPV6TRANS_RTPRECEIVEBUFFER; rtcpsendbuf = RTPUDPV6TRANS_RTCPTRANSMITBUFFER; rtcprecvbuf = RTPUDPV6TRANS_RTCPRECEIVEBUFFER; }

	/** Sets the IP address which is used to bind the sockets to \c ip. */
	void SetBindIP(in6_addr ip)											{ bindIP = ip; }

	/** Sets the multicast interface index. */
	void SetMulticastInterfaceIndex(unsigned int idx)					{ mcastifidx = idx; }

	/** Sets the RTP portbase to \c pbase. This has to be an even number. */
	void SetPortbase(uint16_t pbase)									{ portbase = pbase; }

	/** Sets the multicast TTL to be used to \c mcastTTL. */
	void SetMulticastTTL(uint8_t mcastTTL)								{ multicastTTL = mcastTTL; }

	/** Passes a list of IP addresses which will be used as the local IP addresses. */
	void SetLocalIPList(std::list<in6_addr> &iplist)					{ localIPs = iplist; } 

	/** Clears the list of local IP addresses.
	 *  Clears the list of local IP addresses. An empty list will make the transmission component 
	 *  itself determine the local IP addresses.
	 */
	void ClearLocalIPList()												{ localIPs.clear(); }

	/** Returns the IP address which will be used to bind the sockets. */
	in6_addr GetBindIP() const											{ return bindIP; }

	/** Returns the multicast interface index. */
	unsigned int GetMulticastInterfaceIndex() const						{ return mcastifidx; }

	/** Returns the RTP portbase which will be used (default is 5000). */
	uint16_t GetPortbase() const										{ return portbase; }

	/** Returns the multicast TTL which will be used (default is 1). */
	uint8_t GetMulticastTTL() const										{ return multicastTTL; }

	/** Returns the list of local IP addresses. */
	const std::list<in6_addr> &GetLocalIPList() const					{ return localIPs; }

	/** Sets the RTP socket's send buffer size. */
	void SetRTPSendBuffer(int s)								{ rtpsendbuf = s; }

	/** Sets the RTP socket's receive buffer size. */
	void SetRTPReceiveBuffer(int s)								{ rtprecvbuf = s; }

	/** Sets the RTCP socket's send buffer size. */
	void SetRTCPSendBuffer(int s)								{ rtcpsendbuf = s; }

	/** Sets the RTCP socket's receive buffer size. */
	void SetRTCPReceiveBuffer(int s)							{ rtcprecvbuf = s; }

	/** Returns the RTP socket's send buffer size. */
	int GetRTPSendBuffer() const								{ return rtpsendbuf; }

	/** Returns the RTP socket's receive buffer size. */
	int GetRTPReceiveBuffer() const								{ return rtprecvbuf; }

	/** Returns the RTCP socket's send buffer size. */
	int GetRTCPSendBuffer() const								{ return rtcpsendbuf; }

	/** Returns the RTCP socket's receive buffer size. */
	int GetRTCPReceiveBuffer() const							{ return rtcprecvbuf; }
private:
	uint16_t portbase;
	in6_addr bindIP;
	unsigned int mcastifidx;
	std::list<in6_addr> localIPs;
	uint8_t multicastTTL;
	int rtpsendbuf, rtprecvbuf;
	int rtcpsendbuf, rtcprecvbuf;
};

/** Additional information about the UDP over IPv6 transmitter. */
class RTPUDPv6TransmissionInfo : public RTPTransmissionInfo
{
public:
#if ! (defined(WIN32) || defined(_WIN32_WCE))
	RTPUDPv6TransmissionInfo(std::list<in6_addr> iplist,int rtpsock,int rtcpsock) : RTPTransmissionInfo(RTPTransmitter::IPv6UDPProto) 
#else
	RTPUDPv6TransmissionInfo(std::list<in6_addr> iplist,SOCKET rtpsock,SOCKET rtcpsock) : RTPTransmissionInfo(RTPTransmitter::IPv6UDPProto) 
#endif  // WIN32
															{ localIPlist = iplist; rtpsocket = rtpsock; rtcpsocket = rtcpsock; }

	~RTPUDPv6TransmissionInfo()								{ }

	/** Returns the list of IPv6 addresses the transmitter considers to be the local IP addresses. */
	std::list<in6_addr> GetLocalIPList() const				{ return localIPlist; }
#if ! (defined(WIN32) || defined(_WIN32_WCE))
	/** Returns the socket descriptor used for receiving and transmitting RTP packets. */
	int GetRTPSocket() const								{ return rtpsocket; }

	/** Returns the socket descriptor used for receiving and transmitting RTCP packets. */
	int GetRTCPSocket() const								{ return rtcpsocket; }
#else
	SOCKET GetRTPSocket() const								{ return rtpsocket; }
	SOCKET GetRTCPSocket() const							{ return rtcpsocket; }
#endif // WIN32
private:
	std::list<in6_addr> localIPlist;
#if ! (defined(WIN32) || defined(_WIN32_WCE))
	int rtpsocket,rtcpsocket;
#else
	SOCKET rtpsocket,rtcpsocket;
#endif // WIN32
};
		
class RTPUDPv6Trans_GetHashIndex_IPv6Dest
{
public:
	static int GetIndex(const RTPIPv6Destination &d)					{ in6_addr ip = d.GetIP(); return ((((uint32_t)ip.s6_addr[12])<<24)|(((uint32_t)ip.s6_addr[13])<<16)|(((uint32_t)ip.s6_addr[14])<<8)|((uint32_t)ip.s6_addr[15]))%RTPUDPV6TRANS_HASHSIZE; }
};

class RTPUDPv6Trans_GetHashIndex_in6_addr
{
public:
	static int GetIndex(const in6_addr &ip)							{ return ((((uint32_t)ip.s6_addr[12])<<24)|(((uint32_t)ip.s6_addr[13])<<16)|(((uint32_t)ip.s6_addr[14])<<8)|((uint32_t)ip.s6_addr[15]))%RTPUDPV6TRANS_HASHSIZE; }
};

#define RTPUDPV6TRANS_HEADERSIZE								(40+8)
	
/** An UDP over IPv6 transmitter.
 *  This class inherits the RTPTransmitter interface and implements a transmission component 
 *  which uses UDP over IPv6 to send and receive RTP and RTCP data. The component's parameters 
 *  are described by the class RTPUDPv6TransmissionParams. The functions which have an RTPAddress 
 *  argument require an argument of RTPIPv6Address. The GetTransmissionInfo member function
 *  returns an instance of type RTPUDPv6TransmissionInfo.
 */
class RTPUDPv6Transmitter : public RTPTransmitter
{
public:
	RTPUDPv6Transmitter(RTPMemoryManager *mgr);
	~RTPUDPv6Transmitter();

	int Init(bool treadsafe);
	int Create(size_t maxpacksize,const RTPTransmissionParams *transparams);
	void Destroy();
	RTPTransmissionInfo *GetTransmissionInfo();

	int GetLocalHostName(uint8_t *buffer,size_t *bufferlength);
	bool ComesFromThisTransmitter(const RTPAddress *addr);
	size_t GetHeaderOverhead()								{ return RTPUDPV6TRANS_HEADERSIZE; }
	
	int Poll();
	int WaitForIncomingData(const RTPTime &delay,bool *dataavailable = 0);
	int AbortWait();
	
	int SendRTPData(const void *data,size_t len);	
	int SendRTCPData(const void *data,size_t len);

	int AddDestination(const RTPAddress &addr);
	int DeleteDestination(const RTPAddress &addr);
	void ClearDestinations();

	bool SupportsMulticasting();
	int JoinMulticastGroup(const RTPAddress &addr);
	int LeaveMulticastGroup(const RTPAddress &addr);
	void LeaveAllMulticastGroups();

	int SetReceiveMode(RTPTransmitter::ReceiveMode m);
	int AddToIgnoreList(const RTPAddress &addr);
	int DeleteFromIgnoreList(const RTPAddress &addr);
	void ClearIgnoreList();
	int AddToAcceptList(const RTPAddress &addr);
	int DeleteFromAcceptList(const RTPAddress &addr);
	void ClearAcceptList();
	int SetMaximumPacketSize(size_t s);	
	
	bool NewDataAvailable();
	RTPRawPacket *GetNextPacket();
#ifdef RTPDEBUG
	void Dump();
#endif // RTPDEBUG
private:
	int CreateLocalIPList();
	bool GetLocalIPList_Interfaces();
	void GetLocalIPList_DNS();
	void AddLoopbackAddress();
	void FlushPackets();
	int PollSocket(bool rtp);
	int ProcessAddAcceptIgnoreEntry(in6_addr ip,uint16_t port);
	int ProcessDeleteAcceptIgnoreEntry(in6_addr ip,uint16_t port);
#ifdef RTP_SUPPORT_IPV6MULTICAST
	bool SetMulticastTTL(uint8_t ttl);
#endif // RTP_SUPPORT_IPV6MULTICAST
	bool ShouldAcceptData(in6_addr srcip,uint16_t srcport);
	void ClearAcceptIgnoreInfo();
	
	bool init;
	bool created;
	bool waitingfordata;
#if (defined(WIN32) || defined(_WIN32_WCE))
	SOCKET rtpsock,rtcpsock;
#else // not using winsock
	int rtpsock,rtcpsock;
#endif // WIN32
	in6_addr bindIP;
	unsigned int mcastifidx;
	std::list<in6_addr> localIPs;
	uint16_t portbase;
	uint8_t multicastTTL;
	RTPTransmitter::ReceiveMode receivemode;

	uint8_t *localhostname;
	size_t localhostnamelength;
	
	RTPHashTable<const RTPIPv6Destination,RTPUDPv6Trans_GetHashIndex_IPv6Dest,RTPUDPV6TRANS_HASHSIZE> destinations;
#ifdef RTP_SUPPORT_IPV6MULTICAST
	RTPHashTable<const in6_addr,RTPUDPv6Trans_GetHashIndex_in6_addr,RTPUDPV6TRANS_HASHSIZE> multicastgroups;
#endif // RTP_SUPPORT_IPV6MULTICAST
	std::list<RTPRawPacket*> rawpacketlist;

	bool supportsmulticasting;
	size_t maxpacksize;

	class PortInfo
	{
	public:
		PortInfo() { all = false; }
		
		bool all;
		std::list<uint16_t> portlist;
	};

	RTPKeyHashTable<const in6_addr,PortInfo*,RTPUDPv6Trans_GetHashIndex_in6_addr,RTPUDPV6TRANS_HASHSIZE> acceptignoreinfo;

	// notification descriptors for AbortWait (0 is for reading, 1 for writing)
#if (defined(WIN32) || defined(_WIN32_WCE))
	SOCKET abortdesc[2];
#else
	int abortdesc[2];
#endif // WIN32
	int CreateAbortDescriptors();
	void DestroyAbortDescriptors();
	void AbortWaitInternal();
#ifdef RTP_SUPPORT_THREAD
	JMutex mainmutex,waitmutex;
	int threadsafe;
#endif // RTP_SUPPORT_THREAD
};

#endif // RTP_SUPPORT_IPV6

#endif // RTPUDPV6TRANSMITTER_H

