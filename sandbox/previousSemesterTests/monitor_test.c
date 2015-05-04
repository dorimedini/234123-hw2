#include "hw2_syscalls.h"
#include <stdio.h>
#include <assert.h>

#define HZ 512


#define SCHED_OTHER		0
#define SCHED_FIFO		1
#define SCHED_RR		2
#define SCHED_LSHORT	3

#define SCHED_OTHER		0
#define SCHED_FIFO		1
#define SCHED_RR		2
#define SCHED_LSHORT	3

#define TASK_CREATED 1
#define TASK_ENDED 2
#define TASK_YIELD 3
#define TASK_BECOME_OVERDUE 4
#define TASK_PREVOIUS_WAIT 5
#define TASK_RETURN_WAIT 7
#define TASK_TS_ENDED 8
#define TASK_OTHER 9

struct switch_info info[150];

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

void inializeArray()
{
	int i=0;
	for(i=0;i<150;i++){
		info[i].time=0; 
		info[i].previous_pid=0; 
		info[i].previous_policy=0; 
		info[i].next_pid=0; 
		info[i].next_policy=0; 
		info[i].reason=TASK_OTHER;
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

void testTaskEnded(){
	inializeArray();
	int id = fork();
	int result;
	int status;
	if(id > 0){
		wait(&status);
		result = get_scheduling_statistic(info);
		assert(result != -1);
		assert(check_monitor(id, TASK_ENDED, result) == 1);
	} else {
		_exit(0);
	}
}

void testTaskYield(){
	inializeArray();
	int pid=getpid();
	int id = fork();
	int result;
	int status;
	if(id > 0){
		sched_yield();
		wait(&status);
	} else {
		result = get_scheduling_statistic(info);
		assert(result != -1);
		assert(check_monitor(pid, TASK_YIELD, result) == 1);
		_exit(0);
	}
}

void testTaskBecomeOverDue(){
	inializeArray();
	long i;
	int id = fork();
	int result;
	int status;
	if(id > 0){
		wait(&status);
		result = get_scheduling_statistic(info);
		assert(result != -1);
		assert(check_monitor(id, TASK_BECOME_OVERDUE, result) == 1);
	} else {
		int thisId = getpid();
		struct sched_param param;
		int expected_requested_time = 1000;
		int expected_level = 8;
		param.lshort_params.requested_time = expected_requested_time;
		param.lshort_params.level = expected_level;
		sched_setscheduler(thisId, SCHED_LSHORT, &param);
		while(lshort_query_remaining_time(thisId)!=0){
			;
		}
		_exit(0);
	}
}

void testTaskPreviousWait(){
	inializeArray();
	int pid=getpid();
	int id = fork();
	int result;
	int status;
	if(id > 0){
		wait(&status);
	} else {
		result = get_scheduling_statistic(info);
		assert(result != -1);
		assert(check_monitor(pid, TASK_PREVOIUS_WAIT, result) == 1);
		_exit(0);
	}
}

void test(){
	inializeArray();
	int pid=getpid();
	int id=fork();
	int result;
	int status;
	if(id>0){
		wait(&status);
	} else {
		struct sched_param param;
		int expected_requested_time = 1000;
		int expected_level = 8;
		param.lshort_params.requested_time = expected_requested_time;
		param.lshort_params.level = expected_level;
		sched_setscheduler(getpid(), SCHED_LSHORT, &param);
		sleep(1);
		result = get_scheduling_statistic(info);
		assert(result != -1);
		assert(check_monitor2(getpid(), TASK_RETURN_WAIT, result) == 1);	
	}	
}


void testTaskReturnWait(){
	inializeArray();
	int pid=getpid();
	int id = fork();
	int result;
	int status;
	if(id > 0){
		wait(&status);
		result = get_scheduling_statistic(info);
		assert(result != -1);
		assert(check_monitor(id, TASK_ENDED, result) == 1);
		assert(check_monitor(pid, TASK_RETURN_WAIT, result) == 1);
	} else {
		_exit(0);
	}
}


int main()
{

	printf("Test test() ... \n");
	test();

	printf("Testing TASK_ENDED ... \n");
	testTaskEnded();
	
	printf("Testing TASK_YIELD ... \n");
	testTaskYield();
	
	printf("Testing TASK_OVERDUE (only god knows if it works) ... \n");
	testTaskBecomeOverDue();
	
	printf("Testing TASK_PREVIOUS_WAIT ... \n");
	testTaskPreviousWait();	

	return 0;
}
