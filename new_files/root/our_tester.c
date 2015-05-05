
 #include "test_utils.h"
 #include <stdio.h>
 #include <assert.h>
 #include <stdbool.h>
 #include <hw2_syscalls.h>	// For get_scheduling_statistic and some definitions


// A global log array. We use it in all the tests
struct switch_info info[150];


void initializeArray()
{
	int i=0;
	for(i=0;i<150;i++){ 
		info[i].previous_pid=0;
		info[i].next_pid=0;
		info[i].previous_policy=0; 
		info[i].next_policy=0;
		info[i].time=0; 
		info[i].reason=SWITCH_UNKNOWN;
	}
}

int check_monitor(int pid, int reason, int size){
	if(size == -1)
		return 0;
	int i=0;
	for(i=0; i< size; i++){
		if(info[i].reason == reason && info[i].previous_pid == pid)
			return 1;
	}
	return 0;
}

int check_monitor2(int pid, int reason, int size){
	if(size == -1)
		return 0;
	int i=0;
	for(i=0; i< size; i++){
		if(info[i].reason == reason && info[i].next_pid == pid)
			return 1;
	}
	return 0;
}

/*NEW TESTS */

/* TEST: check if OTHER proc ended */
bool testTaskEnded() {
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
	if(id > 0){
		sched_yield();
		EXIT_PROCS(getpid());
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
		TEST_EQUALS(check_monitor(id, SWITCH_OVERDUE, result),1);
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
		TEST_EQUALS(check_monitor(pid, SWITCH_PREV_WAIT, result),1);
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
	initializeArray();
	int id = fork();
	int result;
	struct sched_param param;
	int expected_requested_time = 5000;
	int expected_trials = 30;
	if (id > 0) {	
		CHANGE_TO_SHORT(id, expected_requested_time, expected_trials);
		TEST_TRUE(is_SHORT(id));
		assert(sched_getparam(id, &param) == 0);
		TEST_EQUALS(param.requested_time, expected_requested_time * HZ / 1000);
		TEST_EQUALS(param.trial_num, expected_trials);
		doShortTask();
		result = get_scheduling_statistic(info);
		TEST_DIFFERENT(result,-1);
		TEST_TRUE(check_monitor(getpid(), SWITCH_PRIO, result));
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
		param.sched_priority = 0;
		param.requested_time = requested;
		param.trial_num = trials;
		TEST_EQUALS(sched_setscheduler(id, SCHED_SHORT, &param), -1);
		assert(errno = EINVAL);
		TEST_FALSE(sched_getscheduler(id));

		requested = 7;
		trials = -1;
		param.sched_priority = 0;
		param.requested_time = requested;
		param.trial_num = trials;
		TEST_EQUALS(sched_setscheduler(id, SCHED_SHORT, &param), -1);
		assert(errno = EINVAL);
		TEST_FALSE(sched_getscheduler(id));

		requested = 5001;
		trials = 10;
		param.sched_priority = 0;
		param.requested_time = requested;
		param.trial_num = trials;
		TEST_EQUALS(sched_setscheduler(id, SCHED_SHORT, &param), -1);
		assert(errno = EINVAL);
		TEST_FALSE(sched_getscheduler(id));

		requested = -1;
		trials = 10;
		param.sched_priority = 0;
		param.requested_time = requested;
		param.trial_num = trials;
		TEST_EQUALS(sched_setscheduler(id, SCHED_SHORT, &param), -1);
		assert(errno = EINVAL);
		TEST_FALSE(sched_getscheduler(id));

		EXIT_PROCS(getpid());
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
		TEST_FALSE(is_SHORT(id));

		EXIT_PROCS(getpid());

	} else {
		int pid = getpid();
		struct sched_param param;
		int expected_requested_time = 3000;
		int expected_trials = 10;
		CHANGE_TO_SHORT(id,expected_requested_time,expected_trials);
		TEST_TRUE(is_SHORT(getpid()));
		TEST_TRUE(remaining_time(pid) <= expected_requested_time);
		TEST_TRUE(remaining_trials(pid) <= expected_trials);
		
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
		CHANGE_TO_SHORT(id,expected_requested_time,expected_trials);
		assert(sched_getparam(id, &param) == 0);
		TEST_EQUALS(param.requested_time, (expected_requested_time * HZ / 1000));
		TEST_EQUALS(param.trial_num, expected_trials);
		EXIT_PROCS(getpid());

	} else {
		while(!is_SHORT(getpid())) {;}	// wait until your dad set you to SHORT

		TEST_EQUALS(remaining_time(getpid()), expected_requested_time);
		expected_trials = remaining_trials(getpid());
		int son = fork();
		if (son == 0)
		{
			int grandson_initial_time = remaining_time(getpid());
			TEST_TRUE(grandson_initial_time <= expected_requested_time/2);
			TEST_TRUE(grandson_initial_time > 0);
			TEST_TRUE(is_SHORT(getpid()));
			TEST_EQUALS(remaining_trials(getpid()), expected_trials/2);
			doMediumTask();
			TEST_TRUE(remaining_time(getpid()) < grandson_initial_time);
			_exit(0);
		}
		else
		{
			TEST_TRUE(remaining_time(getpid()) <= expected_requested_time/2);
			TEST_EQUALS(remaining_trials(getpid()), expected_trials/2);
			EXIT_PROCS(getpid());
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
		TEST_FALSE(is_SHORT(id));
		TEST_EQUALS(remaining_time, 0);
		EXIT_PROCS(getpid());

	} else {
		doShortTask();
		int son = fork();
		if (son == 0)
		{
			TEST_FALSE(remaining_trials(getpid()));
			doShortTask();
			TEST_FALSE(remaining_time(getpid()));
			TEST_FALSE(is_SHORT(getpid()));
			_exit(0);
		}
		else
		{
			EXIT_PROCS(getpid());
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
			doLongTask();
			printf("\tRT son finished\n");
			_exit(0);
		}
		else
		{
			struct sched_param param;
			param.sched_priority = 1;
			CHANGE_TO_SHORT(id,expected_requested_time,expected_trials);	// SHORT process
			sched_setscheduler(id2, SCHED_FIFO, &param);				//FIFO RealTime process
		}
		EXIT_PROCS(getpid());
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
		EXIT_PROCS(getpid());
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
			while (!is_SHORT(getpid())) {}
			doMediumTask();
			printf("\tSHORT son finished\n");
			_exit(0);
		}
		else
		{
			CHANGE_TO_SHORT(id2,expected_requested_time,expected_trials);		// LSHORT process
		}
		EXIT_PROCS(getpid());
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
		EXIT_PROCS(getpid());
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
		EXIT_PROCS(getpid());
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
		EXIT_PROCS(getpid());
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
			while (!is_SHORT(getpid())) {}
			printf("\tSHORT son started\n");
			while (remaining_trials(getpid()) > 0) {
				if (remaining_trials(getpid()) >1) {
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
		EXIT_PROCS(getpid());
		printf("OK\n");
	} else if (id == 0) {
		doMediumTask();
		printf("\t\tSCHED_OTHER son finished\n");
		_exit(0);
	}

}

int main() {
	RUN_TEST(testTaskEnded);
	RUN_TEST(testTaskYield);
	RUN_TEST(testTaskBecomeOverDue);
	RUN_TEST(testTaskPreviousWait);
	RUN_TEST(testMakeSonShort);
	RUN_TEST(testBadParams);
	RUN_TEST(testSysCalls);
	RUN_TEST(testShortFork);
	RUN_TEST(testOverdueFork);


	// Those tests focus on the competition between to different policies.
	// we just print who won
	printf("Testing race: RT vs. SHORT (RT is supposed to win)\n");
	testScheduleRealTimeOverShort();

	printf("Testing race: RT vs. SHORT #2 (RT is supposed to win)\n");
	testScheduleRealTimeOverShort2();

	printf("Testing race: SHORT vs. OTHER (SHORT is supposed to win)\n");
	testScheduleShortOverOther();

	printf("Testing race: SHORT vs. OTHER #2(SHORT is supposed to win)\n");
	testScheduleShortOverOther2();

	printf("Testing race: OTHER vs. OVERDUE (OTHER is supposed to win)\n");
	testScheduleOtherOverOverdue();

	printf("Testing race: OTHER vs. OVERDUE #2(OTHER is supposed to win)\n");
	testScheduleOtherOverOverdue2();

	printf("Testing race: OTHER vs. SHORT who became OVERDUE\n");
	printf("SHORT is supposed to begin, then to become overdue, OTHER supposed to finish\n");
	printf("and then OVERDUE finish:\n");
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