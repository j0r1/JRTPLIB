#include "rtpconfig.h"
#include <iostream>

using namespace std;

#ifdef RTP_SUPPORT_SRTP

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
#include <srtp/srtp.h>
#include <string>

using namespace jrtplib;


void checkerror(int rtperr)
{
	if (rtperr < 0)
	{
		cerr << "ERROR: " << RTPGetErrorString(rtperr) << std::endl;
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
	bool Init(const std::string &key)
	{
		if (!IsActive())
		{
			cerr << "The Create function must be called before this one!" << endl;
			return false;
		}

		if (key.length() != 30)
		{
			cerr << "Key length must be 30";
			return false;
		}

		int status = InitializeSRTPContext();
		if (status < 0)
		{
			int srtpErr = GetLastLibSRTPError();
			if (srtpErr < 0)
				cerr << "libsrtp error: " << srtpErr << endl;
			checkerror(status);
		}

		srtp_policy_t policyIn, policyOut;

		memset(&policyIn, 0, sizeof(srtp_policy_t));
		memset(&policyOut, 0, sizeof(srtp_policy_t));
		
		crypto_policy_set_rtp_default(&policyIn.rtp);
		crypto_policy_set_rtcp_default(&policyIn.rtcp);

		crypto_policy_set_rtp_default(&policyOut.rtp);
		crypto_policy_set_rtcp_default(&policyOut.rtcp);

		policyIn.ssrc.type = ssrc_any_inbound;
		policyIn.key = (uint8_t *)key.c_str();
		policyIn.next = 0;

		policyOut.ssrc.type = ssrc_specific;
		policyOut.ssrc.value = GetLocalSSRC();
		policyOut.key = (uint8_t *)key.c_str();
		policyOut.next = 0;

		srtp_t ctx = LockSRTPContext();
		if (ctx == 0)
		{
			cerr << "Unable to get/lock srtp context" << endl;
			return false;
		}
		err_status_t err = srtp_add_stream(ctx, &policyIn);
		if (err == err_status_ok)
			err = srtp_add_stream(ctx, &policyOut);
		UnlockSRTPContext();

		if (err != err_status_ok)
		{
			cerr << "libsrtp error while adding stream: " << err << endl;
			return false;
		}
		return true;
	}
protected:
	bool OnChangeIncomingData(RTPRawPacket *rawpack)
	{
		char fileName[1024];

		bool isrtp = rawpack->IsRTP();
		sprintf(fileName, "%s-%x-in_enc.dat", (isrtp)?"rtp":"rtcp", GetLocalSSRC());
		FILE *pFile = fopen(fileName, "ab");

		if (pFile)
		{
			fwrite(rawpack->GetData(), 1, rawpack->GetDataLength(), pFile);
			fclose(pFile);
		}

		bool status = RTPSecureSession::OnChangeIncomingData(rawpack);

		sprintf(fileName, "%s-%x-in_dec.dat", (isrtp)?"rtp":"rtcp", GetLocalSSRC());
		pFile = fopen(fileName, "ab");

		if (pFile)
		{
			fwrite(rawpack->GetData(), 1, rawpack->GetDataLength(), pFile);
			fclose(pFile);
		}

		return status;
	}

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
		printf("SSRC %x Received SDES item (%d): %s from SSRC %x\n", GetLocalSSRC(), (int)t, msg, srcdat->GetSSRC());
	}

	void OnErrorChangeIncomingData(int errcode, int libsrtperrorcode) 
	{
		printf("SSRC %x JRTPLIB Error: %s\n", GetLocalSSRC(), RTPGetErrorString(errcode).c_str());
		if (libsrtperrorcode != err_status_ok)
			printf("libsrtp error: %d\n", libsrtperrorcode);
		printf("\n");
	}
};

int main(void)
{
#ifdef RTP_SOCKETTYPE_WINSOCK
	WSADATA dat;
	WSAStartup(MAKEWORD(2,2),&dat);
#endif // RTP_SOCKETTYPE_WINSOCK

	// Initialize the SRTP library
	srtp_init();
	
	MyRTPSession sender, receiver;
	int status;

	uint16_t portbase1 = 5000;
	uint16_t portbase2 = 5002;
	uint32_t destip = ntohl(inet_addr("127.0.0.1"));

	RTPUDPv4TransmissionParams transparams;
	RTPSessionParams sessparams;
	
	sessparams.SetOwnTimestampUnit(1.0/10.0);		
	transparams.SetRTCPMultiplexing(true); 

	transparams.SetPortbase(portbase1);
	sessparams.SetCNAME("sender@host");
	status = sender.Create(sessparams,&transparams);	
	checkerror(status);

	transparams.SetPortbase(portbase2);
	sessparams.SetCNAME("receiver@host");
	status = receiver.Create(sessparams,&transparams);	
	checkerror(status);

	printf("Sender is:   %x\n", sender.GetLocalSSRC());
	printf("Receiver is: %x\n\n", receiver.GetLocalSSRC());

	status = sender.AddDestination(RTPIPv4Address(destip, portbase2, true));
	checkerror(status);
	status = receiver.AddDestination(RTPIPv4Address(destip, portbase1, true));
	checkerror(status);
	
	string key = "012345678901234567890123456789";
	status = sender.Init(key);
	checkerror(status);
	status = receiver.Init(key);
	checkerror(status);

	const int num = 20;
	for (int i = 1 ; i <= num ; i++)
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

	// Make sure we shut the threads down before doing srtp_shutdown
	sender.Destroy();
	receiver.Destroy();
	
	srtp_shutdown();

#ifdef RTP_SOCKETTYPE_WINSOCK
	WSACleanup();
#endif // RTP_SOCKETTYPE_WINSOCK
	return 0;
}

#else

int main(void)
{
	cout << "SRTP support was not enabled at compile time" << endl;
	return 0;
}

#endif // RTP_SUPPORT_SRTP
