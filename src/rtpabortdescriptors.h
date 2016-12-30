#ifndef RTPABORTDESCRIPTORS_H

#define RTPABORTDESCRIPTORS_H

#include "rtpconfig.h"
#include "rtpsocketutil.h"

namespace jrtplib
{

class RTPAbortDescriptors
{
public:
	RTPAbortDescriptors();
	~RTPAbortDescriptors();

	int Init();
	SocketType GetAbortSocket() const													{ return m_descriptors[0]; }
	bool IsInitialized() const															{ return m_init; }
	void Destroy();

	int SendAbortSignal();
	int ReadSignallingByte();
	int ClearAbortSignal();
private:
	SocketType m_descriptors[2];
	bool m_init;
};
 
} // end namespace

#endif // RTPABORTDESCRIPTORS_H
