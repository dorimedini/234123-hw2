/*
 * test.c
 *
 *  Created on: Apr 30, 2015
 *      Author: shimonaz
 */


#include <stdio.h>
#include "Test_Utils.h"

#define rt_task(p)	((p)->prio < 99)

typedef enum {SCHED_SHORT, SCHED_FIFO ,SCHED_RR, SCHED_OTHER } Policy;

typedef struct Task_t {
	Policy policy;
	int remaining_trials;
	int prio;
}task_t;



/**
 * HW2:
 *
 * Returns 1 if this is an overdue SHORT process
 */
int is_overdue(task_t* p) {
	return (p->policy == SCHED_SHORT && !p->remaining_trials) ? 1 : 0;
}

/**
 * HW2:
 *
 * Returns 1 if this is an real time process
 */
int is_rt(task_t* p) {
	return (p->policy == SCHED_RR || p->policy == SCHED_FIFO ) ? 1 : 0;
}

/**
 * HW2:
 *
 * Returns 1 if this is an SHORT process
 */
int is_short(task_t* p) {
	return (p->policy == SCHED_SHORT) ? 1 : 0;
}

/**
 * HW2:
 *
 * Returns 1 if this is an OTHER process
 */
int is_other(task_t* p) {
	return (p->policy == SCHED_OTHER) ? 1 : 0;
}


/* Put here the should_switch function */

int should_switch(task_t* oldt, task_t* newt) {

	// Overdue process never switches any other process
	if (is_overdue(newt)) {
		return 0;
	}
	// New process is rt
	if (rt_task(newt)) {
		return (!rt_task(oldt) || oldt->prio > newt->prio);
	}

	// In this case the new process isn't overdue or rt.
	// So lets see some cases which the new process is short.
	if (is_short(newt) ) {
		if (is_short(oldt)) {
			// is_short func check for short or overdue so in those both cases return 1.
			if (is_overdue(oldt)) {
				// The old process is overdue and a little cute short (not overdue process) wants to run.
				return 1;
			} else {
				// Both processes are shorts (not overdues) let's compare their dick's length.
				return (oldt->prio > newt->prio);
			}
		// The old process is not short
		} else {
			return (!rt_task(oldt));
		}
	} else {
		if (is_short(oldt)) {
			return 0;
		} else {
			// That means the new is other.
			if (is_short(oldt)) {
				return is_overdue(oldt);
			} else {
				if (rt_task(oldt)) {
					return 0;
				} else {
					return newt->prio < oldt->prio;
				}
			}
		}

	}
}



int runTests() {
	task_t rt1, rt2;
	task_t oth1, oth2;
	task_t sho1, sho2;
	task_t ove1, ove2;

	// init policy
	rt1.policy = SCHED_RR;
	oth1.policy = SCHED_OTHER;
	sho1.policy = SCHED_SHORT;
	ove1.policy = SCHED_SHORT;

	rt2.policy = SCHED_RR;
	oth2.policy = SCHED_OTHER;
	sho2.policy = SCHED_SHORT;
	ove2.policy = SCHED_SHORT;

	// init remaining_trials
	rt1.remaining_trials = 2;
	oth1.remaining_trials = 3;
	sho1.remaining_trials = 1;
	ove1.remaining_trials = 0;

	rt2.remaining_trials = 34;
	oth2.remaining_trials = 3;
	sho2.remaining_trials = 1;
	ove2.remaining_trials = 0;

	// init prio
	rt1.prio = 97;
	oth1.prio = 101;
	sho1.prio = 101;
	ove1.prio = 101;

	rt2.prio = 98;
	oth2.prio = 118;
	sho2.prio = 118;
	ove2.prio = 118;

	NO_SWITCH(&ove1,&ove1);
	NO_SWITCH(&ove2,&ove1);
	NO_SWITCH(&ove1,&ove2);
	NO_SWITCH(&sho1,&ove1);
	NO_SWITCH(&sho2,&ove1);
	NO_SWITCH(&oth2,&ove1);
	NO_SWITCH(&oth1,&ove1);
	NO_SWITCH(&rt1,&ove1);

	SWITCH(&rt2,&rt1);
	NO_SWITCH(&rt1,&rt1);

	SWITCH(&oth1,&rt1);
	SWITCH(&sho1,&rt1);

	SWITCH(&ove1,&rt1);
	NO_SWITCH(&rt1,&sho1);

	SWITCH(&sho2,&sho1);
	NO_SWITCH(&sho1,&sho2);

	SWITCH(&oth2,&sho1);
	SWITCH(&oth1,&sho2);

	SWITCH(&ove2,&sho1);
	SWITCH(&ove1,&sho2);

	NO_SWITCH(&rt1,&oth1);

	NO_SWITCH(&sho2,&oth1);
	NO_SWITCH(&sho1,&oth2);

	SWITCH(&oth2,&oth1);
	NO_SWITCH(&oth1,&oth2);

	NO_SWITCH(&ove2,&oth1);
	NO_SWITCH(&ove1,&oth2);

	NO_SWITCH(&rt1,&rt1);
	NO_SWITCH(&sho1,&sho1);
	NO_SWITCH(&oth1,&oth1);
	NO_SWITCH(&sho1,&oth1);

	SWITCH(&oth1,&sho1);
	NO_SWITCH(&sho1,&ove1);
	SWITCH(&ove1,&sho1);
	return 1;
}

int main() {
	RUN_TEST(runTests);
	return 0;
}
