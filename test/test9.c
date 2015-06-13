#include "hw2_syscalls.h"
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

struct sched_param {
	int sched_priority;
	int requested_time;
	int trial_num;
};

int sched_setscheduler(pid_t pid, int policy, const struct sched_param *param);

#define N 100

//expected output: 00...011...122...233...3
int main() {
	int fd[2];
	pipe(fd);
	int i;
	for (i=0;i<N;i++) {
		if (fork()==0) {
			struct sched_param param={0,10,1};
			sched_setscheduler(getpid(),4,&param);
			kill(getpid(),SIGSTOP); //don't begin before all SHORTs were created
			write(fd[1],"0",1);
			while(is_SHORT(getpid()));
			write(fd[1],"1",1);
			sleep(1);
			write(fd[1],"2",1);
			sleep(1);
			write(fd[1],"3",1);
			return 0;
		}
	}
	sleep(1); //make sure all children got SIGSTOP
	kill(0,SIGCONT); //wake up children
	while (wait(NULL)>=0); //wait for children
	char buffer[4*N+1]={0};
	int len=read(fd[0],buffer,4*N);
	printf("pipe contained %d bytes: %s\n",len,buffer);
	return 0;
}

