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


//child should print first
int main() {
	int pid=fork();
	if (pid==0) {
		int i;
		for (i=0;i<30;i++) {
			work();
			printf("child!\n");
		}
	} else {
		struct sched_param param={0,1000,10};
		sched_setscheduler(pid,4,&param);
		printf("parent! (this should be the last print)\n");
	}
	return 0;
}
