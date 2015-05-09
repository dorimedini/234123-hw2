
#include <hw2_syscalls.h>	// For get_scheduling_statistic and some definitions
#include <errno.h>			// In case these aren't included from hw2_syscalls.h
#include <sched.h>			// MAKE SURE THE USERSPACE VERSION OF THIS FILE DECLARES SCHED_PARAM CORRECTLY

#define PRIO_PROCESS 0		// This constant is needed for setting the nice() value of a different process

// Wait for all children
#define WAIT() do { \
		int status; \
		while(wait(&status) != -1); \
	} while(0)

// Given an array, create num children
// and put their IDs in the array
#define FORK(arr,num) do { \
		int i; \
		for (i=0; i<num; ++i) { \
			arr[i] = fork(); \
			if (!arr[i]) break; \
		} \
	} while(0)

// Declare a pids[num] array and create
// num children, putting their IDs in pids[]
#define FORK_PIDS(num) \
	int pids[num]; \
	do { \
		int i; \
		for (i=0;i<num;++i) pids[i]=0; \
		FORK(pids,num); \
	} while(0)

// BE CAREFUL!
// Only continues execution after this
// command if the process is SHORT!
#define WAIT_UNTIL_SHORT() do { \
		int pid = getpid(); \
		while(is_SHORT(pid) == -1); \
	} while(0)

// Run until a specific trial number is
// reached.
// Does nothing if the process is overdue
// or not SHORT
#define REACH_TRIAL(num) do { \
		while(remaining_trials(getpid())>num && is_SHORT(getpid())==1); \
	} while(0)

// Run until a specific time slice value is
// reached.
// Does nothing if the process is overdue
// or not SHORT
#define REACH_TIME(num) do { \
		while(remaining_time(getpid())>num && is_SHORT(getpid())==1); \
	} while(0)


// Run until a specific trial number is
// reached. During execution, every time
// a trial is used up, the input is printed.
// Apart from the first argument, use like printf().
// EXAMPLE:
// - PRINT_EACH_TRIAL_UNTIL(10,"process %d has finished a trial!",getpid());
// Does nothing if the process is overdue
// or not SHORT
#define PRINT_EACH_TRIAL_UNTIL(trial,...) do { \
		int i, tr = remaining_trials(getpid()); \
		for (i=tr-1;i>=trial;i--) { \
			REACH_TRIAL(i); \
			printf(##__VA_ARGS__); \
		} \
	} while(0)

// Like the above, but runs until we become overdue
#define PRINT_EACH_TRIAL(...) do { \
		PRINT_EACH_TRIAL_UNTIL(0,##__VA_ARGS__); \
	} while(0)

// This function returns TRUE if the caller
// has filled the pids[] array - if it was used
// with the above macros, the result is the
// answer to "is the caller the original RT parent
// of all the child procs"?
// Usually, we can replace a call to this function
// with (sched_getscheduler(getpid()) == SCHED_RR)
// but that's implementation based
int is_father(int* pids, int num) {
	int i;
	for (i=0; i<num; ++i) {
		if (!pids[i]) return 0;
	}
	return 1;
}
	
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


void set_all_to_SHORT(int* pids, int num, int requested, int trials) {
	int i;
	for (i=0; i<num; ++i)
		set_to_SHORT(pids[i],requested,trials);
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

void oneone() {
	FORK_PIDS(2);	// Now have an array pids[] with two sons
	if (is_father(pids,2)) {
		set_all_to_SHORT(pids,2,200,4);
	}
	else if(!pids[0]) { // First child
		nice(-10);
		WAIT_UNTIL_SHORT();
		printf("\tHigh prio (%d) starting\n", getpid());
		PRINT_EACH_TRIAL("High prio (%d) at trial %d\n",getpid(),remaining_trials(getpid()));
		printf("\tHigh prio (%d) done\n",getpid());
		exit(0);
	}
	else {				// Second child
		WAIT_UNTIL_SHORT();
		printf("\tLow prio (%d) starting\n", getpid());
		PRINT_EACH_TRIAL("Low prio (%d) at trial %d\n",getpid(),remaining_trials(getpid()));
		printf("\tLow prio (%d) done\n",getpid());
		exit(0);
	}
	WAIT();
}

void onethenone() {
	FORK_PIDS(2);	// Now have an array pids[] with two sons
	if (is_father(pids,2)) {
		set_all_to_SHORT(pids,2,100,8);
	}
	else if(!pids[0]) { // First child
		WAIT_UNTIL_SHORT();
		printf("\tNormal prio (%d) starting\n", getpid());
		PRINT_EACH_TRIAL_UNTIL(4,"\tNormal prio (%d) got to trial %d\n",getpid(),remaining_trials(getpid()));
		printf("\tBecoming high prio (%d)\n",getpid());
		nice(-10);
		PRINT_EACH_TRIAL("\tHigh prio (%d) got to trial %d\n",getpid(),remaining_trials(getpid()));
		printf("\tHigh prio (%d) done\n",getpid());
		exit(0);
	}
	else {				// Second child
		WAIT_UNTIL_SHORT();
		printf("\tLow prio (%d) starting\n", getpid());
		PRINT_EACH_TRIAL("\tLow prio (%d) got to trial %d\n",getpid(),remaining_trials(getpid()));
		printf("\tLow prio (%d) done\n",getpid());
		exit(0);
	}
	WAIT();
}

void twoone() {
	FORK_PIDS(3);	// Now have an array pids[] with two sons
	if (is_father(pids,3)) {
		set_all_to_SHORT(pids,3,100,8);
	}
	else if(!pids[0]) { // First child
		WAIT_UNTIL_SHORT();
		printf("\tNormal prio (%d) starting\n", getpid());
		PRINT_EACH_TRIAL_UNTIL(4,"\tNormal prio (%d) got to trial %d\n",getpid(),remaining_trials(getpid()));
		printf("\tBecoming high prio (%d)\n",getpid());
		nice(-10);
		PRINT_EACH_TRIAL("\tHigh prio (%d) got to trial %d\n",getpid(),remaining_trials(getpid()));
		printf("\tHigh prio (%d) done\n",getpid());
		exit(0);
	}
	else if(!pids[1]) {	// Second child
		WAIT_UNTIL_SHORT();
		printf("\tLow prio #1 (%d) starting\n", getpid());
		PRINT_EACH_TRIAL("\tLow prio #1 (%d) got to trial %d\n",getpid(),remaining_trials(getpid()));
		printf("\tLow prio #1 (%d) done\n",getpid());
		exit(0);
	}
	else {				// Third child
		WAIT_UNTIL_SHORT();
		printf("\tLow prio #2 (%d) starting\n", getpid());
		PRINT_EACH_TRIAL("\tLow prio #2 (%d) got to trial %d\n",getpid(),remaining_trials(getpid()));
		printf("\tLow prio #2 (%d) done\n",getpid());
		exit(0);
	}
	WAIT();
}

void onetwo() {

	// This time, the parent RT process needs to set
	// the children's priority, because otherwise the first
	// child who gives itself a higher priority will run
	// and finish before the other child can give itself
	// a high priority.

	FORK_PIDS(3);		// Now have an array pids[] with two sons
	if (is_father(pids,3)) {
		// IMPORTANT: Give the SHORTs a timeslice s.t. the first few trials take
		// less than one second, but the total time takes more than one second.
		// This is because the RT parent needs to sleep for a bit
		set_all_to_SHORT(pids,3,600,8);
		sleep(1);
		printf("\tBecoming high prio #1 (%d)\n",getpid());
		setpriority(PRIO_PROCESS,pids[0],-10);
		printf("\tBecoming high prio #2 (%d)\n",getpid());
		setpriority(PRIO_PROCESS,pids[1],-10);
	}
	else if(!pids[0]) { // First child
		WAIT_UNTIL_SHORT();
		printf("\tNormal prio #1 (%d) starting\n", getpid());
		PRINT_EACH_TRIAL("\tHigh prio #1 (%d) got to trial %d\n",getpid(),remaining_trials(getpid()));
		printf("\tHigh prio #1 (%d) done\n",getpid());
		exit(0);
	}
	else if(!pids[1]) {	// Second child
		WAIT_UNTIL_SHORT();
		printf("\tNormal prio #2 (%d) starting\n", getpid());
		PRINT_EACH_TRIAL("\tHigh prio #2 (%d) got to trial %d\n",getpid(),remaining_trials(getpid()));
		printf("\tHigh prio #2 (%d) done\n",getpid());
		exit(0);
	}
	else {				// Third child
		WAIT_UNTIL_SHORT();
		printf("\tLow prio (%d) starting\n", getpid());
		PRINT_EACH_TRIAL("\tLow prio (%d) got to trial %d\n",getpid(),remaining_trials(getpid()));
		printf("\tLow prio (%d) done\n",getpid());
		exit(0);
	}
	WAIT();
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
	
	// Try running these tests:
	printf("Two SHORTs, one starts more important\n");
	oneone();
	printf("Two SHORTs, one becomes more important during the run\n");
	onethenone();
	printf("Three SHORTs, one becomes more important during the run\n");
	twoone();
	printf("Three SHORTs, two become more important during the run\n");
	onetwo();
	
	// Get the statistics
	int num_switches = get_scheduling_statistic(info);
	
	// Print them
	print_info(info, num_switches);
	
	return 0;
}
