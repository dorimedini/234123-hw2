
 #include "test_utils.h"
 #include <stdio.h>

/**
 * Call this to create multiple processes with the
 * same policy, storing the PID of the process in pid.
 *
 * This converts the parent process to the highest
 * prioriy SHORT
 *
 * Example:
 *
 * - int pid;
 * - CREATE_PROCS(pid,10,SCHED_SHORT,4000,5);
 *
 * This should create ten SHORT processes, each with 4
 * seconds requested_time and five trials, with the
 * return value of fork() stored in pid
 */
#define CREATE_PROCS(pid,num,policy,req,trials) do { \
		int i; \
		struct sched_param param = { .sched_priority = 0, .requested_time = 5000, .trial_num = 50}; \
		sched_setscheduler(getpid(), is_SHORT(getpid()) ? -1 : SCHED_SHORT, &param); \
		nice(-19); \
		for (i=0; i<num; ++i) { \
			pid = fork(); \
			if (!pid) { \
				param = { .sched_priority = 0, .requested_time = req, .trial_num = trials}; \
				sched_setscheduler(getpid(), policy, &param); \
				break; \
			} \
		} \
	} while(0)

/**
 * To destroy all processes created via CREATE_PROCS,
 * call this with the pid variable sent to CREATE_PROCS.
 * Only the original parent will execute the code after
 * this macro.
 *
 * Example:
 *
 * - ... code executed by one process...
 * - int pid;
 * - CREATE_PROCS(pid,10,SCHED_SHORT,4000,5);
 * - ... more code, 11 processes executing it...
 * - if (pid) CREATE_PROCS(pid,5,SCHED_OTHER, 0,0);
 * - ... more code, 16 processes executing it...
 * - EXIT_PROCS(pid);
 * - ... more code, only one process (the original process) executing it...
 *
*/
#define EXIT_PROCS(pid) do { \
		if (!pid) exit(0); \
		else while(wait() != -1); \
	} while(0)

#define BEST_SHORT(pid) do { \
		struct sched_param param = { .sched_priority = 0, .requested_time = 5000, .trial_num = 50}; \
		sched_setscheduler(pid, is_SHORT(pid) ? -1 : SCHED_SHORT, &param); \
		nice(-19); \
	} while(0)

/** Self explanatory */
int calculate_fibo(int num) {
	if (num < 2) return num;
	return calculate_fibo(num-1)+calculate_fibo(num-2);
}

int getRelevantLogger(int* relevantPids, int n ,
						struct switch_info* relevantLogger) {
	int index = 0;
	struct switch_info info[150];
	// Get the statistics
	int num_switches = get_scheduling_statistic(info);
	for (int i = 0 ; i < num_switches ; ++i) {
		if (findTheRelevant(relevantPids,n,info[i].previous_pid)) {
			relevantLogger[index] = info[i];
			++index;
		}
	}
	return index;
}

int findTheRelevant(int* relevantPids, int n,int thePidToFind) {
	for (int i = 0 ; i < n ; ++i) {
		if (relevantPids[i] == thePidToFind) {
			return 1;
		}
	}
	return 0;
}

/* An OTHER Proc created a new proc which became short. The new proc should run and then
 *	the old proc
*/
bool other_to_short() {
	int relevantPids[2];
	relevantPids[0] = getpid();
	int pid = fork(); // The major dad.
	if (!pid) {
		relevantPids[1] = getpid();
		BEST_SHORT(pid); // make the son short
		calculate_fibo(15);
	} else {
		calculate_fibo(30);
	}
	struct switch_info info[150];
	int size =  getRelevantLogger(relevantPids,2,info);

return true
}


int main() {
	int pid = fork();
	if (pid) {
		printf("Dad pid is %d\n", pid);
	} else {
		printf("Son pid is %d\n", pid);
	}

	return 0;
}