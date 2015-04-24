#ifndef _HW2_SYSCALLS_H
#define _HW2_SYSCALLS_H

#include <errno.h>
#include <linux/sched.h>	// Need this for definition of hw2_switch_info
extern int errno;

/**
 * Returns 1 if the input PID is a SHORT-process,
 * 0 if it's overdue.
 * If it's neither, returns -1 and sets errno to EINVAL
 */
int is_SHORT(int);


/**
 * Returns the remaining time in the timeslice. If overdue,
 * returns 0 (NOTE: a non-overdue SHORT cannot return 0
 * here! When the process reaches zero it deducts 1 from the 
 * remaining trials and sets a new timeslice).
 * If it's neither SHORT nor overdue, returns -1 and sets errno
 * to EINVAL.
 */
int remaining_time(int);


/**
 * Returns the remaining trials left for the process. If 
 * overdue, returns 0.
 * If it's neither SHORT nor overdue, returns -1 and sets errno
 * to EINVAL.
 */
int remaining_trials(int);


/**
 * Call this function to get the switch logging info from
 * the kernel.
 */
int get_scheduling_statistic(hw2_switch_info*);


// Use this to get the correct return value from a system call.
// If a system call returns in error, it will return the correct
// value of ERRNO in negative format.
long convert_to_errno(long res) {
	if ((unsigned long)(res) >= (unsigned long)(-125)) {
		errno = -(res); res = -1;
	}
	return res;
}

// Use this to quickly set return values and errno
int positive_or_errno(int res) {
	if (res<0) {
		errno = -res;
		return -1;
	}
	return res;
}


/**
 * Function wrappers
 */
int is_SHORT_wrapper(int);
int remaining_time_wrapper(int);
int remaining_trials_wrapper(int);
int get_scheduling_statistic_wrapper(hw2_switch_info*);


/**
 * Implementation
 */
int is_SHORT(int pid) {
	return positive_or_errno(is_SHORT_wrapper(pid));
}
int remaining_time(int pid) {
	return positive_or_errno(remaining_time_wrapper(pid));
}
int remaining_trials(int pid) {
	return positive_or_errno(remaining_trials_wrapper(pid));
}
int get_scheduling_statistic(hw2_switch_info* info) {
	return positive_or_errno(get_scheduling_statistic_wrapper(info));
}



/**
 * Wrapper implementation
 */
int is_SHORT_wrapper(int pid) {
	long __res;
	__asm__ volatile (
		"movl $243, %%eax;"				// System call number
		"movl %1, %%ebx;"				// Send "limit" as a parameter
		"int $0x80;"					// System interrupt
		"movl %%eax,%0"					// Store the returned value
		: "=m" (__res)					// in __res
		: "m" ((long)pid)				// Input (%1 in the stack)
		: "%eax","%ebx"					// Registers used
	);
	return (int)convert_to_errno(__res);
}
int remaining_time_wrapper(int pid) {
	long __res;
	__asm__ volatile (
		"movl $244, %%eax;"				// System call number
		"movl %1, %%ebx;"				// Send "limit" as a parameter
		"int $0x80;"					// System interrupt
		"movl %%eax,%0"					// Store the returned value
		: "=m" (__res)					// in __res
		: "m" ((long)pid)				// Input (%1 in the stack)
		: "%eax","%ebx"					// Registers used
	);
	return (int)convert_to_errno(__res);
}
int remaining_trials_wrapper(int pid) {
	long __res;
	__asm__ volatile (
		"movl $245, %%eax;"				// System call number
		"movl %1, %%ebx;"				// Send "limit" as a parameter
		"int $0x80;"					// System interrupt
		"movl %%eax,%0"					// Store the returned value
		: "=m" (__res)					// in __res
		: "m" ((long)pid)				// Input (%1 in the stack)
		: "%eax","%ebx"					// Registers used
	);
	return (int)convert_to_errno(__res);
}
int get_scheduling_statistic_wrapper(hw2_switch_info* info) {
	long __res;
	__asm__ volatile (
		"movl $245, %%eax;"				// System call number
		"movl %1, %%ebx;"				// Send "limit" as a parameter
		"int $0x80;"					// System interrupt
		"movl %%eax,%0"					// Store the returned value
		: "=m" (__res)					// in __res
		: "m" ((long)info)				// Input (%1 in the stack)
		: "%eax","%ebx"					// Registers used
	);
	return (int)convert_to_errno(__res);
}




#endif
