
#include <asm/uaccess.h>	// Need this for copy_to_user()
#include <linux/sched.h>	// Need this for definition of switch_info
#include <linux/errno.h>	// Need this for the definition of EINVAL etc.
#define NO_SUCH_PROC -EINVAL// At the time I didn't know the required error if no such process exists

/**
 * Declare a logger to be used with any function requiring it,
 * and initialize it's fields.
 * To use it in another file write:
 *
 * <code>extern hw2_switch_log hw2_logger;</code>
 *
 */
hw2_switch_log hw2_logger = { .next_index = 0, .logged = 0, .remaining_switches = 0};


/**
 * Declare the functions.
 * For function declarations and documentation see hw2_syscalls.h
 */
int sys_is_SHORT(int);
int sys_remaining_time(int);
int sys_remaining_trials(int);
int sys_get_scheduling_statistic(struct switch_info*);

/**
 * Implementation:
 */
int sys_is_SHORT(int pid) {

	// Get the task and return the policy
	task_t* task = find_task_by_pid(pid);
	if (task->policy != SCHED_SHORT) return -EINVAL;
	return task->remaining_trials ? 1 : 0;
}


int sys_remaining_time(int pid) {
	
	// Get the task
	task_t* task = find_task_by_pid(pid);
	if (!task) return NO_SUCH_PROC;
	
	// Test input
	if (task->policy != SCHED_SHORT) return -EINVAL;
	
	// Return the data
	// NOTE: This depends on the fact that overdue processes
	// have a time-slice of zero!
	return hw2_ticks_to_ms(task->time_slice);
	
}



int sys_remaining_trials(int pid) {
	
	// Get the task
	task_t* task = find_task_by_pid(pid);
	if (!task) return NO_SUCH_PROC;
	
	// Test input
	if (task->policy != SCHED_SHORT) return -EINVAL;
	
	// Return data
	return task->remaining_trials;
	
}



int sys_get_scheduling_statistic(struct switch_info* info) {
	
	int i, counter;

	// If all indices are full, start with the oldest (hw2_logger.next_index).
	// Otherwise, start with the oldest (zero).
	i = hw2_logger.logged == 150? hw2_logger.next_index : 0;
	
	// Iterate and copy (copy exactly hw2_logger.logged entries)
	for(counter = 0; counter < hw2_logger.logged; counter++) {
	
		// The copy_to_user function returns the number of bytes NOT copied,
		// so if this value isn't zero we must return an error
		if (copy_to_user(info+counter, &(hw2_logger.arr[i]), sizeof(hw2_logger.arr[i]))) {
			PRINT("Couldn't copy to user!\ni=%d, counter=%d, hw2_logger.logged=%d, hw2_logger.arr[i].prev_pid=%d\n",
											i,    counter,    hw2_logger.logged,    hw2_logger.arr[i].previous_pid);
			return -EFAULT;
		}
		i = (i+1)%150;
	}
	
	// This should always be the number of copied entries
	PRINT_IF(hw2_logger.logged<0,"hw2_logger.logged=%d! this is bad...\n",hw2_logger.logged);
	return hw2_logger.logged;
	
}



