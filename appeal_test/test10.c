#include "hw2_syscalls.h"
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#define N 6 //can't be greater, because too much forking will make us overdue.
int nices_array[N] ={8,9,6,11,1,2}; //chosen randomly

//expected output: 4,5,2,0,1,3
int main() {
	struct sched_param param={0,1000,50};
	sched_setscheduler(getpid(),4,&param);
	int fd[2];
	pipe(fd);
	int i;
	for (i=0;i<N;i++) {
		int pid=fork();
		if (pid==0) {
			nice(nices_array[i]);
//			sched_yield();
			write(fd[1],&i,sizeof(int));
			return 0;
		}
	}
	while (wait(NULL)>=0); //wait for children
	int arr[N];
	read(fd[0],arr,N*sizeof(int));
	for (i=0;i<N;i++) printf("%d,",arr[i]);
	printf("\n");
	return 0;
}
