#include <unistd.h>

int main(void)
{
	char buf[256];
	getlogin_r(buf,256);
	return 0;
}

