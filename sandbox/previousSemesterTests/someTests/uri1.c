#include "hw2_syscalls.h"
#include <stdio.h>
#include <assert.h>

#define HZ 512

void doLongTask()
{
	long i;
	for (i=1; i > 0; i++)
	{
		;
	}
}

void doShortTask()
{
	short i;
	for (i=1; i != 0; i++)
	{
		;
	}
}

void doMediumTask()
{
	int j;
	for(j=0; j<4000; j++)
	{
		doShortTask();
	}
}

void testOther()
{
	int thisId = getpid();
	assert(sched_getscheduler(thisId) == 0);
	assert(lshort_query_remaining_time(thisId) == -1);
	assert(errno == 22);
	assert(lshort_query_overdue_time(thisId) == -1);
	assert(errno == 22);
	printf("OK\n");
}

void testMakeLshort()
{
	int thisId = getpid();
	struct sched_param param;
	int expected_requested_time = 30000;
	int expected_level = 8;
	param.lshort_params.requested_time = expected_requested_time;
	param.lshort_params.level = expected_level;
	sched_setscheduler(thisId, SCHED_LSHORT, &param);
	assert(sched_getscheduler(thisId) == SCHED_LSHORT);
	assert(sched_getparam(thisId, &param) == 0);
	assert(param.lshort_params.requested_time == expected_requested_time * HZ / 1000);
	assert(param.lshort_params.level == expected_level);
	int i;
	doMediumTask();
	assert(sched_getparam(thisId, &param) == 0);
	int afterTime = lshort_query_remaining_time(thisId);
	assert(afterTime > 0);
	assert(afterTime < expected_requested_time);
	assert(lshort_query_overdue_time(thisId) == 0);
	printf("OK\n");
}

void testMakeSonLshort()
{
	int id = fork();
	int status;
	if (id > 0) {
		struct sched_param param;
		int expected_requested_time = 30000;
		int expected_level = 8;
		param.lshort_params.requested_time = expected_requested_time;
		param.lshort_params.level = expected_level;
		sched_setscheduler(id, SCHED_LSHORT, &param);
		assert(sched_getscheduler(id) == SCHED_LSHORT);
		assert(sched_getparam(id, &param) == 0);
		assert(param.lshort_params.requested_time == expected_requested_time * HZ / 1000);
		assert(param.lshort_params.level == expected_level);
		wait(&status);
		printf("OK\n");
	} else if (id == 0) {
		doShortTask();
		_exit(0);
	}
}

void testBadParams()
{
	int id = fork();
	int status;
	if (id>0)
	{
		struct sched_param param;
		int expected_requested_time = 7;
		int expected_level = 51;
		param.lshort_params.requested_time = expected_requested_time;
		param.lshort_params.level = expected_level;
		assert(sched_setscheduler(id, SCHED_LSHORT, &param) == -1);
		assert(errno = 22);
		assert(sched_getscheduler(id) == 0);

		expected_requested_time = 310000;
		expected_level = 7;
		param.lshort_params.requested_time = expected_requested_time;
		param.lshort_params.level = expected_level;
		assert(sched_setscheduler(id, SCHED_LSHORT, &param) == -1);
		assert(errno = 22);
		assert(sched_getscheduler(id) == 0);
		wait(&status);
		printf("OK\n");
	} else if (id == 0) {
		doShortTask();
		_exit(0);
	}
}

void testSysCalls()
{
	int id = fork();
	int status;
	if (id > 0)
	{
		assert(lshort_query_remaining_time(id) == -1);
		assert(errno == 22);
		assert(lshort_query_overdue_time(id) == -1);
		assert(errno == 22);

		struct sched_param param;
		int expected_requested_time = 30000;
		int expected_level = 8;
		param.lshort_params.requested_time = expected_requested_time;
		param.lshort_params.level = expected_level;
		sched_setscheduler(id, SCHED_LSHORT, &param);
		int remaining_time = lshort_query_remaining_time(id);
		int overdue_time = lshort_query_overdue_time(id);
		assert(remaining_time <= expected_requested_time);
		assert(remaining_time > 0);
		assert(overdue_time == 0);
		wait(&status);
		printf("OK\n");
	} else if (id == 0) {
		doShortTask();
		_exit(0);
	}
}

void testFork()
{
	int expected_requested_time = 30000;
	int id = fork();
	int status;
	if (id > 0) {
		struct sched_param param;
		int expected_level = 8;
		param.lshort_params.requested_time = expected_requested_time;
		param.lshort_params.level = expected_level;
		sched_setscheduler(id, SCHED_LSHORT, &param);
		assert(sched_getscheduler(id) == SCHED_LSHORT);
		assert(sched_getparam(id, &param) == 0);
		assert(param.lshort_params.requested_time == expected_requested_time * HZ / 1000);
		assert(param.lshort_params.level == expected_level);
		wait(&status);
		printf("OK\n");
	} else if (id == 0) {
		assert(lshort_query_remaining_time(getpid()) == expected_requested_time);
		int son = fork();
		if (son == 0)
		{
			int grandson_initial_time = lshort_query_remaining_time(getpid());
			assert(grandson_initial_time < (expected_requested_time*2)/3);
			assert(grandson_initial_time > 0);
			doMediumTask();
			assert(lshort_query_remaining_time(getpid()) < grandson_initial_time);
			_exit(0);
		}
		else
		{
			assert(lshort_query_remaining_time(getpid()) < expected_requested_time/2);
			wait(&status);
		}
		_exit(0);
	}
}

void testBecomingOverdue()
{
	int id = fork();
	int status;
	if (id > 0) {
		struct sched_param param;
		int expected_requested_time = 2;
		int expected_level = 8;
		param.lshort_params.requested_time = expected_requested_time;
		param.lshort_params.level = expected_level;
		sched_setscheduler(id, SCHED_LSHORT, &param);
		assert(sched_getscheduler(id) == SCHED_LSHORT);
		assert(sched_getparam(id, &param) == 0);
		assert(param.lshort_params.requested_time == expected_requested_time * HZ / 1000);
		assert(param.lshort_params.level == expected_level);
		wait(&status);
		printf("OK\n");
	} else if (id == 0) {
		int myId = getpid();
		int i = lshort_query_overdue_time(myId);
		for (i; i < 2; )
		{
			i = lshort_query_overdue_time(myId);
			doShortTask();
		}
		_exit(0);
	}
}

void testScheduleRealTimeOverLShort()
{
	int id = fork();
	int status;
	if (id > 0) {
		struct sched_param param1;
		int expected_requested_time = 30000;
		int expected_level = 8;
		param1.lshort_params.requested_time = expected_requested_time;
		param1.lshort_params.level = expected_level;

		int id2 = fork();
		if (id2 == 0)
		{
			doMediumTask();
			printf("\tRT son finished\n");
			_exit(0);
		}
		else
		{
			struct sched_param param2;
			param2.sched_priority = 1;
			sched_setscheduler(id, SCHED_LSHORT, &param1);	// LSHORT process
			sched_setscheduler(id2, 1, &param2);		//FIFO RealTime process
		}
		wait(&status);
		wait(&status);
		printf("OK\n");
	} else if (id == 0) {
		doMediumTask();
		printf("\t\tLSHORT son finished\n");
		_exit(0);
	}
}

void testScheduleRealTimeOverLShort2()
{
	int id = fork();
	int status;
	if (id > 0) {
		struct sched_param param1;
		int expected_requested_time = 30000;
		int expected_level = 8;
		param1.lshort_params.requested_time = expected_requested_time;
		param1.lshort_params.level = expected_level;

		int id2 = fork();
		if (id2 == 0)
		{
			doMediumTask();
			printf("\t\tLSHORT son finished\n");
			_exit(0);
		}
		else
		{
			struct sched_param param2;
			param2.sched_priority = 1;
			sched_setscheduler(id, 1, &param2);			//FIFO RealTime process
			sched_setscheduler(id2, SCHED_LSHORT, &param1);	// LSHORT process
		}
		wait(&status);
		wait(&status);
		printf("OK\n");
	} else if (id == 0) {
		doMediumTask();
		printf("\tRT son finished\n");
		_exit(0);
	}
}

void testScheduleLShortOverOther()
{
	int id = fork();
	int status;
	if (id > 0) {
		struct sched_param param1;
		int expected_requested_time = 30000;
		int expected_level = 8;
		param1.lshort_params.requested_time = expected_requested_time;
		param1.lshort_params.level = expected_level;

		int id2 = fork();
		if (id2 == 0)
		{
			doMediumTask();
			printf("\tLSHORT son finished\n");
			_exit(0);
		}
		else
		{
			struct sched_param param2;
			param2.sched_priority = 1;
			sched_setscheduler(id, 0, &param2);		// regular SCHED_OTHER
			sched_setscheduler(id2, SCHED_LSHORT, &param1);		// LSHORT process
		}
		wait(&status);
		wait(&status);
		printf("OK\n");
	} else if (id == 0) {
		doMediumTask();
		printf("\t\tSCHED_OTHER son finished\n");
		_exit(0);
	}
}

void testScheduleLShortOverOther2()
{
	int id = fork();
	int status;
	if (id > 0) {
		struct sched_param param1;
		int expected_requested_time = 30000;
		int expected_level = 8;
		param1.lshort_params.requested_time = expected_requested_time;
		param1.lshort_params.level = expected_level;

		int id2 = fork();
		if (id2 == 0)
		{
			doMediumTask();
			printf("\t\tSCHED_OTHER son finished\n");
			_exit(0);
		}
		else
		{
			struct sched_param param2;
			param2.sched_priority = 1;
			sched_setscheduler(id2, 0, &param2);		// regular SCHED_OTHER
			sched_setscheduler(id, SCHED_LSHORT, &param1);		// LSHORT process
		}
		wait(&status);
		wait(&status);
		printf("OK\n");
	} else if (id == 0) {
		doMediumTask();
		printf("\tLSHORT son finished\n");
		_exit(0);
	}
}

void testScheduleOtherOverOverdue()
{
	int id = fork();
	int status;
	if (id > 0) {
		struct sched_param param1;
		int expected_requested_time = 1;
		int expected_level = 8;
		param1.lshort_params.requested_time = expected_requested_time;
		param1.lshort_params.level = expected_level;

		int id2 = fork();
		if (id2 == 0)
		{
			doMediumTask();
			printf("\t\tOVERDUE son finished\n");
			_exit(0);
		}
		else
		{
			struct sched_param param2;
			param2.sched_priority = 1;
			sched_setscheduler(id, 0, &param2);		// regular SCHED_OTHER
			sched_setscheduler(id2, SCHED_LSHORT, &param1);		// LSHORT process
		}
		wait(&status);
		wait(&status);
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
	int status;
	if (id > 0) {
		struct sched_param param1;
		int expected_requested_time = 1;
		int expected_level = 8;
		param1.lshort_params.requested_time = expected_requested_time;
		param1.lshort_params.level = expected_level;

		int id2 = fork();
		if (id2 == 0)
		{
			doMediumTask();
			printf("\tSCHED_OTHER son finished\n");
			_exit(0);
		}
		else
		{
			struct sched_param param2;
			param2.sched_priority = 1;
			sched_setscheduler(id2, 0, &param2);		// regular SCHED_OTHER
			sched_setscheduler(id, SCHED_LSHORT, &param1);		// LSHORT process
		}
		wait(&status);
		wait(&status);
		printf("OK\n");
	} else if (id == 0) {
		doMediumTask();
		printf("\t\tOVERDUE son finished\n");
		_exit(0);
	}
}

void testOverdueRoundRobin()
{
	int id = fork();
	int status;
	if (id > 0) {
		struct sched_param param1;
		int expected_requested_time = 1;
		int expected_level = 8;
		param1.lshort_params.requested_time = expected_requested_time;
		param1.lshort_params.level = expected_level;

		int id2 = fork();
		if (id2 == 0)
		{
			int i, j;
			for (i=0; i < 4; i++)
			{
				doMediumTask();
				printf("\t\tSon2\n");
			}
			_exit(0);
		}
		else
		{
			struct sched_param param2;
			param2.lshort_params.requested_time = expected_requested_time;
			param2.lshort_params.level = expected_level;
			sched_setscheduler(id2, SCHED_LSHORT, &param1);
			sched_setscheduler(id, SCHED_LSHORT, &param2);
		}
		wait(&status);
		wait(&status);
		printf("OK\n");
	} else if (id == 0) {
		int i, j;
		for (i=0; i < 4; i++)
		{
			doMediumTask();
			printf("\tSon1\n");
		}
		_exit(0);
	}
}

int main()
{
	printf("Testing SCHED_OTHER process... ");
	testOther();

	printf("Testing making son process LSHORT... ");
	testMakeSonLshort();

	printf("Testing bad parameters... ");
	testBadParams();

	printf("Testing new System Calls... ");
	testSysCalls();

	printf("Testing becoming overdue... ");
	testBecomingOverdue();

	printf("Testing fork... ");
	testFork();

	printf("Testing Overdues Round-Robin... \n");
	testOverdueRoundRobin();

	printf("Testing race: RT vs. LSHORT (RT is supposed to win)\n");
	testScheduleRealTimeOverLShort();

	printf("Testing race: RT vs. LSHORT #2 (RT is supposed to win)\n");
	testScheduleRealTimeOverLShort2();

	printf("Testing race: LSHORT vs. OTHER (LSHORT is supposed to win)\n");
	testScheduleLShortOverOther();

	printf("Testing race: LSHORT vs. OTHER #2(LSHORT is supposed to win)\n");
	testScheduleLShortOverOther2();

	printf("Testing race: OTHER vs. OVERDUE (OTHER is supposed to win)\n");
	testScheduleOtherOverOverdue();

	printf("Testing race: OTHER vs. OVERDUE #2 (OTHER is supposed to win)\n");
	testScheduleOtherOverOverdue2();

	printf("Testing making this process LSHORT... ");
	testMakeLshort();
	printf("Success!\n");
	return 0;
}
