/*

  This file is a part of JRTPLIB
  Copyright (c) 1999-2017 Jori Liesenborgs

  Contact: jori.liesenborgs@gmail.com

  This library was developed at the Expertise Centre for Digital Media
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
 * \file rtpudpv4transmitter.h
 */

#ifndef RTPUDPV4TRANSMITTER_H

#define RTPUDPV4TRANSMITTER_H

#include "rtpconfig.h"
#include "rtptransmitter.h"
#include "rtpipv4destination.h"
#include "rtphashtable.h"
#include "rtpkeyhashtable.h"
#include "rtpsocketutil.h"
#include "rtpabortdescriptors.h"
#include <list>

#ifdef RTP_SUPPORT_THREAD
	#include <jthread/jmutex.h>
#endif // RTP_SUPPORT_THREAD

#define RTPUDPV4TRANS_HASHSIZE									8317
#define RTPUDPV4TRANS_DEFAULTPORTBASE								5000

#define RTPUDPV4TRANS_RTPRECEIVEBUFFER							32768
#define RTPUDPV4TRANS_RTCPRECEIVEBUFFER							32768
#define RTPUDPV4TRANS_RTPTRANSMITBUFFER							32768
#define RTPUDPV4TRANS_RTCPTRANSMITBUFFER						32768

namespace jrtplib
{

/** Parameters for the UDP over IPv4 transmitter. */
class JRTPLIB_IMPORTEXPORT RTPUDPv4TransmissionParams : public RTPTransmissionParams
{
public:
	RTPUDPv4TransmissionParams();

	/** Sets the IP address which is used to bind the sockets to \c ip. */
	void SetBindIP(uint32_t ip)									{ bindIP = ip; }

	/** Sets the multicast interface IP address. */
	void SetMulticastInterfaceIP(uint32_t ip)					{ mcastifaceIP = ip; }

	/** Sets the RTP portbase to \c pbase, which has to be an even number
	 *  unless RTPUDPv4TransmissionParams::SetAllowOddPortbase was called;
	 *  a port number of zero will cause a port to be chosen automatically. */
	void SetPortbase(uint16_t pbase)							{ portbase = pbase; }

	/** Sets the multicast TTL to be used to \c mcastTTL. */
	void SetMulticastTTL(uint8_t mcastTTL)						{ multicastTTL = mcastTTL; }

	/** Passes a list of IP addresses which will be used as the local IP addresses. */
	void SetLocalIPList(std::list<uint32_t> &iplist)			{ localIPs = iplist; } 

	/** Clears the list of local IP addresses. 
	 *  Clears the list of local IP addresses. An empty list will make the transmission 
	 *  component itself determine the local IP addresses.
	 */
	void ClearLocalIPList()										{ localIPs.clear(); }

	/** Returns the IP address which will be used to bind the sockets. */
	uint32_t GetBindIP() const									{ return bindIP; }

	/** Returns the multicast interface IP address. */
	uint32_t GetMulticastInterfaceIP() const					{ return mcastifaceIP; }

	/** Returns the RTP portbase which will be used (default is 5000). */
	uint16_t GetPortbase() const								{ return portbase; }

	/** Returns the multicast TTL which will be used (default is 1). */
	uint8_t GetMulticastTTL() const								{ return multicastTTL; }

	/** Returns the list of local IP addresses. */
	const std::list<uint32_t> &GetLocalIPList() const			{ return localIPs; }

	/** Sets the RTP socket's send buffer size. */
	void SetRTPSendBuffer(int s)								{ rtpsendbuf = s; }

	/** Sets the RTP socket's receive buffer size. */
	void SetRTPReceiveBuffer(int s)								{ rtprecvbuf = s; }

	/** Sets the RTCP socket's send buffer size. */
	void SetRTCPSendBuffer(int s)								{ rtcpsendbuf = s; }

	/** Sets the RTCP socket's receive buffer size. */
	void SetRTCPReceiveBuffer(int s)							{ rtcprecvbuf = s; }

	/** Enables or disables multiplexing RTCP traffic over the RTP channel, so that only a single port is used. */
	void SetRTCPMultiplexing(bool f)							{ rtcpmux = f; }

	/** Can be used to allow the RTP port base to be any number, not just even numbers. */
	void SetAllowOddPortbase(bool f)							{ allowoddportbase = f; }

	/** Force the RTCP socket to use a specific port, not necessarily one more than
	 *  the RTP port (set this to zero to disable). */
	void SetForcedRTCPPort(uint16_t rtcpport)					{ forcedrtcpport = rtcpport; }

	/** Use sockets that have already been created, no checks on port numbers
	 *  will be done, and no buffer sizes will be set; you'll need to close
	 *  the sockets yourself when done, it will **not** be done automatically. */
	void SetUseExistingSockets(SocketType rtpsocket, SocketType rtcpsocket) { rtpsock = rtpsocket; rtcpsock = rtcpsocket; useexistingsockets = true; }

	/** If non null, the specified abort descriptors will be used to cancel
	 *  the function that's waiting for packets to arrive; set to null (the default
	 *  to let the transmitter create its own instance. */
	void SetCreatedAbortDescriptors(RTPAbortDescriptors *desc) { m_pAbortDesc = desc; }

	/** Returns the RTP socket's send buffer size. */
	int GetRTPSendBuffer() const								{ return rtpsendbuf; }

	/** Returns the RTP socket's receive buffer size. */
	int GetRTPReceiveBuffer() const								{ return rtprecvbuf; }

	/** Returns the RTCP socket's send buffer size. */
	int GetRTCPSendBuffer() const								{ return rtcpsendbuf; }

	/** Returns the RTCP socket's receive buffer size. */
	int GetRTCPReceiveBuffer() const							{ return rtcprecvbuf; }

	/** Returns a flag indicating if RTCP traffic will be multiplexed over the RTP channel. */
	bool GetRTCPMultiplexing() const							{ return rtcpmux; }

	/** If true, any RTP portbase will be allowed, not just even numbers. */
	bool GetAllowOddPortbase() const							{ return allowoddportbase; }

	/** If non-zero, the specified port will be used to receive RTCP traffic. */
	uint16_t GetForcedRTCPPort() const							{ return forcedrtcpport; }

	/** Returns true and fills in sockets if existing sockets were set
	 *  using RTPUDPv4TransmissionParams::SetUseExistingSockets. */
	bool GetUseExistingSockets(SocketType &rtpsocket, SocketType &rtcpsocket) const { if (!useexistingsockets) return false; rtpsocket = rtpsock; rtcpsocket = rtcpsock; return true; }

	/** If non-null, this RTPAbortDescriptors instance will be used internally,
	 *  which can be useful when creating your own poll thread for multiple
	 *  sessions. */
	RTPAbortDescriptors *GetCreatedAbortDescriptors() const		{ return m_pAbortDesc; }
private:
	uint16_t portbase;
	uint32_t bindIP, mcastifaceIP;
	std::list<uint32_t> localIPs;
	uint8_t multicastTTL;
	int rtpsendbuf, rtprecvbuf;
	int rtcpsendbuf, rtcprecvbuf;
	bool rtcpmux;
	bool allowoddportbase;
	uint16_t forcedrtcpport;

	SocketType rtpsock, rtcpsock;
	bool useexistingsockets;

	RTPAbortDescriptors *m_pAbortDesc;
};

inline RTPUDPv4TransmissionParams::RTPUDPv4TransmissionParams() : RTPTransmissionParams(RTPTransmitter::IPv4UDPProto)	
{ 
	portbase = RTPUDPV4TRANS_DEFAULTPORTBASE; 
	bindIP = 0; 
	multicastTTL = 1; 
	mcastifaceIP = 0; 
	rtpsendbuf = RTPUDPV4TRANS_RTPTRANSMITBUFFER; 
	rtprecvbuf = RTPUDPV4TRANS_RTPRECEIVEBUFFER; 
	rtcpsendbuf = RTPUDPV4TRANS_RTCPTRANSMITBUFFER; 
	rtcprecvbuf = RTPUDPV4TRANS_RTCPRECEIVEBUFFER; 
	rtcpmux = false;
	allowoddportbase = false;
	forcedrtcpport = 0;
	useexistingsockets = false;
	rtpsock = 0;
	rtcpsock = 0;
	m_pAbortDesc = 0;
}

/** Additional information about the UDP over IPv4 transmitter. */
class JRTPLIB_IMPORTEXPORT RTPUDPv4TransmissionInfo : public RTPTransmissionInfo
{
public:
	RTPUDPv4TransmissionInfo(std::list<uint32_t> iplist,SocketType rtpsock,SocketType rtcpsock,
	                         uint16_t rtpport, uint16_t rtcpport) : RTPTransmissionInfo(RTPTransmitter::IPv4UDPProto) 
															{ localIPlist = iplist; rtpsocket = rtpsock; rtcpsocket = rtcpsock; m_rtpPort = rtpport; m_rtcpPort = rtcpport; }

	~RTPUDPv4TransmissionInfo()								{ }
	
	/** Returns the list of IPv4 addresses the transmitter considers to be the local IP addresses. */
	std::list<uint32_t> GetLocalIPList() const				{ return localIPlist; }

	/** Returns the socket descriptor used for receiving and transmitting RTP packets. */
	SocketType GetRTPSocket() const							{ return rtpsocket; }

	/** Returns the socket descriptor used for receiving and transmitting RTCP packets. */
	SocketType GetRTCPSocket() const						{ return rtcpsocket; }

	/** Returns the port number that the RTP socket receives packets on. */
	uint16_t GetRTPPort() const								{ return m_rtpPort; }

	/** Returns the port number that the RTCP socket receives packets on. */
	uint16_t GetRTCPPort() const							{ return m_rtcpPort; }
private:
	std::list<uint32_t> localIPlist;
	SocketType rtpsocket,rtcpsocket;
	uint16_t m_rtpPort, m_rtcpPort;
};
	
class JRTPLIB_IMPORTEXPORT RTPUDPv4Trans_GetHashIndex_IPv4Dest
{
public:
	static int GetIndex(const RTPIPv4Destination &d)							{ return d.GetIP()%RTPUDPV4TRANS_HASHSIZE; }
};

class JRTPLIB_IMPORTEXPORT RTPUDPv4Trans_GetHashIndex_uint32_t
{
public:
	static int GetIndex(const uint32_t &k)									{ return k%RTPUDPV4TRANS_HASHSIZE; }
};

#define RTPUDPV4TRANS_HEADERSIZE						(20+8)
	
/** An UDP over IPv4 transmission component.
 *  This class inherits the RTPTransmitter interface and implements a transmission component 
 *  which uses UDP over IPv4 to send and receive RTP and RTCP data. The component's parameters 
 *  are described by the class RTPUDPv4TransmissionParams. The functions which have an RTPAddress 
 *  argument require an argument of RTPIPv4Address. The GetTransmissionInfo member function
 *  returns an instance of type RTPUDPv4TransmissionInfo.
 */
class JRTPLIB_IMPORTEXPORT RTPUDPv4Transmitter : public RTPTransmitter
{
	JRTPLIB_NO_COPY(RTPUDPv4Transmitter)
public:
	RTPUDPv4Transmitter(RTPMemoryManager *mgr);
	~RTPUDPv4Transmitter();

	int Init(bool treadsafe);
	int Create(size_t maxpacksize,const RTPTransmissionParams *transparams);
	void Destroy();
	RTPTransmissionInfo *GetTransmissionInfo();
	void DeleteTransmissionInfo(RTPTransmissionInfo *inf);

	int GetLocalHostName(uint8_t *buffer,size_t *bufferlength);
	bool ComesFromThisTransmitter(const RTPAddress *addr);
	size_t GetHeaderOverhead()							{ return RTPUDPV4TRANS_HEADERSIZE; }
	
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
	int ProcessAddAcceptIgnoreEntry(uint32_t ip,uint16_t port);
	int ProcessDeleteAcceptIgnoreEntry(uint32_t ip,uint16_t port);
#ifdef RTP_SUPPORT_IPV4MULTICAST
	bool SetMulticastTTL(uint8_t ttl);
#endif // RTP_SUPPORT_IPV4MULTICAST
	bool ShouldAcceptData(uint32_t srcip,uint16_t srcport);
	void ClearAcceptIgnoreInfo();
	
	bool init;
	bool created;
	bool waitingfordata;
	SocketType rtpsock,rtcpsock;
	uint32_t mcastifaceIP;
	std::list<uint32_t> localIPs;
	uint16_t m_rtpPort, m_rtcpPort;
	uint8_t multicastTTL;
	RTPTransmitter::ReceiveMode receivemode;

	uint8_t *localhostname;
	size_t localhostnamelength;
	
	RTPHashTable<const RTPIPv4Destination,RTPUDPv4Trans_GetHashIndex_IPv4Dest,RTPUDPV4TRANS_HASHSIZE> destinations;
#ifdef RTP_SUPPORT_IPV4MULTICAST
	RTPHashTable<const uint32_t,RTPUDPv4Trans_GetHashIndex_uint32_t,RTPUDPV4TRANS_HASHSIZE> multicastgroups;
#endif // RTP_SUPPORT_IPV4MULTICAST
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

	RTPKeyHashTable<const uint32_t,PortInfo*,RTPUDPv4Trans_GetHashIndex_uint32_t,RTPUDPV4TRANS_HASHSIZE> acceptignoreinfo;

	bool closesocketswhendone;
	RTPAbortDescriptors m_abortDesc;
	RTPAbortDescriptors *m_pAbortDesc; // in case an external one was specified

#ifdef RTP_SUPPORT_THREAD
	jthread::JMutex mainmutex,waitmutex;
	int threadsafe;
#endif // RTP_SUPPORT_THREAD
};

} // end namespace

#endif // RTPUDPV4TRANSMITTER_H

