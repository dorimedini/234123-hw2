
#include "test_utils.h"

/**
 * Call this to create multiple processes with the
 * same policy, storing the PID of the process in pid.
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
		for (i=0; i<num; ++i) { \
			pid = fork(); \
			if (pid) { \
				struct sched_param param = { .sched_priority = 0, .requested_time = req, .trial_num = trials}; \
				sched_setscheduler(pid, policy, &param); \
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


int main() {

	return 0;
}