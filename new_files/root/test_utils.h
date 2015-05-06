/*
 * Test_Utils.h
 *
 *  Created on: Apr 30, 2015
 *      Author: shimonaz
 */

#ifndef TEST_UTILS_H_
#define TEST_UTILS_H_

#define HZ 100

#include <stdio.h>


#define TEST_EQUALS(a, b) if (((a) != (b))) { \
								return 0; \
							}

#define TEST_DIFFERENT(a, b) if (((a) == (b))) { \
								return 0; \
							}

#define TEST_TRUE(bool) if ((!(bool))) { \
								return 0; \
							}

#define TEST_FALSE(bool) if ((bool)) { \
								return 0; \
							}

#define RUN_TEST(name)  printf("Running "); \
						printf(#name);		\
						printf("... ");		\
						if (!name()) { \
							printf("[FAILED]\n");		\
							return -1; \
						}								\
						printf("[SUCCESS]\n");


#define SWITCH(proc1,proc2) if ((should_switch((proc1),(proc2)) != 1)) { \
									return 0; \
								}


#define NO_SWITCH(proc1,proc2) if ((should_switch((proc1),(proc2)) != 0)) { \
									return 0; \
								}


enum SWITCH_REASONS {
	SWITCH_UNKNOWN = 0,	// 0: Reason unknown - should cause error
	SWITCH_CREATED,		// 1: Switched due to creation of another task
	SWITCH_ENDED,		// 2: A task ended
	SWITCH_YIELD,		// 3: A task gave up the CPU
	SWITCH_OVERDUE,		// 4: A SHORT task became overdue
	SWITCH_PREV_WAIT,	// 5: The task switched from the CPU because it started waiting
	SWITCH_PRIO,		// 6: A task with a higher priority returned from waiting
	SWITCH_SLICE		// 7: The previous task ran out of time
};



void doLongTask()
{
	long i;
	for (i=1; i > 0; i++)
	{
		;
	}
}

void doShortTask()
{
	short i;
	for (i=1; i != 0; i++)
	{
		;
	}
}

void doMediumTask()
{
	int j;
	for(j=0; j<4000; j++)
	{
		doShortTask();
	}
}




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
 *
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
*/
/*
#define CREATE_PROC(pid,policy,req,trials,nice,relevantPids,index) do { \
		int i; \
		pid = fork(); \
		if (!pid) { \
			param = { .sched_priority = 0, .requested_time = req, .trial_num = trials}; \
			sched_setscheduler(getpid(), policy, &param); \
			nice(nice);
			break; \
		} \
		relevantPids[index] = pid; \
	} while(0)
*/
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
		int status;
		if (!pid) exit(0); \
		else while(wait(&status) != -1); \
	} while(0)

/*
#define REMEMBER_RELEVANT(pid, relevantPids, index) do { \
		relevantPids[index] = pid;
	} while(0)
*/

#define CHANGE_TO_SHORT(pid,requested,trials) do { \
		struct sched_param param = { .sched_priority = 0, .requested_time = requested, .trial_num = trials}; \
		sched_setscheduler(pid, is_SHORT(pid) ? -1 : SCHED_SHORT, &param); \
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

int findTheRelevant(int* relevantPids, int n,int thePidToFind) {
	int i;
	for (i = 0 ; i < n ; ++i) {
		if (relevantPids[i] == thePidToFind) {
			return 1;
		}
	}
	return 0;
}

/*
int getRelevantLogger(int* relevantPids, int n ,
						struct switch_info* relevantLogger, bool print) {
	int index = 0;
	struct switch_info info[150];
	// Get the statistics
	int num_switches = get_scheduling_statistic(info);
	int i;
	for (i = 0 ; i < num_switches ; ++i) {
		if (findTheRelevant(relevantPids,n,info[i].previous_pid)) {
			relevantLogger[index] = info[i];
			++index;
		}
	}
	if (print) {
		print_info(info, num_switches);
		printf ("\n\n The Relevant Logger:\n");
		print_info(relevantLogger, index);
	}
	return index;
}
*/
/*
void set_to_SHORT_with_prio(int pid, int requested, int trials, int setNice) {
	struct sched_param param = { .sched_priority = 0, .requested_time = requested, .trial_num = trials};
	sched_setscheduler(pid, SCHED_SHORT, &param);
	nice(setNice);
}
*/
/* Here we check if the switches was (in this order)
 * testProc
 * testProc
 * testProc
 * ..
 * ..
 * someProc
 * someProc
 * ... 
 * ...
 * testProc
 * testProc
 * testProc
 *
bool checkTheReleventSwitches(int testProc, int someProc,struct switch_info* info, int size) {
	int startSeenSon = 0;
	int endSeenSon = 0;
	// Now we have only the dad and son switches 
	for (int i=0 ; i < size ; ++i) {
		if (!startSeenSon) {
			if (info[i].previous_pid == testProc) {
				continue;
			} else {
				startSeenSon = 1;
			}
		} else if (!endSeenSon) {
			if (info[i].previous_pid == someProc) {
				continue;
			} else {
				endSeenSon = 1;
			}
		} else {
			if (info[i].previous_pid == testProc) {
		 		continue;
		 	} else {
		 		return false;			 	
		 	}	
		}
	}
	return true;
}

*/



#endif /* TEST_UTILS_H_ */
