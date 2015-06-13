#include "hw2_syscalls.h"
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

//should print an error 3 times
int main() {
	int pid=-1,ret;
	ret=is_SHORT(pid);
	printf("is_SHORT return value: %d, errno: %s\n",ret,strerror(errno));
	ret=remaining_time(pid);
	printf("remaining_time return value: %d, errno: %s\n",ret,strerror(errno));
	ret=remaining_trials(pid);
	printf("remaining_trials return value: %d, errno: %s\n",ret,strerror(errno));
	return 0;
}

