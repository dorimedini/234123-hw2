
 #include "test_utils.h"
 #include <stdio.h>
 #include <assert.h>


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


// A global log array. We use it in all the tests
struct switch_info info[150];


/*NEW TESTS */

/* TEST: check if OTHER proc ended */
bool testTaskEnded(){
	initializeArray();
	int id = fork();
	int result;
	int status;
	if(id > 0){
		EXIT_PROCS(getpid());
		result = get_scheduling_statistic(info);
		TEST_DIFFERENT(result,-1);
		TEST_EQUALS(check_monitor(id, SWITCH_ENDED, result), 1);
	} else {
		_exit(0);
	}

	return true;
}

/* Checks if a proc has yielded */
bool testTaskYield(){
	initializeArray();
	int pid=getpid();
	int id = fork();
	int result;
	int status;
	if(id > 0){
		sched_yield();
		wait(&status);
	} else {
		result = get_scheduling_statistic(info);
		TEST_DIFFERENT(result,-1);
		TEST_EQUALS(check_monitor(pid, SWITCH_YIELD, result), 1);
		_exit(0);
	}

	return true;
}


/* A SHORT proc runs until it becomes overdue */
bool testTaskBecomeOverDue(){
	initializeArray();
	long i;
	int id = fork();
	int result;
	if(id > 0){
		EXIT_PROCS(getpid());
		result = get_scheduling_statistic(info);
		TEST_DIFFERENT(result,-1);
		TEST_EQUALS(check_monitor(id, SWITCH_BECOME_OVERDUE, result),1);
	} else {
		int thisPid = getpid();
		CHANGE_TO_SHORT(thisPid,2000,4);
		while(remaining_time(thisPid)!=0){
			;
		}
		_exit(0);
	}
	return true;
}

/* Checks if a proc has gone for waiting */
bool testTaskPreviousWait(){
	initializeArray();
	int pid=getpid();
	int id = fork();
	int result;
	if(id > 0){
		EXIT_PROCS(pid);
	} else {
		result = get_scheduling_statistic(info);
		TEST_DIFFERENT(result,-1);
		TEST_EQUALS(check_monitor(pid, TASK_PREVOIUS_WAIT, result),1);
		_exit(0);
	}

	return true;
}

/* TEST: An OTHER proc create a son. than it sets his son's policy to SHORT.
 *
 * RESULT: The son should get the CPU because he has higher priority (SHORT), and the 
 * finish, and the OTHER proc should run
 */
bool testMakeSonShort()
{
	inializeArray();
	int id = fork();
	int result;
	struct sched_param param;
	int expected_requested_time = 5000;
	int expected_trials = 30;
	if (id > 0) {	
		CHANGE_TO_SHORT(id, expected_requested_time, expected_trials);
		TEST_TRUE(is_short(id));
		assert(sched_getparam(id, &param) == 0);
		TEST_TRUE(param.requested_time, expected_requested_time * HZ / 1000);
		assert(param.trials_num == expected_trials);
		doShortTask();
		result = get_scheduling_statistic(info);
		TEST_DIFFERENT(result,-1);
		TEST_TRUE(check_monitor(getPid(), SWITCH_PRIO, result));
		EXIT_PROCS(getpid());
		result = get_scheduling_statistic(info);
		TEST_DIFFERENT(result,-1);
		TEST_TRUE(check_monitor(id, SWITCH_ENDED, result));
	} else if (id == 0) {
		doShortTask();
		_exit(0);
	}

	return true;
}


/* Try to create a new short proc with invalid parameters */
bool testBadParams()
{
	int id = fork();
	if (id>0)
	{
		struct sched_param param;
		int requested = 7;
		int trials = 51;
		param = { .sched_priority = 0, .requested_time = requested, .trial_num = trials};
		TEST_EQUALS(sched_setscheduler(id, SCHED_SHORT, &param), -1);
		assert(errno = EINVAL);
		TEST_FALSE(sched_getscheduler(id));

		requested = 7;
		trials = -1;
		param = { .sched_priority = 0, .requested_time = requested, .trial_num = trials};
		TEST_EQUALS(sched_setscheduler(id, SCHED_SHORT, &param), -1);
		assert(errno = EINVAL);
		TEST_FALSE(sched_getscheduler(id));

		requested = 5001;
		trials = 10;
		param = { .sched_priority = 0, .requested_time = requested, .trial_num = trials};
		TEST_EQUALS(sched_setscheduler(id, SCHED_SHORT, &param), -1);
		assert(errno = EINVAL);
		TEST_FALSE(sched_getscheduler(id));

		requested = -1;
		trials = 10;
		param = { .sched_priority = 0, .requested_time = requested, .trial_num = trials};
		TEST_EQUALS(sched_setscheduler(id, SCHED_SHORT, &param), -1);
		assert(errno = EINVAL);
		TEST_FALSE(sched_getscheduler(id));

		EXIT_PROCS(getPid());
	} else if (id == 0) {
		doShortTask();
		_exit(0);
	}

	return true;
}



/* Checks the sys_calls for a SHORT proc */
bool testSysCalls()
{
	int id = fork();
	if (id > 0)
	{
		TEST_EQUALS(remaining_time(id),-1);
		TEST_EQUALS(errno,EINVAL);
		TEST_EQUALS(remaining_trials(id),-1);
		TEST_EQUALS(errno, EINVAL);
		TEST_FALSE(is_short(id));

		EXIT_PROCS(getPid());

	} else {
		int pid = getPid();
		struct sched_param param;
		int expected_requested_time = 3000;
		int expected_trials = 10;
		CHANGE_TO_SHORT(id,expected_requested_time,expected_trials);
		TEST_TRUE(is_short(getPid()));
		int remaining_time = remaining_time(pid);
		int remaining_trials = remaining_trials(pid);
		TEST_TRUE(remaining_time <= expected_requested_time);
		TEST_TRUE(remaining_trials <= expected_requested_trials);
		
		_exit(0);
	}

	return true;
}


/* A short Proc is created and the forked. We check if all the parameters calculated correctly */
bool testShortFork()
{
	int expected_requested_time = 5000;
	int expected_trials = 40;
	int id = fork();
	if (id > 0) {
		struct sched_param param;
		CHANGE_TO_SHORT(id,expected_requested_time,expected_trials)
		TEST_EQUALS(sched_getparam(id, &param), 0);
		TEST_EQUALS(param.requested_time, (expected_requested_time * HZ / 1000));
		TEST_EQUALS(param.trial_num, expected_trials);
		EXIT_PROCS(getPid());

	} else {
		while(!is_short(getPid())) {}	// wait until your dad set you to SHORT

		TEST_EQUALS(remaining_time(getpid()), expected_requested_time);
		expected_trials = remaining_trials(getPid());
		int son = fork();
		if (son == 0)
		{
			int grandson_initial_time = remaining_time(getpid());
			TEST_TRUE(grandson_initial_time <= expected_requested_time/2);
			TEST_TRUE(grandson_initial_time > 0);
			TEST_TRUE(is_short(getPid());
			TEST_EQUALS(remaining_trials(getPid()), expected_trials/2);
			doMediumTask();
			TEST_TRUE(remaining_time(getpid()) < grandson_initial_time);
			_exit(0);
		}
		else
		{
			TEST_TRUE(remaining_time(getpid()) <= expected_requested_time/2);
			TEST_EQUALS(remaining_trials(getPid()), expected_trials/2);
			EXIT_PROCS(getPid());
			_exit(0);
		}
	}
	return true;
}


/* Checks that overdue fork another overdue */
bool testOverdueFork()
{
	int expected_requested_time = 1;
	int expected_trials = 40;
	int id = fork();
	if (id > 0) {
		CHANGE_TO_SHORT(id,expected_requested_time,expected_trials);		// become overdue here
		TEST_FALSE(is_short(id));
		TEST_EQUALS(remaining_time, 0);
		EXIT_PROCS(getPid());

	} else {
		doShortTask();
		int son = fork();
		if (son == 0)
		{
			TEST_EQUALS(remaining_trials(getPid()));
			doShortTask();
			TEST_TRUE(remaining_time(getpid()) < grandson_initial_time);
			_exit(0);
		}
		else
		{
			EXIT_PROCS(getPid());
			_exit(0);
		}
	}
	return true;
}

/* TEST: we create to sons. 1 is SHORT and 2 is RT (FIFO). Should print that the RT finishes and than
 * the Short finish
*/
void testScheduleRealTimeOverShort()
{
	int id = fork();
	if (id > 0) {
		struct sched_param param1;
		int expected_requested_time = 5000;
		int expected_trials = 20;

		int id2 = fork();
		if (id2 == 0)
		{
			doLongTest();
			printf("\tRT son finished\n");
			_exit(0);
		}
		else
		{
			struct sched_param param;
			param.sched_priority = 1;
			CHANGE_TO_SHORT(id,expected_requested_time,expected_trials)	// SHORT process
			sched_setscheduler(id2, SCHED_FIFO, &param);				//FIFO RealTime process
		}
		EXIT_PROCS(getPid());
		printf("OK\n");
	} else if (id == 0) {
		doMediumTask();
		printf("\t\tSHORT son finished\n");
		_exit(0);
	}
}

/* The same test, except now we create the RT first */
void testScheduleRealTimeOverShort2()
{
	int id = fork();

	if (id > 0) {
		struct sched_param param1;
		int expected_requested_time = 5000;
		int expected_trials = 30;
		

		int id2 = fork();
		if (id2 == 0)
		{
			doMediumTask();
			printf("\t\tSHORT son finished\n");
			_exit(0);
		}
		else
		{
			struct sched_param param;
			param.sched_priority = 1;
			sched_setscheduler(id, 1, &param);			//FIFO RealTime process
			CHANGE_TO_SHORT(id,expected_requested_time,expected_trials);	// LSHORT process
		}
		EXIT_PROCS(getPid());
		printf("OK\n");
	} else if (id == 0) {
		doMediumTask();
		printf("\tRT son finished\n");
		_exit(0);
	}
}


void testScheduleShortOverOther()
{
	int id = fork();
	if (id > 0) {
	
		int expected_requested_time = 5000;
		int expected_trials = 20;

		int id2 = fork();
		if (id2 == 0)
		{
			while (!is_short(getPid)) {}
			doMediumTask();
			printf("\tSHORT son finished\n");
			_exit(0);
		}
		else
		{
			CHANGE_TO_SHORT(id2,expected_requested_time,expected_trials);		// LSHORT process
		}
		EXIT_PROCS(getPid());
		printf("OK\n");
	} else if (id == 0) {
		doMediumTask();
		printf("\t\tSCHED_OTHER son finished\n");
		_exit(0);
	}
}


void testScheduleShortOverOther2()
{
	int id = fork();
	if (id > 0) {
		int expected_requested_time = 4500;
		int expected_trials = 23;

		int id2 = fork();
		if (id2 == 0)
		{
			doMediumTask();
			printf("\t\tSCHED_OTHER son finished\n");
			_exit(0);
		}
		else
		{
			CHANGE_TO_SHORT(id,expected_requested_time,expected_trials);		// LSHORT process
		}
		EXIT_PROCS(getPid());
		printf("OK\n");
	} else if (id == 0) {
		doMediumTask();
		printf("\tLSHORT son finished\n");
		_exit(0);
	}
}

/* 2 Procs. One OTHER, and the second is OVERDUE, from the beginning (!!!) */
void testScheduleOtherOverOverdue()
{
	int id = fork();
	if (id > 0) {
		int expected_requested_time = 1;
		int expected_trials = 20;
		
		int id2 = fork();
		if (id2 == 0)
		{
			doMediumTask();
			printf("\t\tOVERDUE son finished\n");
			_exit(0);
		}
		else
		{
			CHANGE_TO_SHORT(id2,expected_requested_time,expected_trials);
		}
		EXIT_PROCS(getPid());
		printf("OK\n");
	} else if (id == 0) {
		doMediumTask();
		printf("\tSCHED_OTHER son finished\n");
		_exit(0);
	}
}

void testScheduleOtherOverOverdue2()
{
	int id = fork();
	if (id > 0) {
		int expected_requested_time = 1;
		int expected_trials = 20;
		
		int id2 = fork();
		if (id2 == 0)
		{
			doMediumTask();
			printf("\tSCHED_OTHER son finished\n");
			
			_exit(0);
		}
		else
		{
			CHANGE_TO_SHORT(id,expected_requested_time,expected_trials);
		}
		EXIT_PROCS(getPid());
		printf("OK\n");
	} else if (id == 0) {
		doMediumTask();
		printf("\t\tOVERDUE son finished\n");
		_exit(0);
	}
}



bool testScheduleOtherShortWhoBecameOverdue()
{
	int id = fork();
	if (id > 0) {
		int expected_requested_time = 1000;
		int expected_trials = 6;

		int id2 = fork();
		if (id2 == 0)
		{
			while (!is_short(getPid)) {}
			printf("\tSHORT son started\n");
			while (remaining_trials(getPid()) > 0) {
				if (remaining_trials(getPid()) >1) {
					printf("\tSHORT son became Overdue\n");
				}
			}
			doShortTask();
			printf("\tOverdue son finished\n");
			_exit(0);
		}
		else
		{
			CHANGE_TO_SHORT(id2,expected_requested_time,expected_trials);		// LSHORT process
		}
		EXIT_PROCS(getPid());
		printf("OK\n");
	} else if (id == 0) {
		doMediumTask();
		printf("\t\tSCHED_OTHER son finished\n");
		_exit(0);
	}

}

int main() {
	RUN_TEST(testTaskEnded());
	RUN_TEST(testTaskYield());
	RUN_TEST(testTaskBecomeOverDue());
	RUN_TEST(testTaskPreviousWait());
	RUN_TEST(testMakeSonShort());
	RUN_TEST(testBadParams());
	RUN_TEST(testSysCalls());
	RUN_TEST(testShortFork());
	RUN_TEST(testOverdueFork());


	// Those tests focus on the competition between to different policies.
	// we just print who won
	printf("Testing race: RT vs. SHORT (RT is supposed to win)\n");
	testScheduleRealTimeOverShort();

	printf("Testing race: RT vs. SHORT #2 (RT is supposed to win)\n");
	testScheduleRealTimeOverShort2();

	printf("Testing race: SHORT vs. OTHER (SHORT is supposed to win)\n");
	testScheduleShortOverOther();

	printf("Testing race: SHORT vs. OTHER #2(SHORT is supposed to win)\n");
	testScheduleLShortOverOther2();

	printf("Testing race: OTHER vs. OVERDUE (OTHER is supposed to win)\n");
	testScheduleOtherOverOverdue();

	printf("Testing race: OTHER vs. OVERDUE #2(OTHER is supposed to win)\n");
	testScheduleOtherOverOverdue2();

	printf("Testing race: OTHER vs. SHORT who became OVERDUE\n");
	printf("SHORT is supposed to begin, then to become overdue, OTHER supposed to finish\n");
	printf("and then OVERDUE finish:");
	testScheduleOtherShortWhoBecameOverdue();


	/*
	RUN_TEST(other_to_short_with_high_prio());
	RUN_TEST(other_to_short_with_low_prio());
	RUN_TEST(two_shorts_different_prio());
	RUN_TEST(two_short_same_prio());
	RUN_TEST(short_who_finishes_his_trials());
	*/
	return 0;
}


















/* Our OLD TESTS. MAYBE WE CAN USE THEM LATER ON

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// WHEN OTHER IS RUNNING...


// Other proc will create a son process that will become short process





bool other_to_short_with_high_prio() {
	int relevantPids[2];
	int otherPid = getpid();
	int shortPid;

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
		EXIT_PROCS(1);
		//Give the process we want to test long job
		calculate_fibo(30);
		// switch info array for our relevent switches
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
		//Give the process we want to test long job
		calculate_fibo(30);
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




*/

/*
 * TEST: We use a SHORT proc with very high prio as a master proc. It create a first SHORT proc with
 * than create another SHORT proc with same prio. Both procs has 3 trials. they should run till they have
 * 1 trial left.
 *
 * RESULT: It should look like that: dad->son1->son2->son1->son2->dad
*
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
*
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


*/