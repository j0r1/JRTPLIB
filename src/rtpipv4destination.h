/*

  This file is a part of JRTPLIB
  Copyright (c) 1999-2011 Jori Liesenborgs

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
 * \file rtpipv4destination.h
 */

#ifndef RTPIPV4DESTINATION_H

#define RTPIPV4DESTINATION_H

#include "rtpconfig.h"
#if ! (defined(WIN32) || defined(_WIN32_WCE))
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <sys/socket.h>
#endif // WIN32
#ifdef RTPDEBUG
	#include "rtpdefines.h"
	#include <stdio.h>
	#include <string>
#endif // RTPDEBUG
#include <string.h>

class RTPIPv4Destination
{
public:
	RTPIPv4Destination(uint32_t ip,uint16_t rtpportbase)					
	{
		memset(&rtpaddr,0,sizeof(struct sockaddr_in));
		memset(&rtcpaddr,0,sizeof(struct sockaddr_in));
		
		rtpaddr.sin_family = AF_INET;
		rtpaddr.sin_port = htons(rtpportbase);
		rtpaddr.sin_addr.s_addr = htonl(ip);
		
		rtcpaddr.sin_family = AF_INET;
		rtcpaddr.sin_port = htons(rtpportbase+1);
		rtcpaddr.sin_addr.s_addr = htonl(ip);

		RTPIPv4Destination::ip = ip;
	}

	bool operator==(const RTPIPv4Destination &src) const					
	{ 
		if (rtpaddr.sin_addr.s_addr == src.rtpaddr.sin_addr.s_addr && rtpaddr.sin_port == src.rtpaddr.sin_port) 
			return true; 
		return false; 
	}
	uint32_t GetIP() const									{ return ip; }
	// nbo = network byte order
	uint32_t GetIP_NBO() const								{ return rtpaddr.sin_addr.s_addr; }
	uint16_t GetRTPPort_NBO() const								{ return rtpaddr.sin_port; }
	uint16_t GetRTCPPort_NBO() const							{ return rtcpaddr.sin_port; }
	const struct sockaddr_in *GetRTPSockAddr() const					{ return &rtpaddr; }
	const struct sockaddr_in *GetRTCPSockAddr() const					{ return &rtcpaddr; }
#ifdef RTPDEBUG
	std::string GetDestinationString() const;
#endif // RTPDEBUG
private:
	uint32_t ip;
	struct sockaddr_in rtpaddr;
	struct sockaddr_in rtcpaddr;
};

#ifdef RTPDEBUG
inline std::string RTPIPv4Destination::GetDestinationString() const
{
	char str[24];
	uint32_t ip = GetIP();
	uint16_t portbase = ntohs(GetRTPPort_NBO());
	
	RTP_SNPRINTF(str,24,"%d.%d.%d.%d:%d",(int)((ip>>24)&0xFF),(int)((ip>>16)&0xFF),(int)((ip>>8)&0xFF),(int)(ip&0xFF),(int)(portbase));
	return std::string(str);
}
#endif // RTPDEBUG

#endif // RTPIPV4DESTINATION_H

