#define _CRT_RAND_S  
#include <stdlib.h>  

int main(void)
{
	unsigned int number = 0;  
	errno_t err = rand_s(&number);
	return 0;
}
