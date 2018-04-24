#include <ifaddrs.h>

int main(void)
{
	struct ifaddrs *pIfaddr;

	getifaddrs(&pIfaddr);
	freeifaddrs(pIfaddr);

	return 0;
}
