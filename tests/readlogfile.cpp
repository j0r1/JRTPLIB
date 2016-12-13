#include "rtpexternaltransmitter.h"
#include "rtperrors.h"
#include "rtpsession.h"
#include "rtpsessionparams.h"
#include "rtpbyteaddress.h"
#include "rtcpcompoundpacket.h"
#include "rtcpsrpacket.h"
#include "rtcprrpacket.h"
#include "rtcpbyepacket.h"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <vector>

using namespace std;
using namespace jrtplib;

void checkError(int status)
{
	if (status >= 0)
		return;
	
	cerr << "An error occured in the RTP component: " << endl;
	cerr << "Error description: " << RTPGetErrorString(status) << endl;
	
	exit(-1);
}

class MyRTPSession : public RTPSession
{
protected:
	void OnPollThreadStep()
	{
		//cerr << "OnPollThreadStep" << endl;
	}

	void OnPollThreadError(int err)
	{
		cerr << "POLL THREAD ERROR!" << endl;
		checkError(err);
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

class DummySender : public RTPExternalSender
{
public:
	DummySender() { }
	bool SendRTP(const void *data, size_t len) { return true; }
	bool SendRTCP(const void *data, size_t len) { return true; }
	bool ComesFromThisSender(const RTPAddress *a) { return false; }
};

int main(void)
{
	FILE *pFile = fopen("logfile.dat", "rb");
	if (!pFile)
	{
		cerr << "Unable to open logfile.dat" << endl;
		return -1;
	}

	DummySender sender;
	RTPExternalTransmitter trans(0);
	RTPExternalTransmissionParams params(&sender, 20);

	checkError(trans.Init(true));
	checkError(trans.Create(65535, &params));

	RTPExternalTransmissionInfo *pTransInfo = (RTPExternalTransmissionInfo *)trans.GetTransmissionInfo();
	RTPExternalPacketInjecter *pPacketInjecter = pTransInfo->GetPacketInjector();
	trans.DeleteTransmissionInfo(pTransInfo);

	MyRTPSession session;
	RTPSessionParams sessParams;

	sessParams.SetOwnTimestampUnit(1.0/5.0);
	sessParams.SetProbationType(RTPSources::NoProbation);

	checkError(session.Create(sessParams, &trans));

	vector<uint8_t> data;
	double fileStartTime = 0;
	double realStartTime = 0;
	uint8_t host[] = {1, 2, 3, 4};
	RTPByteAddress addr(host, 4);

	while (true)
	{
		char ident[4];
		double t;
		uint32_t dataLength;

		if (fread(ident, 1, 4, pFile) != 4 || fread(&t, 1, sizeof(double), pFile) != sizeof(double) ||
			fread(&dataLength, 1, sizeof(uint32_t), pFile) != sizeof(uint32_t))
			break;

		data.resize(dataLength);
		if (fread(&data[0], 1, dataLength, pFile) != dataLength)
			break;

		if (realStartTime == 0)
		{
			realStartTime = RTPTime::CurrentTime().GetDouble();
			fileStartTime = t;
		}
		else
		{
			// Busy wait until time to inject
			while (true)
			{
				double curTime = RTPTime::CurrentTime().GetDouble();
				if (curTime - realStartTime >= t - fileStartTime)
					break;
			}
		}

//		if (ident[2] == 'C')
//			cerr << "Injecting RTCP packet of length " << dataLength << endl;
		pPacketInjecter->InjectRTPorRTCP(&data[0], dataLength, addr);
	}

	fclose(pFile);
	return 0;
}
