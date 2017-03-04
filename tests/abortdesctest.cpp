#include "rtpconfig.h"
#include "rtpabortdescriptors.h"
#include "rtperrors.h"
#include "rtpsocketutilinternal.h"
#include "rtptimeutilities.h"
#include <stdlib.h>
#include <iostream>

using namespace std;
using namespace jrtplib;

#ifdef RTP_SUPPORT_THREAD

#include <jthread/jthread.h>

using namespace jthread;

void checkError(int status)
{
	if (status >= 0)
		return;

	cerr << "ERROR: " << RTPGetErrorString(status) << endl;
	exit(-1);
}

class SignalThread : public JThread
{
public:
	SignalThread(RTPAbortDescriptors &a, double delay, int sigCount) : m_ad(a), m_delay(delay), m_sigCount(sigCount)
	{
	}

	~SignalThread()
	{
		while (IsRunning())
		{
			cout << "Waiting for thread to finish" << endl;
			RTPTime::Wait(RTPTime(1.0));
		}
	}
private:
	void *Thread()
	{
		JThread::ThreadStarted();

		cout << "Thread started, waiting " << m_delay << " seconds before sending abort signal" << endl;
		RTPTime::Wait(RTPTime(m_delay));
		cout << "Sending " << m_sigCount << " abort signals" << endl;
		for (int i = 0 ; i < m_sigCount ; i++)
			m_ad.SendAbortSignal();

		cout << "Thread finished" << endl;
		return 0;
	}

	RTPAbortDescriptors &m_ad;
	const double m_delay;
	const int m_sigCount;
};

void test1()
{
	RTPAbortDescriptors ad;
	SignalThread st(ad, 5, 1);

	st.Start();
	checkError(ad.Init());
	
	fd_set fdset;
	FD_ZERO(&fdset);
	FD_SET(ad.GetAbortSocket(), &fdset);
	
	if (select(FD_SETSIZE, &fdset, 0, 0, 0) < 0)
	{
		cerr << "Error in select" << endl;
		exit(-1);
	}

	cout << "Checking if select keeps triggering" << endl;
	int count = 0;
	RTPTime start = RTPTime::CurrentTime();
	while (count < 5)
	{
		count++;
		struct timeval tv = { 1, 0 };
		if (select(FD_SETSIZE, &fdset, 0, 0, &tv) < 0)
		{
			cerr << "Error in select" << endl;
			exit(-1);
		}
	}
	RTPTime delay = RTPTime::CurrentTime();
	delay -= start;

	cout << "Elapsed time is " << delay.GetDouble();
	if (delay.GetDouble() < 1.0)
		cout << ", seems OK" << endl;
	else
		cout << ", TAKES TOO LONG!" << endl;
}

void test2()
{
	RTPAbortDescriptors ad;
	SignalThread st(ad, 5, 5);

	st.Start();
	checkError(ad.Init());
	
	fd_set fdset;
	FD_ZERO(&fdset);
	FD_SET(ad.GetAbortSocket(), &fdset);
	
	if (select(FD_SETSIZE, &fdset, 0, 0, 0) < 0)
	{
		cerr << "Error in select" << endl;
		exit(-1);
	}
	cout << "Reading one signalling byte, should continue immediately since we've sent multiple signals" << endl;
	ad.ReadSignallingByte();
	if (select(FD_SETSIZE, &fdset, 0, 0, 0) < 0)
	{
		cerr << "Error in select" << endl;
		exit(-1);
	}

	cout << "Clearing abort signals" << endl;
	ad.ClearAbortSignal();

	cout << "Waiting for signal, should timeout after one second since we've cleared the signal buffers" << endl;
	struct timeval tv = { 1, 0 };
	RTPTime start = RTPTime::CurrentTime();
	if (select(FD_SETSIZE, &fdset, 0, 0, &tv) < 0)
	{
		cerr << "Error in select" << endl;
		exit(-1);
	}
	RTPTime delay = RTPTime::CurrentTime();
	delay -= start;

	cout << "Elapsed time is " << delay.GetDouble();
	if (delay.GetDouble() < 1.0)
		cout << ", BUFFER NOT CLEARED?" << endl;
	else
		cout << ", seems OK" << endl;
}

int main(void)
{
#ifdef RTP_SOCKETTYPE_WINSOCK
	WSADATA dat;
	WSAStartup(MAKEWORD(2, 2), &dat);
#endif // RTP_SOCKETTYPE_WINSOCK

	test1();
	test2();

#ifdef RTP_SOCKETTYPE_WINSOCK
	WSACleanup();
#endif // RTP_SOCKETTYPE_WINSOCK
	return 0;
}

#else

int main(void)
{
	cerr << "Thread support is needed for this example" << endl;
	return 0;
}

#endif // RTP_SUPPORT_THREAD

