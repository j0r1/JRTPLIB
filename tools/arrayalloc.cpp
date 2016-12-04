#include <sys/types.h>
#include <new>

inline void *operator new[](size_t numbytes, int memtype)
{
	return operator new[](numbytes);
}

inline void operator delete[](void *buffer, int memtype)
{
	operator delete[](buffer);
}

int main(void)
{
	int j = 10;
	char *a = new(j) char[10];
	return 0;
}
