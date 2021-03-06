
#include <hw2_syscalls.h>	// For get_scheduling_statistic and some definitions
	
/**
 * Stringify the reason number
 */
char* reason2str(int res) {
	switch(res) {
		case 0://SWITCH_UNKNOWN:
			return "UNKNOWN";
		case 1://SWITCH_CREATED:
			return "PROC CREATED";
		case 2://SWITCH_ENDED:
			return "ENDED";
		case 3://SWITCH_YIELD:
			return "YIELDED";
		case 4://SWITCH_OVERDUE:
			return "OVERDUE";
		case 5://SWITCH_PREV_WAIT:
			return "STARTED WAITING";
		case 6://SWITCH_PRIO:
			return "HIGHER PRIO ARRIVED";
		case 7://SWITCH_SLICE:
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
void set_to_SHORT(int pid, int requested, int trials) {
	struct sched_param param = { .sched_priority = 0, .requested_time = requested, .trial_num = trials};
	if (is_SHORT(pid) != -1) {
		sched_setparam(pid, &param);
		return;
	}
	// Only an OTHER process can become a SHORT.
	// This shouldn't cause a context switch, because
	// we don't turn need_resched on for non-SHORT
	// processes in setscheduler
	if (sched_getscheduler(pid) != SCHED_OTHER) {
		sched_setscheduler(pid, SCHED_OTHER, &param);
	}
	sched_setscheduler(pid, SCHED_SHORT, &param);
}

/** Self explanatory */
long calculate_fibo(long num) {
	if (num < 2) return num;
	return calculate_fibo(num-1)+calculate_fibo(num-2);
}


/**
 * Prints the info in the switch_info array
 */
void print_info(struct switch_info* info, int total) {
	int i;
	printf("\nSTART PRINTING %d SWITCHES:\n", total);
	printf("===================================================================\n");
	printf("     PREV-PID - NEXT-PID - PREV-POL - NEXT-POL -   TIME   -  REASON\n");
	for (i=0; i<total; ++i) {
		printf("%3d: %7d  | %7d  | %8s | %8s | %7d  | %s\n",
				i, info[i].previous_pid, info[i].next_pid, policy2str(info[i].previous_policy), policy2str(info[i].next_policy), info[i].time, reason2str(info[i].reason));
	}
	printf("===================================================================\n");
	printf("END PRINT\n\n");
}


/**
 * Arguments should be passed like so:
 *
 * tester <NUM_TRIALS_1> <FIBO_NUM_1> <NUM_TRIALS_2> <FIBO_NUM_2> <... etc.
 */
int main(int argc, char** argv) {

	// Declare the logger!
	struct switch_info info[150];
	
	// Make ourselves RT, so we get top priority
	struct sched_param param = { .sched_priority = 80 };
	sched_setscheduler(getpid(), SCHED_RR, &param);
	
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
			printf("trials[%d]==%d or numbers[%d]==%d are illegal values\n", i, trials[i], i, numbers[i]);
			return 2;	// Bad input
		}
	}
	
	// Create processes
	int pids[total];
	for (i=0; i<total; ++i) {
		pids[i] = fork();
		if (!pids[i]) {	// Let each child:
			calculate_fibo(numbers[i]);						// And then calculate a Fibonacci number
			exit(0);
		}
		else {			// Make the RT father:
			set_to_SHORT(pids[i], 100, trials[i]);	// Turn the child into a SHORT process (may need to turn into an OTHER first)
		}
	}
	
	// Wait for them
	int status;
	while (wait(&status) != -1);
	
	// Output basic info
	printf("\nGoing to calculate the following Fibonacci numbers (#:trials-number):\n");
	for (i=0; i<total; ++i) {
		printf("#%d:%d-%d   ",i,trials[i],numbers[i]);
	}
	printf("\nParticipating SHORTs:\n");
	for (i=0;i<total;++i) {
		printf("#%d:%d   ",i,pids[i]);
	}
	
	// Get the statistics
	int num_switches = get_scheduling_statistic(info);
	
	// Print them
	print_info(info, num_switches);
	
	return 0;
}
