#include <windows.h>	

int main(void)
{
	LARGE_INTEGER performanceCount;
	QueryPerformanceCounter(&performanceCount);
	return 0;
}
