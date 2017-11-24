#include "unistd.h"

int dup(int oldfd)
{
    return fcntl(oldfd, F_DUPFD, 0);
}

int dup2(int oldfd, int newfd)
{
	if (oldfd == newfd)
		return newfd;

    close(newfd);
	return fcntl(oldfd, F_DUPFD, newfd);
}
