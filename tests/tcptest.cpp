#include "rtpconfig.h"
#include "rtpsocketutil.h"
#include "rtpsocketutilinternal.h"
#include "rtpsession.h"
#include "rtpsessionparams.h"
#include "rtperrors.h"
#include "rtpsourcedata.h"
#include "rtptcpaddress.h"
#include "rtptcptransmitter.h"
#include "rtppacket.h"
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <vector>

using namespace std;
using namespace jrtplib;

inline void checkerror(int rtperr)
{
	if (rtperr < 0)
	{
		cerr << "ERROR: " << RTPGetErrorString(rtperr) << std::endl;
		exit(-1);
	}
}

class MyRTPSession : public RTPSession
{
public:
	MyRTPSession() : RTPSession() { }
	~MyRTPSession() { }
protected:
	void OnValidatedRTPPacket(RTPSourceData *srcdat, RTPPacket *rtppack, bool isonprobation, bool *ispackethandled)
	{
		printf("SSRC %x Got packet (%d bytes) in OnValidatedRTPPacket from source 0x%04x!\n", GetLocalSSRC(), 
			   (int)rtppack->GetPayloadLength(), srcdat->GetSSRC());
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
};

class MyTCPTransmitter : public RTPTCPTransmitter
{
public:
	MyTCPTransmitter(const string &name) : RTPTCPTransmitter(0), m_name(name) { }

	void OnSendError(SocketType sock)
	{
		cout << m_name << ": Error sending over socket " << sock << ", removing destination" << endl;
		DeleteDestination(RTPTCPAddress(sock));
	}
	
	void OnReceiveError(SocketType sock)
	{
		cout << m_name << ": Error receiving from socket " << sock << ", removing destination" << endl;
		DeleteDestination(RTPTCPAddress(sock));
	}
private:
	string m_name;
};

void runTest(int sock1, int sock2)
{
	const int packSize = 45678;
	RTPSessionParams sessParams;
	MyTCPTransmitter trans1("Transmitter1"), trans2("Transmitter2");
	MyRTPSession sess1, sess2;

	sessParams.SetProbationType(RTPSources::NoProbation);
	sessParams.SetOwnTimestampUnit(1.0/packSize);
	sessParams.SetMaximumPacketSize(packSize + 64); // some extra room for rtp header

	bool threadsafe = false;
#ifdef RTP_SUPPORT_THREAD
	threadsafe = true;
#endif // RTP_SUPPORT_THREAD

	checkerror(trans1.Init(threadsafe));
	checkerror(trans2.Init(threadsafe));
	checkerror(trans1.Create(65535, 0));
	checkerror(trans2.Create(65535, 0));

	checkerror(sess1.Create(sessParams, &trans1));
	cout << "Session 1 created " << endl;
	checkerror(sess2.Create(sessParams, &trans2));
	cout << "Session 2 created " << endl;

	checkerror(sess1.AddDestination(RTPTCPAddress(sock1)));
	checkerror(sess2.AddDestination(RTPTCPAddress(sock2)));

	vector<uint8_t> pack(packSize);

	int num = 20;
	for (int i = 1 ; i <= num ; i++)
	{
		printf("\nSending packet %d/%d\n",i,num);
		
		// send a packet, alternating between a large packet and a zero length one
		int len = (i%2 == 0)?pack.size():0;
		checkerror(sess1.SendPacket((void *)&pack[0],len,0,false,10));

		if (i == 10)
			RTPCLOSE(sock1); // Induce an error when sending/receiving

		// Either the background thread or the poll function itself will
		// cause the OnValidatedRTPPacket and OnRTCPSDESItem functions to
		// be called, so in this loop there's not much left to do. 
		
#ifndef RTP_SUPPORT_THREAD
		checkerror(sess1.Poll());
#endif // RTP_SUPPORT_THREAD
		
		RTPTime::Wait(RTPTime(1,0));
	}
}

int main(int argc, char *argv[])
{
#ifdef RTP_SOCKETTYPE_WINSOCK
	WSADATA dat;
	WSAStartup(MAKEWORD(2,2),&dat);
#endif

	if (argc != 2)
	{
		cerr << "Usage: " << argv[0] << " portnumber" << endl;
		return -1;
	}

	// Create a listener socket and listen on it
	SocketType listener = socket(AF_INET, SOCK_STREAM, 0);
	if (listener == RTPSOCKERR)
	{
		cerr << "Can't create listener socket" << endl;
		return -1;
	}

	struct sockaddr_in servAddr;

	memset(&servAddr, 0, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(atoi(argv[1]));

	if (bind(listener, (struct sockaddr *)&servAddr, sizeof(servAddr)) != 0)
	{
		cerr << "Can't bind listener socket" << endl;
		return -1;
	}

	listen(listener, 1);

	// Create a client socket and connect to the listener
	SocketType client = socket(AF_INET, SOCK_STREAM, 0);
	if (client == RTPSOCKERR)
	{
		cerr << "Can't create client socket" << endl;
		return -1;
	}

	servAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
	if (connect(client, (struct sockaddr *)&servAddr, sizeof(servAddr)) != 0)
	{
		cerr << "Can't connect to the listener socket" << endl;
		return -1;
	}

	SocketType server = accept(listener, 0, 0);
	if (server == RTPSOCKERR)
	{
		cerr << "Can't accept incoming connection" << endl;
		return -1;
	}
	RTPCLOSE(listener);

	cout << "Got connected socket pair" << endl;

	runTest(server, client);

	cout << "Done." << endl;
	RTPCLOSE(server);
	RTPCLOSE(client);
#ifdef RTP_SOCKETTYPE_WINSOCK
	WSACleanup();
#endif
	return 0;
}
