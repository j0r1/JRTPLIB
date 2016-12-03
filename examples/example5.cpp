/*
   This is a modified version of example1.cpp to illustrate the use of a memory
   manager.
*/

#include "rtpsession.h"
#include "rtppacket.h"
#include "rtpudpv4transmitter.h"
#include "rtpipv4address.h"
#include "rtpsessionparams.h"
#include "rtperrors.h"
#include "rtpmemorymanager.h"
#ifndef WIN32
	#include <netinet/in.h>
	#include <arpa/inet.h>
#else
	#include <winsock2.h>
#endif // WIN32
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>

//
// This function checks if there was a RTP error. If so, it displays an error
// message and exists.
//

void checkerror(int rtperr)
{
	if (rtperr < 0)
	{
		std::cout << "ERROR: " << RTPGetErrorString(rtperr) << std::endl;
		exit(-1);
	}
}

//
// The main routine
//

#ifdef RTP_SUPPORT_THREAD

class MyMemoryManager : public RTPMemoryManager
{
public:
	MyMemoryManager() 
	{ 
		mutex.Init();
		alloccount = 0; 
		freecount = 0; 
	}
	~MyMemoryManager() 
	{ 
		std::cout << "alloc: " << alloccount << " free: " << freecount << std::endl; 
	}
	void *AllocateBuffer(size_t numbytes, int memtype)
	{
		mutex.Lock();
		void *buf = malloc(numbytes);
		std::cout << "Allocated " << numbytes << " bytes at location " << buf << " (memtype = " << memtype << ")" << std::endl;
		alloccount++;
		mutex.Unlock();
		return buf;
	}

	void FreeBuffer(void *p)
	{
		mutex.Lock();
		std::cout << "Freeing block " << p << std::endl;
		freecount++;
		free(p);
		mutex.Unlock();
	}
private:
	int alloccount,freecount;
	JMutex mutex;
};

#else

class MyMemoryManager : public RTPMemoryManager
{
public:
	MyMemoryManager() 
	{ 
		alloccount = 0; 
		freecount = 0; 
	}
	~MyMemoryManager() 
	{ 
		std::cout << "alloc: " << alloccount << " free: " << freecount << std::endl; 
	}
	void *AllocateBuffer(size_t numbytes, int memtype)
	{
		void *buf = malloc(numbytes);
		std::cout << "Allocated " << numbytes << " bytes at location " << buf << " (memtype = " << memtype << ")" << std::endl;
		alloccount++;
		return buf;
	}

	void FreeBuffer(void *p)
	{
		std::cout << "Freeing block " << p << std::endl;
		freecount++;
		free(p);
	}
private:
	int alloccount,freecount;
};

#endif // RTP_SUPPORT_THREAD

int main(void)
{
#ifdef WIN32
	WSADATA dat;
	WSAStartup(MAKEWORD(2,2),&dat);
#endif // WIN32
	
	MyMemoryManager mgr;
	RTPSession sess(&mgr);
	uint16_t portbase,destport;
	uint32_t destip;
	std::string ipstr;
	int status,i,num;

        // First, we'll ask for the necessary information
		
	std::cout << "Enter local portbase:" << std::endl;
	std::cin >> portbase;
	std::cout << std::endl;
	
	std::cout << "Enter the destination IP address" << std::endl;
	std::cin >> ipstr;
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
	
	std::cout << "Enter the destination port" << std::endl;
	std::cin >> destport;
	
	std::cout << std::endl;
	std::cout << "Number of packets you wish to be sent:" << std::endl;
	std::cin >> num;
	
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
	status = sess.Create(sessparams,&transparams);	
	checkerror(status);
	
	RTPIPv4Address addr(destip,destport);
	
	status = sess.AddDestination(addr);
	checkerror(status);
	
	for (i = 1 ; i <= num ; i++)
	{
		printf("\nSending packet %d/%d\n",i,num);
		
		// send the packet
		status = sess.SendPacket((void *)"1234567890",10,0,false,10);
		checkerror(status);
		
		sess.BeginDataAccess();
		
		// check incoming packets
		if (sess.GotoFirstSourceWithData())
		{
			do
			{
				RTPPacket *pack;
				
				while ((pack = sess.GetNextPacket()) != NULL)
				{
					// You can examine the data here
					printf("Got packet !\n");
					
					// we don't longer need the packet, so
					// we'll delete it
					sess.DeletePacket(pack);
				}
			} while (sess.GotoNextSourceWithData());
		}
		
		sess.EndDataAccess();

#ifndef RTP_SUPPORT_THREAD
		status = sess.Poll();
		checkerror(status);
#endif // RTP_SUPPORT_THREAD
		
		RTPTime::Wait(RTPTime(1,0));
	}
	
	sess.BYEDestroy(RTPTime(10,0),0,0);

#ifdef WIN32
	WSACleanup();
#endif // WIN32
	return 0;
}

