/*

  This file is a part of JRTPLIB
  Copyright (c) 1999-2004 Jori Liesenborgs

  Contact: jori@lumumba.luc.ac.be

  This library was developed at the "Expertisecentrum Digitale Media"
  (http://www.edm.luc.ac.be), a research center of the "Limburgs Universitair
  Centrum" (http://www.luc.ac.be). The library is based upon work done for 
  my thesis at the School for Knowledge Technology (Belgium/The Netherlands).

  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.

*/

#include "rtppollthread.h"

#ifdef RTP_SUPPORT_THREAD

#include "rtpsession.h"
#include "rtcpscheduler.h"
#include "rtperrors.h"
#include "rtprawpacket.h"
#include <time.h>
#include <iostream>

#include "rtpdebug.h"

RTPPollThread::RTPPollThread(RTPSession &session,RTCPScheduler &sched):rtpsession(session),rtcpsched(sched)
{
	stop = false;
	transmitter = 0;
}

RTPPollThread::~RTPPollThread()
{
	Stop();
}
 
int RTPPollThread::Start(RTPTransmitter *trans)
{
	if (JThread::IsRunning())
		return ERR_RTP_POLLTHREAD_ALREADYRUNNING;
	
	transmitter = trans;
	if (!stopmutex.IsInitialized())
	{
		if (stopmutex.Init() < 0)
			return ERR_RTP_POLLTHREAD_CANTINITMUTEX;
	}
	stop = false;
	if (JThread::Start() < 0)
		return ERR_RTP_POLLTHREAD_CANTSTARTTHREAD;
	return 0;
}

void RTPPollThread::Stop()
{
	time_t thetime;
	
	if (!IsRunning())
		return;
	
	stopmutex.Lock();
	stop = true;
	stopmutex.Unlock();
	
	if (transmitter)
		transmitter->AbortWait();
	
	thetime = time(0);
	while (JThread::IsRunning() && (time(0) - thetime) < 5) // wait max 5 sec.
		;
	if (JThread::IsRunning())
	{
		std::cerr << "RTPPollThread: Warning! Having to kill thread!" << std::endl;
		JThread::Kill();
	}
	stop = false;
	transmitter = 0;
}

void *RTPPollThread::Thread()
{
	JThread::ThreadStarted();
	
	bool stopthread;

	stopmutex.Lock();
	stopthread = stop;
	stopmutex.Unlock();
	while (!stopthread)
	{
		int status;

		rtpsession.schedmutex.Lock();
		rtpsession.sourcesmutex.Lock();
		
		RTPTime rtcpdelay = rtcpsched.GetTransmissionDelay();
		
		rtpsession.sourcesmutex.Unlock();
		rtpsession.schedmutex.Unlock();
		
		if ((status = transmitter->WaitForIncomingData(rtcpdelay)) < 0)
		{
			stopthread = true;
			rtpsession.OnPollThreadError(status);
		}
		else
		{
			if ((status = transmitter->Poll()) < 0)
			{
				stopthread = true;
				rtpsession.OnPollThreadError(status);
			}
			else
			{
				if ((status = rtpsession.ProcessPolledData()) < 0)
				{
					stopthread = true;
					rtpsession.OnPollThreadError(status);
				}
				else
				{
					rtpsession.OnPollThreadStep();
					stopmutex.Lock();
					stopthread = stop;
					stopmutex.Unlock();
				}
			}
		}
	}
	return 0;
}

#endif // RTP_SUPPORT_THREAD

