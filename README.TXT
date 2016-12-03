------------------------------------------------------------------------------
                                                              November 8, 2011


                                   JRTPLIB (v3.9.1)


                      Developed at the The Expertise Centre for 
                      Digital Media (EDM), a research institute  
                              of the Hasselt University

                             http://www.edm.uhasselt.be/
                               http://www.uhasselt.be/


LIBRARY LOCATION AND CONTACT
============================

Normally, you should be able to download the latest version of the library
from this url:
	http://research.edm.uhasselt.be/jori/jrtplib/jrtplib.html

If you have questions about the library, you can mail me at:
	jori.liesenborgs@gmail.com
	
There is also a mailing list for the library. To subscribe to the list, 
just send an e-mail to jrtplib-subscribe@edm.uhasselt.be and you'll 
receive further instructions.

ACKNOWLEDGMENT
==============

I would like thank the people at the Expertise Centre for Digital Media
for giving me the opportunity to create this rewrite of the library.

DISCLAIMER & COPYRIGHT
======================

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

INSTALLATION NOTES
==================

* Use the CMake build system to compile the library. In case extra
  include directories or libraries are needed, you can use the 'ADDITIONAL_'
  CMake variables to specify these. They will be stored in both the resulting
  JRTPLIB CMake configuration file and the pkg-config file.

* The library documentation can be generated using Doxygen. An on-line
  version can be found at
  http://research.edm.uhasselt.be/jori/jrtplib/documentation/index.html

* For systems with low memory or for applications which will involve only
  a few participants at a time:
  You can set the HASHSIZE defines in rtpsources.h, rtpudpv4transmitter.h
  and rtpudpv6transmitter.h to a lower value to avoid memory being wasted.
  Note that the library will have to be recompiled.

* Used defines:
	- WIN32:
		For compilation on an Win32 platform.
	- _WIN32_WCE:
		Define needed for compilation on a WinCE platform
	- RTP_HAVE_SYS_FILIO:
		Set if <sys/filio.h> exists.
	- RTP_HAVE_SYS_SOCKIO:
		Set if <sys/sockio.h> exists.
	- RTP_BIG_ENDIAN:
		If set, assume big-endian byte ordering.
	- RTP_SOCKLENTYPE_UINT:
		Indicates that getsockname used an unsigned int as its
                third parameter.
	- RTP_HAVE_SOCKADDR_LEN:
		Indicates that struct sockaddr has an sa_len field.
	- RTP_SUPPORT_IPV4MULTICAST:
		Enables support for IPv4 multicasting.
	- RTP_SUPPORT_THREAD:
		Enables support for JThread.
	- RTP_SUPPORT_SDESPRIV:
		Enables support for RTCP SDES private items.
	- RTP_SUPPORT_PROBATION:
		If set, a few consecutive RTP packets are needed to validate
                a member.
	- RTP_SUPPORT_GETLOGINR:
		If set, the library will use getlogin_r instead of getlogin.
	- RTP_SUPPORT_IPV6:
		If set, IPv6 support is enabled.
	- RTP_SUPPORT_IPV6MULTICAST:
		If set, IPv6 multicasting support is enabled.
	- RTP_SUPPORT_SENDAPP:
		If set, sending of RTCP app packets is enabled.
	- RTP_SUPPORT_MEMORYMANAGEMENT:
		If set, the memory management system is enabled.
	- RTP_SUPPORT_RTCPUNKNOWN:
		If set, sending of unknown RTCP packets is enabled.
	- RTPDEBUG:
		Enables some memory tracking functions and some debug 
                routines.
	
------------------------------------------------------------------------------

