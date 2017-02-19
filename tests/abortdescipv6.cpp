/* 
  This is an example of how you can create a single poll thread for
  multiple transmitters and sessions, using a single pre-created
  RTPAbortDescriptors instance
*/

#include "rtpconfig.h"
#include <iostream>

using namespace std;

#if defined(RTP_SUPPORT_THREAD) && defined(RTP_SUPPORT_IPV6)

#include "rtpsession.h"
#include "rtpudpv6transmitter.h"
#include "rtpipv6address.h"
#include "rtpsessionparams.h"
#include "rtperrors.h"
#include "rtplibraryversion.h"
#include "rtpsourcedata.h"
#include "rtpabortdescriptors.h"
#include "rtpselect.h"
#include "rtprandom.h"
#include <jthread/jthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string>
#include <vector>

using namespace jrtplib;
using namespace jthread;

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
	MyRTPSession(RTPRandom *rnd) : RTPSession(rnd) { }
	~MyRTPSession() { }
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

		vector<int8_t> flags(m_sockets.size());
		bool done = false;
		m_mutex.Lock();
		done = m_stop;
		m_mutex.Unlock();

		while (!done)
		{
			double minInt = 10.0; // wait at most 10 secs
			for (int i = 0 ; i < m_sessions.size() ; i++)
			{
				double nextInt = m_sessions[i]->GetRTCPDelay().GetDouble();

				if (nextInt > 0 && nextInt < minInt)
					minInt = nextInt;
				else if (nextInt <= 0) // call the Poll function to make sure that RTCP packets are sent
				{
					//cout << "RTCP packet should be sent, calling Poll" << endl;
					m_sessions[i]->Poll();
				}
			}

			RTPTime waitTime(minInt);
			//cout << "Waiting at most " << minInt << " seconds in select" << endl;

			int status = RTPSelect(&m_sockets[0], &flags[0], m_sockets.size(), waitTime);
			checkerror(status);

			if (status > 0) // some descriptors were set
			{
				for (int i = 0 ; i < m_sockets.size() ; i++)
				{
					if (flags[i])
					{
						int idx = i/2; // two sockets per session
						if (idx < m_sessions.size())
							m_sessions[idx]->Poll(); 
					}
				}
			}

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

	int numTrans = 5;
	int portbaseBase = 6000;
	vector<RTPUDPv6Transmitter *> transmitters;
	for (int i = 0 ; i < numTrans ; i++)
	{
		RTPUDPv6TransmissionParams transParams;
		transParams.SetPortbase(portbaseBase + i*2);
		transParams.SetCreatedAbortDescriptors(&abortDesc);
		
		RTPUDPv6Transmitter *pTrans = new RTPUDPv6Transmitter(0);
		checkerror(pTrans->Init(true)); // We'll need thread safety
		checkerror(pTrans->Create(64000, &transParams));

		transmitters.push_back(pTrans);
	}

	vector<uint16_t> portBases;
	vector<RTPSession *> sessions;

	RTPRandom *rnd = RTPRandom::CreateDefaultRandomNumberGenerator();

	for (int i = 0 ; i < transmitters.size() ; i++)
	{
		RTPUDPv6Transmitter *pTrans = transmitters[i];
		RTPUDPv6TransmissionInfo *pInfo = static_cast<RTPUDPv6TransmissionInfo *>(pTrans->GetTransmissionInfo());

		pollSockets.push_back(pInfo->GetRTPSocket());
		pollSockets.push_back(pInfo->GetRTCPSocket());
		portBases.push_back(pInfo->GetRTPPort());

		pTrans->DeleteTransmissionInfo(pInfo);

		RTPSession *pSess = new MyRTPSession(rnd); // make them all use the same random number generator
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

	// First, the pollSockets array will contain two sockets per session,
	// and an extra entry will be added for the abort socket
	pollSockets.push_back(abortDesc.GetAbortSocket());

	// Let each session send to the next
	uint8_t localHost[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1 };
	for (int i = 0 ; i < sessions.size() ; i++)
	{
		uint16_t destPortbase = portBases[(i+1)%portBases.size()];
		checkerror(sessions[i]->AddDestination(RTPIPv6Address(localHost, destPortbase)));
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

	delete rnd;
#ifdef RTP_SOCKETTYPE_WINSOCK
	WSACleanup();
#endif // RTP_SOCKETTYPE_WINSOCK
	cout << "Done" << endl;
	return 0;
}

#else

int main(void)
{
	cout << "Thread support or IPv6 support was not enabled at compile time" << endl;
	return 0;
}

#endif // RTP_SUPPORT_THREAD && RTP_SUPPORT_IPV6
