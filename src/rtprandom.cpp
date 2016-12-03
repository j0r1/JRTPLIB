/*

  This file is a part of JRTPLIB
  Copyright (c) 1999-2007 Jori Liesenborgs

  Contact: jori.liesenborgs@gmail.com

  This library was developed at the "Expertisecentrum Digitale Media"
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

#if defined(WIN32) && !defined(_WIN32_WCE)
	#define _CRT_RAND_S
#endif // WIN32 || _WIN32_WCE

#include "rtprandom.h"
#include <time.h>
#ifndef WIN32
	#include <unistd.h>
#else
	#ifndef _WIN32_WCE
		#include <process.h>
	#else
		#include <windows.h>
		#include <kfuncs.h>
	#endif // _WIN32_WINCE
	#include <stdlib.h>
#endif // WIN32

#include "rtpdebug.h"

#if (defined(WIN32) && !defined(_WIN32_WCE)) && (defined(_MSC_VER) && _MSC_VER >= 1400 )
	// If compiling on VC 8 or later for full Windows, we'll attempt to use rand_s,
	// which generates better random numbers.  However, its only supported on Windows XP,
	// Windows Server 2003, and later, so we'll do a run-time check to see if we can
	// use it (it won't work on Windows 2000 SP4 for example).
	#define RTP_SUPPORT_RANDS
	#define QUOTEPROCNAME(x) #x
	#ifndef RtlGenRandom
		#define RtlGenRandom    SystemFunction036
	#endif

	static bool checked_rand_s = false;
	static bool use_rand_s = false;
#endif

uint8_t GetRandom8_Default();
uint16_t GetRandom16_Default();
uint32_t GetRandom32_Default();
double GetRandomDouble_Default();

RTPRandom::RTPRandom()
{

#if defined(RTP_SUPPORT_GNUDRAND) || defined(RTP_SUPPORT_RANDR)
	uint32_t x;

	x = (uint32_t)getpid();
	x += (uint32_t)time(0);
	x -= (uint32_t)clock();
	x ^= (uint32_t)((uint8_t *)this - (uint8_t *)0);

#ifdef RTP_SUPPORT_GNUDRAND
	srand48_r(x,&drandbuffer);
#else
	state = (unsigned int)x;
#endif // RTP_SUPPORT_GNUDRAND
	
#else // use simple rand and srand functions (or maybe rand_s)

#ifdef RTP_SUPPORT_RANDS
	if(checked_rand_s == false)
	{
		checked_rand_s = true;
		use_rand_s = false;

		HMODULE hAdvApi32 = LoadLibrary("ADVAPI32.DLL");
		if(hAdvApi32 != NULL)
		{
			if(NULL != GetProcAddress( hAdvApi32, QUOTEPROCNAME( RtlGenRandom ) ))
			{
				use_rand_s = true;
			}
			FreeLibrary(hAdvApi32);
			hAdvApi32 = NULL;
		}
	}

	// Note: If we use the rand_s function, it does not require initialization of a seed.
	if(!use_rand_s)
	{
#endif

	uint32_t x;

#ifndef _WIN32_WCE
	x = (uint32_t)_getpid();
	x += (uint32_t)time(0);
	x -= (uint32_t)clock();
#else
	x = (uint32_t)GetCurrentProcessId();

	FILETIME ft;
	SYSTEMTIME st;
	
	GetSystemTime(&st);
	SystemTimeToFileTime(&st,&ft);
	
	x += ft.dwLowDateTime;
#endif // _WIN32_WCE
	x ^= (uint32_t)((uint8_t *)this - (uint8_t *)0);
	srand((unsigned int)x);

#ifdef RTP_SUPPORT_RANDS
	} // !support_rand_s
#endif

#endif // RTP_SUPPORT_GNUDRAND || RTP_SUPPORT_RANDR

}

RTPRandom::~RTPRandom()
{
}

#if defined(RTP_SUPPORT_GNUDRAND)

uint8_t RTPRandom::GetRandom8()
{
	double x;
	drand48_r(&drandbuffer,&x);
	uint8_t y = (uint8_t)(x*256.0);
	return y;
}

uint16_t RTPRandom::GetRandom16()
{
	double x;
	drand48_r(&drandbuffer,&x);
	uint16_t y = (uint16_t)(x*65536.0);
	return y;
}

uint32_t RTPRandom::GetRandom32()
{
	uint32_t a = GetRandom16();
	uint32_t b = GetRandom16();
	uint32_t y = (a << 16)|b;
	return y;
}

double RTPRandom::GetRandomDouble()
{
	double x;
	drand48_r(&drandbuffer,&x);
	return x;
}

#elif defined(RTP_SUPPORT_RANDR)

uint8_t RTPRandom::GetRandom8()
{
	uint8_t x;

	x = (uint8_t)(256.0*((double)rand_r(&state))/((double)RAND_MAX+1.0));
	return x;
}

uint16_t RTPRandom::GetRandom16()
{
	uint16_t x;

	x = (uint16_t)(65536.0*((double)rand_r(&state))/((double)RAND_MAX+1.0));
	return x;
}

uint32_t RTPRandom::GetRandom32()
{
	uint32_t x,y;

	x = (uint32_t)(65536.0*((double)rand_r(&state))/((double)RAND_MAX+1.0));
	y = x;
	x = (uint32_t)(65536.0*((double)rand_r(&state))/((double)RAND_MAX+1.0));
	y ^= (x<<8);
	x = (uint32_t)(65536.0*((double)rand_r(&state))/((double)RAND_MAX+1.0));
	y ^= (x<<16);

	return y;
}

double RTPRandom::GetRandomDouble()
{
	double x = ((double)rand_r(&state))/((double)RAND_MAX+1.0);
	return x;
}

#elif defined(RTP_SUPPORT_RANDS)

uint8_t RTPRandom::GetRandom8()
{
	if(use_rand_s)
	{
		uint8_t x;
		unsigned int r;

		rand_s(&r);
		x = (uint8_t)(256.0*((double)r)/((double)UINT_MAX+1.0));
		return x;
	}
	else
	{
		return GetRandom8_Default();
	}
}

uint16_t RTPRandom::GetRandom16()
{
	if(use_rand_s)
	{
		uint16_t x;
		unsigned int r;

		rand_s(&r);
		x = (uint16_t)(65536.0*((double)r)/((double)UINT_MAX+1.0));
		return x;
	}
	else
	{
		return GetRandom16_Default();
	}
}

uint32_t RTPRandom::GetRandom32()
{
	if(use_rand_s)
	{
		uint32_t x,y;
		unsigned int r;
		
		rand_s(&r);
		x = (uint32_t)(65536.0*((double)r)/((double)UINT_MAX+1.0));
		y = x;
		rand_s(&r);
		x = (uint32_t)(65536.0*((double)r)/((double)UINT_MAX+1.0));
		y ^= (x<<8);
		rand_s(&r);
		x = (uint32_t)(65536.0*((double)r)/((double)UINT_MAX+1.0));
		y ^= (x<<16);

		return y;
	}
	else
	{
		return GetRandom32_Default();
	}
}

double RTPRandom::GetRandomDouble()
{
	if(use_rand_s)
	{
		unsigned int r;
		
		rand_s(&r);
		double x = ((double)r)/((double)UINT_MAX+1.0);
		return x;
	}
	else
	{
		return GetRandomDouble_Default();
	}
}

#else // use rand()

uint8_t RTPRandom::GetRandom8()
{
	return GetRandom8_Default();
}

uint16_t RTPRandom::GetRandom16()
{
	return GetRandom16_Default();
}

uint32_t RTPRandom::GetRandom32()
{
	return GetRandom32_Default();
}

double RTPRandom::GetRandomDouble()
{
	return GetRandomDouble_Default();
}

#endif // RTP_SUPPORT_GNUDRAND


uint8_t GetRandom8_Default()
{
	uint8_t x;

	x = (uint8_t)(256.0*((double)rand())/((double)RAND_MAX+1.0));
	return x;
}

uint16_t GetRandom16_Default()
{
	uint16_t x;

	x = (uint16_t)(65536.0*((double)rand())/((double)RAND_MAX+1.0));
	return x;
}

uint32_t GetRandom32_Default()
{
	uint32_t x,y;

	x = (uint32_t)(65536.0*((double)rand())/((double)RAND_MAX+1.0));
	y = x;
	x = (uint32_t)(65536.0*((double)rand())/((double)RAND_MAX+1.0));
	y ^= (x<<8);
	x = (uint32_t)(65536.0*((double)rand())/((double)RAND_MAX+1.0));
	y ^= (x<<16);

	return y;
}

double GetRandomDouble_Default()
{
	double x = ((double)rand())/((double)RAND_MAX+1.0);
	return x;
}

