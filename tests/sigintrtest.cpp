#include "rtpconfig.h"

// Let's assume that non-winsock platforms have SIGALRM and EINTR
#if !defined(RTP_SOCKETTYPE_WINSOCK) && defined(RTP_SUPPORT_THREAD)

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
#include <jthread/jthread.h>

#include <signal.h>
#include <unistd.h>

using namespace jrtplib;
using namespace jthread;
using namespace std;

void checkerror(int rtperr)
{
	if (rtperr < 0)
	{
		cout << "ERROR: " << RTPGetErrorString(rtperr) << endl;
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
		printf("SSRC %x: Received SDES item (%d): %s", (unsigned int)srcdat->GetSSRC(), (int)t, msg);
	}
};

class MyThread : public JThread
{
public:
	MyThread() 
	{ 
		if (m_mutex.Init() < 0)
		{
			cerr << "Can't init mutex" << endl;
			exit(-1);
		}
		m_stop = false; 
	}

	~MyThread()
	{
		m_mutex.Lock();
		m_stop = true;
		m_mutex.Unlock();

		while (IsRunning())
			RTPTime::Wait(RTPTime(0, 10000));
	}

	void *Thread()
	{
		ThreadStarted();

		bool done = false;
		pid_t pid = getpid();

		while (!done)
		{
			kill(pid, SIGALRM);

			RTPTime::Wait(RTPTime(0, 500000));

			m_mutex.Lock();
			done = m_stop;
			m_mutex.Unlock();
		}

		return 0;
	}

private:
	JMutex m_mutex;
	bool m_stop;
};

void handler(int sig)
{
	cerr << "Signal " << sig << endl;
}

int main(void)
{
	if (signal(SIGALRM, handler) == SIG_ERR)
	{
		cerr << "Unable to install signal handler" << endl;
		return -1;
	}

	MyThread t;

	if (t.Start() < 0)
	{
		cerr << "Unable to start thread that sends signals" << endl;
		return -1;
	}

	cerr << "Waiting one second to verify that handler is called and program doesn't exit yet" << endl;
	RTPTime::Wait(RTPTime(1,0));

	MyRTPSession sess;
	uint16_t portbase = 5000, destport = 5000;
	uint32_t destip;
	string ipstr = "127.0.0.1";
	int status,i,num;

	destip = inet_addr(ipstr.c_str());
	if (destip == INADDR_NONE)
	{
		cerr << "Bad IP address specified" << endl;
		return -1;
	}
	destip = ntohl(destip);
	
	num = 20;

	// Now, we'll create a RTP session, set the destination, send some
	// packets and poll for incoming data.
	
	RTPUDPv4TransmissionParams transparams;
	RTPSessionParams sessparams;
	
	sessparams.SetOwnTimestampUnit(1.0/10.0);		
	sessparams.SetAcceptOwnPackets(true);
	sessparams.SetUsePollThread(false);
	transparams.SetPortbase(portbase);

	status = sess.Create(sessparams,&transparams);	
	checkerror(status);
	
	RTPIPv4Address addr(destip,destport);
	
	status = sess.AddDestination(addr);
	checkerror(status);
	
	for (i = 1 ; i <= num ; i++)
	{
		printf("\nSending packet %d/%d\n",i,num);
		
		// send the packet
		status = sess.SendPacket((void *)"1234567890",10,0,false,10);
		checkerror(status);

		// Either the background thread or the poll function itself will
		// cause the OnValidatedRTPPacket and OnRTCPSDESItem functions to
		// be called, so in this loop there's not much left to do. 
		
		status = sess.Poll();
		checkerror(status);
		
		RTPTime startTime = RTPTime::CurrentTime();
		RTPTime diff(0,0);
		do
		{
			status = sess.WaitForIncomingData(RTPTime(0,100));
			checkerror(status);

			diff = RTPTime::CurrentTime();
			diff -= startTime;
		} while (diff.GetDouble() < 1.0); // Make sure we wait a second
	}
	
	return 0;
}

#else 
#include <iostream>

using namespace std;

int main(void)
{
	cerr << "Need JThread support and a unix-like platform for this test" << endl;
	return 0;
}

#endif 

