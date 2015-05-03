
#include <hw2_syscalls.h>	// For get_scheduling_statistic and some definitions

#define REQUESTED 100


/**
 * Stringify the reason number
 */
char* reason2str(int res) {
	switch(res) {
		case 0:
			return "UNKNOWN";
		case 1:
			return "CREATED";
		case 2:
			return "ENDED";
		case 3:
			return "YIELD";
		case 4:
			return "OVERDUE";
		case 5:
			return "WAITING";
		case 6:
			return "BACK FROM WAIT";
		case 7:
			return "OUT OF TIME";
		default:
			return "ILLEGAL REASON VALUE";
	}
}

/**
 * Stringify the policy.
 * For printing reasons, must be no more than 8 characters
 */
char* policy2str(int pol) {
	switch(pol) {
		case 0:
			return "OTHER";
		case 1:
		case 2:
			return "RT";
		case 4:
			return "SHORT";
		default:
			return "ILLEGAL";
	}
}

 
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
	printf("     PREV-PID - NEXT-PID - PREV-POL - NEXT-POL -   TIME   -  REASON\n");
	for (i=0; i<total; ++i) {
		printf("%3d: %7d  | %7d  | %8s | %8s | %7d  | %s\n",
				i, info[i].previous_pid, info[i].next_pid, policy2str(info[i].previous_policy), policy2str(info[i].next_policy), info[i].time, reason2str(info[i].reason));
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
	if (!(argc%2)) {
		printf("Odd number of input arguments...\n");
		return 1;	// Even number of arguments? Illegal!
	}
	int total = (argc-1)/2;
	int trials[total], numbers[total];
	int i;
	for (i=0; i<total; ++i) {
		trials[i] = atoi(argv[2*i+1]);
		numbers[i] = atoi(argv[2*i+2]);
		if (trials[i] < 1 || trials[i] > 50 || numbers[i] < 0) {
			printf("trials[%d]==%d or numbers[%d]==%d are illegl values\n", i, trials[i], i, numbers[i]);
			return 2;	// Bad input
		}
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
	while (wait() != -1);
	
	// Get the statistics
	int num_switches = get_scheduling_statistic(info);
	
	// Print them
	print_info(info, num_switches);
	
	return 0;
}
