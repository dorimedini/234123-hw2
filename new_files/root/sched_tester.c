
#include <linux/sched.h>	// Need this for definition of hw2_switch_info and SCHED_SHORT and such
#include <hw2_syscalls.h>	// For get_scheduling_statistic

#define REQUESTED 100

/**
 * Sets the calling process to SHORT with the
 * designated number of trials
 */
void set_to_SHORT(int pid, int trials) {
	struct sched_param param = { .sched_priority = 0, .requested_time = REQUESTED, .trial_num = trials};
	sched_setscheduler(pid, SCHED_SHORT, &param);
}

/** Self explanatory */
int calculate_fibo(int num) {
	if (num < 2) return num;
	return calculate_fibo(num-1)+calculate_fibo(num-2);
}


/**
 * Prints the info in the hw2_switch_info array
 */
void print_info(hw2_switch_info* info, int total) {
	int i;
	printf("START PRINTING %d SWITCHES:\n", total);
	for (i=0; i<total; ++i) {
		printf("  PREV-PID: %d\n  NEXT-PID: %d\n  PREV-POL: %d\n  NEXT-POL: %d\n      TIME: %lu\n    REASON: %d\n",
				info[i].previous_pid, info[i].next_pid, info[i].previous_policy, info[i].next_policy, info[i].time, info[i].reason);
	}
	printf("END PRINT\n");
}


/**
 * Arguments should be passed like so:
 *
 * tester <NUM_TRIALS_1> <FIBO_NUM_1> <NUM_TRIALS_2> <FIBO_NUM_2> <... etc.
 */
int main(int argc, char** argv) {

	// Declare the logger!
	hw2_switch_info info[150];
	
	// Parse the input
	if (!(argc%2)) return 1;	// Even number of arguments? Illegal!
	int total = (argc-1)/2;
	int trials[total], numbers[total];
	int i;
	for (i=0; i<total; ++i) {
		trials[i] = atoi(argv[2*i+1]);
		numbers[i] = atoi(argv[2*i+2]);
		if (trials[i] < 1 || numbers[i] < 0) return 2;	// Bad input
	}
	
	// Create processes
	int pids[total];
	for (i=0; i<total; ++i) {
		int pid = fork();
		if (!pid) {	// Let each child:
			set_to_SHORT(pid, trials[i]);	// Turn into a SHORT process
			calculate_fibo(numbers[i]);		// And then calculate a Fibonacci number
			exit(0);
		}
		else {	// And let the father:
			pids[i] = pid;	// Remember the child
		}
	}
	
	// Wait for them
	while (wait(NULL) != -1);
	
	// Get the statistics
	int num_switches = get_scheduling_statistic(info);
	
	// Print them
	print_info(info, num_switches);
	
	return 0;
}