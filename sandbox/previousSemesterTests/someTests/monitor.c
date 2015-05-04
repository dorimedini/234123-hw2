#include "hw2_syscalls.h"
#include <stdlib.h>
#include <stdio.h>

int fibonaci(int n)
{
	if (n < 2)
	{
		return n;
	}
	return fibonaci(n-1) + fibonaci(n-2);
}


const char* policies[] =
{
			"SCHED_OTHER",
			"SCHED_FIFO",
			"SCHED_RR",
			"SCHED_LSHORT",
			"Default"
};

const char* reasons[] =
{
			"Default",
			"A task was created",
			"A task ended",
			"A task yields the CPU",
			"An LSHORT process became overdue",
			"A previous task goes out for waiting",
			"A change in the scheduling params",
			"A task with higher priority returns from waiting",
			"The time slice of the previous task has ended"
};

int main(int argc, char *argv[])
{
	char *endptr;
	int status;
	int time;
	int level;
	int num;
	int id;
	int i = 1;
	if (argc % 3 != 0)
	{
		argc -= argc % 3;
	}
	while (i < argc)
	{
		// Parse the arguments from strings to integers
		time = strtol(argv[i], &endptr, 10);
		i++;
		level = strtol(argv[i], &endptr, 10);
		i++;
		num = strtol(argv[i], &endptr, 10);
		i++;
		// Create a child process which will run the fibonaci as an LSHORT
		id = fork();
		if (id != 0) {
			struct sched_param param1;
			param1.lshort_params.requested_time = time;
			param1.lshort_params.level = level;
			sched_setscheduler(id, SCHED_LSHORT, &param1);
		}
		else {
			sched_yield();
			fibonaci(num);
			_exit(0);
		}
	}
	while (wait(&status) != -1);		// Wait for all sons to finish
	struct switch_info info[150];

	for (i=0 ; i<150 ; i++) {
		info[i].time=0;
		info[i].previous_pid=0;
		info[i].previous_policy=0;
		info[i].next_pid=0;
		info[i].next_policy=0;
		info[i].reason=8;
	}
	
	int result = get_scheduling_statistic(info);
	if (result > 150)
		result = 150;
	// Print the output
	printf("|Time\t|Prev\t|PreviousPolicy\t|Next\t|Next Policy\t|Reason\n");
	for (i=0; i < result; i++)
	{
		printf("%lu\t|%d\t|", info[i].time, info[i].previous_pid);
		printf("%s", policies[info[i].previous_policy]);
		printf("\t|%d\t|", info[i].next_pid);
		printf("%s", policies[info[i].next_policy]);
		printf("\t|");
		printf("%s", reasons[info[i].reason]);
		printf("\n");
	}
}
