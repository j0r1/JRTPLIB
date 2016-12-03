#if defined(WIN32) || defined(_WIN32_WCE)
	#include <winsock2.h>	
	#include <ws2tcpip.h>
	#ifndef _WIN32_WCE
		#include <sys/types.h>
	#endif // !_WIN32_WCE
#else 
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
#endif // WIN32 || _WIN32_WCE

int main(void)
{
	int testval;
	struct ip_mreq mreq,mreq2;
	mreq = mreq2; // avoid 'unused variable'
	testval = IP_MULTICAST_TTL;
	testval = IP_ADD_MEMBERSHIP;
	return 0;
}
