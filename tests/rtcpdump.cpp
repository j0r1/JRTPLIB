#include "rtpsession.h"
#include "rtpipv4address.h"
#include "rtpsessionparams.h"
#include "rtpudpv4transmitter.h"
#include "rtperrors.h"
#include "rtcpcompoundpacket.h"
#include "rtcpsrpacket.h"
#include "rtcprrpacket.h"
#include "rtcpbyepacket.h"
#include "rtprawpacket.h"
#include <stdio.h>
#include <iostream>

using namespace jrtplib;

void checkError(int status)
{
	if (status >= 0)
		return;
	
	std::cerr << "An error occured in the RTP component: " << std::endl;
	std::cerr << "Error description: " << RTPGetErrorString(status) << std::endl;
	
	exit(-1);
}

class MyRTPSession : public RTPSession
{
private:
	FILE *pLogFile;
public:
	MyRTPSession()
	{
		SetChangeIncomingData(true);
		
		pLogFile = fopen("logfile.dat", "wb");
	}

	~MyRTPSession()
	{
		if (pLogFile)
			fclose(pLogFile);
	}
protected:
	void OnValidatedRTPPacket(RTPSourceData *srcdat, RTPPacket *rtppack, bool isonprobation, bool *ispackethandled)
	{
		// Make sure no RTP packets are stored internally, we'd just be wasting memory
		DeletePacket(rtppack);
		*ispackethandled = true;
	}

	bool OnChangeIncomingData(RTPRawPacket *pPack)
	{
		if (pLogFile)
		{
			double t = pPack->GetReceiveTime().GetDouble();
			bool isRTP = pPack->IsRTP();
			uint32_t dataLength = (uint32_t)pPack->GetDataLength();

			if (isRTP)
				fwrite("RTP ", 1, 4, pLogFile);
			else
				fwrite("RTCP", 1, 4, pLogFile);

			fwrite(&t, 1, sizeof(double), pLogFile);
			fwrite(&dataLength, 1, sizeof(uint32_t),pLogFile);
			fwrite(pPack->GetData(), 1, dataLength, pLogFile);
		}
		return true;
	}

	void OnRTCPCompoundPacket(RTCPCompoundPacket *p, const RTPTime &receivetime, const RTPAddress *senderaddress)
	{	
		printf("%u.%06u RECEIVED\n",receivetime.GetSeconds(),receivetime.GetMicroSeconds());
		
		DumpCompoundPacket(stdout,p);
	}

	void OnSendRTCPCompoundPacket(RTCPCompoundPacket *p)
	{	
		RTPTime t = RTPTime::CurrentTime();
		
		printf("%u.%06u SENDING\n",t.GetSeconds(),t.GetMicroSeconds());
		
		DumpCompoundPacket(stdout,p);
	}
	
	void DumpCompoundPacket(FILE *f, RTCPCompoundPacket *p)
	{
		RTCPPacket *pack;

		p->GotoFirstPacket();
		while ((pack = p->GetNextPacket()) != 0)
		{
			if (pack->GetPacketType() == RTCPPacket::SR)
			{
				RTCPSRPacket *p = (RTCPSRPacket *)pack;

				RTPTime t(p->GetNTPTimestamp());
				
				fprintf(f,"  SR packet\n    SSRC %27u\n",p->GetSenderSSRC());
				fprintf(f,"    NTP timestamp: %10u.%06u\n    RTP timestamp: %17u\n    Packets sent: %18u\n    Octets sent: %19u\n",t.GetSeconds(),t.GetMicroSeconds(),p->GetRTPTimestamp(),p->GetSenderPacketCount(),p->GetSenderOctetCount());
					
				for (int i = 0 ; i < p->GetReceptionReportCount() ; i++)
					fprintf(f,"    RR block %d\n      SSRC %25u\n      Fraction lost: %15d\n      Packets lost: %16d\n      Ext. high. seq. nr: %10u\n      Jitter: %22u\n      LSR: %25u\n      DLSR: %24u\n",(i+1),
							p->GetSSRC(i),(int)p->GetFractionLost(i),p->GetLostPacketCount(i),p->GetExtendedHighestSequenceNumber(i),p->GetJitter(i),p->GetLSR(i),
							p->GetDLSR(i));
			}
			else if (pack->GetPacketType() == RTCPPacket::RR)
			{
				RTCPRRPacket *p = (RTCPRRPacket *)pack;

				fprintf(f,"  RR packet\n    SSRC %27u\n",p->GetSenderSSRC());
					
				for (int i = 0 ; i < p->GetReceptionReportCount() ; i++)
					fprintf(f,"    RR block %d\n      SSRC %25u\n      Fraction lost: %15d\n      Packets lost: %16d\n      Ext. high. seq. nr: %10u\n      Jitter: %22u\n      LSR: %25u\n      DLSR: %24u\n",(i+1),
							p->GetSSRC(i),(int)p->GetFractionLost(i),p->GetLostPacketCount(i),p->GetExtendedHighestSequenceNumber(i),p->GetJitter(i),p->GetLSR(i),
							p->GetDLSR(i));
			}
			else if (pack->GetPacketType() == RTCPPacket::SDES)
			{
				RTCPSDESPacket *p = (RTCPSDESPacket *)pack;
				char str[1024];
				
				if (!p->GotoFirstChunk())
					return;
				
				do
				{
					fprintf(f,"  SDES Chunk:\n");
					fprintf(f,"    SSRC: %26u\n",p->GetChunkSSRC());
					if (p->GotoFirstItem())
					{
						do
						{
							switch (p->GetItemType())
							{
							case RTCPSDESPacket::None:
								strcpy(str,"None    ");
								break;
							case RTCPSDESPacket::CNAME:
								strcpy(str,"CNAME:  ");
								break;
							case RTCPSDESPacket::NAME:
								strcpy(str,"NAME:   ");
								break;
							case RTCPSDESPacket::EMAIL:
								strcpy(str,"EMAIL:  ");
								break;
							case RTCPSDESPacket::PHONE:
								strcpy(str,"PHONE:  ");
								break;
							case RTCPSDESPacket::LOC:
								strcpy(str,"LOC:    ");
								break;
							case RTCPSDESPacket::TOOL:
								strcpy(str,"TOOL:   ");
								break;
							case RTCPSDESPacket::NOTE:
								strcpy(str,"NOTE:   ");
								break;
							case RTCPSDESPacket::PRIV:
								strcpy(str,"PRIV:   ");
								break;
							case RTCPSDESPacket::Unknown:
							default:
								strcpy(str,"Unknown ");
							}
							fprintf(f,"    %s",str);
							
							if (p->GetItemType() != RTCPSDESPacket::PRIV)
							{
								char str[1024];
								memcpy(str,p->GetItemData(),p->GetItemLength());
								str[p->GetItemLength()] = 0;
								fprintf(f,"%24s\n",str);
							}
						} while (p->GotoNextItem());
					}
				} while (p->GotoNextChunk());
			}
			else if (pack->GetPacketType() == RTCPPacket::BYE)
			{
				fprintf(f,"  BYE packet:\n");
				
				RTCPBYEPacket *p = (RTCPBYEPacket *)pack;
				
				int num = p->GetSSRCCount();
				int i;
			
				for (i = 0 ; i < num ; i++)
					fprintf(f,"    SSRC: %26u\n",p->GetSSRC(i));
				if (p->HasReasonForLeaving())
				{
					char str[1024];
					memcpy(str,p->GetReasonData(),p->GetReasonLength());
					str[p->GetReasonLength()] = 0;
					fprintf(f,"    Reason: %24s\n",str);
				}
			}
		}
		fprintf(f,"\n");
	}
};

int main(int argc, char *argv[])
{
	if (argc != 4)
	{
		fprintf(stderr, "Usage: rtcpdump portbase destIP destport\n");
		return -1;
	}

	int portBase = atoi(argv[1]);
	int destPort = atoi(argv[3]);
	std::string destIP(argv[2]);

	RTPIPv4Address dest;
	RTPUDPv4TransmissionParams transParams;
	RTPSessionParams sessParams;
	MyRTPSession session;

	dest.SetIP(ntohl(inet_addr(destIP.c_str())));
	dest.SetPort((uint16_t)destPort);

	transParams.SetPortbase((uint16_t)portBase);
	transParams.SetRTPReceiveBuffer(1024*1024);
	transParams.SetRTCPReceiveBuffer(1024*1024);
	sessParams.SetOwnTimestampUnit(1.0/5.0);
	sessParams.SetProbationType(RTPSources::NoProbation);

	int status;

	status = session.Create(sessParams, &transParams);
	checkError(status);

	status = session.AddDestination(dest);
	checkError(status);

	int i = 0;

	getchar();

	return 0;
}
