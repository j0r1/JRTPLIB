#include "rtptimeutilities.h"
#include "rtpudpv4transmitter.h"
#include <iostream>

int main(void)
{
	jrtplib::RTPUDPv4Transmitter trans(0);
	jrtplib::RTPTime t(0);
	std::cout << t.GetDouble() << std::endl;
	return 0;
}
