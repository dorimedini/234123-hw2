
 #include "test_utils.h"
 #include <stdio.h>


/*
 * TEST: An OTHER Proc created a new proc which became short.  The proiority of the new proc (The short one)
 * is higher than the old one (the OTHER) .
 *
 * RESULT: The new proc should run and after it finish the old proc
 * should run
*/
/*
 * TEST: An OTHER Proc created a new proc which became short.  The proiority of the new proc (The short one)
 * is lower than the old one (the OTHER) .
 *
 * RESULT: The new proc should run and after it finish the old proc
 * should run
*/
bool other_to_short_with_low_prio() {
	int relevantPids[2];
	int dad_pid = getpid();
	int pid;
	REMEMBER_RELEVANT(dad_pid, relevantPids, 0);
	CREATE_PROC(pid, SCHED_SHORT, 2000, 10, 2);
	if (!pid) {    									// son proc. the tested proc
		while (remaining_trials(son_pid) > 1) {}	// it will stop running just before it becomes overdue
		exit(0);
	} else {
		int son_pid = pid;
		REMEMBER_RELEVANT(son_pid, relevantPids, 1);
		calculate_fibo(40);
		EXIT_PROCS(dad_pid);
		
		struct switch_info info[150];
		int size =  getRelevantLogger(relevantPids,2,info, 0);

		TEST_EQUALS(size, 3);
		TEST_EQUALS(info[0].previous_pid, dad_pid);
		TEST_EQUALS(info[1].previous_pid, son_pid);
		TEST_EQUALS(info[1].reason, SWITCH_ENDED);
		TEST_EQUALS(info[2].previous_pid, dad_pid);
	}
return true;
}


/*
 * TEST: An OTHER Proc created a new proc which became short.  The proiority of the new proc (The short one)
 * is higher than the old one (the OTHER) .
 *
 * RESULT: The new proc should run and after it finish the old proc
 * should run
*/
bool other_to_short_with_high_prio() {
	int relevantPids[2];
	int dad_pid = getpid();
	int pid;
	REMEMBER_RELEVANT(dad_pid, relevantPids, 0);
	CREATE_PROC(pid, SCHED_SHORT, 2000, 10, -2);
	if (!pid) {    									// son proc. the tested proc
		while (remaining_trials(son_pid) > 1) {}	// it will stop running just before it becomes overdue
		exit(0);
	} else {
		int son_pid = pid;
		REMEMBER_RELEVANT(son_pid, relevantPids, 1);
		calculate_fibo(40);
		EXIT_PROCS(dad_pid);
		
		struct switch_info info[150];
		int size =  getRelevantLogger(relevantPids,2,info, 0);

		TEST_EQUALS(size, 3);
		TEST_EQUALS(info[0].previous_pid, dad_pid);
		TEST_EQUALS(info[1].previous_pid, son_pid);
		TEST_EQUALS(info[1].reason, SWITCH_ENDED);
		TEST_EQUALS(info[2].previous_pid, dad_pid);
	}
return true;
}



/*
 * TEST: We use a SHORT proc with very high prio as a master proc. It create a first SHORT proc with
 * low prio. than create another SHORT proc with high prio.
 *
 * RESULT: The second son should run. after it finishes, the first son should run, and finally the dad
 * runs
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
		CREATE_PROC(pid2, SCHED_SHORT, 2000, 10, 2);	// create a new short with static prio of +2
		if (!pid2) {
			while (remaining_trials(son_pid) > 1) {}	// it will stop running just before it becomes overdue
			exit(0);
		} else {
			int son_pid1 = pid2;
			REMEMBER_RELEVANT(son_pid1, relevantPids, 1);	// remember the first son
			CREATE_PROC(pid3, SCHED_SHORT, 2000, 10, 1);	// create another shor with static prio of +1
			if (!pid3) {
				while (remaining_trials(son_pid) > 1) {}	// it will stop running just before it becomes overdue
				exit(0);
			} else {
				int son_pid2 = pid3;
				REMEMBER_RELEVANT(son_pid2, relevantPids, 2);	// remember the second son
				EXIT_PROCS(dad_pid);

				struct switch_info info[150];
				int size =  getRelevantLogger(relevantPids,2,info, 0);

				TEST_EQUALS(size, 4);
				TEST_EQUALS(info[0].previous_pid, dad_pid);
				TEST_EQUALS(info[1].previous_pid, son_pid2);
				TEST_EQUALS(info[1].reason, SWITCH_ENDED);
				TEST_EQUALS(info[2].previous_pid, son_pid1);
				TEST_EQUALS(info[2].reason, SWITCH_ENDED);
				TEST_EQUALS(info[3].previous_pid, dad_pid);
			}
		}
	}
	EXIT_PROCS(getpid());
	return true;
}

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




