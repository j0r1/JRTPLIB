#include "rtpsession.h"
#include "rtpsessionparams.h"
#include "rtpudpv4transmitter.h"
#include "rtperrors.h"
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

	// Now, we'll create a RTP session, set the destination, send some
	// packets and poll for incoming data.
	
	RTPUDPv4TransmissionParams transparams;
	RTPSessionParams sessparams;
	
	sessparams.SetOwnTimestampUnit(1.0/10.0);
	sessparams.SetAcceptOwnPackets(true);
	transparams.SetPortbase(0);

	//transparams.SetRTCPMultiplexing(true);
	//transparams.SetAllowOddPortbase(true);

	int status = sess.Create(sessparams,&transparams);	
	checkerror(status);

	RTPUDPv4TransmissionInfo *pInf = (RTPUDPv4TransmissionInfo *)sess.GetTransmissionInfo();
	uint16_t rtpPort = pInf->GetRTPPort();
	uint16_t rtcpPort = pInf->GetRTCPPort();	

	printf("Using RTP port %d and RTCP port %d\n", (int)rtpPort, (int)rtcpPort);
	
	uint32_t destip = ntohl(inet_addr("127.0.0.1"));

	// We're assuming that the destination is also using RTCP multiplexing 
	// ('true' means that the same port will be used for RTCP)
	RTPIPv4Address addr(destip,rtpPort,rtcpPort); 
	
	status = sess.AddDestination(addr);
	checkerror(status);
	
	const int num = 10;
	for (int i = 1 ; i <= num ; i++)
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

	sess.Destroy();

#ifdef RTP_SOCKETTYPE_WINSOCK
	WSACleanup();
#endif // RTP_SOCKETTYPE_WINSOCK
	return 0;
}

