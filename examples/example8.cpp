/* 
  This is an example of how you can create a single poll thread for
  multiple transmitters and sessions, using a single pre-created
  RTPAbortDescriptors instance
*/

#include "rtpconfig.h"
#include <iostream>

using namespace std;

#ifdef RTP_SUPPORT_THREAD

#include "rtpsession.h"
#include "rtpudpv4transmitter.h"
#include "rtpipv4address.h"
#include "rtpsessionparams.h"
#include "rtperrors.h"
#include "rtplibraryversion.h"
#include "rtpsourcedata.h"
#include "rtpabortdescriptors.h"
#include <jthread/jthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <vector>

using namespace jrtplib;
using namespace jthread;

void checkerror(int rtperr)
{
	if (rtperr < 0)
	{
		cerr << "ERROR: " << RTPGetErrorString(rtperr) << std::endl;
		exit(-1);
	}
}

class MyRTPSession : public RTPSession
{
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
		printf("SSRC %x Received SDES item (%d): %s from SSRC %x\n", GetLocalSSRC(), (int)t, msg, srcdat->GetSSRC());
	}
};

class MyPollThread : public JThread
{
public:
	MyPollThread(const vector<SocketType> &sockets, const vector<RTPSession *> &sessions)
		: m_sockets(sockets), m_sessions(sessions)
	{
		if (m_mutex.Init() < 0)
		{
			cerr << "ERROR: unable to initialize mutex" << endl;
			exit(-1);
		}
		m_stop = false;
	}

	void SignalStop()
	{
		m_mutex.Lock();
		m_stop = true;
		m_mutex.Unlock();
	}
private:
	void *Thread()
	{
		JThread::ThreadStarted();

		bool done = false;
		m_mutex.Lock();
		done = m_stop;
		m_mutex.Unlock();

		while (!done)
		{
			fd_set fdset;
			FD_ZERO(&fdset);
			for (int i = 0 ; i < m_sockets.size() ; i++)
				FD_SET(m_sockets[i], &fdset);

			double minInt = 10.0; // wait at most 10 secs
			for (int i = 0 ; i < m_sessions.size() ; i++)
			{
				double nextInt = m_sessions[i]->GetRTCPDelay().GetDouble();

				if (nextInt > 0 && nextInt < minInt)
					minInt = nextInt;
			}

			RTPTime waitTime(minInt);
			//cout << "Waiting at most " << minInt << " seconds in select" << endl;
			struct timeval tv = { waitTime.GetSeconds(), waitTime.GetMicroSeconds() };
			if (select(FD_SETSIZE, &fdset, 0, 0, &tv) < 0)
			{
				cerr << "ERROR: error in select" << endl;
				exit(-1);
			}

			for (int i = 0 ; i < m_sessions.size() ; i++)
				m_sessions[i]->Poll();

			m_mutex.Lock();
			done = m_stop;
			m_mutex.Unlock();
		}

		return 0;
	}

	JMutex m_mutex;
	bool m_stop;
	vector<SocketType> m_sockets;
	vector<RTPSession *> m_sessions;
};

int main(void)
{
#ifdef RTP_SOCKETTYPE_WINSOCK
	WSADATA dat;
	WSAStartup(MAKEWORD(2,2),&dat);
#endif // RTP_SOCKETTYPE_WINSOCK

	RTPAbortDescriptors abortDesc;
	vector<SocketType> pollSockets;

	checkerror(abortDesc.Init());
	pollSockets.push_back(abortDesc.GetAbortSocket());

	int numTrans = 5;
	int portbaseBase = 6000;
	vector<RTPUDPv4Transmitter *> transmitters;
	for (int i = 0 ; i < numTrans ; i++)
	{
		RTPUDPv4TransmissionParams transParams;
		transParams.SetPortbase(portbaseBase + i*2);
		transParams.SetCreatedAbortDescriptors(&abortDesc);
		
		RTPUDPv4Transmitter *pTrans = new RTPUDPv4Transmitter(0);
		checkerror(pTrans->Init(true)); // We'll need thread safety
		checkerror(pTrans->Create(64000, &transParams));

		transmitters.push_back(pTrans);
	}

	vector<uint16_t> portBases;
	vector<RTPSession *> sessions;

	for (int i = 0 ; i < transmitters.size() ; i++)
	{
		RTPUDPv4Transmitter *pTrans = transmitters[i];
		RTPUDPv4TransmissionInfo *pInfo = static_cast<RTPUDPv4TransmissionInfo *>(pTrans->GetTransmissionInfo());

		pollSockets.push_back(pInfo->GetRTPSocket());
		pollSockets.push_back(pInfo->GetRTCPSocket());
		portBases.push_back(pInfo->GetRTPPort());

		pTrans->DeleteTransmissionInfo(pInfo);

		RTPSession *pSess = new MyRTPSession();
		RTPSessionParams sessParams;

		// We're going to use our own poll thread!
		// IMPORTANT: don't use a single external RTPAbortDescriptors instance
		//            with the internal poll thread! It will cause threads to
		//            hang!
		sessParams.SetUsePollThread(false); 
		sessParams.SetOwnTimestampUnit(1.0/8000.0);
		checkerror(pSess->Create(sessParams, pTrans));

		sessions.push_back(pSess);
	}

	// Let each session send to the next
	uint8_t localHost[4] = { 127, 0, 0, 1 };
	for (int i = 0 ; i < sessions.size() ; i++)
	{
		uint16_t destPortbase = portBases[(i+1)%portBases.size()];
		checkerror(sessions[i]->AddDestination(RTPIPv4Address(localHost, destPortbase)));
	}

	MyPollThread myPollThread(pollSockets, sessions);
	if (myPollThread.Start() < 0)
	{
		cerr << "ERROR: couldn't start own poll thread" << endl;
		return -1;
	}

	cout << "Own poll thread started" << endl;

	cout << "Main thread will sleep for 30 seconds (the sessions should still send RTCP packets to each other)" << endl;
	RTPTime::Wait(RTPTime(30.0));

	myPollThread.SignalStop(); // will cause the thread to end after an iteration
	while (myPollThread.IsRunning())
	{
		abortDesc.SendAbortSignal(); // will make sure the thread isn't waiting for incoming data
		RTPTime::Wait(RTPTime(0.01));
	}

	cout << "Own poll thread ended, cleaning up..." << endl;

	for (int i = 0 ; i < sessions.size() ; i++)
		delete sessions[i];
	for (int i = 0 ; i < transmitters.size() ; i++)
		delete transmitters[i];
#ifdef RTP_SOCKETTYPE_WINSOCK
	WSACleanup();
#endif // RTP_SOCKETTYPE_WINSOCK
	cout << "Done" << endl;
	return 0;
}

#else

int main(void)
{
	cout << "Thread support was not enabled at compile time" << endl;
	return 0;
}

#endif // RTP_SUPPORT_THREAD
