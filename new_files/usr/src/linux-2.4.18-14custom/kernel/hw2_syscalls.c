
#include <linux/sched.h>
#include <linux/errno.h>	// Need this for the definition of EINVAL

#define NO_SUCH_PROC

/**
 * For function declarations see hw2_syscalls.h
 */
int sys_is_SHORT(int pid) {

	// Use existing logic. If pid is not SHORT,
	// x will be set to -EINVAL anyway.
	int x = sys_remaining_trials(pid);
	return x > 0? 1 : x;
	
}


int sys_remaining_time(int pid) {
	
	// Get the task
	task_t* task = find_task_by_pid(pid);
	if (!task) return NO_SUCH_PROC;
	
	// Test input
	if (p->policy != SCHED_SHORT) return -EINVAL;
	
	// Return the data
	// NOTE: This depends on the fact that overdue processes
	// have a time-slice of zero!
	return p->time_slice;
	
}



int sys_remaining_trials(int pid) {
	
	// Get the task
	task_t* task = find_task_by_pid(pid);
	if (!task) return NO_SUCH_PROC;
	
	// Test input
	if (p->policy != SCHED_SHORT) return -EINVAL;
	
	// Return data
	return p->remaining_trials;
	
}



int sys_get_scheduling_statistic(struct switch_info* info) {
	
}



