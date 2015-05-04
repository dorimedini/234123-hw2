
 #include "test_utils.h"
 #include <stdio.h>

/*
struct switch_info {
	int previous_pid;
	int next_pid;
	int previous_policy;
	int next_policy;
	unsigned long time;	// The value of jiffies at moment of switch
	int reason;
};

*/


// GET THIS OUT TO UTILS
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

/** Self explanatory */
int calculate_fibo(int num) {
	if (num < 2) return num;
	return calculate_fibo(num-1)+calculate_fibo(num-2);
}

void set_to_SHORT_with_prio(int pid, int requested, int trials, int setNice) {
	struct sched_param param = { .sched_priority = 0, .requested_time = requested, .trial_num = trials};
	sched_setscheduler(pid, SCHED_SHORT, &param);
	nice(setNice);
}

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
 */
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

#define EXIT_PROCS(pid) do { \
		if (!pid) exit(0); \
		else while(wait() != -1); \
	} while(0)

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// WHEN OTHER IS RUNNING...


// Other proc will create a son process that will become short process 
bool other_to_short_with_high_prio() {
	int relevantPids[2];
	int otherPid = getpid();
	int shortPid;
	//Give the process we want to test long job
	calculate_fibo(30);

	// Rememeber the other pid
	relevantPids[0] = otherPid
	shortPid = fork();
	if (!shortPid) {
		// We are inside the son , let's make him short with high prio.
		set_to_SHORT_with_prio(getpid(),20,20,-19);
		// This short process should now run.
		while (remaining_trials(getpid()) > 1) {}	// it will stop running just before it becomes overdue
		// Kill this short
		exit(0);
	} else { // Inside the dad
		relevantPids[1] = shortPid;		
		// switch info array for our relevent switches
		EXIT_PROCS(1);
		struct switch_info info[150];
		int size =  getRelevantLogger(relevantPids,2,info, 0);
		// Check the switches
		return checkTheReleventSwitches(otherPid,shortPid,info,size);	
	}
	return false;
}




// Other proc will create a son process that will become short process 
bool other_to_short_with_low_prio() {
	int relevantPids[2];
	int otherPid = getpid();
	int shortPid;
	//Give the process we want to test long job
	calculate_fibo(30);

	// Rememeber the other pid
	relevantPids[0] = otherPid
	shortPid = fork();
	if (!shortPid) {
		// We are inside the son , let's make him short with high prio.
		set_to_SHORT_with_prio(getpid(),20,20,0);
		// This short process should now run.
		while (remaining_trials(getpid()) > 1) {}	// it will stop running just before it becomes overdue
		// Kill this short
		exit(0);
	} else { // Inside the dad
		relevantPids[1] = shortPid;		
		// switch info array for our relevent switches
		EXIT_PROCS(1);
		struct switch_info info[150];
		int size =  getRelevantLogger(relevantPids,2,info, 0);
		// Check the switches
		return checkTheReleventSwitches(otherPid,shortPid,info,size);	
	}
	return false;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// WHEN SHORT IS RUNNING...

// Section 1
bool rt_is_sched() {
	int relevantPids[2];
	// Give this proc long job
	calculate_fibo(30);

	// Creating two sons
	for (int i = 0 ; i < 2 ; ++i) {
		int pid = fork();
		if (!pid) {
			// Give each son long job
			calculate_fibo(30);
			break;
		} else {
			// Insert the sons in to the array
			relevantPids[i] = pid;
		}
	}
	// Make the first son to be short and the other real time 

	// Ayal continue..
	

}


// Section 2
bool short_is_sched_high_prio() {
	
}


// Section 3
bool short_is_sched_same_prio() {
	
}

// Section 4
bool other_is_sched_high_prio() {
	
}

// Section 5
bool other_is_sched_low_prio() {
	
}

// Section 6
// No shorts left in the runnig short curr prio and there is low prio short to run 
bool low_prio_short() {
	
}

// Section 7
// The last short is the running short and there are shorts in the same prio and short in lowest prio
bool other_shorts_in_the_same_prio() {
	
}




//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++



/*
 * TEST: We use a SHORT proc with very high prio as a master proc. It create a first SHORT proc with
 * than create another SHORT proc with same prio. Both procs has 3 trials. they should run till they have
 * 1 trial left.
 *
 * RESULT: It should look like that: dad->son1->son2->son1->son2->dad
*/
bool two_shorts_different_prio() {
	int pid1;
	CREATE_PROC(pid, SCHED_SHORT, 5000, 50, -19);	//create a master proc to control the test
	if (!pid1) {    									
		int relevantPids[3];
		int dad_pid = getpid();
		REMEMBER_RELEVANT(dad_pid, relevantPids, 0);	
		int pid2;
		int pid3
		CREATE_PROC(pid2, SCHED_SHORT, 2000, 3, 0);	// create a new short with static prio of +2
		if (!pid2) {
			while (remaining_trials(son_pid) > 1) {}	// it will stop running just before it becomes overdue
			exit(0);
		} else {
			int son_pid1 = pid2;
			REMEMBER_RELEVANT(son_pid1, relevantPids, 1);	// remember the first son
			CREATE_PROC(pid3, SCHED_SHORT, 2000, 3, 0);	// create another shor with static prio of +1
			if (!pid3) {
				while (remaining_trials(son_pid) > 1) {}	// it will stop running just before it becomes overdue
				exit(0);
			} else {
				int son_pid2 = pid3;
				REMEMBER_RELEVANT(son_pid2, relevantPids, 2);	// remember the second son
				EXIT_PROCS(dad_pid);

				struct switch_info info[150];
				int size =  getRelevantLogger(relevantPids,2,info, 0);

				TEST_EQUALS(size, 6);
				TEST_EQUALS(info[0].previous_pid, dad_pid);
				TEST_EQUALS(info[1].previous_pid, son_pid1);
				TEST_EQUALS(info[1].reason, SWITCH_SLICE);
				TEST_EQUALS(info[2].previous_pid, son_pid2);
				TEST_EQUALS(info[2].reason, SWITCH_SLICE);
				TEST_EQUALS(info[3].previous_pid, son_pid1);
				TEST_EQUALS(info[3].reason, SWITCH_ENDED);
				TEST_EQUALS(info[4].previous_pid, son_pid2);
				TEST_EQUALS(info[4].reason, SWITCH_ENDED);
				TEST_EQUALS(info[5].previous_pid, dad_pid);
			}
		}
	}
	EXIT_PROCS(getpid());
	return true;
}


/*
 * TEST: An OTHER Proc created a new SHORT with only few trials. 
 *
 * RESULT: The new proc should run but after a while it should become overdue, and then
 * the old proc should run
*/
bool short_who_finishes_his_trials() {
	int relevantPids[2];
	int dad_pid = getpid();
	int pid;
	REMEMBER_RELEVANT(dad_pid, relevantPids, 0);
	CREATE_PROC(pid, SCHED_SHORT, 2000, 3, -2);			// Create a Short with few trials
	if (!pid) {    									// son proc. the tested proc
		calculate_fibo(50);							// give him some shitty task. should become overdue							
		exit(0);
	} else {
		int son_pid = pid;
		REMEMBER_RELEVANT(son_pid, relevantPids, 1);
		calculate_fibo(40);
		EXIT_PROCS(dad_pid);
		
		struct switch_info info[150];
		int size =  getRelevantLogger(relevantPids,2,info, 0);

		TEST_EQUALS(info[0].previous_pid, dad_pid);
		TEST_EQUALS(info[1].previous_pid, son_pid);
		TEST_EQUALS(info[1].reason, SWITCH_OVERDUE);
		TEST_EQUALS(info[2].previous_pid, dad_pid);
	}
return true;
}


int main() {
	RUN_TEST(other_to_short_with_high_prio());
	RUN_TEST(other_to_short_with_low_prio());
	RUN_TEST(two_shorts_different_prio());
	RUN_TEST(two_short_same_prio());
	RUN_TEST(short_who_finishes_his_trials());
	return 0;
}




