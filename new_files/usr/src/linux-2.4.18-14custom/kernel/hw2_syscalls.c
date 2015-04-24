
#include <linux/sched.h>	// Need this for definition of hw2_switch_info
#include <linux/errno.h>	// Need this for the definition of EINVAL etc.

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



int sys_get_scheduling_statistic(hw2_switch_info* info) {
	
	int i, counter;

	// If all indices are full, start with the oldest (hw2_logger.next_index).
	// Otherwise, start with the oldest (zero).
	i = hw2_logger.logged == 150? hw2_logger.next_index : 0;
	
	// Iterate and copy (copy exactly hw2_logger.logged entries)
	for(counter = 0; counter < hw2_logger.logged; counter++) {
	
		// The copy_to_user function returns the number of bytes NOT copied,
		// so if this value isn't zero we must return an error
		if (copy_to_user(info+counter, &(hw2_logger.arr[i]), sizeof(hw2_logger.arr[i]))) return -EFAULT;
	}
	
	// This should always be the number of copied entries
	return hw2_logger.logged;
	
}



