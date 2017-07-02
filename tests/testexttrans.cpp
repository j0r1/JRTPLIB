#include "rtpsession.h"
#include "rtpsessionparams.h"
#include "rtpexternaltransmitter.h"
#include "rtperrors.h"
#include "rtpsourcedata.h"
#include "rtpipv4address.h"
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>

using namespace jrtplib;
using namespace std;

void checkerror(int rtperr)
{
	if (rtperr < 0)
	{
		cout << "ERROR: " << RTPGetErrorString(rtperr) << endl;
		exit(-1);
	}
}

class ExtSender : public RTPExternalSender
{
public:
	ExtSender() { }
	~ExtSender() { }
private:
	bool SendRTP(const void *pData, size_t len)
	{
		return true;
	}
	
	bool SendRTCP(const void *pData, size_t len)
	{
		return true;
	}

	bool ComesFromThisSender(const RTPAddress *pAddr)
	{
		return false;
	}
};

int main(void)
{
	ExtSender sender;
	RTPSessionParams sessParams;
	RTPExternalTransmissionParams transParams(&sender, 0);
	RTPSession session;

	sessParams.SetOwnTimestampUnit(1.0/48000.0); // Just set something, not relevant for this test
	int status = session.Create(sessParams, &transParams, RTPTransmitter::ExternalProto);
	checkerror(status);

	RTPExternalTransmissionInfo *pTransInf = static_cast<RTPExternalTransmissionInfo *>(session.GetTransmissionInfo());
	RTPExternalPacketInjecter *pPacketInjector = pTransInf->GetPacketInjector();
	session.DeleteTransmissionInfo(pTransInf);

	for (int i = 0 ; i < 1024*1024 ; i++)
	{
		if (i%1000 == 0)
			cout << "Writing packet " << i << endl;
		RTPIPv4Address addr(0x7f000001, 1234);
		pPacketInjector->InjectRTP("12345", 5, addr);
	}

	return 0;
}
