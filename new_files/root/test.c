#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syscall.h>
#include <time.h>
#include <unistd.h>


struct switch_info {
	int arr[6];
};


#define is_short(pid) syscall(243, pid)
#define rem_time(pid) syscall(244, pid)
#define rem_trials(pid) syscall(245, pid)
#define get_sched_stats(buf) syscall(246, buf)



static int become_short(unsigned int req_time, unsigned int trial_num) {
	struct {
		int prio;
		int req_time;
		int trial_num;
	} p;

	p.req_time = req_time;
	p.trial_num = trial_num;
	return sched_setscheduler(getpid(), 4, (struct sched_param *) &p);
}


static int run_forked(int (*func)() ) {
	int err, ret;
	int pid;
	int pipes[2];

	if (pipe(pipes)) {
		perror("pipe");
		exit(1);
	}

	if ((pid = fork()) < 0) {
		perror("fork");
		exit(1);
	}
	if (!pid) {
		ret = func();
		write(pipes[1], &ret, sizeof (int));
		exit(0);
	} else {
		err = waitpid(pid, NULL, 0);
		if (err < 0) {
			perror("waitpid");
			exit(1);
		}
		read(pipes[0], &ret, sizeof (int));
		close(pipes[0]);
		close(pipes[1]);
		return ret;
	}
}


/////////////////////////////////////////// testes //////////////////////////////////////////


// 1
static int req_too_small() {
	if (become_short(0, 1) == -1)
		return 1;
	return 0;
}
static int test_req_too_small() {
	return run_forked(req_too_small);
}


// 2
static int req_too_big() {
	if (become_short(5001, 1) == -1)
		return 1;
	return 0;
}
static int test_req_too_big() {
	return run_forked(req_too_big);
}


// 3
static int trials_too_small() {
	if (become_short(1, 0) == -1)
		return 1;
	return 0;
}
static int test_trials_too_small() {
	return run_forked(trials_too_small);
}


// 4
static int trials_too_big() {
	if (become_short(5000, 51) == -1)
		return 1;
	return 0;
}
static int test_trials_too_big() {
	return run_forked(trials_too_big);
}


// 5
static int setsched_success() {
	errno = 0;
	if (become_short(3, 2) == -1) {
		perror("setsched errno is");
		return 0;
	}
//	printf("remmmmm  %d",rem_trials(getpid()));
	return (sched_getscheduler(0)==4 && is_short(getpid())==1);
}
static int test_setsched_success() {
	return run_forked(setsched_success);
}


// 6		can be set twice
static int setsched_twice() {
	if (become_short(4000, 1) == -1) {
		return 0;
	}
	return (become_short(4000, 1) == -1);
}
static int test_setsched_twice() {
	return run_forked(setsched_twice);
}


// 7	can't change the trial val
static int setparams_after_setsched() {
	struct {
		int prio;
		int req_time;
		int trial_num;
	} param;
	param.req_time = 1;
	param.trial_num = 2;
	if (become_short(5000, 1) == -1) {
		return 0;
	}
	return (sched_setparam(getpid(), (struct sched_param *) &param) == -1);
}
static int test_setparam_after_setsched() {
	return run_forked(setparams_after_setsched);
}


// 8
static int getparam(void *p) {
	struct {
		int prio;
		int req_time;
		int trial_num;
	} param;
	if (become_short(5000, 1) == -1) {
		return 0;
	}
	if (sched_getparam(getpid(), (struct sched_param *) &param))
		return 0;
	if (param.req_time != 5000 || param.trial_num != 1)
		return 0;
	return 1;
}
static int test_getparam() {
	return run_forked(getparam);
}


// 9
static int rem_time_not_overdue() {
	if (become_short(5000, 50) == -1)
		return 0;
	return (rem_time(getpid()) > 0);
}
static int test_rem_time_not_overdue() {
	return run_forked(rem_time_not_overdue);
}


// 10		check num of trialsssss
static int rem_trails_not_overdue() {
	if (become_short(3000, 20) == -1)
		return 0;
	return (rem_trials(getpid()) > 0);
}
static int test_rem_trails_not_overdue() {
	return run_forked(rem_trails_not_overdue);
}


// 11
static int rem_time_overdue() {
	int start = time(NULL);
	if (become_short(5, 1) == -1) {
		return 0;
	}
	while (time(NULL) - start < 2)
		;
	return (rem_time(getpid())==0 && is_short(getpid())==0);
}
static int test_rem_time_overdue() {
	return run_forked(rem_time_overdue);
}


// 12
static int rem_trials_overdue() {
	errno = 0;
	int start = time(NULL);
	if (become_short(5, 1) == -1) {
		perror("test 12 - setsched errno is");
		return 0;
	}
	while (time(NULL) - start < 2)
		;
	return (rem_trials(getpid())==0 && is_short(getpid())==0);
}
static int test_rem_trials_overdue() {
	return run_forked(rem_trials_overdue);
}


// 13	?????????????????????????????
static int short_preempts_other() {
	int start;
	if (become_short(5000, 1) == -1)
		return 0;
	start = time(NULL);
	while(time(NULL) - start < 5)
		;
	return 1;
}
static int test_short_preempts_other() {
	struct timeval start, end;
	int pid; 
	if (gettimeofday(&start, NULL)) {
		perror("gettimeofday");
		exit(1);
	}
	pid = fork();
	if (pid < 0) {
		perror("if fork fail");
		exit(1);
	}
	if (!pid) {
		short_preempts_other();
		exit(0);
	} else {
		sleep(1);
		if (gettimeofday(&end, NULL)) {
			perror("gettimeofday");
			exit(1);
		}
		waitpid(pid, NULL, 0);
	}
	if (end.tv_sec - start.tv_sec < 5)
		return 0;
	return 1;
}


// 14		????????????????????????????????????
static int other_preempts_overdue() {
	int start;
	if (become_short(1, 1) == -1)
		return 0;
	while (rem_time(getpid()))
		;
	start = time(NULL);
	while (time(NULL) - start < 2)
		;
	return 1;
}
static int test_other_preempts_overdue() {
	struct timeval start, end;
	int pid = fork();
	if (pid < 0) {
		perror("if fork fail");
		exit(1);
	}
	if (!pid) {
		other_preempts_overdue();
		exit(0);
	} else {
		sleep(1);
		if (gettimeofday(&start, NULL)) {
			perror("gettimeofday");
			exit(1);
		}
		waitpid(pid, NULL, 0);
		if (gettimeofday(&end, NULL)) {
			perror("gettimeofday");
			exit(1);
		}
	}
	if (end.tv_sec - start.tv_sec < 1)
		return 0;
	return 1;
}


// 15		req_time stay the same
static int __fork_child_requested_time() {
	struct {
		int prio;
		int req_time;
		int trials;
	} param;
	sched_getparam(getpid(), (struct sched_param *) &param);
	return param.req_time;
}
static int fork_child_requested_time() {
	int child_req_time;
	if (become_short(1000, 20) == -1)
		return 0;
	return 0;
	child_req_time = run_forked(__fork_child_requested_time);
	return (child_req_time == 1000);
}
static int test_fork_requested_time() {
	return run_forked(fork_child_requested_time);
}


// 16 , 17	and parent////////////////////////////////////
static int __fork_child_trials() {
	struct {
		int prio;
		int req_time;
		int trials;
	} param;
	sched_getparam(getpid(), (struct sched_param *) &param);
	return param.trials;
}

// 16	trials/2
static int fork_child_trials_even() {
	int trials = 20;
	if (become_short(1000, trials) == -1)
		return 0;
	return 0;
	return (run_forked(__fork_child_trials) == (trials/2));
}
static int test_fork_child_trials_even() {
	return run_forked(fork_child_trials_even);
}


// 17	trials/2 +1
static int fork_child_trials_odd() {
	int trials = 21;
	if (become_short(1000, trials) == -1)
		return 0;
	return 0;
	return (run_forked(__fork_child_trials) == (trials/2)+1);
}
static int test_fork_child_trials_odd() {
	return run_forked(fork_child_trials_odd);
}

// 18		parent childd
static int fork_parent_remaining_time() {
	int rem;
	if (become_short(1000, 2) == -1)
		return 0;
	rem = rem_time(getpid());		//before fork 
	switch (fork()) {
	case -1:
		perror("fork failed");
	case 0:
		exit(1);
	}		
	return((rem/2) == rem_time(getpid()));	//after fork
}
static int test_fork_remaining_time() {
	return run_forked(fork_parent_remaining_time);
}


// 19

/*
static int test_read_stats_150_records() {
	struct switch_info infos[150];
	return 150 == read_stats(&infos);
}
*/


////////////////////////////////////// tests macro //////////////////////////////////////////

struct test_def {
	int (*func)();
	const char *name;
};

#define DEFINE_TEST(func) { func, #func }

struct test_def tests[] = {
	DEFINE_TEST(test_req_too_small),			// 1
	DEFINE_TEST(test_req_too_big),				// 2
	DEFINE_TEST(test_trials_too_small),			// 3
	DEFINE_TEST(test_trials_too_big),			// 4
	DEFINE_TEST(test_setsched_success),			// 5
	DEFINE_TEST(test_setsched_twice),			// 6
	DEFINE_TEST(test_setparam_after_setsched),	// 7
	DEFINE_TEST(test_getparam),					// 8
	DEFINE_TEST(test_rem_time_not_overdue),		// 9
	DEFINE_TEST(test_rem_trails_not_overdue),	// 10
	DEFINE_TEST(test_rem_time_overdue),			// 11
	DEFINE_TEST(test_rem_trials_overdue),		// 12
/*	DEFINE_TEST(test_short_preempts_other),		// 13
	DEFINE_TEST(test_other_preempts_overdue),	// 14
	DEFINE_TEST(test_fork_requested_time),		// 15
	DEFINE_TEST(test_fork_child_trials_even),	// 16
	DEFINE_TEST(test_fork_child_trials_odd),	// 17
	DEFINE_TEST(test_fork_remaining_time),		// 18*/


	{ NULL, "The End" },
};

int main() {
	struct test_def *current = &tests[0];
	int counter = 1;
	while (current->func) {
		printf("%d %-35s:\t%s\n",
		       counter, current->name,
		       (1 == current->func()) ? "PASS" : "FAIL");
		current++;
		counter++;
	};
	return 0;
}

