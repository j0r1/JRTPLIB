#include <winsock2.h>	
#include <ws2tcpip.h>

int main(void)
{
	WSADATA wsaData;
	WORD version = MAKEWORD(2, 2);
	//WSAStartup(version, &wsaData); // Would need to specify winsock lib for linking
	return 0;
}
