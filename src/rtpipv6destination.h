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

#ifndef RTPIPV6DESTINATION

#define RTPIPV6DESTINATION

#include "rtpconfig.h"

#ifdef RTP_SUPPORT_IPV6

#include "rtptypes.h"
#include <string.h>
#ifndef WIN32
	#include <netinet/in.h>
#else
	#include <winsock2.h>
#endif // WIN32

class RTPIPv6Destination
{
public:
	RTPIPv6Destination(in6_addr ip,u_int16_t portbase)	 			{ RTPIPv6Destination::ip = ip; rtpport_hbo = portbase; rtcpport_hbo = portbase+1; rtpport_nbo = htons(portbase); rtcpport_nbo = htons(portbase+1); }
	in6_addr GetIP() const								{ return ip; }
	u_int16_t GetRTPPort_HBO() const						{ return rtpport_hbo; }
	u_int16_t GetRTPPort_NBO() const						{ return rtpport_nbo; }
	u_int16_t GetRTCPPort_HBO() const						{ return rtcpport_hbo; }
	u_int16_t GetRTCPPort_NBO() const						{ return rtcpport_nbo; }
	bool operator==(const RTPIPv6Destination &src) const				{ if (src.rtpport_nbo == rtpport_nbo && (memcmp(&(src.ip),&ip,sizeof(in6_addr)) == 0)) return true; return false; } // NOTE: I only check IP and portbase
private:
	in6_addr ip;
	u_int16_t rtpport_hbo,rtcpport_hbo;
	u_int16_t rtpport_nbo,rtcpport_nbo;
};

#endif // RTP_SUPPORT_IPV6

#endif // RTPIPV6DESTINATION

