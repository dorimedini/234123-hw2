#include "hw2_syscalls.h"
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

struct sched_param {
	int sched_priority;
	int requested_time;
	int trial_num;
};

int sched_setscheduler(pid_t pid, int policy, const struct sched_param *param);

//should return 1,125,3
int main() {
	int pid=getpid(),ret;
	struct sched_param param={0,125,3};
	sched_setscheduler(pid,4,&param);
	ret=is_SHORT(pid);
	printf("is_SHORT return value: %d, errno: %s\n",ret,strerror(errno));
	ret=remaining_time(pid);
	printf("remaining_time return value: %d, errno: %s\n",ret,strerror(errno));
	ret=remaining_trials(pid);
	printf("remaining_trials return value: %d, errno: %s\n",ret,strerror(errno));
	return 0;
}
