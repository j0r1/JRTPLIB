#include "rtpsecuresession.h"
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
using namespace jrtplib;

#ifdef RTP_SUPPORT_SRTP

void checkerror(int rtperr)
{
	if (rtperr < 0)
	{
		std::cout << "ERROR: " << RTPGetErrorString(rtperr) << std::endl;
		exit(-1);
	}
}

void checkerror(bool ok)
{
	if (!ok)
		exit(-1);
}

class MyRTPSession : public RTPSecureSession
{
public:
	bool Init(const std::string &key, uint32_t otherSSRC)
	{
		if (!IsActive())
		{
			cout << "The Create function must be called before this one!" << endl;
			return false;
		}

		if (otherSSRC == GetLocalSSRC())
		{
			cout << "Can't use own SSRC as other SSRC value" << endl;
			return false;
		}

		if (key.length() != 30)
		{
			cout << "Key length must be 30";
			return false;
		}

		int status = InitializeSRTPContext();
		if (status < 0)
		{
			int srtpErr = GetLastLibSRTPError();
			if (srtpErr < 0)
				cout << "libsrtp error: " << srtpErr << endl;
			checkerror(status);
		}

		srtp_policy_t policyIn, policyOut;

		memset(&policyIn, 0, sizeof(srtp_policy_t));
		memset(&policyOut, 0, sizeof(srtp_policy_t));
		
		crypto_policy_set_rtp_default(&policyIn.rtp);
		crypto_policy_set_rtcp_default(&policyIn.rtcp);

		crypto_policy_set_rtp_default(&policyOut.rtp);
		crypto_policy_set_rtcp_default(&policyOut.rtcp);

		policyIn.ssrc.type = ssrc_specific;
		policyIn.ssrc.value = otherSSRC;
		policyIn.key = (unsigned char *)key.c_str();
		policyIn.next = 0;

		policyOut.ssrc.type = ssrc_specific;
		policyOut.ssrc.value = GetLocalSSRC();
		policyOut.key = (unsigned char *)key.c_str();
		policyOut.next = 0;

		srtp_t ctx = LockSRTPContext();
		if (ctx == 0)
		{
			cout << "Unable to get/lock srtp context" << endl;
			return false;
		}
		err_status_t err = srtp_add_stream(ctx, &policyIn);
		if (err == err_status_ok)
			err = srtp_add_stream(ctx, &policyOut);
		UnlockSRTPContext();

		if (err != err_status_ok)
		{
			cout << "libsrtp error while adding stream: " << err << endl;
			return false;
		}
		return true;
	}
protected:
	void OnValidatedRTPPacket(RTPSourceData *srcdat, RTPPacket *rtppack, bool isonprobation, bool *ispackethandled)
	{
		printf("SSRC %x Got packet in OnValidatedRTPPacket from source 0x%04x!\n", GetLocalSSRC(), srcdat->GetSSRC());
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
		printf("SSRC %d Received SDES item (%d): %s from SSRC\n", GetLocalSSRC(), (int)t, msg, srcdat->GetSSRC());
	}

	void OnErrorChangeIncomingData(int errcode, int libsrtperrorcode) 
	{
		cout << "JRTPLIB Error: " << RTPGetErrorString(errcode) << endl;
		if (libsrtperrorcode != err_status_ok)
			cout << "libsrtp error: " << libsrtperrorcode << endl;
		cout << endl;
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
	transparams.SetRTCPMultiplexing(true); 

	transparams.SetPortbase(portbase1);
	status = sender.Create(sessparams,&transparams);	
	checkerror(status);
	status = sender.AddDestination(RTPIPv4Address(destip, portbase2, true));
	checkerror(status);

	transparams.SetPortbase(portbase2);
	status = receiver.Create(sessparams,&transparams);	
	checkerror(status);
	status = receiver.AddDestination(RTPIPv4Address(destip, portbase1, true));
	checkerror(status);
	
	status = sender.Init("012345678901234567890123456789", receiver.GetLocalSSRC());
	checkerror(status);
	status = receiver.Init("012345678901234567890123456789", sender.GetLocalSSRC());
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
