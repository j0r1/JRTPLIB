/*

  This file is a part of JRTPLIB
  Copyright (c) 1999-2016 Jori Liesenborgs

  Contact: jori.liesenborgs@gmail.com

  This library was developed at the Expertise Centre for Digital Media
  (http://www.edm.uhasselt.be), a research center of the Hasselt University
  (http://www.uhasselt.be). The library is based upon work done for 
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

/**
 * \file rtptimeutilities.h
 */

#ifndef RTPTIMEUTILITIES_H

#define RTPTIMEUTILITIES_H

#include "rtpconfig.h"
#include "rtptypes.h"
#ifndef RTP_HAVE_QUERYPERFORMANCECOUNTER
	#include <sys/time.h>
	#include <time.h>
#endif // RTP_HAVE_QUERYPERFORMANCECOUNTER

#define RTP_NTPTIMEOFFSET									2208988800UL

#ifdef RTP_HAVE_VSUINT64SUFFIX
#define C1000000 1000000ui64
#define CEPOCH 11644473600000000ui64
#else
#define C1000000 1000000ULL
#define CEPOCH 11644473600000000ULL
#endif // RTP_HAVE_VSUINT64SUFFIX

namespace jrtplib
{

/**
 * This is a simple wrapper for the most significant word (MSW) and least 
 * significant word (LSW) of an NTP timestamp.
 */
class JRTPLIB_IMPORTEXPORT RTPNTPTime
{
public:
	/** This constructor creates and instance with MSW \c m and LSW \c l. */
	RTPNTPTime(uint32_t m,uint32_t l)							{ msw = m ; lsw = l; }

	/** Returns the most significant word. */
	uint32_t GetMSW() const								{ return msw; }

	/** Returns the least significant word. */
	uint32_t GetLSW() const								{ return lsw; }
private:
	uint32_t msw,lsw;
};

/** This class is used to specify wallclock time, delay intervals etc.
 *  This class is used to specify wallclock time, delay intervals etc. 
 *  It stores a number of seconds and a number of microseconds.
 */
class JRTPLIB_IMPORTEXPORT RTPTime
{
public:
	/** Returns an RTPTime instance representing the current wallclock time. 
	 *  Returns an RTPTime instance representing the current wallclock time. This is expressed 
	 *  as a number of seconds since 00:00:00 UTC, January 1, 1970.
	 */
	static RTPTime CurrentTime();

	/** This function waits the amount of time specified in \c delay. */
	static void Wait(const RTPTime &delay);
		
	/** Creates an RTPTime instance representing \c t, which is expressed in units of seconds. */
	RTPTime(double t);

	/** Creates an instance that corresponds to \c ntptime. 
	 *  Creates an instance that corresponds to \c ntptime.  If
	 *  the conversion cannot be made, both the seconds and the
	 *  microseconds are set to zero.
	 */
	RTPTime(RTPNTPTime ntptime);

	/** Creates an instance corresponding to \c seconds and \c microseconds. */
	RTPTime(uint32_t seconds, uint32_t microseconds)						{ sec = seconds; microsec = microseconds; }

	/** Returns the number of seconds stored in this instance. */
	uint32_t GetSeconds() const										{ return sec; }

	/** Returns the number of microseconds stored in this instance. */
	uint32_t GetMicroSeconds() const								{ return microsec; }

	/** Returns the time stored in this instance, expressed in units of seconds. */
	double GetDouble() const 										{ return (((double)sec)+(((double)microsec)/1000000.0)); }

	/** Returns the NTP time corresponding to the time stored in this instance. */
	RTPNTPTime GetNTPTime() const;

	RTPTime &operator-=(const RTPTime &t);
	RTPTime &operator+=(const RTPTime &t);
	bool operator<(const RTPTime &t) const;
	bool operator>(const RTPTime &t) const;
	bool operator<=(const RTPTime &t) const;
	bool operator>=(const RTPTime &t) const;
private:
#ifdef RTP_HAVE_QUERYPERFORMANCECOUNTER
	static inline uint64_t CalculateMicroseconds(uint64_t performancecount,uint64_t performancefrequency);
#endif // RTP_HAVE_QUERYPERFORMANCECOUNTER

	uint32_t sec,microsec;
};

inline RTPTime::RTPTime(double t)
{
	sec = (uint32_t)t;

	double t2 = t-((double)sec);
	t2 *= 1000000.0;
	microsec = (uint32_t)t2;
}

inline RTPTime::RTPTime(RTPNTPTime ntptime)
{
	if (ntptime.GetMSW() < RTP_NTPTIMEOFFSET)
	{
		sec = 0;
		microsec = 0;
	}
	else
	{
		sec = ntptime.GetMSW() - RTP_NTPTIMEOFFSET;
		
		double x = (double)ntptime.GetLSW();
		x /= (65536.0*65536.0);
		x *= 1000000.0;
		microsec = (uint32_t)x;
	}
}

#ifdef RTP_HAVE_QUERYPERFORMANCECOUNTER

inline uint64_t RTPTime::CalculateMicroseconds(uint64_t performancecount,uint64_t performancefrequency)
{
	uint64_t f = performancefrequency;
	uint64_t a = performancecount;
	uint64_t b = a/f;
	uint64_t c = a%f; // a = b*f+c => (a*1000000)/f = b*1000000+(c*1000000)/f

	return b*C1000000+(c*C1000000)/f;
}

inline RTPTime RTPTime::CurrentTime()
{
	static int inited = 0;
	static uint64_t microseconds, initmicroseconds;
	static LARGE_INTEGER performancefrequency;

	uint64_t emulate_microseconds, microdiff;
	SYSTEMTIME systemtime;
	FILETIME filetime;

	LARGE_INTEGER performancecount;

	QueryPerformanceCounter(&performancecount);
    
	if(!inited){
		inited = 1;
		QueryPerformanceFrequency(&performancefrequency);
		GetSystemTime(&systemtime);
		SystemTimeToFileTime(&systemtime,&filetime);
		microseconds = ( ((uint64_t)(filetime.dwHighDateTime) << 32) + (uint64_t)(filetime.dwLowDateTime) ) / (uint64_t)10;
		microseconds-= CEPOCH; // EPOCH
		initmicroseconds = CalculateMicroseconds(performancecount.QuadPart, performancefrequency.QuadPart);
	}
    
	emulate_microseconds = CalculateMicroseconds(performancecount.QuadPart, performancefrequency.QuadPart);

	microdiff = emulate_microseconds - initmicroseconds;

	return RTPTime((uint32_t)((microseconds + microdiff) / C1000000),((uint32_t)((microseconds + microdiff) % C1000000)));
}

inline void RTPTime::Wait(const RTPTime &delay)
{
	DWORD t;

	t = ((DWORD)delay.GetSeconds())*1000+(((DWORD)delay.GetMicroSeconds())/1000);
	Sleep(t);
}

class JRTPLIB_IMPORTEXPORT RTPTimeInitializer
{
public:
	RTPTimeInitializer();
	void Dummy() { dummy++; }
private:
	int dummy;
};

extern RTPTimeInitializer timeinit;

#else // unix style

inline RTPTime RTPTime::CurrentTime()
{
	struct timeval tv;
	
	gettimeofday(&tv,0);
	return RTPTime((uint32_t)tv.tv_sec,(uint32_t)tv.tv_usec);
}

inline void RTPTime::Wait(const RTPTime &delay)
{
	struct timespec req,rem;

	req.tv_sec = (time_t)delay.sec;
	req.tv_nsec = ((long)delay.microsec)*1000;
	nanosleep(&req,&rem);
}

#endif // RTP_HAVE_QUERYPERFORMANCECOUNTER

inline RTPTime &RTPTime::operator-=(const RTPTime &t)
{ 
	sec -= t.sec; 
	if (t.microsec > microsec)
	{
		sec--;
		microsec += 1000000;
	}
	microsec -= t.microsec;
	return *this;
}

inline RTPTime &RTPTime::operator+=(const RTPTime &t)
{ 
	sec += t.sec; 
	microsec += t.microsec;
	if (microsec >= 1000000)
	{
		sec++;
		microsec -= 1000000;
	}
	return *this;
}

inline RTPNTPTime RTPTime::GetNTPTime() const
{
	uint32_t msw = sec+RTP_NTPTIMEOFFSET;
	uint32_t lsw;
	double x;
	
      	x = microsec/1000000.0;
	x *= (65536.0*65536.0);
	lsw = (uint32_t)x;

	return RTPNTPTime(msw,lsw);
}

inline bool RTPTime::operator<(const RTPTime &t) const
{
	if (sec < t.sec)
		return true;
	if (sec > t.sec)
		return false;
	if (microsec < t.microsec)
		return true;
	return false;
}

inline bool RTPTime::operator>(const RTPTime &t) const
{
	if (sec > t.sec)
		return true;
	if (sec < t.sec)
		return false;
	if (microsec > t.microsec)
		return true;
	return false;
}

inline bool RTPTime::operator<=(const RTPTime &t) const
{
	if (sec < t.sec)
		return true;
	if (sec > t.sec)
		return false;
	if (microsec <= t.microsec)
		return true;
	return false;
}

inline bool RTPTime::operator>=(const RTPTime &t) const
{
	if (sec > t.sec)
		return true;
	if (sec < t.sec)
		return false;
	if (microsec >= t.microsec)
		return true;
	return false;
}

} // end namespace

#endif // RTPTIMEUTILITIES_H

