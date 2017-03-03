/*

  This file is a part of JRTPLIB
  Copyright (c) 1999-2016 Jori Liesenborgs

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

#include "rtptcptransmitter.h"
#include "rtprawpacket.h"
#include "rtptcpaddress.h"
#include "rtptimeutilities.h"
#include "rtpdefines.h"
#include "rtpstructs.h"
#include "rtpsocketutilinternal.h"
#include "rtpinternalutils.h"
#include "rtpselect.h"
#include <stdio.h>
#include <assert.h>
#include <vector>
#ifdef RTPDEBUG
	#include <iostream>
#endif // RTPDEBUG

#include <iostream>

#include "rtpdebug.h"

using namespace std;

#define RTPTCPTRANS_MAXPACKSIZE							65535

#ifdef RTP_SUPPORT_THREAD
	#define MAINMUTEX_LOCK 		{ if (m_threadsafe) m_mainMutex.Lock(); }
	#define MAINMUTEX_UNLOCK	{ if (m_threadsafe) m_mainMutex.Unlock(); }
	#define WAITMUTEX_LOCK		{ if (m_threadsafe) m_waitMutex.Lock(); }
	#define WAITMUTEX_UNLOCK	{ if (m_threadsafe) m_waitMutex.Unlock(); }
#else
	#define MAINMUTEX_LOCK
	#define MAINMUTEX_UNLOCK
	#define WAITMUTEX_LOCK
	#define WAITMUTEX_UNLOCK
#endif // RTP_SUPPORT_THREAD

namespace jrtplib
{

RTPTCPTransmitter::RTPTCPTransmitter(RTPMemoryManager *mgr) : RTPTransmitter(mgr)
{
	m_created = false;
	m_init = false;
}

RTPTCPTransmitter::~RTPTCPTransmitter()
{
	Destroy();
}

int RTPTCPTransmitter::Init(bool tsafe)
{
	if (m_init)
		return ERR_RTP_TCPTRANS_ALREADYINIT;
	
#ifdef RTP_SUPPORT_THREAD
	m_threadsafe = tsafe;
	if (m_threadsafe)
	{
		int status;
		
		status = m_mainMutex.Init();
		if (status < 0)
			return ERR_RTP_TCPTRANS_CANTINITMUTEX;
		status = m_waitMutex.Init();
		if (status < 0)
			return ERR_RTP_TCPTRANS_CANTINITMUTEX;
	}
#else
	if (tsafe)
		return ERR_RTP_NOTHREADSUPPORT;
#endif // RTP_SUPPORT_THREAD

	m_maxPackSize = RTPTCPTRANS_MAXPACKSIZE;
	m_init = true;
	return 0;
}

int RTPTCPTransmitter::Create(size_t maximumpacketsize, const RTPTransmissionParams *transparams)
{
	const RTPTCPTransmissionParams *params,defaultparams;
	int status;

	if (!m_init)
		return ERR_RTP_TCPTRANS_NOTINIT;
	
	MAINMUTEX_LOCK

	if (m_created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_ALREADYCREATED;
	}
	
	// Obtain transmission parameters
	
	if (transparams == 0)
		params = &defaultparams;
	else
	{
		if (transparams->GetTransmissionProtocol() != RTPTransmitter::TCPProto)
		{
			MAINMUTEX_UNLOCK
			return ERR_RTP_TCPTRANS_ILLEGALPARAMETERS;
		}
		params = static_cast<const RTPTCPTransmissionParams *>(transparams);
	}

	if (!params->GetCreatedAbortDescriptors())
	{
		if ((status = m_abortDesc.Init()) < 0)
		{
			MAINMUTEX_UNLOCK
			return status;
		}
		m_pAbortDesc = &m_abortDesc;
	}
	else
	{
		m_pAbortDesc = params->GetCreatedAbortDescriptors();
		if (!m_pAbortDesc->IsInitialized())
		{
			MAINMUTEX_UNLOCK
			return ERR_RTP_ABORTDESC_NOTINIT;
		}
	}

	m_waitingForData = false;
	m_created = true;
	MAINMUTEX_UNLOCK 
	return 0;
}

void RTPTCPTransmitter::Destroy()
{
	if (!m_init)
		return;

	MAINMUTEX_LOCK
	if (!m_created)
	{
		MAINMUTEX_UNLOCK;
		return;
	}

	ClearDestSockets();
	FlushPackets();
	m_created = false;
	
	if (m_waitingForData)
	{
		m_pAbortDesc->SendAbortSignal();
		m_abortDesc.Destroy(); // Doesn't do anything if not initialized
		MAINMUTEX_UNLOCK
		WAITMUTEX_LOCK // to make sure that the WaitForIncomingData function ended
		WAITMUTEX_UNLOCK
	}
	else
		m_abortDesc.Destroy(); // Doesn't do anything if not initialized

	MAINMUTEX_UNLOCK
}

RTPTransmissionInfo *RTPTCPTransmitter::GetTransmissionInfo()
{
	if (!m_init)
		return 0;

	MAINMUTEX_LOCK
	RTPTransmissionInfo *tinf = RTPNew(GetMemoryManager(),RTPMEM_TYPE_CLASS_RTPTRANSMISSIONINFO) RTPTCPTransmissionInfo();
	MAINMUTEX_UNLOCK
	return tinf;
}

void RTPTCPTransmitter::DeleteTransmissionInfo(RTPTransmissionInfo *i)
{
	if (!m_init)
		return;

	RTPDelete(i, GetMemoryManager());
}

int RTPTCPTransmitter::GetLocalHostName(uint8_t *buffer,size_t *bufferlength)
{
	if (!m_init)
		return ERR_RTP_TCPTRANS_NOTINIT;

	MAINMUTEX_LOCK
	if (!m_created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_NOTCREATED;
	}

	if (m_localHostname.size() == 0)
	{
		//
		// TODO
		// TODO
		// TODO
		// TODO
		//
		m_localHostname.resize(9);
		memcpy(&m_localHostname[0], "localhost", m_localHostname.size());
	}
	
	if ((*bufferlength) < m_localHostname.size())
	{
		*bufferlength = m_localHostname.size(); // tell the application the required size of the buffer
		MAINMUTEX_UNLOCK
		return ERR_RTP_TRANS_BUFFERLENGTHTOOSMALL;
	}

	memcpy(buffer,&m_localHostname[0], m_localHostname.size());
	*bufferlength = m_localHostname.size();
	
	MAINMUTEX_UNLOCK
	return 0;
}

bool RTPTCPTransmitter::ComesFromThisTransmitter(const RTPAddress *addr)
{
	if (!m_init)
		return false;

	if (addr == 0)
		return false;
	
	MAINMUTEX_LOCK
	
	if (!m_created)
		return false;

	if (addr->GetAddressType() != RTPAddress::TCPAddress)
		return false;

	const RTPTCPAddress *pAddr = static_cast<const RTPTCPAddress *>(addr);
	bool v = false;

	// TODO: for now, we're assuming that we can't just send to the same transmitter

	MAINMUTEX_UNLOCK
	return v;
}

int RTPTCPTransmitter::Poll()
{
	if (!m_init)
		return ERR_RTP_TCPTRANS_NOTINIT;

	MAINMUTEX_LOCK
	if (!m_created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_NOTCREATED;
	}

	std::map<SocketType, SocketData>::iterator it = m_destSockets.begin();
	std::map<SocketType, SocketData>::iterator end = m_destSockets.end();
	int status = 0;

	vector<SocketType> errSockets;

	while (it != end)
	{
		SocketType sock = it->first;
		status = PollSocket(sock, it->second);
		if (status < 0)
		{
			// Stop immediately on out of memory
			if (status == ERR_RTP_OUTOFMEM)
				break;
			else
				errSockets.push_back(sock);
		}
		++it;
	}
	MAINMUTEX_UNLOCK

	for (int i = 0 ; i < errSockets.size() ; i++)
		OnReceiveError(errSockets[i]);

	return status;
}

int RTPTCPTransmitter::WaitForIncomingData(const RTPTime &delay,bool *dataavailable)
{
	if (!m_init)
		return ERR_RTP_TCPTRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	
	if (!m_created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_NOTCREATED;
	}
	if (m_waitingForData)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_ALREADYWAITING;
	}
	
	m_tmpSocks.resize(m_destSockets.size()+1);
	m_tmpFlags.resize(m_tmpSocks.size());
	SocketType abortSocket = m_pAbortDesc->GetAbortSocket();

	std::map<SocketType, SocketData>::iterator it = m_destSockets.begin();
	std::map<SocketType, SocketData>::iterator end = m_destSockets.end();
	int idx = 0;

	while (it != end)
	{
		m_tmpSocks[idx] = it->first;
		m_tmpFlags[idx] = 0;
		++it;
		idx++;
	}
	m_tmpSocks[idx] = abortSocket;
	m_tmpFlags[idx] = 0;
	int idxAbort = idx;

	m_waitingForData = true;
	
	WAITMUTEX_LOCK
	MAINMUTEX_UNLOCK

	//cout << "Waiting for " << delay.GetDouble() << " seconds for data on " << m_tmpSocks.size() << " sockets" << endl;
	int status = RTPSelect(&m_tmpSocks[0], &m_tmpFlags[0], m_tmpSocks.size(), delay);
	if (status < 0)
	{
		MAINMUTEX_LOCK
		m_waitingForData = false;
		MAINMUTEX_UNLOCK
		WAITMUTEX_UNLOCK
		return status;
	}
	
	MAINMUTEX_LOCK
	m_waitingForData = false;
	if (!m_created) // destroy called
	{
		MAINMUTEX_UNLOCK;
		WAITMUTEX_UNLOCK
		return 0;
	}
		
	// if aborted, read from abort buffer
	if (m_tmpFlags[idxAbort])
		m_pAbortDesc->ReadSignallingByte();

	if (dataavailable != 0)
	{
		bool avail = false;

		for (int i = 0 ; i < m_tmpFlags.size() ; i++)
		{
			if (m_tmpFlags[i])
			{
				avail = true;
				//cout << "Data available!" << endl;
				break;
			}
		}

		if (avail)
			*dataavailable = true;
		else
			*dataavailable = false;
	}	
	
	MAINMUTEX_UNLOCK
	WAITMUTEX_UNLOCK
	return 0;
}

int RTPTCPTransmitter::AbortWait()
{
	if (!m_init)
		return ERR_RTP_TCPTRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	if (!m_created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_NOTCREATED;
	}
	if (!m_waitingForData)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_NOTWAITING;
	}

	m_pAbortDesc->SendAbortSignal();
	
	MAINMUTEX_UNLOCK
	return 0;
}

int RTPTCPTransmitter::SendRTPData(const void *data,size_t len)	
{
	return SendRTPRTCPData(data, len);
}

int RTPTCPTransmitter::SendRTCPData(const void *data,size_t len)
{
	return SendRTPRTCPData(data, len);
}

int RTPTCPTransmitter::AddDestination(const RTPAddress &addr)
{
	if (!m_init)
		return ERR_RTP_TCPTRANS_NOTINIT;
	
	MAINMUTEX_LOCK

	if (!m_created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_NOTCREATED;
	}

	if (addr.GetAddressType() != RTPAddress::TCPAddress)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_INVALIDADDRESSTYPE;
	}

	const RTPTCPAddress &a = static_cast<const RTPTCPAddress &>(addr);
	SocketType s = a.GetSocket();
	if (s == 0)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_NOSOCKETSPECIFIED;
	}

	int status = ValidateSocket(s);
	if (status != 0)
	{
		MAINMUTEX_UNLOCK
		return status;
	}
	
	std::map<SocketType, SocketData>::iterator it = m_destSockets.find(s);
	if (it != m_destSockets.end())
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_SOCKETALREADYINDESTINATIONS;
	}
	m_destSockets[s] = SocketData();

	// Because the sockets are also used for incoming data, we'll abort a wait
	// that may be in progress, otherwise it could take a few seconds until the
	// new socket is monitored for incoming data
	m_pAbortDesc->SendAbortSignal();

	MAINMUTEX_UNLOCK
	return 0;
}

int RTPTCPTransmitter::DeleteDestination(const RTPAddress &addr)
{
	if (!m_init)
		return ERR_RTP_TCPTRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	
	if (!m_created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_NOTCREATED;
	}
	
	if (addr.GetAddressType() != RTPAddress::TCPAddress)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_INVALIDADDRESSTYPE;
	}

	const RTPTCPAddress &a = static_cast<const RTPTCPAddress &>(addr);
	SocketType s = a.GetSocket();
	if (s == 0)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_NOSOCKETSPECIFIED;
	}

	std::map<SocketType, SocketData>::iterator it = m_destSockets.find(s);
	if (it == m_destSockets.end())
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_SOCKETNOTFOUNDINDESTINATIONS;
	}

	// Clean up possibly allocated memory
	uint8_t *pBuf = it->second.ExtractDataBuffer();
	if (pBuf)
		RTPDeleteByteArray(pBuf, GetMemoryManager());

	m_destSockets.erase(it);

	MAINMUTEX_UNLOCK
	return 0;
}

void RTPTCPTransmitter::ClearDestinations()
{
	if (!m_init)
		return;
	
	MAINMUTEX_LOCK
	if (m_created)
		ClearDestSockets();
	MAINMUTEX_UNLOCK
}

bool RTPTCPTransmitter::SupportsMulticasting()
{
	return false;
}

int RTPTCPTransmitter::JoinMulticastGroup(const RTPAddress &addr)
{
	return ERR_RTP_TCPTRANS_NOMULTICASTSUPPORT;
}

int RTPTCPTransmitter::LeaveMulticastGroup(const RTPAddress &addr)
{
	return ERR_RTP_TCPTRANS_NOMULTICASTSUPPORT;
}

void RTPTCPTransmitter::LeaveAllMulticastGroups()
{
}

int RTPTCPTransmitter::SetReceiveMode(RTPTransmitter::ReceiveMode m)
{
	if (m != RTPTransmitter::AcceptAll)
		return ERR_RTP_TCPTRANS_RECEIVEMODENOTSUPPORTED;
	return 0;
}

int RTPTCPTransmitter::AddToIgnoreList(const RTPAddress &addr)
{
	return ERR_RTP_TCPTRANS_RECEIVEMODENOTSUPPORTED;
}

int RTPTCPTransmitter::DeleteFromIgnoreList(const RTPAddress &addr)
{
	return ERR_RTP_TCPTRANS_RECEIVEMODENOTSUPPORTED;
}

void RTPTCPTransmitter::ClearIgnoreList()
{
}

int RTPTCPTransmitter::AddToAcceptList(const RTPAddress &addr)
{
	return ERR_RTP_TCPTRANS_RECEIVEMODENOTSUPPORTED;
}

int RTPTCPTransmitter::DeleteFromAcceptList(const RTPAddress &addr)
{
	return ERR_RTP_TCPTRANS_RECEIVEMODENOTSUPPORTED;
}

void RTPTCPTransmitter::ClearAcceptList()
{
}

int RTPTCPTransmitter::SetMaximumPacketSize(size_t s)	
{
	if (!m_init)
		return ERR_RTP_TCPTRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	if (!m_created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_NOTCREATED;
	}
	if (s > RTPTCPTRANS_MAXPACKSIZE)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_SPECIFIEDSIZETOOBIG;
	}
	m_maxPackSize = s;
	MAINMUTEX_UNLOCK
	return 0;
}

bool RTPTCPTransmitter::NewDataAvailable()
{
	if (!m_init)
		return false;
	
	MAINMUTEX_LOCK
	
	bool v;
		
	if (!m_created)
		v = false;
	else
	{
		if (m_rawpacketlist.empty())
			v = false;
		else
			v = true;
	}
	
	MAINMUTEX_UNLOCK
	return v;
}

RTPRawPacket *RTPTCPTransmitter::GetNextPacket()
{
	if (!m_init)
		return 0;
	
	MAINMUTEX_LOCK
	
	RTPRawPacket *p;
	
	if (!m_created)
	{
		MAINMUTEX_UNLOCK
		return 0;
	}
	if (m_rawpacketlist.empty())
	{
		MAINMUTEX_UNLOCK
		return 0;
	}

	p = *(m_rawpacketlist.begin());
	m_rawpacketlist.pop_front();

	MAINMUTEX_UNLOCK
	return p;
}

// Here the private functions start...

void RTPTCPTransmitter::FlushPackets()
{
	std::list<RTPRawPacket*>::const_iterator it;

	for (it = m_rawpacketlist.begin() ; it != m_rawpacketlist.end() ; ++it)
		RTPDelete(*it,GetMemoryManager());
	m_rawpacketlist.clear();
}

int RTPTCPTransmitter::PollSocket(SocketType sock, SocketData &sdata)
{
#ifdef RTP_SOCKETTYPE_WINSOCK
	unsigned long len;
#else 
	size_t len;
#endif // RTP_SOCKETTYPE_WINSOCK
	bool dataavailable;
	
	do
	{
		len = 0;
		RTPIOCTL(sock, FIONREAD, &len);

		if (len <= 0) 
			dataavailable = false;
		else
			dataavailable = true;
		
		if (dataavailable)
		{
			RTPTime curtime = RTPTime::CurrentTime();
			int relevantLen = RTPTCPTRANS_MAXPACKSIZE+2;
			
			if ((int)len < relevantLen)
				relevantLen = (int)len;

			bool complete = false;
			int status = sdata.ProcessAvailableBytes(sock, relevantLen, complete, GetMemoryManager());
			if (status < 0)
				return status;
			
			if (complete)
			{
				uint8_t *pBuf = sdata.ExtractDataBuffer();
				if (pBuf)
				{
					int dataLength = sdata.m_dataLength;
					sdata.Reset();

					RTPTCPAddress *pAddr = RTPNew(GetMemoryManager(),RTPMEM_TYPE_CLASS_RTPADDRESS) RTPTCPAddress(sock);
					if (pAddr == 0)
						return ERR_RTP_OUTOFMEM;

					bool isrtp = true;
					if (dataLength > sizeof(RTCPCommonHeader))
					{
						RTCPCommonHeader *rtcpheader = (RTCPCommonHeader *)pBuf;
						uint8_t packettype = rtcpheader->packettype;

						if (packettype >= 200 && packettype <= 204)
							isrtp = false;
					}
						
					RTPRawPacket *pPack = RTPNew(GetMemoryManager(),RTPMEM_TYPE_CLASS_RTPRAWPACKET) RTPRawPacket(pBuf, dataLength, pAddr, curtime, isrtp, GetMemoryManager());
					if (pPack == 0)
					{
						RTPDelete(pAddr,GetMemoryManager());
						RTPDeleteByteArray(pBuf,GetMemoryManager());
						return ERR_RTP_OUTOFMEM;
					}
					m_rawpacketlist.push_back(pPack);	
				}
			}
		}
	} while (dataavailable);

	return 0;
}

#ifdef RTPDEBUG
void RTPTCPTransmitter::Dump()
{
	/*
	if (!m_init)
		std::cout << "Not initialized" << std::endl;
	else
	{
		MAINMUTEX_LOCK
	
		if (!m_created)
			std::cout << "Not created" << std::endl;
		else
		{
			char str[16];
			uint32_t ip;
			std::list<uint32_t>::const_iterator it;
			
			std::cout << "RTP Port:                       " << m_rtpPort << std::endl;
			std::cout << "RTCP Port:                      " << m_rtcpPort << std::endl;
			std::cout << "RTP socket descriptor:          " << rtpsock << std::endl;
			std::cout << "RTCP socket descriptor:         " << rtcpsock << std::endl;
			ip = mcastifaceIP;
			RTP_SNPRINTF(str,16,"%d.%d.%d.%d",(int)((ip>>24)&0xFF),(int)((ip>>16)&0xFF),(int)((ip>>8)&0xFF),(int)(ip&0xFF));
			std::cout << "Multicast interface IP address: " << str << std::endl;
			std::cout << "Local IP addresses:" << std::endl;
			for (it = localIPs.begin() ; it != localIPs.end() ; it++)
			{
				ip = (*it);
				RTP_SNPRINTF(str,16,"%d.%d.%d.%d",(int)((ip>>24)&0xFF),(int)((ip>>16)&0xFF),(int)((ip>>8)&0xFF),(int)(ip&0xFF));
				std::cout << "    " << str << std::endl;
			}
			std::cout << "Multicast TTL:                  " << (int)multicastTTL << std::endl;
			std::cout << "Receive mode:                   ";
			switch (receivemode)
			{
			case RTPTransmitter::AcceptAll:
				std::cout << "Accept all";
				break;
			case RTPTransmitter::AcceptSome:
				std::cout << "Accept some";
				break;
			case RTPTransmitter::IgnoreSome:
				std::cout << "Ignore some";
			}
			std::cout << std::endl;
			if (receivemode != RTPTransmitter::AcceptAll)
			{
				acceptignoreinfo.GotoFirstElement();
				while(acceptignoreinfo.HasCurrentElement())
				{
					ip = acceptignoreinfo.GetCurrentKey();
					RTP_SNPRINTF(str,16,"%d.%d.%d.%d",(int)((ip>>24)&0xFF),(int)((ip>>16)&0xFF),(int)((ip>>8)&0xFF),(int)(ip&0xFF));
					PortInfo *pinfo = acceptignoreinfo.GetCurrentElement();
					std::cout << "    " << str << ": ";
					if (pinfo->all)
					{
						std::cout << "All ports";
						if (!pinfo->portlist.empty())
							std::cout << ", except ";
					}
					
					std::list<uint16_t>::const_iterator it;
					
					for (it = pinfo->portlist.begin() ; it != pinfo->portlist.end() ; )
					{
						std::cout << (*it);
						it++;
						if (it != pinfo->portlist.end())
							std::cout << ", ";
					}
					std::cout << std::endl;
				}
			}
			
			std::cout << "Local host name:                ";
			if (localhostname == 0)
				std::cout << "Not set";
			else
				std::cout << localhostname;
			std::cout << std::endl;

			std::cout << "List of destinations:           ";
			destinations.GotoFirstElement();
			if (destinations.HasCurrentElement())
			{
				std::cout << std::endl;
				do
				{
					std::cout << "    " << destinations.GetCurrentElement().GetDestinationString() << std::endl;
					destinations.GotoNextElement();
				} while (destinations.HasCurrentElement());
			}
			else
				std::cout << "Empty" << std::endl;
		
			std::cout << "Supports multicasting:          " << ((supportsmulticasting)?"Yes":"No") << std::endl;
#ifdef RTP_SUPPORT_IPV4MULTICAST
			std::cout << "List of multicast groups:       ";
			multicastgroups.GotoFirstElement();
			if (multicastgroups.HasCurrentElement())
			{
				std::cout << std::endl;
				do
				{
					ip = multicastgroups.GetCurrentElement();
					RTP_SNPRINTF(str,16,"%d.%d.%d.%d",(int)((ip>>24)&0xFF),(int)((ip>>16)&0xFF),(int)((ip>>8)&0xFF),(int)(ip&0xFF));
					std::cout << "    " << str << std::endl;
					multicastgroups.GotoNextElement();
				} while (multicastgroups.HasCurrentElement());
			}
			else
				std::cout << "Empty" << std::endl;
#endif // RTP_SUPPORT_IPV4MULTICAST
			
			std::cout << "Number of raw packets in queue: " << rawpacketlist.size() << std::endl;
			std::cout << "Maximum allowed packet size:    " << maxpacksize << std::endl;
		}
		
		MAINMUTEX_UNLOCK
	}
*/
}
#endif // RTPDEBUG

int RTPTCPTransmitter::SendRTPRTCPData(const void *data,size_t len)	
{
	if (!m_init)
		return ERR_RTP_TCPTRANS_NOTINIT;

	MAINMUTEX_LOCK
	
	if (!m_created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_NOTCREATED;
	}
	if (len > RTPTCPTRANS_MAXPACKSIZE)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_TCPTRANS_SPECIFIEDSIZETOOBIG;
	}
	
	std::map<SocketType, SocketData>::iterator it = m_destSockets.begin();
	std::map<SocketType, SocketData>::iterator end = m_destSockets.end();
	int status = 0;

	vector<SocketType> errSockets;

	while (it != end)
	{
		uint8_t lengthBytes[2] = { (len >> 8)&0xff, len&0xff };
		SocketType sock = it->first;

		if (send(sock,(const char *)lengthBytes,2,0) < 0 ||
			send(sock,(const char *)data,len,0) < 0)
			errSockets.push_back(sock);
		++it;
	}
	
	MAINMUTEX_UNLOCK

	if (errSockets.size() != 0)
	{
		status = ERR_RTP_TCPTRANS_ERRORINSEND;

		for (int i = 0 ; i < errSockets.size() ; i++)
			OnSendError(errSockets[i]);
	}

	return status;
}

int RTPTCPTransmitter::ValidateSocket(SocketType s)
{
	// TODO: should we even do a check (for a TCP socket)? 
	return 0;
}

void RTPTCPTransmitter::ClearDestSockets()
{
	std::map<SocketType, SocketData>::iterator it = m_destSockets.begin();
	std::map<SocketType, SocketData>::iterator end = m_destSockets.end();

	while (it != end)
	{
		uint8_t *pBuf = it->second.ExtractDataBuffer();
		if (pBuf)
			RTPDeleteByteArray(pBuf, GetMemoryManager());

		++it;
	}
	m_destSockets.clear();
}

RTPTCPTransmitter::SocketData::SocketData()
{
	Reset();
}

void RTPTCPTransmitter::SocketData::Reset()
{
	m_lengthBufferOffset = 0;
	m_dataLength = 0; 
	m_dataBufferOffset = 0;
	m_pDataBuffer = 0;
}

RTPTCPTransmitter::SocketData::~SocketData()
{
	assert(m_pDataBuffer == 0); // Should be deleted externally to avoid storing a memory manager in the class
}

int RTPTCPTransmitter::SocketData::ProcessAvailableBytes(SocketType sock, int availLen, bool &complete, RTPMemoryManager *pMgr)
{
	const int numLengthBuffer = 2;
	if (m_lengthBufferOffset < numLengthBuffer) // first we need to get the length
	{
		assert(m_pDataBuffer == 0);
		int num = numLengthBuffer-m_lengthBufferOffset;
		if (num > availLen)
			num = availLen;

		int r = 0;
		if (num > 0)
		{
			r = (int)recv(sock, (char *)(m_lengthBuffer+m_lengthBufferOffset), num, 0);
			if (r < 0)
				return ERR_RTP_TCPTRANS_ERRORINRECV;
		}

		m_lengthBufferOffset += r;
		availLen -= r;

		assert(m_lengthBufferOffset <= numLengthBuffer);
		if (m_lengthBufferOffset == numLengthBuffer) // we can constuct a length
		{
			int l = 0;
			for (int i = numLengthBuffer-1, shift = 0 ; i >= 0 ; i--, shift += 8)
				l |= ((int)m_lengthBuffer[i]) << shift;

			m_dataLength = l;
			m_dataBufferOffset = 0;

			//cout << "Expecting " << m_dataLength << " bytes" << endl;

			// avoid allocation of length 0
			if (l == 0)
				l = 1;

			// We don't yet know if it's an RTP or RTCP packet, so we'll stick to RTP
			m_pDataBuffer = RTPNew(pMgr, RTPMEM_TYPE_BUFFER_RECEIVEDRTPPACKET) uint8_t[l];
			if (m_pDataBuffer == 0)
				return ERR_RTP_OUTOFMEM;
		}
	}

	if (m_lengthBufferOffset == numLengthBuffer && m_pDataBuffer) // the last one is to make sure we didn't run out of memory
	{
		if (m_dataBufferOffset < m_dataLength)
		{
			int num = m_dataLength-m_dataBufferOffset;
			if (num > availLen)
				num = availLen;

			int r = 0;
			if (num > 0)
			{
				r = (int)recv(sock, (char *)(m_pDataBuffer+m_dataBufferOffset), num, 0);
				if (r < 0)
					return ERR_RTP_TCPTRANS_ERRORINRECV;
			}

			m_dataBufferOffset += r;
			availLen -= r;
		}

		if (m_dataBufferOffset == m_dataLength)
			complete = true;
	}
	return 0;
}

} // end namespace


