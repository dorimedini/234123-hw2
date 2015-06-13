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

void work() {
	int i;
	for (i=0;i<1000000;i++);
}


//sched_setscheduler should fail with EPERM
int main() {
	int pid=getpid(),ret;
	struct sched_param param={0,30,1};
	sched_setscheduler(pid,4,&param);
	ret=sched_setscheduler(pid,0,&param);
	printf("sched_setscheduler return value: %d, errno: %s\n",ret,strerror(errno));
	ret=is_SHORT(pid);
	printf("is_SHORT return value: %d, errno: %s\n",ret,strerror(errno));
	while (is_SHORT(pid)>0) {
		work();
		printf("waiting to be overdue...\n");
	}
	ret=is_SHORT(pid);
	printf("is_SHORT return value: %d, errno: %s\n",ret,strerror(errno));
	ret=sched_setscheduler(pid,0,&param);
	printf("sched_setscheduler return value: %d, errno: %s\n",ret,strerror(errno));
	return 0;
}
