#include "rtpsession.h"
#include "rtpudpv4transmitter.h"
#include "rtpipv4address.h"
#include "rtpsessionparams.h"
#include "rtperrors.h"
#include "rtplibraryversion.h"
#include "rtpsourcedata.h"
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>

using namespace jrtplib;

void checkerror(int rtperr)
{
	if (rtperr < 0)
	{
		std::cout << "ERROR: " << RTPGetErrorString(rtperr) << std::endl;
		exit(-1);
	}
}

class MyRTPSession : public RTPSession
{
protected:
	void OnValidatedRTPPacket(RTPSourceData *srcdat, RTPPacket *rtppack, bool isonprobation, bool *ispackethandled)
	{
		printf("Got packet in OnValidatedRTPPacket from source 0x%04x!\n", srcdat->GetSSRC());
		DeletePacket(rtppack);
		*ispackethandled = true;
	}

	void OnRTCPSDESItem(RTPSourceData *srcdat, RTCPSDESPacket::ItemType t, const void *itemdata, size_t itemlength)
	{
		char msg[1024];

		memset(msg, 0, sizeof(msg));
		if (itemlength >= sizeof(msg))
			itemlength = sizeof(msg)-1;

		memcpy(msg, itemdata, itemlength);
		printf("Received SDES item (%d): %s", (int)t, msg);
	}
};

int main(void)
{
#ifdef RTP_SOCKETTYPE_WINSOCK
	WSADATA dat;
	WSAStartup(MAKEWORD(2,2),&dat);
#endif // RTP_SOCKETTYPE_WINSOCK
	
	MyRTPSession sess;
	uint16_t portbase,destport;
	uint32_t destip;
	std::string ipstr;
	int status,i,num;

	std::cout << "Using version " << RTPLibraryVersion::GetVersion().GetVersionString() << std::endl;

	// First, we'll ask for the necessary information
		
	portbase = 5000;
	ipstr = "127.0.0.1";

	destip = inet_addr(ipstr.c_str());
	if (destip == INADDR_NONE)
	{
		std::cerr << "Bad IP address specified" << std::endl;
		return -1;
	}
	
	// The inet_addr function returns a value in network byte order, but
	// we need the IP address in host byte order, so we use a call to
	// ntohl
	destip = ntohl(destip);
	destport = 5000;
	num = 10;

	// Now, we'll create a RTP session, set the destination, send some
	// packets and poll for incoming data.
	
	RTPUDPv4TransmissionParams transparams;
	RTPSessionParams sessparams;
	
	// IMPORTANT: The local timestamp unit MUST be set, otherwise
	//            RTCP Sender Report info will be calculated wrong
	// In this case, we'll be sending 10 samples each second, so we'll
	// put the timestamp unit to (1.0/10.0)
	sessparams.SetOwnTimestampUnit(1.0/10.0);		
	
	sessparams.SetAcceptOwnPackets(true);
	transparams.SetPortbase(portbase);
	
	// Let's also use RTCP multiplexing for this example
	transparams.SetRTCPMultiplexing(true); 

	status = sess.Create(sessparams,&transparams);	
	checkerror(status);
	
	// We're assuming that the destination is also using RTCP multiplexing 
	// ('true' means that the same port will be used for RTCP)
	RTPIPv4Address addr(destip,destport,true); 
	
	status = sess.AddDestination(addr);
	checkerror(status);
	
	for (i = 1 ; i <= num ; i++)
	{
		printf("\nSending packet %d/%d\n",i,num);
		
		// send the packet
		status = sess.SendPacket((void *)"1234567890",10,0,false,10);
		checkerror(status);
		
#ifndef RTP_SUPPORT_THREAD
		status = sess.Poll();
		checkerror(status);
#endif // RTP_SUPPORT_THREAD
		
		RTPTime::Wait(RTPTime(1,0));
	}
	
	sess.BYEDestroy(RTPTime(10,0),0,0);

#ifdef RTP_SOCKETTYPE_WINSOCK
	WSACleanup();
#endif // RTP_SOCKETTYPE_WINSOCK
	return 0;
}

