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
 * \file rtprawpacket.h
 */

#ifndef RTPRAWPACKET_H

#define RTPRAWPACKET_H

#include "rtpconfig.h"
#include "rtptimeutilities.h"
#include "rtpaddress.h"
#include "rtptypes.h"
#include "rtpmemoryobject.h"

/** This class is used by the transmission component to store the incoming RTP and RTCP data in. */
class RTPRawPacket : public RTPMemoryObject
{
public:	
    	/** Creates an instance which stores data from \c data with length \c datalen.
	 *  Creates an instance which stores data from \c data with length \c datalen. Only the pointer 
	 *  to the data is stored, no actual copy is made! The address from which this packet originated 
	 *  is set to \c address and the time at which the packet was received is set to \c recvtime. 
	 *  The flag which indicates whether this data is RTP or RTCP data is set to \c rtp. A memory
	 *  manager can be installed as well.
	 */
	RTPRawPacket(uint8_t *data,size_t datalen,RTPAddress *address,RTPTime &recvtime,bool rtp,RTPMemoryManager *mgr = 0);
	~RTPRawPacket();
	
	/** Returns the pointer to the data which is contained in this packet. */
	uint8_t *GetData()														{ return packetdata; }

	/** Returns the length of the packet described by this instance. */
	size_t GetDataLength() const											{ return packetdatalength; }

	/** Returns the time at which this packet was received. */
	RTPTime GetReceiveTime() const											{ return receivetime; }

	/** Returns the address stored in this packet. */
	const RTPAddress *GetSenderAddress() const								{ return senderaddress; }

	/** Returns \c true if this data is RTP data, \c false if it is RTCP data. */
	bool IsRTP() const														{ return isrtp; }

	/** Sets the pointer to the data stored in this packet to zero.
	 *  Sets the pointer to the data stored in this packet to zero. This will prevent 
	 *  a \c delete call for the actual data when the destructor of RTPRawPacket is called. 
	 *  This function is used by the RTPPacket and RTCPCompoundPacket classes to obtain 
	 *  the packet data (without having to copy it)	and to make sure the data isn't deleted 
	 *  when the destructor of RTPRawPacket is called.
	 */
	void ZeroData()															{ packetdata = 0; packetdatalength = 0; }
private:
	uint8_t *packetdata;
	size_t packetdatalength;
	RTPTime receivetime;
	RTPAddress *senderaddress;
	bool isrtp;
};

inline RTPRawPacket::RTPRawPacket(uint8_t *data,size_t datalen,RTPAddress *address,RTPTime &recvtime,bool rtp,RTPMemoryManager *mgr):RTPMemoryObject(mgr),receivetime(recvtime)
{
	packetdata = data;
	packetdatalength = datalen;
	senderaddress = address;
	isrtp = rtp;
}

inline RTPRawPacket::~RTPRawPacket()
{
	if (packetdata)
		RTPDeleteByteArray(packetdata,GetMemoryManager());
	if (senderaddress)
		RTPDelete(senderaddress,GetMemoryManager());
}

#endif // RTPRAWPACKET_H

