/*

  This file is a part of JRTPLIB
  Copyright (c) 1999-2011 Jori Liesenborgs

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

#include "rtpexternaltransmitter.h"
#include "rtprawpacket.h"
#include "rtptimeutilities.h"
#include "rtpdefines.h"
#include "rtperrors.h"
#include <stdio.h>
#include <string.h>

#ifdef RTPDEBUG
	#include <iostream>
#endif // RTPDEBUG

#include "rtpdebug.h"

#include <iostream>

#if (defined(WIN32) || defined(_WIN32_WCE))
	#define RTPSOCKERR								INVALID_SOCKET
	#define RTPCLOSE(x)								closesocket(x)
	#define RTPSOCKLENTYPE								int
	#define RTPIOCTL								ioctlsocket
#else // not Win32
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <sys/ioctl.h>
	#include <net/if.h>
	#include <string.h>
	#include <netdb.h>
	#include <unistd.h>

	#ifdef RTP_HAVE_SYS_FILIO
		#include <sys/filio.h>
	#endif // RTP_HAVE_SYS_FILIO
	#ifdef RTP_HAVE_SYS_SOCKIO
		#include <sys/sockio.h>
	#endif // RTP_HAVE_SYS_SOCKIO
	#ifdef RTP_SUPPORT_IFADDRS
		#include <ifaddrs.h>
	#endif // RTP_SUPPORT_IFADDRS

	#define RTPSOCKERR								-1
	#define RTPCLOSE(x)								close(x)

	#ifdef RTP_SOCKLENTYPE_UINT
		#define RTPSOCKLENTYPE							unsigned int
	#else
		#define RTPSOCKLENTYPE							int
	#endif // RTP_SOCKLENTYPE_UINT

	#define RTPIOCTL								ioctl
#endif // WIN32

#ifdef RTP_SUPPORT_THREAD
	#define MAINMUTEX_LOCK 		{ if (threadsafe) mainmutex.Lock(); }
	#define MAINMUTEX_UNLOCK	{ if (threadsafe) mainmutex.Unlock(); }
	#define WAITMUTEX_LOCK		{ if (threadsafe) waitmutex.Lock(); }
	#define WAITMUTEX_UNLOCK	{ if (threadsafe) waitmutex.Unlock(); }
#else
	#define MAINMUTEX_LOCK
	#define MAINMUTEX_UNLOCK
	#define WAITMUTEX_LOCK
	#define WAITMUTEX_UNLOCK
#endif // RTP_SUPPORT_THREAD

namespace jrtplib
{

RTPExternalTransmitter::RTPExternalTransmitter(RTPMemoryManager *mgr) : RTPTransmitter(mgr), packetinjector((RTPExternalTransmitter *)this)
{
	created = false;
	init = false;
#if (defined(WIN32) || defined(_WIN32_WCE))
	timeinit.Dummy();
#endif // WIN32 || _WIN32_WCE
}

RTPExternalTransmitter::~RTPExternalTransmitter()
{
	Destroy();
}

int RTPExternalTransmitter::Init(bool tsafe)
{
	if (init)
		return ERR_RTP_EXTERNALTRANS_ALREADYINIT;
	
#ifdef RTP_SUPPORT_THREAD
	threadsafe = tsafe;
	if (threadsafe)
	{
		int status;
		
		status = mainmutex.Init();
		if (status < 0)
			return ERR_RTP_EXTERNALTRANS_CANTINITMUTEX;
		status = waitmutex.Init();
		if (status < 0)
			return ERR_RTP_EXTERNALTRANS_CANTINITMUTEX;
	}
#else
	if (tsafe)
		return ERR_RTP_NOTHREADSUPPORT;
#endif // RTP_SUPPORT_THREAD

	init = true;
	return 0;
}

int RTPExternalTransmitter::Create(size_t maximumpacketsize,const RTPTransmissionParams *transparams)
{
	const RTPExternalTransmissionParams *params;
	int status;

	if (!init)
		return ERR_RTP_EXTERNALTRANS_NOTINIT;
	
	MAINMUTEX_LOCK

	if (created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_EXTERNALTRANS_ALREADYCREATED;
	}
	
	// Obtain transmission parameters
	
	if (transparams == 0)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_EXTERNALTRANS_ILLEGALPARAMETERS;
	}
	if (transparams->GetTransmissionProtocol() != RTPTransmitter::ExternalProto)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_EXTERNALTRANS_ILLEGALPARAMETERS;
	}
		
	params = (const RTPExternalTransmissionParams *)transparams;

	if ((status = CreateAbortDescriptors()) < 0)
	{
		MAINMUTEX_UNLOCK
		return status;
	}
	
	maxpacksize = maximumpacketsize;
	sender = params->GetSender();
	headersize = params->GetAdditionalHeaderSize();

	localhostname = 0;
	localhostnamelength = 0;

	waitingfordata = false;
	created = true;
	MAINMUTEX_UNLOCK
	return 0;
}

void RTPExternalTransmitter::Destroy()
{
	if (!init)
		return;

	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK;
		return;
	}

	if (localhostname)
	{
		RTPDeleteByteArray(localhostname,GetMemoryManager());
		localhostname = 0;
		localhostnamelength = 0;
	}
	
	FlushPackets();
	created = false;
	
	if (waitingfordata)
	{
		AbortWaitInternal();
		DestroyAbortDescriptors();
		MAINMUTEX_UNLOCK
		WAITMUTEX_LOCK // to make sure that the WaitForIncomingData function ended
		WAITMUTEX_UNLOCK
	}
	else
		DestroyAbortDescriptors();

	MAINMUTEX_UNLOCK
}

RTPTransmissionInfo *RTPExternalTransmitter::GetTransmissionInfo()
{
	if (!init)
		return 0;

	MAINMUTEX_LOCK
	RTPTransmissionInfo *tinf = RTPNew(GetMemoryManager(),RTPMEM_TYPE_CLASS_RTPTRANSMISSIONINFO) RTPExternalTransmissionInfo(&packetinjector);
	MAINMUTEX_UNLOCK
	return tinf;
}

void RTPExternalTransmitter::DeleteTransmissionInfo(RTPTransmissionInfo *i)
{
	if (!init)
		return;

	RTPDelete(i, GetMemoryManager());
}

int RTPExternalTransmitter::GetLocalHostName(uint8_t *buffer,size_t *bufferlength)
{
	if (!init)
		return ERR_RTP_EXTERNALTRANS_NOTINIT;

	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_EXTERNALTRANS_NOTCREATED;
	}

	if (localhostname == 0)
	{
		// We'll just use 'gethostname' for simplicity

		char name[1024];

		if (gethostname(name,1023) != 0)
			strcpy(name, "localhost"); // failsafe
		else
			name[1023] = 0; // ensure null-termination

		localhostnamelength = strlen(name);
		localhostname = RTPNew(GetMemoryManager(),RTPMEM_TYPE_OTHER) uint8_t [localhostnamelength+1];

		memcpy(localhostname, name, localhostnamelength);
		localhostname[localhostnamelength] = 0;
	}
	
	if ((*bufferlength) < localhostnamelength)
	{
		*bufferlength = localhostnamelength; // tell the application the required size of the buffer
		MAINMUTEX_UNLOCK
		return ERR_RTP_TRANS_BUFFERLENGTHTOOSMALL;
	}

	memcpy(buffer,localhostname,localhostnamelength);
	*bufferlength = localhostnamelength;
	
	MAINMUTEX_UNLOCK
	return 0;
}

bool RTPExternalTransmitter::ComesFromThisTransmitter(const RTPAddress *addr)
{
	MAINMUTEX_LOCK
	bool value = false;
	if (sender)
		value = sender->ComesFromThisSender(addr);
	MAINMUTEX_UNLOCK
	return value;
}

int RTPExternalTransmitter::Poll()
{
	return 0;
}

int RTPExternalTransmitter::WaitForIncomingData(const RTPTime &delay,bool *dataavailable)
{
	if (!init)
		return ERR_RTP_EXTERNALTRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	
	fd_set fdset;
	struct timeval tv;
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_EXTERNALTRANS_NOTCREATED;
	}
	if (waitingfordata)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_EXTERNALTRANS_ALREADYWAITING;
	}
	
	FD_ZERO(&fdset);
	FD_SET(abortdesc[0],&fdset);
	tv.tv_sec = delay.GetSeconds();
	tv.tv_usec = delay.GetMicroSeconds();
	
	waitingfordata = true;

	if (!rawpacketlist.empty())
	{
		if (dataavailable != 0)
			*dataavailable = true;
		waitingfordata = false;
		MAINMUTEX_UNLOCK
		return 0;
	}
	
	WAITMUTEX_LOCK
	MAINMUTEX_UNLOCK

	if (select(FD_SETSIZE,&fdset,0,0,&tv) < 0)
	{
		MAINMUTEX_LOCK
		waitingfordata = false;
		MAINMUTEX_UNLOCK
		WAITMUTEX_UNLOCK
		return ERR_RTP_EXTERNALTRANS_ERRORINSELECT;
	}
	
	MAINMUTEX_LOCK
	waitingfordata = false;
	if (!created) // destroy called
	{
		MAINMUTEX_UNLOCK;
		WAITMUTEX_UNLOCK
		return 0;
	}
		
	// if aborted, read from abort buffer
	if (FD_ISSET(abortdesc[0],&fdset))
	{
#define BUFLEN 256
#if (defined(WIN32) || defined(_WIN32_WCE))
		char buf[BUFLEN];
		unsigned long len, len2;
#else 
		size_t len, len2;
		unsigned char buf[BUFLEN];
#endif // WIN32

		len = 0;
		RTPIOCTL(abortdesc[0],FIONREAD,&len);

		while (len > 0)
		{
			len2 = len;
			if (len2 > BUFLEN)
				len2 = BUFLEN;

#if (defined(WIN32) || defined(_WIN32_WCE))
			recv(abortdesc[0],buf,len2,0);
#else 
			if (read(abortdesc[0],buf,len2)) 	{ } // To get rid of __wur related compiler warnings
#endif // WIN32
			len -= len2;
		}
	}

	if (dataavailable != 0)
	{
		if (rawpacketlist.empty())
			*dataavailable = false;
		else
			*dataavailable = true;
	}	
	
	MAINMUTEX_UNLOCK
	WAITMUTEX_UNLOCK
	return 0;
}

int RTPExternalTransmitter::AbortWait()
{
	if (!init)
		return ERR_RTP_EXTERNALTRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_EXTERNALTRANS_NOTCREATED;
	}
	if (!waitingfordata)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_EXTERNALTRANS_NOTWAITING;
	}

	AbortWaitInternal();
	
	MAINMUTEX_UNLOCK
	return 0;
}

int RTPExternalTransmitter::SendRTPData(const void *data,size_t len)	
{
	if (!init)
		return ERR_RTP_EXTERNALTRANS_NOTINIT;

	MAINMUTEX_LOCK
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_EXTERNALTRANS_NOTCREATED;
	}
	if (len > maxpacksize)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_EXTERNALTRANS_SPECIFIEDSIZETOOBIG;
	}
	
	if (!sender)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_EXTERNALTRANS_NOSENDER;
	}

	MAINMUTEX_UNLOCK

	if (!sender->SendRTP(data, len))
		return ERR_RTP_EXTERNALTRANS_SENDERROR;

	return 0;
}

int RTPExternalTransmitter::SendRTCPData(const void *data,size_t len)
{
	if (!init)
		return ERR_RTP_EXTERNALTRANS_NOTINIT;

	MAINMUTEX_LOCK
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_EXTERNALTRANS_NOTCREATED;
	}
	if (len > maxpacksize)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_EXTERNALTRANS_SPECIFIEDSIZETOOBIG;
	}
	
	if (!sender)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_EXTERNALTRANS_NOSENDER;
	}
	MAINMUTEX_UNLOCK

	if (!sender->SendRTCP(data, len))
		return ERR_RTP_EXTERNALTRANS_SENDERROR;
		
	return 0;
}

int RTPExternalTransmitter::AddDestination(const RTPAddress &addr)
{
	return ERR_RTP_EXTERNALTRANS_NODESTINATIONSSUPPORTED;
}

int RTPExternalTransmitter::DeleteDestination(const RTPAddress &addr)
{
	return ERR_RTP_EXTERNALTRANS_NODESTINATIONSSUPPORTED;
}

void RTPExternalTransmitter::ClearDestinations()
{
}

bool RTPExternalTransmitter::SupportsMulticasting()
{
	return false;
}

int RTPExternalTransmitter::JoinMulticastGroup(const RTPAddress &addr)
{
	return ERR_RTP_EXTERNALTRANS_NOMULTICASTSUPPORT;
}

int RTPExternalTransmitter::LeaveMulticastGroup(const RTPAddress &addr)
{
	return ERR_RTP_EXTERNALTRANS_NOMULTICASTSUPPORT;
}

void RTPExternalTransmitter::LeaveAllMulticastGroups()
{
}

int RTPExternalTransmitter::SetReceiveMode(RTPTransmitter::ReceiveMode m)
{
	if (!init)
		return ERR_RTP_EXTERNALTRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_EXTERNALTRANS_NOTCREATED;
	}
	if (m != RTPTransmitter::AcceptAll)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_EXTERNALTRANS_BADRECEIVEMODE;
	}
	MAINMUTEX_UNLOCK
	return 0;
}

int RTPExternalTransmitter::AddToIgnoreList(const RTPAddress &addr)
{
	return ERR_RTP_EXTERNALTRANS_NOIGNORELIST;
}

int RTPExternalTransmitter::DeleteFromIgnoreList(const RTPAddress &addr)
{
	return ERR_RTP_EXTERNALTRANS_NOIGNORELIST;
}

void RTPExternalTransmitter::ClearIgnoreList()
{
}

int RTPExternalTransmitter::AddToAcceptList(const RTPAddress &addr)
{
	return ERR_RTP_EXTERNALTRANS_NOACCEPTLIST;
}

int RTPExternalTransmitter::DeleteFromAcceptList(const RTPAddress &addr)
{
	return ERR_RTP_EXTERNALTRANS_NOACCEPTLIST;
}

void RTPExternalTransmitter::ClearAcceptList()
{
}

int RTPExternalTransmitter::SetMaximumPacketSize(size_t s)	
{
	if (!init)
		return ERR_RTP_EXTERNALTRANS_NOTINIT;
	
	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return ERR_RTP_EXTERNALTRANS_NOTCREATED;
	}
	maxpacksize = s;
	MAINMUTEX_UNLOCK
	return 0;
}

bool RTPExternalTransmitter::NewDataAvailable()
{
	if (!init)
		return false;
	
	MAINMUTEX_LOCK
	
	bool v;
		
	if (!created)
		v = false;
	else
	{
		if (rawpacketlist.empty())
			v = false;
		else
			v = true;
	}
	
	MAINMUTEX_UNLOCK
	return v;
}

RTPRawPacket *RTPExternalTransmitter::GetNextPacket()
{
	if (!init)
		return 0;
	
	MAINMUTEX_LOCK
	
	RTPRawPacket *p;
	
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return 0;
	}
	if (rawpacketlist.empty())
	{
		MAINMUTEX_UNLOCK
		return 0;
	}

	p = *(rawpacketlist.begin());
	rawpacketlist.pop_front();

	MAINMUTEX_UNLOCK
	return p;
}

// Here the private functions start...

void RTPExternalTransmitter::FlushPackets()
{
	std::list<RTPRawPacket*>::const_iterator it;

	for (it = rawpacketlist.begin() ; it != rawpacketlist.end() ; ++it)
		RTPDelete(*it,GetMemoryManager());
	rawpacketlist.clear();
}

#if (defined(WIN32) || defined(_WIN32_WCE))

int RTPExternalTransmitter::CreateAbortDescriptors()
{
	SOCKET listensock;
	int size;
	struct sockaddr_in addr;

	listensock = socket(PF_INET,SOCK_STREAM,0);
	if (listensock == RTPSOCKERR)
		return ERR_RTP_EXTERNALTRANS_CANTCREATEABORTDESCRIPTORS;
	
	memset(&addr,0,sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	if (bind(listensock,(struct sockaddr *)&addr,sizeof(struct sockaddr_in)) != 0)
	{
		RTPCLOSE(listensock);
		return ERR_RTP_EXTERNALTRANS_CANTCREATEABORTDESCRIPTORS;
	}

	memset(&addr,0,sizeof(struct sockaddr_in));
	size = sizeof(struct sockaddr_in);
	if (getsockname(listensock,(struct sockaddr*)&addr,&size) != 0)
	{
		RTPCLOSE(listensock);
		return ERR_RTP_EXTERNALTRANS_CANTCREATEABORTDESCRIPTORS;
	}

	unsigned short connectport = ntohs(addr.sin_port);

	abortdesc[0] = socket(PF_INET,SOCK_STREAM,0);
	if (abortdesc[0] == RTPSOCKERR)
	{
		RTPCLOSE(listensock);
		return ERR_RTP_EXTERNALTRANS_CANTCREATEABORTDESCRIPTORS;
	}

	memset(&addr,0,sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	if (bind(abortdesc[0],(struct sockaddr *)&addr,sizeof(struct sockaddr_in)) != 0)
	{
		RTPCLOSE(listensock);
		RTPCLOSE(abortdesc[0]);
		return ERR_RTP_EXTERNALTRANS_CANTCREATEABORTDESCRIPTORS;
	}

	if (listen(listensock,1) != 0)
	{
		RTPCLOSE(listensock);
		RTPCLOSE(abortdesc[0]);
		return ERR_RTP_EXTERNALTRANS_CANTCREATEABORTDESCRIPTORS;
	}

	memset(&addr,0,sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(connectport);
	
	if (connect(abortdesc[0],(struct sockaddr *)&addr,sizeof(struct sockaddr_in)) != 0)
	{
		RTPCLOSE(listensock);
		RTPCLOSE(abortdesc[0]);
		return ERR_RTP_EXTERNALTRANS_CANTCREATEABORTDESCRIPTORS;
	}

	memset(&addr,0,sizeof(struct sockaddr_in));
	size = sizeof(struct sockaddr_in);
	abortdesc[1] = accept(listensock,(struct sockaddr *)&addr,&size);
	if (abortdesc[1] == RTPSOCKERR)
	{
		RTPCLOSE(listensock);
		RTPCLOSE(abortdesc[0]);
		return ERR_RTP_EXTERNALTRANS_CANTCREATEABORTDESCRIPTORS;
	}

	// okay, got the connection, close the listening socket

	RTPCLOSE(listensock);
	return 0;
}

void RTPExternalTransmitter::DestroyAbortDescriptors()
{
	RTPCLOSE(abortdesc[0]);
	RTPCLOSE(abortdesc[1]);
}

#else // in a non winsock environment we can use pipes

int RTPExternalTransmitter::CreateAbortDescriptors()
{
	if (pipe(abortdesc) < 0)
		return ERR_RTP_EXTERNALTRANS_CANTCREATEPIPE;
	return 0;
}

void RTPExternalTransmitter::DestroyAbortDescriptors()
{
	close(abortdesc[0]);
	close(abortdesc[1]);
}

#endif // WIN32

void RTPExternalTransmitter::AbortWaitInternal()
{
#if (defined(WIN32) || defined(_WIN32_WCE))
	send(abortdesc[1],"*",1,0);
#else
	if (write(abortdesc[1],"*",1))
	{
		// To get rid of __wur related compiler warnings
	}
#endif // WIN32
}

void RTPExternalTransmitter::InjectRTP(const void *data, size_t len, const RTPAddress &a)
{
	if (!init)
		return;

	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return;
	}

	RTPAddress *addr = a.CreateCopy(GetMemoryManager());
	if (addr == 0)
		return;

	uint8_t *datacopy;

	datacopy = RTPNew(GetMemoryManager(),RTPMEM_TYPE_BUFFER_RECEIVEDRTPPACKET) uint8_t[len];
	if (datacopy == 0)
	{
		RTPDelete(addr,GetMemoryManager());
		return;
	}
	memcpy(datacopy, data, len);

	RTPTime curtime = RTPTime::CurrentTime();
	RTPRawPacket *pack;

	pack = RTPNew(GetMemoryManager(),RTPMEM_TYPE_CLASS_RTPRAWPACKET) RTPRawPacket(datacopy,len,addr,curtime,true,GetMemoryManager());
	if (pack == 0)
	{
		RTPDelete(addr,GetMemoryManager());
		RTPDeleteByteArray(localhostname,GetMemoryManager());
		return;
	}
	rawpacketlist.push_back(pack);
	AbortWaitInternal();

	MAINMUTEX_UNLOCK
}

void RTPExternalTransmitter::InjectRTCP(const void *data, size_t len, const RTPAddress &a)
{
	if (!init)
		return;

	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return;
	}

	RTPAddress *addr = a.CreateCopy(GetMemoryManager());
	if (addr == 0)
		return;

	uint8_t *datacopy;

	datacopy = RTPNew(GetMemoryManager(),RTPMEM_TYPE_BUFFER_RECEIVEDRTCPPACKET) uint8_t[len];
	if (datacopy == 0)
	{
		RTPDelete(addr,GetMemoryManager());
		return;
	}
	memcpy(datacopy, data, len);

	RTPTime curtime = RTPTime::CurrentTime();
	RTPRawPacket *pack;

	pack = RTPNew(GetMemoryManager(),RTPMEM_TYPE_CLASS_RTPRAWPACKET) RTPRawPacket(datacopy,len,addr,curtime,false,GetMemoryManager());
	if (pack == 0)
	{
		RTPDelete(addr,GetMemoryManager());
		RTPDeleteByteArray(localhostname,GetMemoryManager());
		return;
	}
	rawpacketlist.push_back(pack);
	AbortWaitInternal();

	MAINMUTEX_UNLOCK
}

void RTPExternalTransmitter::InjectRTPorRTCP(const void *data, size_t len, const RTPAddress &a)
{
	if (!init)
		return;

	MAINMUTEX_LOCK
	if (!created)
	{
		MAINMUTEX_UNLOCK
		return;
	}

	RTPAddress *addr = a.CreateCopy(GetMemoryManager());
	if (addr == 0)
		return;

	uint8_t *datacopy;
	bool rtp = true;

	if (len >= 2)
	{
		const uint8_t *pData = (const uint8_t *)data;
		if (pData[1] >= 200 && pData[1] <= 204)
			rtp = false;
	}

	datacopy = RTPNew(GetMemoryManager(),(rtp)?RTPMEM_TYPE_BUFFER_RECEIVEDRTPPACKET:RTPMEM_TYPE_BUFFER_RECEIVEDRTCPPACKET) uint8_t[len];
	if (datacopy == 0)
	{
		RTPDelete(addr,GetMemoryManager());
		return;
	}
	memcpy(datacopy, data, len);

	RTPTime curtime = RTPTime::CurrentTime();
	RTPRawPacket *pack;

	pack = RTPNew(GetMemoryManager(),RTPMEM_TYPE_CLASS_RTPRAWPACKET) RTPRawPacket(datacopy,len,addr,curtime,rtp,GetMemoryManager());
	if (pack == 0)
	{
		RTPDelete(addr,GetMemoryManager());
		RTPDeleteByteArray(localhostname,GetMemoryManager());
		return;
	}
	rawpacketlist.push_back(pack);
	AbortWaitInternal();

	MAINMUTEX_UNLOCK

}

#ifdef RTPDEBUG
void RTPExternalTransmitter::Dump()
{
	if (!init)
		std::cout << "Not initialized" << std::endl;
	else
	{
		MAINMUTEX_LOCK
	
		if (!created)
			std::cout << "Not created" << std::endl;
		else
		{
			std::cout << "Number of raw packets in queue: " << rawpacketlist.size() << std::endl;
			std::cout << "Maximum allowed packet size:    " << maxpacksize << std::endl;
		}
		
		MAINMUTEX_UNLOCK
	}
}
#endif // RTPDEBUG

} // end namespace

