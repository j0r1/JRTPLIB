#include "srtpsession.h"
#include "rtpudpv4transmitter.h"
#include "rtpipv4address.h"
#include "rtpsessionparams.h"
#include "rtperrors.h"
#include "rtplibraryversion.h"
#include "rtpsourcedata.h"
#include "rtprawpacket.h"
#include <stdlib.h>
#include <stdio.h>
#include <srtp.h>
#include <iostream>
#include <string>

using namespace std;

#ifdef RTP_SUPPORT_SRTP

using namespace jrtplib;

void checkerror(int rtperr)
{
	if (rtperr < 0)
	{
		std::cout << "ERROR: " << RTPGetErrorString(rtperr) << std::endl;
		exit(-1);
	}
}

class MyRTPSession : public SRTPSession
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
		printf("SSRC %x: Received SDES item (%d): %s\n", (unsigned int)srcdat->GetSSRC(), (int)t, msg);
	}
};

int main(void)
{
#ifdef WIN32
	WSADATA dat;
	WSAStartup(MAKEWORD(2,2),&dat);
#endif // WIN32

	srtp_init();
	
	MyRTPSession sender, receiver;
	uint16_t portbase1,portbase2;
	uint32_t destip;
	std::string ipstr;
	int status,i,num;

	std::cout << "Using version " << RTPLibraryVersion::GetVersion().GetVersionString() << std::endl;

	// First, we'll ask for the necessary information
		
	portbase1 = 5000;
	destip = ntohl(inet_addr("127.0.0.1"));
	portbase2 = 5002;
	num = 20;

	RTPUDPv4TransmissionParams transparams;
	RTPSessionParams sessparams;
	
	sessparams.SetOwnTimestampUnit(1.0/10.0);		
	transparams.SetPortbase(portbase1);
	transparams.SetRTCPMultiplexing(true); 

	status = sender.Create(sessparams,&transparams);	
	checkerror(status);
	status = sender.InitializeEncryption("AES_CM_128_HMAC_SHA1_80", "012345678901234567890123456789");
	checkerror(status);
	status = sender.AddDestination(RTPIPv4Address(destip, portbase2, true));
	checkerror(status);

	transparams.SetPortbase(portbase2);
	status = receiver.Create(sessparams,&transparams);	
	checkerror(status);
	status = receiver.InitializeEncryption("AES_CM_128_HMAC_SHA1_80", "012345678901234567890123456789");
	checkerror(status);
	status = receiver.AddDestination(RTPIPv4Address(destip, portbase1, true));
	checkerror(status);
	
	for (i = 1 ; i <= num ; i++)
	{
		printf("\nSending packet %d/%d\n",i,num);
		
		// send the packet
		status = sender.SendPacket((void *)"1234567890",10,0,false,10);
		checkerror(status);
		
#ifndef RTP_SUPPORT_THREAD
		status = sender.Poll();
		checkerror(status);
		status = receiver.Poll();
		checkerror(status);
#endif // RTP_SUPPORT_THREAD
		
		RTPTime::Wait(RTPTime(1,0));
	}
	
	srtp_shutdown();

#ifdef WIN32
	WSACleanup();
#endif // WIN32
	return 0;
}

#else

int main(void)
{
	cout << "SRTP support was not enabled at compile time" << endl;
	return 0;
}

#endif // RTP_SUPPORT_SRTP
