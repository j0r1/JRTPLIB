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

#include "srtpsession.h"

#ifdef RTP_SUPPORT_SRTP

#include "rtprawpacket.h"
#include <srtp.h>
#include <iostream>
#include <vector>

using namespace std;

namespace jrtplib
{

class RTPCrypt : public RTPMemoryObject
{
public: 
	enum CryptoSuite { AES_CM_128_HMAC_SHA1_80, AES_CM_128_HMAC_SHA1_32, Undefined };

	RTPCrypt(RTPMemoryManager *mgr);
	~RTPCrypt();

	int init(const string &enc_suite, const string &srtp_key);
	bool decryptRawPacket(RTPRawPacket *rawpack);
	bool encryptData(uint8_t *pData, int &dataLength, bool rtp);
private:
	void addSSRC(uint32_t ssrc);

	srtp_t srtpcontext;
	CryptoSuite cryptosuite;
	uint8_t key[30];

	bool m_initialized;
};

RTPCrypt::RTPCrypt(RTPMemoryManager *mgr) : RTPMemoryObject(mgr)
{
	m_initialized = false;
}

RTPCrypt::~RTPCrypt()
{
	if (m_initialized)
		srtp_dealloc(srtpcontext);
}

int RTPCrypt::init(const string &enc_suite, const string &srtp_key)
{
	if (m_initialized)
		return ERR_RTP_SRTPSESSION_ENCRYPTIONALREADYINIT;
  
	if(enc_suite == "AES_CM_128_HMAC_SHA1_80")
		cryptosuite = AES_CM_128_HMAC_SHA1_80;
	else if(enc_suite == "AES_CM_128_HMAC_SHA1_32")
		cryptosuite = AES_CM_128_HMAC_SHA1_32;
	else
		return ERR_RTP_SRTPSESSION_UNKNOWNCRYPTOSUITE;

	if(srtp_key.size() != 30)
		return ERR_RTP_SRTPSESSION_INVALIDKEYLENGTH;

	memcpy(key, srtp_key.c_str(), 30);

	err_status_t result = srtp_create(&srtpcontext, NULL);
	if (result != err_status_ok)
		return ERR_RTP_SRTPSESSION_CANTINITIALIZE_SRTPCONTEXT;

	m_initialized = true;

	return 0;
}

void RTPCrypt::addSSRC(uint32_t ssrc)
{
	if (!m_initialized)
		return;

	srtp_policy_t policy;
	memset((void*)&policy, 0, sizeof(srtp_policy_t));
	if(cryptosuite == AES_CM_128_HMAC_SHA1_80)
	{
		crypto_policy_set_rtp_default(&policy.rtp);
		crypto_policy_set_rtcp_default(&policy.rtcp);
	}
	else if(cryptosuite == AES_CM_128_HMAC_SHA1_32)
	{
		crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy.rtp);
		crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy.rtcp);
	}
	policy.ssrc.type = ssrc_specific;
	policy.ssrc.value = ssrc;
	policy.key = key;
	policy.next = NULL;
	srtp_add_stream(srtpcontext, &policy);
}

bool RTPCrypt::decryptRawPacket(RTPRawPacket *rawpack)
{
	if (!m_initialized)
		return false;

	if (!rawpack)
		return false;

	if (rawpack->IsRTP())
	{
		for (int i = 0 ; i < 2 ; i++) // Possibly loop, if the ssrc still needed to be added
		{
			// Reassign variable, since it can contain garbage after error
			uint8_t *pData = rawpack->GetData();
			int dataLength = (int)rawpack->GetDataLength();

			if (dataLength < sizeof(uint32_t)*3)
				return false;

			err_status_t result = srtp_unprotect(srtpcontext, (void*)pData, &dataLength);

			if (result == err_status_ok || result == err_status_replay_fail)
			{
				rawpack->ZeroData(); // make sure we don't delete the data we're going to store
				rawpack->SetData(pData, (size_t)dataLength);
				return true;
			}
			else if (i == 0 && result == err_status_no_ctx)
			{
				uint32_t srtp_ssrc = ntohl(*(reinterpret_cast<uint32_t*>(rawpack->GetData()) + 2));
				addSSRC(srtp_ssrc);
			}
			else
				break;
		}
	
		// remove this ssrc
		uint32_t srtp_ssrc = ntohl(*(reinterpret_cast<uint32_t*>(rawpack->GetData()) + 2));
		srtp_remove_stream(srtpcontext, srtp_ssrc);
	}
	else // RTCP
	{
		for (int i = 0 ; i < 2 ; i++) // Possibly loop, if the ssrc still needed to be added
		{
			// Reassign variable, since it can contain garbage after error
			uint8_t *pData = rawpack->GetData();
			int dataLength = (int)rawpack->GetDataLength();

			if (dataLength < sizeof(uint32_t)*2)
				return false;

			err_status_t result = srtp_unprotect_rtcp(srtpcontext, (void *)pData, &dataLength);
			if (result == err_status_ok || result == err_status_replay_fail)
			{
				rawpack->ZeroData();
				rawpack->SetData(pData, (size_t)dataLength);
				return true;
			}
			else if (i == 0 && result == err_status_no_ctx)
			{
				uint32_t srtcp_ssrc = ntohl(*(reinterpret_cast<uint32_t*>(rawpack->GetData()) + 1));
				addSSRC(srtcp_ssrc);
			}
			else
				break;
		}
	
		// remove this ssrc
		uint32_t srtp_ssrc = ntohl(*(reinterpret_cast<uint32_t*>(rawpack->GetData()) + 2));
		srtp_remove_stream(srtpcontext, srtp_ssrc);
	}

	return false;
}

bool RTPCrypt::encryptData(uint8_t *pData, int &dataLength, bool rtp)
{
	if (!m_initialized)
		return false;

	if (!pData)
		return false;

	if (rtp)
	{
		if (dataLength < sizeof(uint32_t)*3)
			return false;

		for (int i = 0 ; i < 2 ; i++) // Possibly loop, if the ssrc still needed to be added
		{
			int length = dataLength;

			err_status_t result = srtp_protect(srtpcontext, (void *)pData, &length);

			if (result == err_status_ok || result == err_status_replay_fail)
			{
				dataLength = length;
				return true;
			}
			else if (i == 0 && result == err_status_no_ctx)
			{
				uint32_t srtp_ssrc = ntohl(*(reinterpret_cast<uint32_t*>(pData) + 2));
				addSSRC(srtp_ssrc);
			}
			else
				break;
		}
	
		// remove this ssrc
		uint32_t srtp_ssrc = ntohl(*(reinterpret_cast<uint32_t*>(pData) + 2));
		srtp_remove_stream(srtpcontext, srtp_ssrc);
	}
	else // rtcp
	{
		if (dataLength < sizeof(uint32_t)*2)
			return false;

		for (int i = 0 ; i < 2 ; i++) // Possibly loop, if the ssrc still needed to be added
		{
			int length = dataLength;

			err_status_t result = srtp_protect_rtcp(srtpcontext, (void *)pData, &length);
			if (result == err_status_ok || result == err_status_replay_fail)
			{
				dataLength = length;
				return true;
			}
			else if (i == 0 && result == err_status_no_ctx)
			{
				uint32_t srtcp_ssrc = ntohl(*(reinterpret_cast<uint32_t*>(pData) + 1));
				addSSRC(srtcp_ssrc);
			}
			else
				break;
		}

		// remove this ssrc
		uint32_t srtp_ssrc = ntohl(*(reinterpret_cast<uint32_t*>(pData) + 1));
		srtp_remove_stream(srtpcontext, srtp_ssrc);
	}	
	return false;
}

// SRTP library needs to be initialized already!

SRTPSession::SRTPSession(RTPRandom *rnd, RTPMemoryManager *mgr) : RTPSession(rnd, mgr)
{
	m_pCrypt = new RTPCrypt(mgr);
}

SRTPSession::~SRTPSession()
{
	delete m_pCrypt;
}

int SRTPSession::InitializeEncryption(const string &enc_suite, const string &srtp_key)
{
	return m_pCrypt->init(enc_suite, srtp_key);
}

bool SRTPSession::OnChangeRTPOrRTCPData(const void *origdata, size_t origlen, bool isrtp, void **senddata, size_t *sendlen)
{
	if (!origdata || origlen == 0)
		return false;

	// Need to add some extra bytes, and we'll add a few more to be really safe
	uint8_t *pDataCopy = RTPNew(GetMemoryManager(), RTPMEM_TYPE_BUFFER_SRTPDATA) uint8_t [origlen + SRTP_MAX_TRAILER_LEN + 32];
	memcpy(pDataCopy, origdata, origlen);

	int dataLength = (int)origlen;
	//cout << "before: " << origlen << endl;
	if (!m_pCrypt->encryptData(pDataCopy, dataLength, isrtp))
	{
		RTPDeleteByteArray(pDataCopy, GetMemoryManager());
		return false;
	}
	//cout << "after: " << dataLength << endl << endl;

	*senddata = pDataCopy;
	*sendlen = dataLength;

	return true;
}

bool SRTPSession::OnChangeIncomingData(RTPRawPacket *rawpack)
{
	if (!m_pCrypt->decryptRawPacket(rawpack))
		return false;

	return true;
}

void SRTPSession::OnSentRTPOrRTCPData(void *senddata, size_t sendlen, bool isrtp)
{
	if (senddata)
		RTPDeleteByteArray((uint8_t *)senddata, GetMemoryManager());
}

} // end namespace

#endif // RTP_SUPPORT_SRTP

