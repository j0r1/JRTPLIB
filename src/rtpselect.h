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
 * \file rtpselect.h
 */

#ifndef RTPSELECT_H

#define RTPSELECT_H

#include "rtpconfig.h"
#include "rtperrors.h"
#include "rtptimeutilities.h"
#include "rtpsocketutil.h"

#ifndef RTP_SOCKETTYPE_WINSOCK
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#endif // !RTP_SOCKETTYPE_WINSOCK

namespace jrtplib
{

inline int RTPSelect(const SocketType *sockets, bool *readflags, size_t numsocks, RTPTime timeout)
{
	struct timeval tv;
	struct timeval *pTv = 0;

	if (timeout.GetDouble() >= 0)
	{
		tv.tv_sec = timeout.GetSeconds();
		tv.tv_usec = timeout.GetMicroSeconds();
		pTv = &tv;
	}

	fd_set fdset;
	FD_ZERO(&fdset);
	for (size_t i = 0 ; i < numsocks ; i++)
	{
		if (sockets[i] >= FD_SETSIZE)
			return ERR_RTP_SELECT_SOCKETDESCRIPTORTOOLARGE;
		FD_SET(sockets[i], &fdset);
		readflags[i] = false;
	}

	int status = select(FD_SETSIZE, &fdset, 0, 0, pTv);
	if (status < 0)
		return ERR_RTP_SELECT_ERRORINSELECT;

	if (status > 0) // some descriptors were set, check them
	{
		for (size_t i = 0 ; i < numsocks ; i++)
		{
			if (FD_ISSET(sockets[i], &fdset))
				readflags[i] = true;
		}
	}
	return status;
}

} // end namespace
#endif // RTPSELECT_H
