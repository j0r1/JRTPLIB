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

#include "rtpsession.h"
#include "rtperrors.h"
#include "rtppollthread.h"
#include "rtpudpv4transmitter.h"
#include "rtpudpv6transmitter.h"
#include "rtptcptransmitter.h"
#include "rtpexternaltransmitter.h"
#include "rtpsessionparams.h"
#include "rtpdefines.h"
#include "rtprawpacket.h"
#include "rtppacket.h"
#include "rtptimeutilities.h"
#include "rtpmemorymanager.h"
#include "rtprandomrand48.h"
#include "rtprandomrands.h"
#include "rtprandomurandom.h"
#ifdef RTP_SUPPORT_SENDAPP
	#include "rtcpcompoundpacket.h"
#endif // RTP_SUPPORT_SENDAPP
#include "rtpinternalutils.h"
#ifndef WIN32
	#include <unistd.h>
	#include <stdlib.h>
#else
	#include <winbase.h>
#endif // WIN32

#ifdef RTPDEBUG
	#include <iostream>
#endif // RTPDEBUG

#include "rtpdebug.h"

#ifdef RTP_SUPPORT_THREAD
	#define SOURCES_LOCK					{ if (needthreadsafety) sourcesmutex.Lock(); }
	#define SOURCES_UNLOCK					{ if (needthreadsafety) sourcesmutex.Unlock(); }
	#define BUILDER_LOCK					{ if (needthreadsafety) buildermutex.Lock(); }
	#define BUILDER_UNLOCK					{ if (needthreadsafety) buildermutex.Unlock(); }
	#define SCHED_LOCK						{ if (needthreadsafety) schedmutex.Lock(); }
	#define SCHED_UNLOCK					{ if (needthreadsafety) schedmutex.Unlock(); }
	#define PACKSENT_LOCK					{ if (needthreadsafety) packsentmutex.Lock(); }
	#define PACKSENT_UNLOCK					{ if (needthreadsafety) packsentmutex.Unlock(); } 
#else
	#define SOURCES_LOCK
	#define SOURCES_UNLOCK
	#define BUILDER_LOCK
	#define BUILDER_UNLOCK
	#define SCHED_LOCK
	#define SCHED_UNLOCK
	#define PACKSENT_LOCK
	#define PACKSENT_UNLOCK
#endif // RTP_SUPPORT_THREAD

namespace jrtplib
{

RTPSession::RTPSession(RTPRandom *r,RTPMemoryManager *mgr) 
	: RTPMemoryObject(mgr),rtprnd(GetRandomNumberGenerator(r)),sources(*this,mgr),packetbuilder(*rtprnd,mgr),rtcpsched(sources,*rtprnd),
	  rtcpbuilder(sources,packetbuilder,mgr),collisionlist(mgr)
{
	// We're not going to set these flags in Create, so that the constructor of a derived class
	// can already change them
	m_changeIncomingData = false;
	m_changeOutgoingData = false;

	created = false;
	timeinit.Dummy();

	//std::cout << (void *)(rtprnd) << std::endl;
}

RTPSession::~RTPSession()
{
	Destroy();

	if (deletertprnd)
		delete rtprnd;
}

int RTPSession::Create(const RTPSessionParams &sessparams,const RTPTransmissionParams *transparams /* = 0 */,
		       RTPTransmitter::TransmissionProtocol protocol)
{
	int status;
	
	if (created)
		return ERR_RTP_SESSION_ALREADYCREATED;

	usingpollthread = sessparams.IsUsingPollThread();
	needthreadsafety = sessparams.NeedThreadSafety();
	if (usingpollthread && !needthreadsafety)
		return ERR_RTP_SESSION_THREADSAFETYCONFLICT;

	useSR_BYEifpossible = sessparams.GetSenderReportForBYE();
	sentpackets = false;
	
	// Check max packet size
	
	if ((maxpacksize = sessparams.GetMaximumPacketSize()) < RTP_MINPACKETSIZE)
		return ERR_RTP_SESSION_MAXPACKETSIZETOOSMALL;
		
	// Initialize the transmission component
	
	rtptrans = 0;
	switch(protocol)
	{
	case RTPTransmitter::IPv4UDPProto:
		rtptrans = RTPNew(GetMemoryManager(),RTPMEM_TYPE_CLASS_RTPTRANSMITTER) RTPUDPv4Transmitter(GetMemoryManager());
		break;
#ifdef RTP_SUPPORT_IPV6
	case RTPTransmitter::IPv6UDPProto:
		rtptrans = RTPNew(GetMemoryManager(),RTPMEM_TYPE_CLASS_RTPTRANSMITTER) RTPUDPv6Transmitter(GetMemoryManager());
		break;
#endif // RTP_SUPPORT_IPV6
	case RTPTransmitter::ExternalProto:
		rtptrans = RTPNew(GetMemoryManager(),RTPMEM_TYPE_CLASS_RTPTRANSMITTER) RTPExternalTransmitter(GetMemoryManager());
		break;
	case RTPTransmitter::UserDefinedProto:
		rtptrans = NewUserDefinedTransmitter();
		if (rtptrans == 0)
			return ERR_RTP_SESSION_USERDEFINEDTRANSMITTERNULL;
		break;
	case RTPTransmitter::TCPProto:
		rtptrans = RTPNew(GetMemoryManager(),RTPMEM_TYPE_CLASS_RTPTRANSMITTER) RTPTCPTransmitter(GetMemoryManager());
		break;
	default:
		return ERR_RTP_SESSION_UNSUPPORTEDTRANSMISSIONPROTOCOL;
	}
	
	if (rtptrans == 0)
		return ERR_RTP_OUTOFMEM;
	if ((status = rtptrans->Init(needthreadsafety)) < 0)
	{
		RTPDelete(rtptrans,GetMemoryManager());
		return status;
	}
	if ((status = rtptrans->Create(maxpacksize,transparams)) < 0)
	{
		RTPDelete(rtptrans,GetMemoryManager());
		return status;
	}

	deletetransmitter = true;
	return InternalCreate(sessparams);
}

int RTPSession::Create(const RTPSessionParams &sessparams,RTPTransmitter *transmitter)
{
	int status;
	
	if (created)
		return ERR_RTP_SESSION_ALREADYCREATED;

	usingpollthread = sessparams.IsUsingPollThread();
	needthreadsafety = sessparams.NeedThreadSafety();
	if (usingpollthread && !needthreadsafety)
		return ERR_RTP_SESSION_THREADSAFETYCONFLICT;

	useSR_BYEifpossible = sessparams.GetSenderReportForBYE();
	sentpackets = false;
	
	// Check max packet size
	
	if ((maxpacksize = sessparams.GetMaximumPacketSize()) < RTP_MINPACKETSIZE)
		return ERR_RTP_SESSION_MAXPACKETSIZETOOSMALL;
		
	rtptrans = transmitter;

	if ((status = rtptrans->SetMaximumPacketSize(maxpacksize)) < 0)
		return status;

	deletetransmitter = false;
	return InternalCreate(sessparams);
}

int RTPSession::InternalCreate(const RTPSessionParams &sessparams)
{
	int status;

	// Initialize packet builder
	
	if ((status = packetbuilder.Init(maxpacksize)) < 0)
	{
		if (deletetransmitter)
			RTPDelete(rtptrans,GetMemoryManager());
		return status;
	}

	if (sessparams.GetUsePredefinedSSRC())
		packetbuilder.AdjustSSRC(sessparams.GetPredefinedSSRC());

#ifdef RTP_SUPPORT_PROBATION

	// Set probation type
	sources.SetProbationType(sessparams.GetProbationType());

#endif // RTP_SUPPORT_PROBATION

	// Add our own ssrc to the source table
	
	if ((status = sources.CreateOwnSSRC(packetbuilder.GetSSRC())) < 0)
	{
		packetbuilder.Destroy();
		if (deletetransmitter)
			RTPDelete(rtptrans,GetMemoryManager());
		return status;
	}

	// Set the initial receive mode
	
	if ((status = rtptrans->SetReceiveMode(sessparams.GetReceiveMode())) < 0)
	{
		packetbuilder.Destroy();
		sources.Clear();
		if (deletetransmitter)
			RTPDelete(rtptrans,GetMemoryManager());
		return status;
	}

	// Init the RTCP packet builder
	
	double timestampunit = sessparams.GetOwnTimestampUnit();
	uint8_t buf[1024];
	size_t buflen = 1024;
	std::string forcedcname = sessparams.GetCNAME(); 

	if (forcedcname.length() == 0)
	{
		if ((status = CreateCNAME(buf,&buflen,sessparams.GetResolveLocalHostname())) < 0)
		{
			packetbuilder.Destroy();
			sources.Clear();
			if (deletetransmitter)
				RTPDelete(rtptrans,GetMemoryManager());
			return status;
		}
	}
	else
	{
		RTP_STRNCPY((char *)buf, forcedcname.c_str(), buflen);
		buf[buflen-1] = 0;
		buflen = strlen((char *)buf);
	}
	
	if ((status = rtcpbuilder.Init(maxpacksize,timestampunit,buf,buflen)) < 0)
	{
		packetbuilder.Destroy();
		sources.Clear();
		if (deletetransmitter)
			RTPDelete(rtptrans,GetMemoryManager());
		return status;
	}

	// Set scheduler parameters
	
	rtcpsched.Reset();
	rtcpsched.SetHeaderOverhead(rtptrans->GetHeaderOverhead());

	RTCPSchedulerParams schedparams;

	sessionbandwidth = sessparams.GetSessionBandwidth();
	controlfragment = sessparams.GetControlTrafficFraction();
	
	if ((status = schedparams.SetRTCPBandwidth(sessionbandwidth*controlfragment)) < 0)
	{
		if (deletetransmitter)
			RTPDelete(rtptrans,GetMemoryManager());
		packetbuilder.Destroy();
		sources.Clear();
		rtcpbuilder.Destroy();
		return status;
	}
	if ((status = schedparams.SetSenderBandwidthFraction(sessparams.GetSenderControlBandwidthFraction())) < 0)
	{
		if (deletetransmitter)
			RTPDelete(rtptrans,GetMemoryManager());
		packetbuilder.Destroy();
		sources.Clear();
		rtcpbuilder.Destroy();
		return status;
	}
	if ((status = schedparams.SetMinimumTransmissionInterval(sessparams.GetMinimumRTCPTransmissionInterval())) < 0)
	{
		if (deletetransmitter)
			RTPDelete(rtptrans,GetMemoryManager());
		packetbuilder.Destroy();
		sources.Clear();
		rtcpbuilder.Destroy();
		return status;
	}
	schedparams.SetUseHalfAtStartup(sessparams.GetUseHalfRTCPIntervalAtStartup());
	schedparams.SetRequestImmediateBYE(sessparams.GetRequestImmediateBYE());
	
	rtcpsched.SetParameters(schedparams);

	// copy other parameters
	
	acceptownpackets = sessparams.AcceptOwnPackets();
	membermultiplier = sessparams.GetSourceTimeoutMultiplier();
	sendermultiplier = sessparams.GetSenderTimeoutMultiplier();
	byemultiplier = sessparams.GetBYETimeoutMultiplier();
	collisionmultiplier = sessparams.GetCollisionTimeoutMultiplier();
	notemultiplier = sessparams.GetNoteTimeoutMultiplier();

	// Do thread stuff if necessary
	
#ifdef RTP_SUPPORT_THREAD
	pollthread = 0;
	if (usingpollthread)
	{
		if (!sourcesmutex.IsInitialized())	
		{
			if (sourcesmutex.Init() < 0)
			{
				if (deletetransmitter)
					RTPDelete(rtptrans,GetMemoryManager());
				packetbuilder.Destroy();
				sources.Clear();
				rtcpbuilder.Destroy();
				return ERR_RTP_SESSION_CANTINITMUTEX;
			}
		}
		if (!buildermutex.IsInitialized())
		{
			if (buildermutex.Init() < 0)
			{
				if (deletetransmitter)
					RTPDelete(rtptrans,GetMemoryManager());
				packetbuilder.Destroy();
				sources.Clear();
				rtcpbuilder.Destroy();
				return ERR_RTP_SESSION_CANTINITMUTEX;
			}
		}
		if (!schedmutex.IsInitialized())
		{
			if (schedmutex.Init() < 0)
			{
				if (deletetransmitter)
					RTPDelete(rtptrans,GetMemoryManager());
				packetbuilder.Destroy();
				sources.Clear();
				rtcpbuilder.Destroy();
				return ERR_RTP_SESSION_CANTINITMUTEX;
			}
		}
		if (!packsentmutex.IsInitialized())
		{
			if (packsentmutex.Init() < 0)
			{
				if (deletetransmitter)
					RTPDelete(rtptrans,GetMemoryManager());
				packetbuilder.Destroy();
				sources.Clear();
				rtcpbuilder.Destroy();
				return ERR_RTP_SESSION_CANTINITMUTEX;
			}
		}
		
		pollthread = RTPNew(GetMemoryManager(),RTPMEM_TYPE_CLASS_RTPPOLLTHREAD) RTPPollThread(*this,rtcpsched);
		if (pollthread == 0)
		{
			if (deletetransmitter)
				RTPDelete(rtptrans,GetMemoryManager());
			packetbuilder.Destroy();
			sources.Clear();
			rtcpbuilder.Destroy();
			return ERR_RTP_OUTOFMEM;
		}
		if ((status = pollthread->Start(rtptrans)) < 0)
		{
			if (deletetransmitter)
				RTPDelete(rtptrans,GetMemoryManager());
			RTPDelete(pollthread,GetMemoryManager());
			packetbuilder.Destroy();
			sources.Clear();
			rtcpbuilder.Destroy();
			return status;
		}
	}
#endif // RTP_SUPPORT_THREAD	

	created = true;
	return 0;
}


void RTPSession::Destroy()
{
	if (!created)
		return;

#ifdef RTP_SUPPORT_THREAD
	if (pollthread)
		RTPDelete(pollthread,GetMemoryManager());
#endif // RTP_SUPPORT_THREAD
	
	if (deletetransmitter)
		RTPDelete(rtptrans,GetMemoryManager());
	packetbuilder.Destroy();
	rtcpbuilder.Destroy();
	rtcpsched.Reset();
	collisionlist.Clear();
	sources.Clear();

	std::list<RTCPCompoundPacket *>::const_iterator it;

	for (it = byepackets.begin() ; it != byepackets.end() ; it++)
		RTPDelete(*it,GetMemoryManager());
	byepackets.clear();
	
	created = false;
}

void RTPSession::BYEDestroy(const RTPTime &maxwaittime,const void *reason,size_t reasonlength)
{
	if (!created)
		return;

	// first, stop the thread so we have full control over all components
	
#ifdef RTP_SUPPORT_THREAD
	if (pollthread)
		RTPDelete(pollthread,GetMemoryManager());
#endif // RTP_SUPPORT_THREAD

	RTPTime stoptime = RTPTime::CurrentTime();
	stoptime += maxwaittime;

	// add bye packet to the list if we've sent data

	RTCPCompoundPacket *pack;

	if (sentpackets)
	{
		int status;
		
		reasonlength = (reasonlength>RTCP_BYE_MAXREASONLENGTH)?RTCP_BYE_MAXREASONLENGTH:reasonlength;
	       	status = rtcpbuilder.BuildBYEPacket(&pack,reason,reasonlength,useSR_BYEifpossible);
		if (status >= 0)
		{
			byepackets.push_back(pack);
	
			if (byepackets.size() == 1)
				rtcpsched.ScheduleBYEPacket(pack->GetCompoundPacketLength());
		}
	}
	
	if (!byepackets.empty())
	{
		bool done = false;
		
		while (!done)
		{
			RTPTime curtime = RTPTime::CurrentTime();
			
			if (curtime >= stoptime)
				done = true;
		
			if (rtcpsched.IsTime())
			{
				pack = *(byepackets.begin());
				byepackets.pop_front();
			
				SendRTCPData(pack->GetCompoundPacketData(),pack->GetCompoundPacketLength());
				
				OnSendRTCPCompoundPacket(pack); // we'll place this after the actual send to avoid tampering
				
				RTPDelete(pack,GetMemoryManager());
				if (!byepackets.empty()) // more bye packets to send, schedule them
					rtcpsched.ScheduleBYEPacket((*(byepackets.begin()))->GetCompoundPacketLength());
				else
					done = true;
			}
			if (!done)
				RTPTime::Wait(RTPTime(0,100000));
		}
	}
	
	if (deletetransmitter)
		RTPDelete(rtptrans,GetMemoryManager());
	packetbuilder.Destroy();
	rtcpbuilder.Destroy();
	rtcpsched.Reset();
	collisionlist.Clear();
	sources.Clear();

	// clear rest of bye packets
	std::list<RTCPCompoundPacket *>::const_iterator it;

	for (it = byepackets.begin() ; it != byepackets.end() ; it++)
		RTPDelete(*it,GetMemoryManager());
	byepackets.clear();
	
	created = false;
}

bool RTPSession::IsActive()
{
	return created;
}

uint32_t RTPSession::GetLocalSSRC()
{
	if (!created)
		return 0;
	
	uint32_t ssrc;

	BUILDER_LOCK
	ssrc = packetbuilder.GetSSRC();
	BUILDER_UNLOCK
	return ssrc;
}

int RTPSession::AddDestination(const RTPAddress &addr)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	return rtptrans->AddDestination(addr);
}

int RTPSession::DeleteDestination(const RTPAddress &addr)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	return rtptrans->DeleteDestination(addr);
}

void RTPSession::ClearDestinations()
{
	if (!created)
		return;
	rtptrans->ClearDestinations();
}

bool RTPSession::SupportsMulticasting()
{
	if (!created)
		return false;
	return rtptrans->SupportsMulticasting();
}

int RTPSession::JoinMulticastGroup(const RTPAddress &addr)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	return rtptrans->JoinMulticastGroup(addr);
}

int RTPSession::LeaveMulticastGroup(const RTPAddress &addr)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	return rtptrans->LeaveMulticastGroup(addr);
}

void RTPSession::LeaveAllMulticastGroups()
{
	if (!created)
		return;
	rtptrans->LeaveAllMulticastGroups();
}

int RTPSession::SendPacket(const void *data,size_t len)
{
	int status;
	
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	BUILDER_LOCK
	if ((status = packetbuilder.BuildPacket(data,len)) < 0)
	{
		BUILDER_UNLOCK
		return status;
	}
	if ((status = SendRTPData(packetbuilder.GetPacket(),packetbuilder.GetPacketLength())) < 0)
	{
		BUILDER_UNLOCK
		return status;
	}
	BUILDER_UNLOCK

	SOURCES_LOCK
	sources.SentRTPPacket();
	SOURCES_UNLOCK
	PACKSENT_LOCK
	sentpackets = true;
	PACKSENT_UNLOCK
	return 0;
}

int RTPSession::SendPacket(const void *data,size_t len,
                uint8_t pt,bool mark,uint32_t timestampinc)
{
	int status;

	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	
	BUILDER_LOCK
	if ((status = packetbuilder.BuildPacket(data,len,pt,mark,timestampinc)) < 0)
	{
		BUILDER_UNLOCK
		return status;
	}
	if ((status = SendRTPData(packetbuilder.GetPacket(),packetbuilder.GetPacketLength())) < 0)
	{
		BUILDER_UNLOCK
		return status;
	}
	BUILDER_UNLOCK
	
	SOURCES_LOCK
	sources.SentRTPPacket();
	SOURCES_UNLOCK
	PACKSENT_LOCK
	sentpackets = true;
	PACKSENT_UNLOCK
	return 0;
}

int RTPSession::SendPacketEx(const void *data,size_t len,
                  uint16_t hdrextID,const void *hdrextdata,size_t numhdrextwords)
{
	int status;
	
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	BUILDER_LOCK
	if ((status = packetbuilder.BuildPacketEx(data,len,hdrextID,hdrextdata,numhdrextwords)) < 0)
	{
		BUILDER_UNLOCK
		return status;
	}
	if ((status = SendRTPData(packetbuilder.GetPacket(),packetbuilder.GetPacketLength())) < 0)
	{
		BUILDER_UNLOCK
		return status;
	}
	BUILDER_UNLOCK

	SOURCES_LOCK
	sources.SentRTPPacket();
	SOURCES_UNLOCK
	PACKSENT_LOCK
	sentpackets = true;
	PACKSENT_UNLOCK
	return 0;
}

int RTPSession::SendPacketEx(const void *data,size_t len,
                  uint8_t pt,bool mark,uint32_t timestampinc,
                  uint16_t hdrextID,const void *hdrextdata,size_t numhdrextwords)
{
	int status;
	
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	
	BUILDER_LOCK
	if ((status = packetbuilder.BuildPacketEx(data,len,pt,mark,timestampinc,hdrextID,hdrextdata,numhdrextwords)) < 0)
	{
		BUILDER_UNLOCK
		return status;
	}
	if ((status = SendRTPData(packetbuilder.GetPacket(),packetbuilder.GetPacketLength())) < 0)
	{
		BUILDER_UNLOCK
		return status;
	}
	BUILDER_UNLOCK

	SOURCES_LOCK
	sources.SentRTPPacket();
	SOURCES_UNLOCK
	PACKSENT_LOCK
	sentpackets = true;
	PACKSENT_UNLOCK
	return 0;
}

#ifdef RTP_SUPPORT_SENDAPP

int RTPSession::SendRTCPAPPPacket(uint8_t subtype, const uint8_t name[4], const void *appdata, size_t appdatalen)
{
	int status;

	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	BUILDER_LOCK
	uint32_t ssrc = packetbuilder.GetSSRC();
	BUILDER_UNLOCK
	
	RTCPCompoundPacketBuilder pb(GetMemoryManager());

	status = pb.InitBuild(maxpacksize);
	
	if(status < 0)
		return status;

	//first packet in an rtcp compound packet should always be SR or RR
	if((status = pb.StartReceiverReport(ssrc)) < 0)
		return status;

	//add SDES packet with CNAME item
	if ((status = pb.AddSDESSource(ssrc)) < 0)
		return status;
	
	BUILDER_LOCK
	size_t owncnamelen = 0;
	uint8_t *owncname = rtcpbuilder.GetLocalCNAME(&owncnamelen);

	if ((status = pb.AddSDESNormalItem(RTCPSDESPacket::CNAME,owncname,owncnamelen)) < 0)
	{
		BUILDER_UNLOCK
		return status;
	}
	BUILDER_UNLOCK
	
	//add our application specific packet
	if((status = pb.AddAPPPacket(subtype, ssrc, name, appdata, appdatalen)) < 0)
		return status;

	if((status = pb.EndBuild()) < 0)
		return status;

	//send packet
	status = SendRTCPData(pb.GetCompoundPacketData(),pb.GetCompoundPacketLength());
	if(status < 0)
		return status;

	PACKSENT_LOCK
	sentpackets = true;
	PACKSENT_UNLOCK

	return pb.GetCompoundPacketLength();
}

#endif // RTP_SUPPORT_SENDAPP

#ifdef RTP_SUPPORT_RTCPUNKNOWN

int RTPSession::SendUnknownPacket(bool sr, uint8_t payload_type, uint8_t subtype, const void *data, size_t len)
{
	int status;

	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	BUILDER_LOCK
	uint32_t ssrc = packetbuilder.GetSSRC();
	BUILDER_UNLOCK
	
	RTCPCompoundPacketBuilder* rtcpcomppack = RTPNew(GetMemoryManager(),RTPMEM_TYPE_CLASS_RTCPCOMPOUNDPACKETBUILDER) RTCPCompoundPacketBuilder(GetMemoryManager());
	if (rtcpcomppack == 0)
	{
		RTPDelete(rtcpcomppack,GetMemoryManager());
		return ERR_RTP_OUTOFMEM;
	}

	status = rtcpcomppack->InitBuild(maxpacksize);	
	if(status < 0)
	{
		RTPDelete(rtcpcomppack,GetMemoryManager());
		return status;
	}

	if (sr)
	{
		// setup for the rtcp 
		RTPTime rtppacktime = packetbuilder.GetPacketTime();
		uint32_t rtppacktimestamp = packetbuilder.GetPacketTimestamp();
		uint32_t packcount = packetbuilder.GetPacketCount();
		uint32_t octetcount = packetbuilder.GetPayloadOctetCount();
		RTPTime curtime = RTPTime::CurrentTime();
		RTPTime diff = curtime;
		diff -= rtppacktime;
		diff += 1; // add transmission delay or RTPTime(0,0);

		double timestampunit = 90000;

		uint32_t tsdiff = (uint32_t)((diff.GetDouble()/timestampunit)+0.5);
		uint32_t rtptimestamp = rtppacktimestamp+tsdiff;
		RTPNTPTime ntptimestamp = curtime.GetNTPTime();

		//first packet in an rtcp compound packet should always be SR or RR
		if((status = rtcpcomppack->StartSenderReport(ssrc,ntptimestamp,rtptimestamp,packcount,octetcount)) < 0)
		{
			RTPDelete(rtcpcomppack,GetMemoryManager());
			return status;
		}
	}
	else
	{
		//first packet in an rtcp compound packet should always be SR or RR
		if((status = rtcpcomppack->StartReceiverReport(ssrc)) < 0)
		{
			RTPDelete(rtcpcomppack,GetMemoryManager());
			return status;
		}

	}

	//add SDES packet with CNAME item
	if ((status = rtcpcomppack->AddSDESSource(ssrc)) < 0)
	{
		RTPDelete(rtcpcomppack,GetMemoryManager());
		return status;
	}
	
	BUILDER_LOCK
	size_t owncnamelen = 0;
	uint8_t *owncname = rtcpbuilder.GetLocalCNAME(&owncnamelen);

	if ((status = rtcpcomppack->AddSDESNormalItem(RTCPSDESPacket::CNAME,owncname,owncnamelen)) < 0)
	{
		BUILDER_UNLOCK
		RTPDelete(rtcpcomppack,GetMemoryManager());
		return status;
	}
	BUILDER_UNLOCK
	
	//add our packet
	if((status = rtcpcomppack->AddUnknownPacket(payload_type, subtype, ssrc, data, len)) < 0)
	{
		RTPDelete(rtcpcomppack,GetMemoryManager());
		return status;
	}

	if((status = rtcpcomppack->EndBuild()) < 0)
	{
		RTPDelete(rtcpcomppack,GetMemoryManager());
		return status;
	}

	//send packet
	status = SendRTCPData(rtcpcomppack->GetCompoundPacketData(), rtcpcomppack->GetCompoundPacketLength());
	if(status < 0)
	{
		RTPDelete(rtcpcomppack,GetMemoryManager());
		return status;
	}

	PACKSENT_LOCK
	sentpackets = true;
	PACKSENT_UNLOCK

	OnSendRTCPCompoundPacket(rtcpcomppack); // we'll place this after the actual send to avoid tampering

	int retlen = rtcpcomppack->GetCompoundPacketLength();

	RTPDelete(rtcpcomppack,GetMemoryManager());
	return retlen;
}

#endif // RTP_SUPPORT_RTCPUNKNOWN 

int RTPSession::SendRawData(const void *data, size_t len, bool usertpchannel)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	int status;

	if (usertpchannel)
		status = rtptrans->SendRTPData(data, len);
	else
		status = rtptrans->SendRTCPData(data, len);
	return status;
}

int RTPSession::SetDefaultPayloadType(uint8_t pt)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	
	int status;
	
	BUILDER_LOCK
	status = packetbuilder.SetDefaultPayloadType(pt);
	BUILDER_UNLOCK
	return status;
}

int RTPSession::SetDefaultMark(bool m)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	int status;

	BUILDER_LOCK
	status = packetbuilder.SetDefaultMark(m);
	BUILDER_UNLOCK
	return status;
}

int RTPSession::SetDefaultTimestampIncrement(uint32_t timestampinc)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	int status;

	BUILDER_LOCK
	status = packetbuilder.SetDefaultTimestampIncrement(timestampinc);
	BUILDER_UNLOCK
	return status;
}

int RTPSession::IncrementTimestamp(uint32_t inc)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	int status;

	BUILDER_LOCK
	status = packetbuilder.IncrementTimestamp(inc);
	BUILDER_UNLOCK
	return status;
}

int RTPSession::IncrementTimestampDefault()
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	int status;
	
	BUILDER_LOCK
	status = packetbuilder.IncrementTimestampDefault();
	BUILDER_UNLOCK
	return status;
}

int RTPSession::SetPreTransmissionDelay(const RTPTime &delay)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	int status;

	BUILDER_LOCK
	status = rtcpbuilder.SetPreTransmissionDelay(delay);
	BUILDER_UNLOCK
	return status;
}

RTPTransmissionInfo *RTPSession::GetTransmissionInfo()
{
	if (!created)
		return 0;
	return rtptrans->GetTransmissionInfo();
}

void RTPSession::DeleteTransmissionInfo(RTPTransmissionInfo *inf)
{
	if (!created)
		return;
	rtptrans->DeleteTransmissionInfo(inf);
}

int RTPSession::Poll()
{
	int status;
	
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	if (usingpollthread)
		return ERR_RTP_SESSION_USINGPOLLTHREAD;
	if ((status = rtptrans->Poll()) < 0)
		return status;
	return ProcessPolledData();
}

int RTPSession::WaitForIncomingData(const RTPTime &delay,bool *dataavailable)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	if (usingpollthread)
		return ERR_RTP_SESSION_USINGPOLLTHREAD;
	return rtptrans->WaitForIncomingData(delay,dataavailable);
}

int RTPSession::AbortWait()
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	if (usingpollthread)
		return ERR_RTP_SESSION_USINGPOLLTHREAD;
	return rtptrans->AbortWait();
}

RTPTime RTPSession::GetRTCPDelay()
{
	if (!created)
		return RTPTime(0,0);
	if (usingpollthread)
		return RTPTime(0,0);

	SOURCES_LOCK
	SCHED_LOCK
	RTPTime t = rtcpsched.GetTransmissionDelay();
	SCHED_UNLOCK
	SOURCES_UNLOCK
	return t;
}

int RTPSession::BeginDataAccess()
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	SOURCES_LOCK
	return 0;
}

bool RTPSession::GotoFirstSource()
{
	if (!created)
		return false;
	return sources.GotoFirstSource();
}

bool RTPSession::GotoNextSource()
{
	if (!created)
		return false;
	return sources.GotoNextSource();
}

bool RTPSession::GotoPreviousSource()
{
	if (!created)
		return false;
	return sources.GotoPreviousSource();
}

bool RTPSession::GotoFirstSourceWithData()
{
	if (!created)
		return false;
	return sources.GotoFirstSourceWithData();
}

bool RTPSession::GotoNextSourceWithData()
{
	if (!created)
		return false;
	return sources.GotoNextSourceWithData();
}

bool RTPSession::GotoPreviousSourceWithData()
{
	if (!created)
		return false;
	return sources.GotoPreviousSourceWithData();
}

RTPSourceData *RTPSession::GetCurrentSourceInfo()
{
	if (!created)
		return 0;
	return sources.GetCurrentSourceInfo();
}

RTPSourceData *RTPSession::GetSourceInfo(uint32_t ssrc)
{
	if (!created)
		return 0;
	return sources.GetSourceInfo(ssrc);
}

RTPPacket *RTPSession::GetNextPacket()
{
	if (!created)
		return 0;
	return sources.GetNextPacket();
}

uint16_t RTPSession::GetNextSequenceNumber() const
{
    return packetbuilder.GetSequenceNumber();
}

void RTPSession::DeletePacket(RTPPacket *p)
{
	RTPDelete(p,GetMemoryManager());
}

int RTPSession::EndDataAccess()
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	SOURCES_UNLOCK
	return 0;
}

int RTPSession::SetReceiveMode(RTPTransmitter::ReceiveMode m)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	return rtptrans->SetReceiveMode(m);
}

int RTPSession::AddToIgnoreList(const RTPAddress &addr)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	return rtptrans->AddToIgnoreList(addr);
}

int RTPSession::DeleteFromIgnoreList(const RTPAddress &addr)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	return rtptrans->DeleteFromIgnoreList(addr);
}

void RTPSession::ClearIgnoreList()
{
	if (!created)
		return;
	rtptrans->ClearIgnoreList();
}

int RTPSession::AddToAcceptList(const RTPAddress &addr)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	return rtptrans->AddToAcceptList(addr);
}

int RTPSession::DeleteFromAcceptList(const RTPAddress &addr)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;
	return rtptrans->DeleteFromAcceptList(addr);
}

void RTPSession::ClearAcceptList()
{
	if (!created)
		return;
	rtptrans->ClearAcceptList();
}

int RTPSession::SetMaximumPacketSize(size_t s)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	if (s < RTP_MINPACKETSIZE)
		return ERR_RTP_SESSION_MAXPACKETSIZETOOSMALL;
	
	int status;

	if ((status = rtptrans->SetMaximumPacketSize(s)) < 0)
		return status;

	BUILDER_LOCK
	if ((status = packetbuilder.SetMaximumPacketSize(s)) < 0)
	{
		BUILDER_UNLOCK
		// restore previous max packet size
		rtptrans->SetMaximumPacketSize(maxpacksize);
		return status;
	}
	if ((status = rtcpbuilder.SetMaximumPacketSize(s)) < 0)
	{
		// restore previous max packet size
		packetbuilder.SetMaximumPacketSize(maxpacksize);
		BUILDER_UNLOCK
		rtptrans->SetMaximumPacketSize(maxpacksize);
		return status;
	}
	BUILDER_UNLOCK
	maxpacksize = s;
	return 0;
}

int RTPSession::SetSessionBandwidth(double bw)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	int status;
	SCHED_LOCK
	RTCPSchedulerParams p = rtcpsched.GetParameters();
	status = p.SetRTCPBandwidth(bw*controlfragment);
	if (status >= 0)
	{
		rtcpsched.SetParameters(p);
		sessionbandwidth = bw;
	}
	SCHED_UNLOCK
	return status;
}

int RTPSession::SetTimestampUnit(double u)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	int status;

	BUILDER_LOCK
	status = rtcpbuilder.SetTimestampUnit(u);
	BUILDER_UNLOCK
	return status;
}

void RTPSession::SetNameInterval(int count)
{
	if (!created)
		return;
	BUILDER_LOCK
	rtcpbuilder.SetNameInterval(count);
	BUILDER_UNLOCK
}

void RTPSession::SetEMailInterval(int count)
{
	if (!created)
		return;
	BUILDER_LOCK
	rtcpbuilder.SetEMailInterval(count);
	BUILDER_UNLOCK
}

void RTPSession::SetLocationInterval(int count)
{
	if (!created)
		return;
	BUILDER_LOCK
	rtcpbuilder.SetLocationInterval(count);
	BUILDER_UNLOCK
}

void RTPSession::SetPhoneInterval(int count)
{
	if (!created)
		return;
	BUILDER_LOCK
	rtcpbuilder.SetPhoneInterval(count);
	BUILDER_UNLOCK
}

void RTPSession::SetToolInterval(int count)
{
	if (!created)
		return;
	BUILDER_LOCK
	rtcpbuilder.SetToolInterval(count);
	BUILDER_UNLOCK
}

void RTPSession::SetNoteInterval(int count)
{
	if (!created)
		return;
	BUILDER_LOCK
	rtcpbuilder.SetNoteInterval(count);
	BUILDER_UNLOCK
}

int RTPSession::SetLocalName(const void *s,size_t len)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	int status;
	BUILDER_LOCK
	status = rtcpbuilder.SetLocalName(s,len);
	BUILDER_UNLOCK
	return status;
}

int RTPSession::SetLocalEMail(const void *s,size_t len)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	int status;
	BUILDER_LOCK
	status = rtcpbuilder.SetLocalEMail(s,len);
	BUILDER_UNLOCK
	return status;
}

int RTPSession::SetLocalLocation(const void *s,size_t len)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	int status;
	BUILDER_LOCK
	status = rtcpbuilder.SetLocalLocation(s,len);
	BUILDER_UNLOCK
	return status;
}

int RTPSession::SetLocalPhone(const void *s,size_t len)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	int status;
	BUILDER_LOCK
	status = rtcpbuilder.SetLocalPhone(s,len);
	BUILDER_UNLOCK
	return status;
}

int RTPSession::SetLocalTool(const void *s,size_t len)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	int status;
	BUILDER_LOCK
	status = rtcpbuilder.SetLocalTool(s,len);
	BUILDER_UNLOCK
	return status;
}

int RTPSession::SetLocalNote(const void *s,size_t len)
{
	if (!created)
		return ERR_RTP_SESSION_NOTCREATED;

	int status;
	BUILDER_LOCK
	status = rtcpbuilder.SetLocalNote(s,len);
	BUILDER_UNLOCK
	return status;
}

int RTPSession::ProcessPolledData()
{
	RTPRawPacket *rawpack;
	int status;
	
	SOURCES_LOCK
	while ((rawpack = rtptrans->GetNextPacket()) != 0)
	{
		if (m_changeIncomingData)
		{
			// Provide a way to change incoming data, for decryption for example
			if (!OnChangeIncomingData(rawpack))
			{
				RTPDelete(rawpack,GetMemoryManager());
				continue;
			}
		}

		sources.ClearOwnCollisionFlag();

		// since our sources instance also uses the scheduler (analysis of incoming packets)
		// we'll lock it
		SCHED_LOCK
		if ((status = sources.ProcessRawPacket(rawpack,rtptrans,acceptownpackets)) < 0)
		{
			SCHED_UNLOCK
			SOURCES_UNLOCK
			RTPDelete(rawpack,GetMemoryManager());
			return status;
		}
		SCHED_UNLOCK
				
		if (sources.DetectedOwnCollision()) // collision handling!
		{
			bool created;
			
			if ((status = collisionlist.UpdateAddress(rawpack->GetSenderAddress(),rawpack->GetReceiveTime(),&created)) < 0)
			{
				SOURCES_UNLOCK
				RTPDelete(rawpack,GetMemoryManager());
				return status;
			}

			if (created) // first time we've encountered this address, send bye packet and
			{            // change our own SSRC
				PACKSENT_LOCK
				bool hassentpackets = sentpackets;
				PACKSENT_UNLOCK

				if (hassentpackets)
				{
					// Only send BYE packet if we've actually sent data using this
					// SSRC
					
					RTCPCompoundPacket *rtcpcomppack;

					BUILDER_LOCK
					if ((status = rtcpbuilder.BuildBYEPacket(&rtcpcomppack,0,0,useSR_BYEifpossible)) < 0)
					{
						BUILDER_UNLOCK
						SOURCES_UNLOCK
						RTPDelete(rawpack,GetMemoryManager());
						return status;
					}
					BUILDER_UNLOCK

					byepackets.push_back(rtcpcomppack);
					if (byepackets.size() == 1) // was the first packet, schedule a BYE packet (otherwise there's already one scheduled)
					{
						SCHED_LOCK
						rtcpsched.ScheduleBYEPacket(rtcpcomppack->GetCompoundPacketLength());
						SCHED_UNLOCK
					}
				}
				// bye packet is built and scheduled, now change our SSRC
				// and reset the packet count in the transmitter
				
				BUILDER_LOCK
				uint32_t newssrc = packetbuilder.CreateNewSSRC(sources);
				BUILDER_UNLOCK
					
				PACKSENT_LOCK
				sentpackets = false;
				PACKSENT_UNLOCK
	
				// remove old entry in source table and add new one

				if ((status = sources.DeleteOwnSSRC()) < 0)
				{
					SOURCES_UNLOCK
					RTPDelete(rawpack,GetMemoryManager());
					return status;
				}
				if ((status = sources.CreateOwnSSRC(newssrc)) < 0)
				{
					SOURCES_UNLOCK
					RTPDelete(rawpack,GetMemoryManager());
					return status;
				}
			}
		}
		RTPDelete(rawpack,GetMemoryManager());
	}

	SCHED_LOCK
	RTPTime d = rtcpsched.CalculateDeterministicInterval(false);
	SCHED_UNLOCK
	
	RTPTime t = RTPTime::CurrentTime();
	double Td = d.GetDouble();
	RTPTime sendertimeout = RTPTime(Td*sendermultiplier);
	RTPTime generaltimeout = RTPTime(Td*membermultiplier);
	RTPTime byetimeout = RTPTime(Td*byemultiplier);
	RTPTime colltimeout = RTPTime(Td*collisionmultiplier);
	RTPTime notetimeout = RTPTime(Td*notemultiplier);
	
	sources.MultipleTimeouts(t,sendertimeout,byetimeout,generaltimeout,notetimeout);
	collisionlist.Timeout(t,colltimeout);
	
	// We'll check if it's time for RTCP stuff

	SCHED_LOCK
	bool istime = rtcpsched.IsTime();
	SCHED_UNLOCK
	
	if (istime)
	{
		RTCPCompoundPacket *pack;
	
		// we'll check if there's a bye packet to send, or just a normal packet

		if (byepackets.empty())
		{
			BUILDER_LOCK
			if ((status = rtcpbuilder.BuildNextPacket(&pack)) < 0)
			{
				BUILDER_UNLOCK
				SOURCES_UNLOCK
				return status;
			}
			BUILDER_UNLOCK
			if ((status = SendRTCPData(pack->GetCompoundPacketData(),pack->GetCompoundPacketLength())) < 0)
			{
				SOURCES_UNLOCK
				RTPDelete(pack,GetMemoryManager());
				return status;
			}
		
			PACKSENT_LOCK
			sentpackets = true;
			PACKSENT_UNLOCK

			OnSendRTCPCompoundPacket(pack); // we'll place this after the actual send to avoid tampering
		}
		else
		{
			pack = *(byepackets.begin());
			byepackets.pop_front();
			
			if ((status = SendRTCPData(pack->GetCompoundPacketData(),pack->GetCompoundPacketLength())) < 0)
			{
				SOURCES_UNLOCK
				RTPDelete(pack,GetMemoryManager());
				return status;
			}
			
			PACKSENT_LOCK
			sentpackets = true;
			PACKSENT_UNLOCK

			OnSendRTCPCompoundPacket(pack); // we'll place this after the actual send to avoid tampering
			
			if (!byepackets.empty()) // more bye packets to send, schedule them
			{
				SCHED_LOCK
				rtcpsched.ScheduleBYEPacket((*(byepackets.begin()))->GetCompoundPacketLength());
				SCHED_UNLOCK
			}
		}
		
		SCHED_LOCK
		rtcpsched.AnalyseOutgoing(*pack);
		SCHED_UNLOCK

		RTPDelete(pack,GetMemoryManager());
	}
	SOURCES_UNLOCK
	return 0;
}

int RTPSession::CreateCNAME(uint8_t *buffer,size_t *bufferlength,bool resolve)
{
#ifndef WIN32
	bool gotlogin = true;
	char defUser[]="root";
#ifdef RTP_SUPPORT_GETLOGINR
	buffer[0] = 0;
	if (getlogin_r((char *)buffer,*bufferlength) != 0)
		gotlogin = false;
	else
	{
		if (buffer[0] == 0)
			gotlogin = false;
	}
	
	if (!gotlogin) // try regular getlogin
	{
		char *loginname = getlogin();
		if (loginname == 0)
			gotlogin = false;
		else
			strncpy((char *)buffer,loginname,*bufferlength);
	}
#else
	char *loginname = getlogin();
	if (loginname == 0)
		gotlogin = false;
	else
		strncpy((char *)buffer,loginname,*bufferlength);
#endif // RTP_SUPPORT_GETLOGINR
	if (!gotlogin)
	{
		char *logname=NULL;
		for(int i = 0 ; environ[i] ;i++)
        {
			logname=strstr(environ[i], "LOGNAME");
			if(logname)
				break;  
        }
		if(logname){
			logname = getenv("LOGNAME");
			if (logname == 0)
				return ERR_RTP_SESSION_CANTGETLOGINNAME;
		}
		else
			logname=defUser;
		
		strncpy((char *)buffer,logname,*bufferlength);
	}
#else // Win32 version

#ifndef _WIN32_WCE
	DWORD len = *bufferlength;
	if (!GetUserName((LPTSTR)buffer,&len))
		RTP_STRNCPY((char *)buffer,"unknown",*bufferlength);
#else 
	RTP_STRNCPY((char *)buffer,"unknown",*bufferlength);
#endif // _WIN32_WCE
	
#endif // WIN32
	buffer[*bufferlength-1] = 0;

	size_t offset = strlen((const char *)buffer);
	if (offset < (*bufferlength-1))
		buffer[offset] = (uint8_t)'@';
	offset++;

	size_t buflen2 = *bufferlength-offset;
	int status;
	
	if (resolve)
	{
		if ((status = rtptrans->GetLocalHostName(buffer+offset,&buflen2)) < 0)
			return status;
		*bufferlength = buflen2+offset;
	}
	else
	{
		char hostname[1024];
		
		RTP_STRNCPY(hostname,"localhost",1024); // just in case gethostname fails

		gethostname(hostname,1024);
		RTP_STRNCPY((char *)(buffer+offset),hostname,buflen2);

		*bufferlength = offset+strlen(hostname);
	}
	if (*bufferlength > RTCP_SDES_MAXITEMLENGTH)
		*bufferlength = RTCP_SDES_MAXITEMLENGTH;
	return 0;
}

RTPRandom *RTPSession::GetRandomNumberGenerator(RTPRandom *r)
{
	RTPRandom *rnew = 0;

	if (r == 0)
	{
		rnew = RTPRandom::CreateDefaultRandomNumberGenerator();
		deletertprnd = true;
	}
	else
	{
		rnew = r;
		deletertprnd = false;
	}

	return rnew;
}

int RTPSession::SendRTPData(const void *data, size_t len)
{
	if (!m_changeOutgoingData)
		return rtptrans->SendRTPData(data, len);

	void *pSendData = 0;
	size_t sendLen = 0;
	int status = 0;

	status = OnChangeRTPOrRTCPData(data, len, true, &pSendData, &sendLen);
	if (status < 0)
		return status;

	if (pSendData)
	{
		status = rtptrans->SendRTPData(pSendData, sendLen);
		OnSentRTPOrRTCPData(pSendData, sendLen, true);
	}

	return status;
}

int RTPSession::SendRTCPData(const void *data, size_t len)
{
	if (!m_changeOutgoingData)
		return rtptrans->SendRTCPData(data, len);

	void *pSendData = 0;
	size_t sendLen = 0;
	int status = 0;

	status = OnChangeRTPOrRTCPData(data, len, false, &pSendData, &sendLen);
	if (status < 0)
		return status;

	if (pSendData)
	{
		status = rtptrans->SendRTCPData(pSendData, sendLen);
		OnSentRTPOrRTCPData(pSendData, sendLen, false);
	}

	return status;
}

#ifdef RTPDEBUG
void RTPSession::DumpSources()
{
	BeginDataAccess();
	std::cout << "----------------------------------------------------------------" << std::endl;
	sources.Dump();
	EndDataAccess();
}

void RTPSession::DumpTransmitter()
{
	if (created)
		rtptrans->Dump();
}
#endif // RTPDEBUG

} // end namespace

