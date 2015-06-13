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



//expected output: A(10),B(5),B(5),A(5),C(3),B(2)
int main() {
	char me='A';
	struct sched_param param={0,1000,10};
	sched_setscheduler(getpid(),4,&param);
	int i;
	for (i=0;i<2;i++) {
		printf("%c (before fork): number of trials: %d\n",me,remaining_trials(getpid()));
		int pid=fork();
		if (pid>0) {
			printf("%c (after fork): number of trials: %d\n",me,remaining_trials(getpid()));
			return 0;
		}
		else {
			me++;
			printf("%c (just forked): number of trials: %d\n",me,remaining_trials(getpid()));
		}
	}
	return 0;
}
