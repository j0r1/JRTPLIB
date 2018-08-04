#include "rtpsession.h"
#include "rtpudpv4transmitter.h"
#include "rtpipv4address.h"
#include "rtpsessionparams.h"
#include "rtperrors.h"
#include "rtplibraryversion.h"
#include "rtpsourcedata.h"
#include "rtprawpacket.h"
#include "rtcpcompoundpacket.h"
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <list>
#include <vector>

using namespace jrtplib;
using namespace std;

void checkerror(int rtperr)
{
	if (rtperr < 0)
	{
		std::cout << "ERROR: " << RTPGetErrorString(rtperr) << std::endl;
		exit(-1);
	}
}

class PacketData
{
public:
	PacketData(const void *pData, size_t len)
	{
		m_data.resize(len);
		if (len)
			memcpy(&(m_data[0]), pData, len);
	}

	uint8_t *CopyData() const
	{
		if (m_data.size() == 0)
			return 0;

		uint8_t *pData = new uint8_t[m_data.size()];
		memcpy(pData, &(m_data[0]), m_data.size());
		return pData;
	}

	vector<uint8_t> m_data;
};

class MyRTPSession : public RTPSession
{
public:
	void OnValidatedRTPPacket(RTPSourceData *srcdat, RTPPacket *rtppack, bool isonprobation, bool *ispackethandled)
	{
		m_packets.push_back(PacketData(rtppack->GetPacketData(), rtppack->GetPacketLength()));
		*ispackethandled = true;
		DeletePacket(rtppack);
	}

	void OnRTCPCompoundPacket(RTCPCompoundPacket *pack, const RTPTime &receivetime, const RTPAddress *senderaddress)
	{
		m_packets.push_back(PacketData(pack->GetCompoundPacketData(), pack->GetCompoundPacketLength()));
	}

	list<PacketData> m_packets;
};

int main(void)
{
#ifdef RTP_SOCKETTYPE_WINSOCK
	WSADATA dat;
	WSAStartup(MAKEWORD(2,2),&dat);
#endif // RTP_SOCKETTYPE_WINSOCK
	
	MyRTPSession sess;
	uint16_t portbase,destport;
	uint32_t destip;
	std::string ipstr;
	int status,i,num;

	std::cout << "Using version " << RTPLibraryVersion::GetVersion().GetVersionString() << std::endl;

	// First, we'll ask for the necessary information
		
	portbase = 5000;
	ipstr = "127.0.0.1";

	destip = inet_addr(ipstr.c_str());
	if (destip == INADDR_NONE)
	{
		std::cerr << "Bad IP address specified" << std::endl;
		return -1;
	}
	
	// The inet_addr function returns a value in network byte order, but
	// we need the IP address in host byte order, so we use a call to
	// ntohl
	destip = ntohl(destip);
	destport = 5000;
	num = 20;

	// Now, we'll create a RTP session, set the destination, send some
	// packets and poll for incoming data.
	
	RTPUDPv4TransmissionParams transparams;
	RTPSessionParams sessparams;
	
	// IMPORTANT: The local timestamp unit MUST be set, otherwise
	//            RTCP Sender Report info will be calculated wrong
	// In this case, we'll be sending 10 samples each second, so we'll
	// put the timestamp unit to (1.0/10.0)
	sessparams.SetOwnTimestampUnit(1.0/10.0);		
	
	sessparams.SetAcceptOwnPackets(true);
	transparams.SetPortbase(portbase);
	
	// Let's also use RTCP multiplexing for this example
	transparams.SetRTCPMultiplexing(true); 

	status = sess.Create(sessparams,&transparams);	
	checkerror(status);
	
	// We're assuming that the destination is also using RTCP multiplexing 
	// ('true' means that the same port will be used for RTCP)
	RTPIPv4Address addr(destip,destport,true); 
	
	status = sess.AddDestination(addr);
	checkerror(status);
	
	for (i = 1 ; i <= num ; i++)
	{
		cout << "\nSending packet " << i << "/" << num << endl;
		
		// send the packet
		status = sess.SendPacket((void *)"1234567890",10,0,false,10);
		checkerror(status);
		
#ifndef RTP_SUPPORT_THREAD
		status = sess.Poll();
		checkerror(status);
#endif // RTP_SUPPORT_THREAD
		
		RTPTime::Wait(RTPTime(1,0));
	}
	
	sess.BYEDestroy(RTPTime(10,0),0,0);

#ifdef RTP_SOCKETTYPE_WINSOCK
	WSACleanup();
#endif // RTP_SOCKETTYPE_WINSOCK

	cout << "Logged " << sess.m_packets.size() << " packets" << endl;

	for (list<PacketData>::iterator it = sess.m_packets.begin() ; it != sess.m_packets.end() ; ++it)
	{
		uint8_t *pData = it->CopyData();
		size_t len = it->m_data.size();

		RTPTime t = RTPTime::CurrentTime();
		RTPRawPacket rawPack(pData, len, new RTPIPv4Address(), t);

		if (rawPack.IsRTP())
		{
			RTPPacket p(rawPack);
			int err;

			if ((err = p.GetCreationError()) == 0)
				cout << "Valid RTP packet" << endl;
			else
				cout << "Bad RTP packet: " << RTPGetErrorString(err) << endl;
		}
		else
		{
			RTCPCompoundPacket p(rawPack);
			int err;

			if ((err = p.GetCreationError()) == 0)
				cout << "Valid RTCP packet" << endl;
			else
				cout << "Bad RTCP packet: " << RTPGetErrorString(err) << endl;
		}
	}
	return 0;
}

