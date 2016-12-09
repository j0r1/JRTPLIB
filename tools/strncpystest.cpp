#include <string.h>

int main(void)
{
	char hostname[1024];
	strncpy_s(hostname,1024,"localhost",_TRUNCATE);
	return 0;
}
