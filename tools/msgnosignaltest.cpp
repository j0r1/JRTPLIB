#ifdef RTP_SOCKETTYPE_WINSOCK
	#include <winsock2.h>	
#else 
	#include <sys/types.h>
	#include <sys/socket.h>
#endif // RTP_SOCKETTYPE_WINSOCK

int main(void)
{
	return MSG_NOSIGNAL;
}
