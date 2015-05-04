
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
	int son_pid = pid;
	REMEMBER_RELEVANT(dad_pid, relevantPids, 0)
	CREATE_PROC(pid, SCHED_SHORT, 2000, 10, 2);
	if (!pid) {    									// son proc. the tested proc
		while (remaining_trials(son_pid) > 1) {}	// it will stop running just before it becomes overdue
		exit(0);
	} else {
		son_pid = pid;
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
 * is lower than the old one (the OTHER) .
 *
 * RESULT: The new proc should run and after it finish the old proc
 * should run
*/
bool other_to_short_with_low_prio() {
	int relevantPids[2];
	int dad_pid = getpid();
	int son_pid = pid;
	REMEMBER_RELEVANT(dad_pid, relevantPids, 0)
	CREATE_PROC(pid, SCHED_SHORT, 2000, 10, -2);
	if (!pid) {    									// son proc. the tested proc
		while (remaining_trials(son_pid) > 1) {}	// it will stop running just before it becomes overdue
		exit(0);
	} else {
		son_pid = pid;
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
 * TEST: An OTHER Proc created a new SHORT with only few trials. 
 *
 * RESULT: The new proc should run but after a while it should become overdue, and then
 * the old proc should run
*/
bool short_who_finishes_his_trials() {
	int relevantPids[2];
	int dad_pid = getpid();
	int son_pid = pid;
	REMEMBER_RELEVANT(dad_pid, relevantPids, 0)
	CREATE_PROC(pid, SCHED_SHORT, 2000, 3, -2);			// Create a Short with few trials
	if (!pid) {    									// son proc. the tested proc
		calculate_fibo(50);							// give him some shitty task. should become overdue							
		exit(0);
	} else {
		son_pid = pid;
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
	RUN_TEST(short_who_finishes_his_trials());

	return 0;
}




