#include <poll.h>

int main(void)
{
	struct pollfd pfd = { 0, 0, 0};
	int status = poll(&pfd, 1, 0);
	return status;
}
