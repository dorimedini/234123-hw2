/*
 *  kernel/sched.c
 *
 *  Kernel scheduler and related syscalls
 *
 *  Copyright (C) 1991-2002  Linus Torvalds
 *
 *  1996-12-23  Modified by Dave Grothe to fix bugs in semaphores and
 *              make semaphores SMP safe
 *  1998-11-19	Implemented schedule_timeout() and related stuff
 *		by Andrea Arcangeli
 *  2002-01-04	New ultra-scalable O(1) scheduler by Ingo Molnar:
 *  		hybrid priority-list and round-robin design with
 *  		an array-switch method of distributing timeslices
 *  		and per-CPU runqueues.  Additional code by Davide
 *  		Libenzi, Robert Love, and Rusty Russell.
 */
 
#include <linux/mm.h>
#include <linux/nmi.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/highmem.h>
#include <linux/smp_lock.h>
#include <asm/mmu_context.h>
#include <linux/interrupt.h>
#include <linux/completion.h>
#include <linux/kernel_stat.h>

/**
 * HW2:
 *
 * Declare the global logger (declared in sched.h)
 */
extern hw2_switch_log hw2_logger;

/**
 * HW2:
 *
 * Set this debug flag to 1 if you want OTHER
 * processes to run before SHORT (hoping this works)
 */
int hw2_debug = 0;


/**
 * HW2:
 *
 * Implementation of the switch logger functions (declared in sched.h).
 */
void hw2_start_logging(hw2_switch_log* logger) {
	logger->remaining_switches = 30;
}
void hw2_log_switch(hw2_switch_log* logger, task_t *prev, task_t *next) {

	// If we don't need to log any more, do nothing
	if (!logger->remaining_switches) return;
	
	// Update switch fields:
	struct switch_info* info = logger->arr;
	int index = logger->next_index;
	info[index].previous_pid = prev->pid;
	info[index].next_pid = next->pid;
	info[index].previous_policy = prev->policy;
	info[index].next_policy = next->policy;
	info[index].time = jiffies;
	info[index].reason = prev->switch_reason;
	
	// DEBUG:
	PRINT("LOGGING SWITCH: PID=%d-->%d, POLICY=%d-->%d, REASON=%d\n", prev->pid, next->pid, prev->policy, next->policy, prev->switch_reason);
	
	// Reset the switch reason so we don't log
	// the same reason twice by accident...
	UPDATE_REASON(prev,SWITCH_UNKNOWN);
	
	// Update outer fields:
	logger->remaining_switches--;
	logger->next_index++;
	logger->next_index %= 150;
	if (logger->logged < 150)
		logger->logged++;
}

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


/**
 * HW2:  TESTED AND PASSED !
 *
 * Calculate whether or not the old task is more important
 * than the new, regarding the value of prio.
 *
 * Note that we should only use this function if
 * hw2_debug is OFF (=0)
 *
 * Doesn't calculated dynamic priority.
 *
 * Implementation bellow.
 */
int should_switch(task_t*, task_t*);




/*
 * Convert user-nice values [ -20 ... 0 ... 19 ]
 * to static priority [ MAX_RT_PRIO..MAX_PRIO-1 ],
 * and back.
 */
#define NICE_TO_PRIO(nice)	(MAX_RT_PRIO + (nice) + 20)
#define PRIO_TO_NICE(prio)	((prio) - MAX_RT_PRIO - 20)
#define TASK_NICE(p)		PRIO_TO_NICE((p)->static_prio)

/*
 * 'User priority' is the nice value converted to something we
 * can work with better when scaling various scheduler parameters,
 * it's a [ 0 ... 39 ] range.
 */
#define USER_PRIO(p)		((p)-MAX_RT_PRIO)
#define TASK_USER_PRIO(p)	USER_PRIO((p)->static_prio)
#define MAX_USER_PRIO		(USER_PRIO(MAX_PRIO))

/*
 * These are the 'tuning knobs' of the scheduler:
 *
 * Minimum timeslice is 10 msecs, default timeslice is 150 msecs,
 * maximum timeslice is 300 msecs. Timeslices get refilled after
 * they expire.
 */
#define MIN_TIMESLICE		( 10 * HZ / 1000)
#define MAX_TIMESLICE		(300 * HZ / 1000)
#define CHILD_PENALTY		95
#define PARENT_PENALTY		100
#define EXIT_WEIGHT		3
#define PRIO_BONUS_RATIO	25
#define INTERACTIVE_DELTA	2
#define MAX_SLEEP_AVG		(2*HZ)
#define STARVATION_LIMIT	(2*HZ)

/*
 * If a task is 'interactive' then we reinsert it in the active
 * array after it has expired its current timeslice. (it will not
 * continue to run immediately, it will still roundrobin with
 * other interactive tasks.)
 *
 * This part scales the interactivity limit depending on niceness.
 *
 * We scale it linearly, offset by the INTERACTIVE_DELTA delta.
 * Here are a few examples of different nice levels:
 *
 *  TASK_INTERACTIVE(-20): [1,1,1,1,1,1,1,1,1,0,0]
 *  TASK_INTERACTIVE(-10): [1,1,1,1,1,1,1,0,0,0,0]
 *  TASK_INTERACTIVE(  0): [1,1,1,1,0,0,0,0,0,0,0]
 *  TASK_INTERACTIVE( 10): [1,1,0,0,0,0,0,0,0,0,0]
 *  TASK_INTERACTIVE( 19): [0,0,0,0,0,0,0,0,0,0,0]
 *
 * (the X axis represents the possible -5 ... 0 ... +5 dynamic
 *  priority range a task can explore, a value of '1' means the
 *  task is rated interactive.)
 *
 * Ie. nice +19 tasks can never get 'interactive' enough to be
 * reinserted into the active array. And only heavily CPU-hog nice -20
 * tasks will be expired. Default nice 0 tasks are somewhere between,
 * it takes some effort for them to get interactive, but it's not
 * too hard.
 */

#define SCALE(v1,v1_max,v2_max) \
	(v1) * (v2_max) / (v1_max)

#define DELTA(p) \
	(SCALE(TASK_NICE(p), 40, MAX_USER_PRIO*PRIO_BONUS_RATIO/100) + \
		INTERACTIVE_DELTA)

#define TASK_INTERACTIVE(p) \
	((p)->prio <= (p)->static_prio - DELTA(p))

/*
 * TASK_TIMESLICE scales user-nice values [ -20 ... 19 ]
 * to time slice values.
 *
 * The higher a process's priority, the bigger timeslices
 * it gets during one round of execution. But even the lowest
 * priority process gets MIN_TIMESLICE worth of execution time.
 */

#define TASK_TIMESLICE(p) (MIN_TIMESLICE + \
	((MAX_TIMESLICE - MIN_TIMESLICE) * (MAX_PRIO-1-(p)->static_prio)/39))

/*
 * These are the runqueue data structures:
 */

#define BITMAP_SIZE ((((MAX_PRIO+1+7)/8)+sizeof(long)-1)/sizeof(long))

typedef struct runqueue runqueue_t;

struct prio_array {
	int nr_active;
	unsigned long bitmap[BITMAP_SIZE];
	list_t queue[MAX_PRIO];
};

/*
 * This is the main, per-CPU runqueue data structure.
 *
 * Locking rule: those places that want to lock multiple runqueues
 * (such as the load balancing or the process migration code), lock
 * acquire operations must be ordered by ascending &runqueue.
 */
struct runqueue {
	spinlock_t lock;
	unsigned long nr_running, nr_switches, expired_timestamp;
	signed long nr_uninterruptible;
	task_t *curr, *idle;
	prio_array_t *active, *expired, arrays[/*2*/3];	// We need an extra prio_array
	int prev_nr_running[NR_CPUS];
	task_t *migration_thread;
	list_t migration_queue;
	
	/**
	 * HW2:
	 *
	 * active_SHORT:
	 * Behaves like the *active queue, only it
	 * contains SHORT processes only. Allows us to run prioritized
	 * SHORT processes round-robin style.
	 */
	prio_array_t *active_SHORT;
	
	/**
	 * HW2:
	 *
	 * expired_SHORT:
	 * As the particular way SHORT processes handle
	 * round-robin scheduling, only members of the same priority
	 * group should go around in round-robin. Thus, when deciding
	 * on which SHORT to run, we must check:
	 *    Is the highest non-empty priority group P in active_SHORT
	 *    a higher/equal priority to all non-empty priority groups
	 *    in expired_SHORT?
	 *    ---> If so, choose a process of priority P from the 
	 *         active_SHORT queue.
	 *    ---> Otherwise, switch pointers from the highest priority
	 *         non-empty priority group Q in expired_SHORT with it's
	 *         corresponding pointer in active_SHORT. Then, choose
	 *         a process to run from the Q group in the new active_SHORT
	 *         queue.
	 *
	 * UPDATE: WE DON'T NEED THIS!
	 */
	//prio_array_t *expired_SHORT;
	
	/**
	 * HW2:
	 *
	 * overdue_SHORT:
	 * As the overdue processes only need to run
	 * FIFO-style, all we need is a regular queue. Insert newly
	 * overdue SHORT processes at the start of the list, and when
	 * running them (no OTHER, SHORT or real-time process to run)
	 * start with the last process in the list.
	 */
	list_t overdue_SHORT;
	
	/**
	 * HW2:
	 *
	 * curr_overdue_SHORT:
	 * A pointer to the last overdue-SHORT process in overdue_SHORT[]
	 * (which is in fact the next overdue-SHORT process to run)
	 *
	 * After reading a bit about the linux list_t, and realising that
	 * the last element can be reached in O(1) because it's a circular
	 * list, I think we don't need this field at all...
	 */
	//list_t curr_overdue_SHORT;
	
} ____cacheline_aligned;

static struct runqueue runqueues[NR_CPUS] __cacheline_aligned;

#define cpu_rq(cpu)		(runqueues + (cpu))
#define this_rq()		cpu_rq(smp_processor_id())
#define task_rq(p)		cpu_rq((p)->cpu)
#define cpu_curr(cpu)		(cpu_rq(cpu)->curr)
#define rt_task(p)		((p)->prio < MAX_RT_PRIO)

/*
 * Default context-switch locking:
 */
#ifndef prepare_arch_schedule
# define prepare_arch_schedule(prev)	do { } while(0)
# define finish_arch_schedule(prev)	do { } while(0)
# define prepare_arch_switch(rq)	do { } while(0)
# define finish_arch_switch(rq)		spin_unlock_irq(&(rq)->lock)
#endif

/**
 * HW2:
 *
 * Defined above. See definition for documentation.
 */
int should_switch(task_t* oldt, task_t* newt) {
	
	// FOR DEBUGGING PURPOSES:
	// Don't run SHORTs before OTHERs, in case
	// this breaks the system.
	
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




/*
 * task_rq_lock - lock the runqueue a given task resides on and disable
 * interrupts.  Note the ordering: we can safely lookup the task_rq without
 * explicitly disabling preemption.
 */
static inline runqueue_t *task_rq_lock(task_t *p, unsigned long *flags)
{
	struct runqueue *rq;

repeat_lock_task:
	local_irq_save(*flags);
	rq = task_rq(p);
	spin_lock(&rq->lock);
	if (unlikely(rq != task_rq(p))) {
		spin_unlock_irqrestore(&rq->lock, *flags);
		goto repeat_lock_task;
	}
	return rq;
}

static inline void task_rq_unlock(runqueue_t *rq, unsigned long *flags)
{
	spin_unlock_irqrestore(&rq->lock, *flags);
}

/*
 * rq_lock - lock a given runqueue and disable interrupts.
 */
static inline runqueue_t *this_rq_lock(void)
{
	runqueue_t *rq;

	local_irq_disable();
	rq = this_rq();
	spin_lock(&rq->lock);

	return rq;
}

static inline void rq_unlock(runqueue_t *rq)
{
	spin_unlock(&rq->lock);
	local_irq_enable();
}

/*
 * Adding/removing a task to/from a priority array:
 */
static inline void dequeue_task(struct task_struct *p, prio_array_t *array)
{
	array->nr_active--;
	list_del(&p->run_list);
	if (list_empty(array->queue + p->prio))
		__clear_bit(p->prio, array->bitmap);
}

static inline void enqueue_task(struct task_struct *p, prio_array_t *array)
{
	list_add_tail(&p->run_list, array->queue + p->prio);
	__set_bit(p->prio, array->bitmap);
	array->nr_active++;
	p->array = array;
}

/**
 * HW2:
 *
 * Test if a process is in a run queue already.
 * This should be useful so we don't accidentally
 * enqueue/dequeue a process twice.
 */
int is_queued(task_t* p) {
	return !list_empty(&p->run_list);
}

/**
 * HW2:
 *
 * Before calls to enqueue or dequeue, we need
 * to decide which prio_array or list_t to add
 * the process to.
 * Call this instead of enqueue() for enqueueing
 *
 * Before enqueueing the task we need to check where
 * to enqueue it.
 * If it's an OTHER process, just enqueue it in rq->active.
 * If it's a non-overdue SHORT, enqueue in rq->active_SHORT
 * If it's overdue, add to the head of the overdue_SHORT list
 *
 * When dequeueing a non-overdue process, perform similar
 * actions as in hw2_enqueue. If we're dequeueing an overdue
 * SHORT task we need to take care of the list structure.
 *
 * enqueue_task also does p->array->nr_running++,
 * but an overdue task doesn't have a counter.
 * It doesn't matter - the runqueue's counter isn't
 * touched by enqueue_task, so it's value should be
 * valid here.
 * For example, we can see in the code things like:
 * - enqueue_task(p,rq->active);
 * - rq->nr_running++;
 * meaning enqueue_task isn't responsible for counting
 * the number of tasks in the runqueue.
 */
void hw2_enqueue(task_t* p, runqueue_t* rq, int make_active) {
	
	// Regular process (or debug mode)
	if (hw2_debug || !is_short(p)) {
		enqueue_task(p, make_active ? rq->active : rq->expired);
	}
	// Non-overdue SHORT (no expired array)
	else if (!is_overdue(p)) {
		// Note that with this form of round-robin,
		// we want to enqueue "expired" tasks to the
		// end of the priority list.
		enqueue_task(p, rq->active_SHORT);
	}
	// Overdue SHORT. make_active is irrelevant,
	// but make sure p isn't already in the queue
	else /*if (!is_queued(p)) */{
		// Add the overdue task to it's FIFO list
		list_add(&p->run_list, &rq->overdue_SHORT);
		// enqueue_task is responsible for this pointer,
		// so we should nullify it here
		p->array = NULL;
	}
}
void hw2_dequeue(task_t* p, prio_array_t* array) {

	// If this isn't an overdue process, dequeue_task
	// doesn't need to know if this is a SHORT or not.
	// However, if it's overdue, make sure we're not
	// dequeueing again
	if (!hw2_debug && is_overdue(p)/* && is_queued(p)*/) {
		// Remove the task from the list.
		// Use list_del_init instead of list_del
		// so we can test the pointers and test - 
		// using only p - if p is in a list or
		// not (see is_queued)
		list_del_init(&p->run_list);
	}
	else {
		dequeue_task(p, array);
	}
}


/**
 * HW2:
 *
 * WARNING: ONLY CALL THIS FUNCTION IF CALLING
 * ENQUEUE AND DEQUEUE IS SAFE!
 *
 * Helper function to put a SHORT process back
 * at the end of the line, be it overdue or not.
 *
 * This is useful because the original scheduler
 * never had to do shit like this...
 */
void hw2_back_of_the_queue(task_t* p, runqueue_t* rq) {
	if (!is_short(p)) {
		return;
	}
	hw2_dequeue(p, p->array);
	hw2_enqueue(p, rq, 1);
}
 
#define OVERDUE_PUSH_BACK(p,rq) do { \
		if (is_overdue(p)) hw2_back_of_the_queue(p, rq); \
	} while(0)




static inline int effective_prio(task_t *p)
{
	int bonus, prio;
	
	/**
	 * HW2:
	 *
	 * If the process is SHORT, just return the static_prio.
	 * The process's nice is already calculated in the
	 * static_prio, and we don't want any bonuses of any kind!
	 */
	/** START HW2 */
	if (p->policy == SCHED_SHORT)
		return p->static_prio;
	/** END HW2 */
	
	/*
	 * Here we scale the actual sleep average [0 .... MAX_SLEEP_AVG]
	 * into the -5 ... 0 ... +5 bonus/penalty range.
	 *
	 * We use 25% of the full 0...39 priority range so that:
	 *
	 * 1) nice +19 interactive tasks do not preempt nice 0 CPU hogs.
	 * 2) nice -20 CPU hogs do not get preempted by nice 0 tasks.
	 *
	 * Both properties are important to certain workloads.
	 */
	bonus = MAX_USER_PRIO*PRIO_BONUS_RATIO*p->sleep_avg/MAX_SLEEP_AVG/100 -
			MAX_USER_PRIO*PRIO_BONUS_RATIO/100/2;

	prio = p->static_prio - bonus;
	if (prio < MAX_RT_PRIO)
		prio = MAX_RT_PRIO;
	if (prio > MAX_PRIO-1)
		prio = MAX_PRIO-1;
	return prio;
}

static inline void activate_task(task_t *p, runqueue_t *rq)
{
	unsigned long sleep_time = jiffies - p->sleep_timestamp;
	prio_array_t *array = rq->active;

	if (!rt_task(p) && sleep_time) {
		/*
		 * This code gives a bonus to interactive tasks. We update
		 * an 'average sleep time' value here, based on
		 * sleep_timestamp. The more time a task spends sleeping,
		 * the higher the average gets - and the higher the priority
		 * boost gets as well.
		 */
		p->sleep_avg += sleep_time;
		if (p->sleep_avg > MAX_SLEEP_AVG)
			p->sleep_avg = MAX_SLEEP_AVG;
		p->prio = effective_prio(p);
	}
	/** START HW2 */
	// enqueue_task(p, array); //Original code
	/** END HW2 */
	hw2_enqueue(p, rq, 1);
	rq->nr_running++;
}

static inline void deactivate_task(struct task_struct *p, runqueue_t *rq)
{
	rq->nr_running--;
	if (p->state == TASK_UNINTERRUPTIBLE)
		rq->nr_uninterruptible++;
	/** START HW2 */
	// dequeue_task(p, p->array); Original code
	hw2_dequeue(p, p->array);
	/** END HW2 */
	p->array = NULL;
}

static inline void resched_task(task_t *p)
{
#ifdef CONFIG_SMP
	int need_resched;

	need_resched = p->need_resched;
	wmb();
	set_tsk_need_resched(p);
	if (!need_resched && (p->cpu != smp_processor_id()))
		smp_send_reschedule(p->cpu);
#else
	set_tsk_need_resched(p);
#endif
}

#ifdef CONFIG_SMP

/*
 * Wait for a process to unschedule. This is used by the exit() and
 * ptrace() code.
 */
void wait_task_inactive(task_t * p)
{
	unsigned long flags;
	runqueue_t *rq;

repeat:
	rq = task_rq(p);
	if (unlikely(rq->curr == p)) {
		cpu_relax();
		goto repeat;
	}
	rq = task_rq_lock(p, &flags);
	if (unlikely(rq->curr == p)) {
		task_rq_unlock(rq, &flags);
		goto repeat;
	}
	task_rq_unlock(rq, &flags);
}

/*
 * Kick the remote CPU if the task is running currently,
 * this code is used by the signal code to signal tasks
 * which are in user-mode as quickly as possible.
 *
 * (Note that we do this lockless - if the task does anything
 * while the message is in flight then it will notice the
 * sigpending condition anyway.)
 */
void kick_if_running(task_t * p)
{
	if (p == task_rq(p)->curr)
		resched_task(p);	// This is only called from kernel/signal.c, I don't think we need to log something here
}
#endif

/*
 * Wake up a process. Put it on the run-queue if it's not
 * already there.  The "current" process is always on the
 * run-queue (except when the actual re-schedule is in
 * progress), and as such you're allowed to do the simpler
 * "current->state = TASK_RUNNING" to mark yourself runnable
 * without the overhead of this.
 */
static int try_to_wake_up(task_t * p, int sync)
{
	unsigned long flags;
	int success = 0;
	long old_state;
	runqueue_t *rq;

repeat_lock_task:
	rq = task_rq_lock(p, &flags);
	old_state = p->state;
	/**
	 * HW2:
	 *
	 * The !p->array test isn't good for overdue
	 * tasks... they may be enqueued in the queue
	 * but their array field may be NULL.
	 *
	 * For non-overdues, the test is sufficient - 
	 * but for overdue processes we need to test
	 * if they're enqueued
	 */
	/** START HW2 */
//	if (!p->array) {	OLD IF STATEMENT
	if ((!p->array && !is_overdue(p)) || (is_overdue(p) && !is_queued(p))) {
	/** END HW2 */
		/*
		 * Fast-migrate the task if it's not running or runnable
		 * currently. Do not violate hard affinity.
		 */
		if (unlikely(sync && (rq->curr != p) &&
			(p->cpu != smp_processor_id()) &&
			(p->cpus_allowed & (1UL << smp_processor_id())))) {
			
			p->cpu = smp_processor_id();
			task_rq_unlock(rq, &flags);
			goto repeat_lock_task;
		}
		if (old_state == TASK_UNINTERRUPTIBLE)
			rq->nr_uninterruptible--;
		activate_task(p, rq);
		/*
		 * If sync is set, a resched_task() is a NOOP
		 */
		/**
		 * HW2:
		 *
		 * We need to do some more tests to see if the woken-up task
		 * needs to be scheduled instead of the current one.
		 */
		// Old code:
		if (hw2_debug && p->prio < rq->curr->prio)
			resched_task(rq->curr);
		// New code:
		else if (!hw2_debug && should_switch(rq->curr, p)) {
			resched_task(rq->curr);
			UPDATE_REASON(rq->curr, SWITCH_PRIO);
			/**
			 * HW2:
			 *
			 * If the currently running process is overdue,
			 * it needs to go back to the back of the line
			 * after switching...
			 */
			OVERDUE_PUSH_BACK(rq->curr, rq);
		}
		success = 1;
	}
	p->state = TASK_RUNNING;
	task_rq_unlock(rq, &flags);

	return success;
}

int wake_up_process(task_t * p)
{
	return try_to_wake_up(p, 0);
}

void wake_up_forked_process(task_t * p)
{
	runqueue_t *rq = this_rq_lock();

	p->state = TASK_RUNNING;
	if (!rt_task(p)) {
		/*
		 * We decrease the sleep average of forking parents
		 * and children as well, to keep max-interactive tasks
		 * from forking tasks that are max-interactive.
		 */
		current->sleep_avg = current->sleep_avg * PARENT_PENALTY / 100;
		p->sleep_avg = p->sleep_avg * CHILD_PENALTY / 100;
		p->prio = effective_prio(p);
	}
	p->cpu = smp_processor_id();
	activate_task(p, rq);

	rq_unlock(rq);
}

/*
 * Potentially available exiting-child timeslices are
 * retrieved here - this way the parent does not get
 * penalized for creating too many processes.
 *
 * (this cannot be used to 'generate' timeslices
 * artificially, because any timeslice recovered here
 * was given away by the parent in the first place.)
 */
void sched_exit(task_t * p)
{
	__cli();
	/**
	 * HW2:
	 *
	 * Don't give daddy his time slice back!
	 */
	if (p->first_time_slice/** START HW2 */ && !is_short(p)/** END HW2 */) {
		current->time_slice += p->time_slice;
		if (unlikely(current->time_slice > MAX_TIMESLICE))
			current->time_slice = MAX_TIMESLICE;
	}
	__sti();
	/*
	 * If the child was a (relative-) CPU hog then decrease
	 * the sleep_avg of the parent as well.
	 */
	if (p->sleep_avg < current->sleep_avg)
		current->sleep_avg = (current->sleep_avg * EXIT_WEIGHT +
			p->sleep_avg) / (EXIT_WEIGHT + 1);
}

#if CONFIG_SMP
asmlinkage void schedule_tail(task_t *prev)
{
	finish_arch_switch(this_rq());
	finish_arch_schedule(prev);
}
#endif

static inline task_t * context_switch(task_t *prev, task_t *next)
{
	struct mm_struct *mm = next->mm;
	struct mm_struct *oldmm = prev->active_mm;

	if (unlikely(!mm)) {
		next->active_mm = oldmm;
		atomic_inc(&oldmm->mm_count);
		enter_lazy_tlb(oldmm, next, smp_processor_id());
	} else
		switch_mm(oldmm, mm, next, smp_processor_id());

	if (unlikely(!prev->mm)) {
		prev->active_mm = NULL;
		mmdrop(oldmm);
	}

	/* Here we just switch the register state and the stack. */
	switch_to(prev, next, prev);

	return prev;
}

unsigned long nr_running(void)
{
	unsigned long i, sum = 0;

	for (i = 0; i < smp_num_cpus; i++)
		sum += cpu_rq(cpu_logical_map(i))->nr_running;

	return sum;
}

unsigned long nr_uninterruptible(void)
{
	unsigned long i, sum = 0;

	for (i = 0; i < smp_num_cpus; i++)
		sum += cpu_rq(cpu_logical_map(i))->nr_uninterruptible;

	return sum;
}

unsigned long nr_context_switches(void)
{
	unsigned long i, sum = 0;

	for (i = 0; i < smp_num_cpus; i++)
		sum += cpu_rq(cpu_logical_map(i))->nr_switches;

	return sum;
}

#if CONFIG_SMP
/*
 * Lock the busiest runqueue as well, this_rq is locked already.
 * Recalculate nr_running if we have to drop the runqueue lock.
 */
static inline unsigned int double_lock_balance(runqueue_t *this_rq,
	runqueue_t *busiest, int this_cpu, int idle, unsigned int nr_running)
{
	if (unlikely(!spin_trylock(&busiest->lock))) {
		if (busiest < this_rq) {
			spin_unlock(&this_rq->lock);
			spin_lock(&busiest->lock);
			spin_lock(&this_rq->lock);
			/* Need to recalculate nr_running */
			if (idle || (this_rq->nr_running > this_rq->prev_nr_running[this_cpu]))
				nr_running = this_rq->nr_running;
			else
				nr_running = this_rq->prev_nr_running[this_cpu];
		} else
			spin_lock(&busiest->lock);
	}
	return nr_running;
}

/*
 * Current runqueue is empty, or rebalance tick: if there is an
 * inbalance (current runqueue is too short) then pull from
 * busiest runqueue(s).
 *
 * We call this with the current runqueue locked,
 * irqs disabled.
 */
static void load_balance(runqueue_t *this_rq, int idle)
{
	int imbalance, nr_running, load, max_load,
		idx, i, this_cpu = smp_processor_id();
	task_t *next = this_rq->idle, *tmp;
	runqueue_t *busiest, *rq_src;
	prio_array_t *array;
	list_t *head, *curr;

	/*
	 * We search all runqueues to find the most busy one.
	 * We do this lockless to reduce cache-bouncing overhead,
	 * we re-check the 'best' source CPU later on again, with
	 * the lock held.
	 *
	 * We fend off statistical fluctuations in runqueue lengths by
	 * saving the runqueue length during the previous load-balancing
	 * operation and using the smaller one the current and saved lengths.
	 * If a runqueue is long enough for a longer amount of time then
	 * we recognize it and pull tasks from it.
	 *
	 * The 'current runqueue length' is a statistical maximum variable,
	 * for that one we take the longer one - to avoid fluctuations in
	 * the other direction. So for a load-balance to happen it needs
	 * stable long runqueue on the target CPU and stable short runqueue
	 * on the local runqueue.
	 *
	 * We make an exception if this CPU is about to become idle - in
	 * that case we are less picky about moving a task across CPUs and
	 * take what can be taken.
	 */
	if (idle || (this_rq->nr_running > this_rq->prev_nr_running[this_cpu]))
		nr_running = this_rq->nr_running;
	else
		nr_running = this_rq->prev_nr_running[this_cpu];

	busiest = NULL;
	max_load = 1;
	for (i = 0; i < smp_num_cpus; i++) {
		int logical = cpu_logical_map(i);

		rq_src = cpu_rq(logical);
		if (idle || (rq_src->nr_running < this_rq->prev_nr_running[logical]))
			load = rq_src->nr_running;
		else
			load = this_rq->prev_nr_running[logical];
		this_rq->prev_nr_running[logical] = rq_src->nr_running;

		if ((load > max_load) && (rq_src != this_rq)) {
			busiest = rq_src;
			max_load = load;
		}
	}

	/**
	 * HW2:
	 *
	 * As we're assuming there's only one CPU, this function
	 * will always return here. The only way busiest!=NULL
	 * is if rq_src!=this_rq sometime in the code above - as
	 * there is only one runqueue, this won't happen.
	 */
	if (likely(!busiest))
		return;

	imbalance = (max_load - nr_running) / 2;

	/* It needs an at least ~25% imbalance to trigger balancing. */
	if (!idle && (imbalance < (max_load + 3)/4))
		return;

	nr_running = double_lock_balance(this_rq, busiest, this_cpu, idle, nr_running);
	/*
	 * Make sure nothing changed since we checked the
	 * runqueue length.
	 */
	if (busiest->nr_running <= nr_running + 1)
		goto out_unlock;

	/*
	 * We first consider expired tasks. Those will likely not be
	 * executed in the near future, and they are most likely to
	 * be cache-cold, thus switching CPUs has the least effect
	 * on them.
	 */
	if (busiest->expired->nr_active)
		array = busiest->expired;
	else
		array = busiest->active;

new_array:
	/* Start searching at priority 0: */
	idx = 0;
skip_bitmap:
	if (!idx)
		idx = sched_find_first_bit(array->bitmap);
	else
		idx = find_next_bit(array->bitmap, MAX_PRIO, idx);
	if (idx == MAX_PRIO) {
		if (array == busiest->expired) {
			array = busiest->active;
			goto new_array;
		}
		goto out_unlock;
	}

	head = array->queue + idx;
	curr = head->prev;
skip_queue:
	tmp = list_entry(curr, task_t, run_list);

	/*
	 * We do not migrate tasks that are:
	 * 1) running (obviously), or
	 * 2) cannot be migrated to this CPU due to cpus_allowed, or
	 * 3) are cache-hot on their current CPU.
	 */

#define CAN_MIGRATE_TASK(p,rq,this_cpu)					\
	((jiffies - (p)->sleep_timestamp > cache_decay_ticks) &&	\
		((p) != (rq)->curr) &&					\
			((p)->cpus_allowed & (1UL << (this_cpu))))

	curr = curr->prev;

	if (!CAN_MIGRATE_TASK(tmp, busiest, this_cpu)) {
		if (curr != head)
			goto skip_queue;
		idx++;
		goto skip_bitmap;
	}
	next = tmp;
	/*
	 * take the task out of the other runqueue and
	 * put it into this one:
	 */
	// HW2: No need to change this (see above comment)
	dequeue_task(next, array);
	busiest->nr_running--;
	next->cpu = this_cpu;
	this_rq->nr_running++;
	enqueue_task(next, this_rq->active);
	if (next->prio < current->prio)
		set_need_resched();	// HW2: No need to log this, we're assuming one CPU
	if (!idle && --imbalance) {
		if (curr != head)
			goto skip_queue;
		idx++;
		goto skip_bitmap;
	}
out_unlock:
	spin_unlock(&busiest->lock);
}

/*
 * One of the idle_cpu_tick() or the busy_cpu_tick() function will
 * gets called every timer tick, on every CPU. Our balancing action
 * frequency and balancing agressivity depends on whether the CPU is
 * idle or not.
 *
 * busy-rebalance every 250 msecs. idle-rebalance every 1 msec. (or on
 * systems with HZ=100, every 10 msecs.)
 */
#define BUSY_REBALANCE_TICK (HZ/4 ?: 1)
#define IDLE_REBALANCE_TICK (HZ/1000 ?: 1)

static inline void idle_tick(void)
{
	if (jiffies % IDLE_REBALANCE_TICK)
		return;
	spin_lock(&this_rq()->lock);
	load_balance(this_rq(), 1);
	spin_unlock(&this_rq()->lock);
}

#endif

/*
 * We place interactive tasks back into the active array, if possible.
 *
 * To guarantee that this does not starve expired tasks we ignore the
 * interactivity of a task if the first expired task had to wait more
 * than a 'reasonable' amount of time. This deadline timeout is
 * load-dependent, as the frequency of array switched decreases with
 * increasing number of running tasks:
 */
#define EXPIRED_STARVING(rq) \
		((rq)->expired_timestamp && \
		(jiffies - (rq)->expired_timestamp >= \
			STARVATION_LIMIT * ((rq)->nr_running) + 1))

/*
 * This function gets called by the timer code, with HZ frequency.
 * We call it with interrupts disabled.
 */
void scheduler_tick(int user_tick, int system)
{	
	
	int cpu = smp_processor_id();
	runqueue_t *rq = this_rq();									// The current run queue
	task_t *p = current;
	PRINT_ONCE_IF(is_overdue(p),"IN TICK WITH OVERDUE PROC %d\n", p->pid);
	if (p == rq->idle) {
		if (local_bh_count(cpu) || local_irq_count(cpu) > 1)
			kstat.per_cpu_system[cpu] += system;
#if CONFIG_SMP
		idle_tick();
#endif
		return;
	}															// Idle shit :)
	if (TASK_NICE(p) > 0)
		kstat.per_cpu_nice[cpu] += user_tick;
	else
		kstat.per_cpu_user[cpu] += user_tick;
	kstat.per_cpu_system[cpu] += system;

	/* Task might have expired already, but not scheduled off yet */

	/** 
	 * HW2: 
	 * If the current proc's policy is different
	 * from SCHED_SHORT, so continue with the regular check.
	 *
	 * Note that we don't need to do the same for SHORTs
	 * because
	 * 1. Overdue SHORTs don't have time slices and they don't expire
	 * 2. Regular SHORTs don't expire anyway
	 */
	/** START HW2 */
	if (p->policy != SCHED_SHORT) {
		if (p->array != rq->active) {				// The process is in the expired prio_array, but is still running...
			set_tsk_need_resched(p);
			/** START HW2 */
			// TA said to write this up as an end-of-timeslice context switch
			UPDATE_REASON(p, SWITCH_SLICE);
			/** END HW2 */
			return;
		}
	}
	/** END HW2 */
	
	spin_lock(&rq->lock);
	/**
	 * HW2:
	 *
	 * If we're overdue, it's like a SCHED_FIFO task as far as
	 * scheduler_tick() is concerned: add it to the if() statement
	 */
	if (unlikely(rt_task(p))/** START HW2 */ || is_overdue(p)/** END HW2 */) {
		/*
		 * RR tasks need a special form of timeslice management.
		 * FIFO tasks have no timeslices.
		 */
		if ((p->policy == SCHED_RR) && !--p->time_slice) {		// handle RR policy. Not interesting
			p->time_slice = TASK_TIMESLICE(p);
			p->first_time_slice = 0;
			set_tsk_need_resched(p);
			/** START HW2 */
			UPDATE_REASON(p, SWITCH_SLICE);
			/** END HW2 */

			/* put it at the end of the queue: */
			// HW2: No need to change this because
			// p isn't a SHORT process
			dequeue_task(p, rq->active);
			enqueue_task(p, rq->active);
		}
		goto out;
	}
	/*
	 * The task was running during this tick - update the
	 * time slice counter and the sleep average. Note: we
	 * do not update a process's priority until it either
	 * goes to sleep or uses up its timeslice. This makes
	 * it possible for interactive tasks to use up their
	 * timeslices at their highest priority levels.
	 */
	if (p->sleep_avg)									
		p->sleep_avg--;								// The procces isn't sleeping so decrease its sleep avg
	if (!--p->time_slice) {
		PRINT("IN TICK: pid=%d, NOT RT, ZERO TIME LEFT, ",p->pid);
		/** 
		 * HW2: 
		 * If the current proc's policy is different
		 * from SCHED_SHORT, so continue with the regular check.  
		 */		
		if (p->policy != SCHED_SHORT) {
			PRINT_NO_TICK("NOT SHORT\n");
			// HW2: No need to change this because
			// p isn't a SHORT process
			dequeue_task(p, rq->active);
			set_tsk_need_resched(p);
			/** START HW2 */
			UPDATE_REASON(p, SWITCH_SLICE);
			/** END HW2 */

			p->prio = effective_prio(p);
			p->first_time_slice = 0;
			p->time_slice = TASK_TIMESLICE(p);

			if (!TASK_INTERACTIVE(p) || EXPIRED_STARVING(rq)) {
				if (!rq->expired_timestamp)
					rq->expired_timestamp = jiffies;
				enqueue_task(p, rq->expired);
			} else
				enqueue_task(p, rq->active);
		/** 
		 * HW2: 
		 *
		 * Dealing with SCHED_SHORT policy.
		 * We SHOULD NOT get here with an overdue process!
		 * Overdue handling is done above, with RT handling
		 */
		}
		else if (is_overdue(p)) {
			PRINT_NO_TICK("ERROR: In scheduler_tick(), line %d, with an overdue process...\n",__LINE__);
		}
		else {
			PRINT_NO_TICK("SHORT\n");
			hw2_dequeue(p, p->array/*==rq->active_SHORT*/);	// Get the proc out from our active array
			set_tsk_need_resched(p);
			UPDATE_REASON(p, SWITCH_SLICE);

			p->first_time_slice = 0; 
			
			// First, check if the next timeslice is zero.
			// If it is, the process should become overdue
			// regardless of the number of remaining trials.
			// Note that the process may NOT already be overdue,
			// because to get in here we checked the value
			// of p->remaining_trials
			--p->remaining_trials;
			++p->current_trial;
			p->time_slice = (hw2_ms_to_ticks(p->requested_time)) / (p->current_trial);
			/**
			 * HW2 DEBUGGING:
			 *
			 * Switch between the next two blocks of code if we
			 * fix the bug
			 */
			// START DEBUG CODE:
			/**
			if (!p->remaining_trials || !p->time_slice) {	// If it became overdue
				INIT_LIST_HEAD(&p->run_list);	// If we don't do this, hw2_enqueue will think it's enqueued already
				p->remaining_trials=0;
				p->time_slice=0;
				PRINT_NO_TICK("BECAME OVERDUE: pid=%d. &runlist=%p, runlist.next=%p, runlist.prev=%p. The answer to is_queued is %s\n", p->pid, &p->run_list, p->run_list.next, p->run_list.prev, is_queued(p)? "YES" : "NO");
				UPDATE_REASON(p, SWITCH_OVERDUE);
				list_add(&p->run_list,&rq->overdue_SHORT);
				PRINT("OVERDUE LIST: HEAD=%p, NEXT=%p, NEXT->NEXT=%p\n",
							&rq->overdue_SHORT,
							rq->overdue_SHORT.next ? rq->overdue_SHORT.next : 0,
							rq->overdue_SHORT.next ? rq->overdue_SHORT.next->next : 0);
			}
			else {
				hw2_enqueue(p,rq,1);
			}
			/**/
			// END DEBUG CODE
			// START "GOOD" CODE (OR NOT)
			/**/
			if (!p->remaining_trials || !p->time_slice) {	// If it became overdue
				INIT_LIST_HEAD(&p->run_list);				// If we don't do this, hw2_enqueue will think it's enqueued already
				UPDATE_REASON(p, SWITCH_OVERDUE);
				p->remaining_trials = p->time_slice = 0;
			}
			hw2_enqueue(p,rq,1);							// Re insert into the runqueue (works for new overdue as well)
			/**/
			// END "GOOD" CODE 
		}
	}
out:
	PRINT_ONCE_IF(is_overdue(p),"LEAVING TICK WITH OVERDUE %d\n",p->pid);
#if CONFIG_SMP
	if (!(jiffies % BUSY_REBALANCE_TICK))
		load_balance(rq, 0);
#endif
	spin_unlock(&rq->lock);
}

void scheduling_functions_start_here(void) { }

/*
 * 'schedule()' is the main scheduler function.
 */
asmlinkage void schedule(void)
{
	task_t *prev, *next;
	runqueue_t *rq;
	prio_array_t *array;
	list_t *queue;
	int idx;
	
	
	if (unlikely(in_interrupt()))
		BUG();

need_resched:
	prev = current;									// Assume we're going to switch the current process to something else
	rq = this_rq();									// Get a pointer to the runqueue

	PRINT_IF(is_overdue(prev),"IN SCHEDULE WITH OVERDUE: CURRENT=%d, POLICY=%d, REASON=%d\n", prev->pid, prev->policy, prev->switch_reason);

	release_kernel_lock(prev, smp_processor_id());	// Lock something
	prepare_arch_schedule(prev);					// Just architecture things
	prev->sleep_timestamp = jiffies;				// This is to help decide if the process is interactive or calculative
	spin_lock_irq(&rq->lock);						// Lock another thing

	switch (prev->state) {							// Check what kind of process we were
	case TASK_INTERRUPTIBLE:						// If we were interruptible...
		if (unlikely(signal_pending(prev))) {		// ...and the thing we were waiting for happened...
			prev->state = TASK_RUNNING;				// ...become running
			break;
		}
	default:
		deactivate_task(prev, rq);					// If we're uninterruptible, deactivate the task
		/** START HW2 */
		UPDATE_REASON(current, SWITCH_PREV_WAIT);
		/** END HW2 */
	case TASK_RUNNING:								// Otherwise no one cares so shut up
		;
	}
#if CONFIG_SMP
pick_next_task:
#endif
	if (unlikely(!rq->nr_running)) {				// If there's nothing to run, run the IDLE task
#if CONFIG_SMP
		load_balance(rq, 1);
		if (rq->nr_running)
			goto pick_next_task;
#endif
		next = rq->idle;
		rq->expired_timestamp = 0;
		goto switch_tasks;
	}
	
	
	PRINT_IF(is_short(prev),"In SCHEDULE, Daddy SHORT (pid=%d) has %d remaining time and %d trials\n",
						prev->pid,prev->time_slice,prev->remaining_trials);

	
	array = rq->active;								// Get a pointer to the active array.
													// We may need to change thins because there are other runnable tasks
													// not in rq->active
	/**
	 * HW2:
	 *
	 * We need to add another test here because the original
	 * code assumes if rq->nr_running isn't 0 then there are
	 * processes in rq->active or rq->expired.
	 *
	 * In our case, both of those queues may be empty if there
	 * is an overdue process
	 */
	/** START HW2 */
//	if (unlikely(!array->nr_active)) { OLD CODE
	if (unlikely(!array->nr_active) && list_empty(&rq->overdue_SHORT)) {
	/** END HW2 */
		/*
		 * Switch the active and expired arrays.
		 */
		rq->active = rq->expired;
		rq->expired = array;
		array = rq->active;
		rq->expired_timestamp = 0;
	}

	idx = sched_find_first_bit(array->bitmap);
	
	/**
	 * HW2:
	 *
	 * If a SHORT task was forked, the parent must
	 * go to the end of the queue (weird, but true...)
	 *
	 * This is also true for overdue SHORTs as well,
	 * in any case (if an overdue process is swapped
	 * for a different one it needs to go to the back
	 * of the queue). However, if an overdue process
	 * got here via waiting OR exiting, it has already
	 * been kicked from the queue so no need to do
	 * anything.
	 *
	 * We need to make sure the task is in RUNNING state
	 * before we do this, otherwise we may push a dead
	 * process back to the queue...
	 *
	 * NOTE: This function call does hw2_dequeue() and
	 * hw2_enqueue()
	 */
	/** START HW2 */
	if (!hw2_debug && prev->state == TASK_RUNNING) {
		if (is_overdue(prev)) {
			PRINT_NO_TICK("PUSHING OVERDUE BACK\n");
			hw2_back_of_the_queue(prev, rq);
			PRINT_IF_NO_TICK(is_overdue(prev),"OVERDUE SCHEDULE: LINE %d, PUSHING BACK\n",__LINE__);
		}
		else if (is_short(prev) && prev->switch_reason == SWITCH_CREATED) {
			hw2_back_of_the_queue(prev, rq);
		}
	}
	/** END HW2 */
	
	/**
	 * HW2:
	 *
	 * Check if there are no real-time tasks to run.
	 * If this is the case, we need to check if there are SHORT
	 * tasks to run instead of OTHER tasks.
	 *
	 * To do this:
	 * 1. Check if there's a RT process by checking if idx<=MAX_RT_PRIO.
	 * 2. Check if there's a SHORT task to run by checking if there's any
	 *    process in active_SHORT
	 */
	/** START HW2 */
	if (!hw2_debug) {
		PRINT_IF(is_short(prev),"NON-DEBUG MODE: idx=%d, ",idx);
		int highest_prio_short = sched_find_first_bit(rq->active_SHORT->bitmap);
		PRINT_IF_NO_TICK(is_overdue(prev),"OVERDUE SCHEDULE: LINE %d\n",__LINE__);
		PRINT_IF_NO_TICK(is_short(prev),"highestshort=%d, ", highest_prio_short);
		// Are there SHORTS we should run?
		// Check if idx isn't a RT priority and that there is some SHORT to run
		if (idx > MAX_RT_PRIO-1 && highest_prio_short != MAX_PRIO) {
			PRINT_NO_TICK("running a SHORT process: ");
			// YES!!
			
			// Set the next task
			queue = rq->active_SHORT->queue + highest_prio_short;
			next = list_entry(queue->next, task_t, run_list);
			PRINT_NO_TICK("pid=%d, policy=%d\n", next->pid,next->policy);
			PRINT_IF_NO_TICK(is_short(prev) && is_short(next) && next != prev, "SWAPPING BETWEEN SHORTS: %p-->%p\n THE QUEUE: HEAD==%p-->%p-->%p\n",&prev->run_list,&next->run_list,queue,queue->next,queue->next->next);
			PRINT_IF_NO_TICK(is_overdue(prev),"OVERDUE SCHEDULE: LINE %d, THINKS WE'RE SHORT!\n",__LINE__);
			
		}
		// Otherwise, are there any RT or OTHER tasks to run?
		// If idx==MAX_PRIO then rq->active is empty, so there
		// must be only overdues to run...
		else if (idx != MAX_PRIO) {
			
			// In this case, do the default (original) scheduling choice
			queue = array->queue + idx;
			next = list_entry(queue->next, task_t, run_list);
			PRINT_IF_NO_TICK(is_short(prev),"running a RT/OTHER process: pid=%d, policy=%d\n",next->pid,next->policy);
			PRINT_IF_NO_TICK(is_overdue(prev),"OVERDUE SCHEDULE: LINE %d, THINKS WE'RE OTHER OR RT!\n",__LINE__);
		}
		// If there are no SHORT, OTHER or RT tasks then there MUST be an Overdue task
		// (because rq->nr_running != 0)
		else {
			
			// Let's assume the overdue_SHORT list isn't empty.
			// This can be easily tested by calling
			// list_empty(&rq->overdue_SHORT), if we want.
			// As the list_t is circular, to get the last element
			// just do .prev
			// Also note that if we came here via an overdue
			// process (current is an overdue process) we should
			// still run the next one (forking an overdue process
			// should cause it to switch - see Piazza)
			next = list_entry(rq->overdue_SHORT.prev, task_t, run_list);
			PRINT_IF_NO_TICK(is_short(prev),"running an overdue process: pid=%d, policy=%d\n",next->pid,next->policy);
			PRINT_IF_NO_TICK(is_overdue(prev),"OVERDUE SCHEDULE: LINE %d. NEXT OVERDUE: %p, CURRENT OVERDUE: %p. POINTERS: next=%p AND prev=%p\n",__LINE__, &next->run_list, &prev->run_list, next, prev);
			
		}
	}
	// If hw2_debug is ON, execute the original code:
	else {
		queue = array->queue + idx;
		next = list_entry(queue->next, task_t, run_list);
		PRINT_IF_NO_TICK(is_overdue(prev),"OVERDUE SCHEDULE: LINE %d, ORIGINAL CODE!\n",__LINE__);
	}
	/** END HW2 */
	
	PRINT_IF(is_short(prev) && prev==next,"In SCHEDULE, Daddy SHORT (pid=%d) has %d remaining time and %d trials and no switch\n",
						prev->pid,prev->time_slice,prev->remaining_trials);
	PRINT_IF(is_short(prev) && prev!=next,"In SCHEDULE, Daddy SHORT (pid=%d) has %d remaining time and %d trials and switching to pid=%d, ts=%d, rt=%d\n",
						prev->pid,prev->time_slice,prev->remaining_trials,next->pid,next->time_slice,next->remaining_trials);
	
switch_tasks:
	prefetch(next);
	// HW2: DO NOT clear the reason yet, we still need to log it!
	// We'll call UPDATE_REASON(prev,SWITCH_UNKNOWN) in a moment
	clear_tsk_need_resched(prev);

	if (likely(prev != next)) {						// If we've switched the task: do this.
													// We need to try to log this switch here
		rq->nr_switches++;
		rq->curr = next;
	
		PRINT_IF_NO_TICK(is_overdue(prev),"OVERDUE SCHEDULE: LINE %d, SWITCHING!\n",__LINE__);
		
		prepare_arch_switch(rq);					// Architecture shit
		/**
		 * HW2:
		 *
		 * Now's the time to log the context switch
		 */
		/** START HW2 */
		hw2_log_switch(&hw2_logger, prev, next);
		/** END HW2 */
		prev = context_switch(prev, next);			// Switch context
		barrier();
		rq = this_rq();
		finish_arch_switch(rq);
	} else
		spin_unlock_irq(&rq->lock);
	finish_arch_schedule(prev);
	
	reacquire_kernel_lock(current);
	if (need_resched())								// Why would we need this...? Interrupt for some hardware signal?
		goto need_resched;

	/**
	 * HW2:
	 *
	 * Even if no context switch happened, the bit has been cleared
	 * and if the switch will happen it will be for a different reason.
	 */
	/** START HW2 */
	UPDATE_REASON(prev,SWITCH_UNKNOWN);
	/** END HW2 */

}

/*
 * The core wakeup function.  Non-exclusive wakeups (nr_exclusive == 0) just
 * wake everything up.  If it's an exclusive wakeup (nr_exclusive == small +ve
 * number) then we wake all the non-exclusive tasks and one exclusive task.
 *
 * There are circumstances in which we can try to wake a task which has already
 * started to run but is not in state TASK_RUNNING.  try_to_wake_up() returns
 * zero in this (rare) case, and we handle it by continuing to scan the queue.
 */
static inline void __wake_up_common(wait_queue_head_t *q, unsigned int mode, int nr_exclusive, int sync)
{
	struct list_head *tmp;
	unsigned int state;
	wait_queue_t *curr;
	task_t *p;

	list_for_each(tmp, &q->task_list) {
		curr = list_entry(tmp, wait_queue_t, task_list);
		p = curr->task;
		state = p->state;
		if ((state & mode) && try_to_wake_up(p, sync) &&
			((curr->flags & WQ_FLAG_EXCLUSIVE) && !--nr_exclusive))
				break;
	}
}

void __wake_up(wait_queue_head_t *q, unsigned int mode, int nr_exclusive)
{
	unsigned long flags;

	if (unlikely(!q))
		return;

	spin_lock_irqsave(&q->lock, flags);
	__wake_up_common(q, mode, nr_exclusive, 0);
	spin_unlock_irqrestore(&q->lock, flags);
}

#if CONFIG_SMP

void __wake_up_sync(wait_queue_head_t *q, unsigned int mode, int nr_exclusive)
{
	unsigned long flags;

	if (unlikely(!q))
		return;

	spin_lock_irqsave(&q->lock, flags);
	if (likely(nr_exclusive))
		__wake_up_common(q, mode, nr_exclusive, 1);
	else
		__wake_up_common(q, mode, nr_exclusive, 0);
	spin_unlock_irqrestore(&q->lock, flags);
}

#endif
 
void complete(struct completion *x)
{
	unsigned long flags;

	spin_lock_irqsave(&x->wait.lock, flags);
	x->done++;
	__wake_up_common(&x->wait, TASK_UNINTERRUPTIBLE | TASK_INTERRUPTIBLE, 1, 0);
	spin_unlock_irqrestore(&x->wait.lock, flags);
}

void wait_for_completion(struct completion *x)
{
	spin_lock_irq(&x->wait.lock);
	if (!x->done) {
		DECLARE_WAITQUEUE(wait, current);

		wait.flags |= WQ_FLAG_EXCLUSIVE;
		__add_wait_queue_tail(&x->wait, &wait);
		do {
			__set_current_state(TASK_UNINTERRUPTIBLE);
			spin_unlock_irq(&x->wait.lock);
			schedule();
			spin_lock_irq(&x->wait.lock);
		} while (!x->done);
		__remove_wait_queue(&x->wait, &wait);
	}
	x->done--;
	spin_unlock_irq(&x->wait.lock);
}

#define	SLEEP_ON_VAR				\
	unsigned long flags;			\
	wait_queue_t wait;			\
	init_waitqueue_entry(&wait, current);

#define	SLEEP_ON_HEAD					\
	spin_lock_irqsave(&q->lock,flags);		\
	__add_wait_queue(q, &wait);			\
	spin_unlock(&q->lock);

#define	SLEEP_ON_TAIL						\
	spin_lock_irq(&q->lock);				\
	__remove_wait_queue(q, &wait);				\
	spin_unlock_irqrestore(&q->lock, flags);

void interruptible_sleep_on(wait_queue_head_t *q)
{
	SLEEP_ON_VAR

	current->state = TASK_INTERRUPTIBLE;

	SLEEP_ON_HEAD
	schedule();
	SLEEP_ON_TAIL
}

long interruptible_sleep_on_timeout(wait_queue_head_t *q, long timeout)
{
	SLEEP_ON_VAR

	current->state = TASK_INTERRUPTIBLE;

	SLEEP_ON_HEAD
	timeout = schedule_timeout(timeout);
	SLEEP_ON_TAIL

	return timeout;
}

void sleep_on(wait_queue_head_t *q)
{
	SLEEP_ON_VAR
	
	current->state = TASK_UNINTERRUPTIBLE;

	SLEEP_ON_HEAD
	schedule();
	SLEEP_ON_TAIL
}

long sleep_on_timeout(wait_queue_head_t *q, long timeout)
{
	SLEEP_ON_VAR
	
	current->state = TASK_UNINTERRUPTIBLE;

	SLEEP_ON_HEAD
	timeout = schedule_timeout(timeout);
	SLEEP_ON_TAIL

	return timeout;
}

void scheduling_functions_end_here(void) { }

void set_user_nice(task_t *p, long nice)
{
	unsigned long flags;
	prio_array_t *array;
	runqueue_t *rq;

	if (TASK_NICE(p) == nice || nice < -20 || nice > 19)
		return;
	
	/**
	 * HW2:
	 *
	 * If the process is overdue, it's NICE doesn't matter.
	 * If we keep running this function, there will be bugs
	 * because an overdue process doesn't have a valid p->array
	 * value.
	 */
	/** START HW2 */
	if (is_overdue(p))
		return;
	/** END HW2 */
	
	/*
	 * We have to be careful, if called from sys_setpriority(),
	 * the task might be in the middle of scheduling on another CPU.
	 */
	rq = task_rq_lock(p, &flags);
	if (rt_task(p)) {
		p->static_prio = NICE_TO_PRIO(nice);
		goto out_unlock;
	}
	array = p->array;
	if (array)
		hw2_dequeue(p, array);	/** HW2: This is OK for shorts as well */
	p->static_prio = NICE_TO_PRIO(nice);
	p->prio = NICE_TO_PRIO(nice);
	if (array) {
		enqueue_task(p, array);	/** HW2: This is OK for shorts as well */
		/*
		 * If the task is running and lowered its priority,
		 * or increased its priority then reschedule its CPU:
		 */
		if ((NICE_TO_PRIO(nice) < p->static_prio) || (p == rq->curr)/** START HW2 */ || (!hw2_debug && should_switch(rq->curr, p))/** END HW2 */) {
			resched_task(rq->curr);
			/**
			 * If the current process is overdue we need to
			 * push it to the back...
			 */
			/** START HW2 */
			UPDATE_REASON(rq->curr, SWITCH_PRIO);
			OVERDUE_PUSH_BACK(rq->curr, rq);
			/** END HW2 */
		}
	}
out_unlock:
	task_rq_unlock(rq, &flags);
}

#ifndef __alpha__

/*
 * This has been replaced by sys_setpriority.  Maybe it should be
 * moved into the arch dependent tree for those ports that require
 * it for backward compatibility?
 */

asmlinkage long sys_nice(int increment)
{
	long nice;

	/*
	 *	Setpriority might change our priority at the same moment.
	 *	We don't have to worry. Conceptually one call occurs first
	 *	and we have a single winner.
	 */
	if (increment < 0) {
		if (!capable(CAP_SYS_NICE))
			return -EPERM;
		if (increment < -40)
			increment = -40;
	}
	if (increment > 40)
		increment = 40;

	nice = PRIO_TO_NICE(current->static_prio) + increment;
	if (nice < -20)
		nice = -20;
	if (nice > 19)
		nice = 19;
	set_user_nice(current, nice);
	return 0;
}

#endif

/*
 * This is the priority value as seen by users in /proc
 *
 * RT tasks are offset by -200. Normal tasks are centered
 * around 0, value goes from -16 to +15.
 */
int task_prio(task_t *p)
{
	return p->prio - MAX_USER_RT_PRIO;
}

int task_nice(task_t *p)
{
	return TASK_NICE(p);
}

int idle_cpu(int cpu)
{
	return cpu_curr(cpu) == cpu_rq(cpu)->idle;
}

static inline task_t *find_process_by_pid(pid_t pid)
{
	return pid ? find_task_by_pid(pid) : current;
}

static int setscheduler(pid_t pid, int policy, struct sched_param *param)
{
	struct sched_param lp;
	int retval = -EINVAL;
	prio_array_t *array;
	unsigned long flags;
	runqueue_t *rq;
	task_t *p;
	
	PRINT("IN SETSCHEDULER WITH POLICY == %d FOR PID=%d: ",policy,pid);
	PRINT_IF_NO_TICK(!param,"NULL PARAM STRUCT!\n");
	
	if (!param || pid < 0)
		goto out_nounlock;

	retval = -EFAULT;
	if (copy_from_user(&lp, param, sizeof(struct sched_param)))
		goto out_nounlock;

	PRINT_NO_TICK("REQ=%d, TRIALS=%d, PRIO=%d, ", lp.requested_time, lp.trial_num, lp.sched_priority);
	
	/*
	 * We play safe to avoid deadlocks.
	 */
	read_lock_irq(&tasklist_lock);

	p = find_process_by_pid(pid);

	retval = -ESRCH;
	if (!p)
		goto out_unlock_tasklist;

	
	/*
	 * To be able to change p->policy safely, the apropriate
	 * runqueue lock must be held.
	 */
	rq = task_rq_lock(p, &flags);
	
	/**
	 * HW2:
	 *
	 * We should allow calls to setscheduler if we promise not
	 * to change the policy, even if the current policy is
	 * SHORT.
	 *
	 * Keep a variable telling us if the scheduler promises
	 * not to change policies
	 */
	/** START HW2*/
	int no_change = 0;
	/** END HW2 */
	if (policy < 0) {
		policy = p->policy;
		/** START HW2 */
		no_change = 1;
		/** END HW2 */
	}
	else {
		retval = -EINVAL;
		/**
		 * HW2:
		 *
		 * Add check for SCHED_SHORT as well
		 */
		if (policy != SCHED_FIFO && policy != SCHED_RR &&
				policy != SCHED_OTHER/** START HW2 */&& policy != SCHED_SHORT/** END HW2 */)
			goto out_unlock;
	}

	/*
	 * Valid priorities for SCHED_FIFO and SCHED_RR are
	 * 1..MAX_USER_RT_PRIO-1, valid priority for SCHED_OTHER is 0.
	 */
	retval = -EINVAL;
	if (lp.sched_priority < 0 || lp.sched_priority > MAX_USER_RT_PRIO-1)
		goto out_unlock;
	if ((policy == SCHED_OTHER/** START HW2 */ || policy == SCHED_SHORT/** END HW2 */) != (lp.sched_priority == 0))
		goto out_unlock;
	/**
	 * HW2:
	 *
	 * If we're changing to SCHED_SHORT, make sure the values
	 * are legal
	 */
	/** START HW2 */
	if (policy == SCHED_SHORT && (lp.requested_time > 5000 || lp.requested_time < 1 || lp.trial_num > 50 || lp.trial_num < 1)) {
		PRINT_NO_TICK("TRIED TO CREATE SHORT AND FAILED: REQTIME=%d, TRIALS=%d",lp.requested_time,lp.trial_num);
		goto out_unlock;
	}
	/** END HW2 */

	retval = -EPERM;
	if ((policy == SCHED_FIFO || policy == SCHED_RR) &&
	    !capable(CAP_SYS_NICE))
		goto out_unlock;
	if ((current->euid != p->euid) && (current->euid != p->uid) &&
	    !capable(CAP_SYS_NICE))
		goto out_unlock;
	/**
	 * HW2:
	 *
	 * Some new things we have to test for and maybe return EPERM
	 * (operation not permitted): 
	 * - If the old policy is SCHED_SHORT, this is an illegal call
	 * - If the old policy isn't OTHER but the new policy is SHORT,
	 *   then this is an illegal call
	 *
	 * Note that if the caller promises not to change policies,
	 * this should be a legal call! This is because setscheduler()
	 * is also called by setparam() (which should be a legal call,
	 * even for SHORT processes), and setparam can promise setscheduler
	 * not to change policies
	 */
	/** START HW2 */
	if (p->policy == SCHED_SHORT && !no_change) {
		PRINT_NO_TICK("TRIED TO CHANGE SHORT SHORT TO SHORT: PID=%d",p->pid);
		goto out_unlock;
	}
	if (p->policy != SCHED_OTHER && policy == SCHED_SHORT && !no_change) {
		PRINT_NO_TICK("TRIED TO CHANGE NON-OTHER TO SHORT: PID=%d",p->pid);
		goto out_unlock;
	}
	/** END HW2 */

	array = p->array;
	if (array)
		deactivate_task(p, task_rq(p));
	retval = 0;
	
	/**
	 * HW2:
	 *
	 * Update requested_time and trial_num
	 */
	/** START HW2 */
	if (policy == SCHED_SHORT) {
		
		p->requested_time = lp.requested_time;
		
		// Only set this if the old policy wasn't SHORT
		if (p->policy != SCHED_SHORT) {
		
			p->trial_num = p->remaining_trials = lp.trial_num;
			p->current_trial = 1;
			
			// Requested time is also the first time slice
			p->time_slice = (hw2_ms_to_ticks(lp.requested_time) == 0) ? 1 : hw2_ms_to_ticks(lp.requested_time);
			
			// Assaf said that we should resched the process
			// after becoming SHORT, that it should happen in
			// this function but doesn't (it does in later
			// versions of the kernel). Question 146 in Piazza
			current->need_resched = 1;
			UPDATE_REASON(current, SWITCH_PRIO);
		}
	}
	/** END HW2 */
	
	p->policy = policy;
	if (policy == SCHED_SHORT)
		PRINT_NO_TICK("POLICY CHANGED TO SHORT FOR PID=%d", p->pid);
	p->rt_priority = lp.sched_priority;
	/** 
	 * HW2:
	 *
	 * We want a prioritizing system similar to SCHED_OTHER's
	 * priority system
	 */
	if (policy != SCHED_OTHER/** START HW2 */ && policy != SCHED_SHORT /** END HW2 */)
		p->prio = MAX_USER_RT_PRIO-1 - p->rt_priority;
	else
		p->prio = p->static_prio;
	if (array)
		activate_task(p, task_rq(p));
	
	PRINT_NO_TICK("\n");
	
out_unlock:
	task_rq_unlock(rq, &flags);
out_unlock_tasklist:
	read_unlock_irq(&tasklist_lock);

out_nounlock:
	return retval;
}

asmlinkage long sys_sched_setscheduler(pid_t pid, int policy,
				      struct sched_param *param)
{
	return setscheduler(pid, policy, param);
}

asmlinkage long sys_sched_setparam(pid_t pid, struct sched_param *param)
{
	return setscheduler(pid, -1, param);
}

asmlinkage long sys_sched_getscheduler(pid_t pid)
{
	int retval = -EINVAL;
	task_t *p;

	if (pid < 0)
		goto out_nounlock;

	retval = -ESRCH;
	read_lock(&tasklist_lock);
	p = find_process_by_pid(pid);
	if (p)
		retval = p->policy;
	read_unlock(&tasklist_lock);

out_nounlock:
	return retval;
}

asmlinkage long sys_sched_getparam(pid_t pid, struct sched_param *param)
{
	struct sched_param lp;
	int retval = -EINVAL;
	task_t *p;

	if (!param || pid < 0)
		goto out_nounlock;

	read_lock(&tasklist_lock);
	p = find_process_by_pid(pid);
	retval = -ESRCH;
	if (!p)
		goto out_unlock;
	lp.sched_priority = p->rt_priority;
	/** START HW2 */
	lp.trial_num = p->trial_num;
	lp.requested_time = p->requested_time;
	/** END HW2 */
	read_unlock(&tasklist_lock);

	/*
	 * This one might sleep, we cannot do it with a spinlock held ...
	 */
	retval = copy_to_user(param, &lp, sizeof(*param)) ? -EFAULT : 0;

out_nounlock:
	return retval;

out_unlock:
	read_unlock(&tasklist_lock);
	return retval;
}

/**
 * sys_sched_setaffinity - set the cpu affinity of a process
 * @pid: pid of the process
 * @len: length in bytes of the bitmask pointed to by user_mask_ptr
 * @user_mask_ptr: user-space pointer to the new cpu mask
 */
asmlinkage int sys_sched_setaffinity(pid_t pid, unsigned int len,
				      unsigned long *user_mask_ptr)
{
	unsigned long new_mask;
	int retval;
	task_t *p;

	if (len < sizeof(new_mask))
		return -EINVAL;

	if (copy_from_user(&new_mask, user_mask_ptr, sizeof(new_mask)))
		return -EFAULT;

	new_mask &= cpu_online_map;
	if (!new_mask)
		return -EINVAL;

	read_lock(&tasklist_lock);

	p = find_process_by_pid(pid);
	if (!p) {
		read_unlock(&tasklist_lock);
		return -ESRCH;
	}

	/*
	 * It is not safe to call set_cpus_allowed with the
	 * tasklist_lock held.  We will bump the task_struct's
	 * usage count and then drop tasklist_lock.
	 */
	get_task_struct(p);
	read_unlock(&tasklist_lock);

	if (!capable(CAP_SYS_NICE))
		new_mask &= p->cpus_allowed_mask;
	if (capable(CAP_SYS_NICE))
		p->cpus_allowed_mask |= new_mask;
	if (!new_mask) {
		retval = -EINVAL;
		goto out_unlock;
	}

	retval = -EPERM;
	if ((current->euid != p->euid) && (current->euid != p->uid) &&
			!capable(CAP_SYS_NICE))
		goto out_unlock;

	retval = 0;
	set_cpus_allowed(p, new_mask);

out_unlock:
	free_task_struct(p);
	return retval;
}

/**
 * sys_sched_getaffinity - get the cpu affinity of a process
 * @pid: pid of the process
 * @len: length in bytes of the bitmask pointed to by user_mask_ptr
 * @user_mask_ptr: user-space pointer to hold the current cpu mask
 */
asmlinkage int sys_sched_getaffinity(pid_t pid, unsigned int len,
				      unsigned long *user_mask_ptr)
{
	unsigned int real_len;
	unsigned long mask;
	int retval;
	task_t *p;

	real_len = sizeof(mask);
	if (len < real_len)
		return -EINVAL;

	read_lock(&tasklist_lock);

	retval = -ESRCH;
	p = find_process_by_pid(pid);
	if (!p)
		goto out_unlock;

	retval = 0;
	mask = p->cpus_allowed & cpu_online_map;

out_unlock:
	read_unlock(&tasklist_lock);
	if (retval)
		return retval;
	if (copy_to_user(user_mask_ptr, &mask, real_len))
		return -EFAULT;
	return real_len;
}

asmlinkage long sys_sched_yield(void)
{
	
	/**
	 * HW2:
	 *
	 * No need to do anything here: we can assume
	 * a SHORT process never yields (even an overdue
	 * SHORT process).
	 *
	 * However, we do need to log the switch reason
	 * (this happens bellow)
	 */
	if (is_short(current)) {
		PRINT("BUG: SHORT PROCESS YIELDING! SCHED.C, LINE %d\n",__LINE__);
		while(1);
	}
	
	runqueue_t *rq = this_rq_lock();
	prio_array_t *array = current->array;
	int i;

	if (unlikely(rt_task(current))) {
		list_del(&current->run_list);
		list_add_tail(&current->run_list, array->queue + current->prio);
		goto out_unlock;
	}
	
	list_del(&current->run_list);
	if (!list_empty(array->queue + current->prio)) {
		list_add(&current->run_list, array->queue[current->prio].next);
		goto out_unlock;
	}
	__clear_bit(current->prio, array->bitmap);

	i = sched_find_first_bit(array->bitmap);

	if (i == MAX_PRIO || i <= current->prio)
		i = current->prio;
	else
		current->prio = i;

	list_add(&current->run_list, array->queue[i].next);
	__set_bit(i, array->bitmap);
	
out_unlock:
	spin_unlock(&rq->lock);
	
	/** START HW2 */
	UPDATE_REASON(current, SWITCH_YIELD);
	/** END HW2 */
	schedule();

	return 0;
}


asmlinkage long sys_sched_get_priority_max(int policy)
{
	int ret = -EINVAL;

	switch (policy) {
	case SCHED_FIFO:
	case SCHED_RR:
		ret = MAX_USER_RT_PRIO-1;
		break;
	case SCHED_OTHER:
	/** START HW2 */
	case SCHED_SHORT:
	/** END HW2 */
		ret = 0;
		break;
	}
	return ret;
}

asmlinkage long sys_sched_get_priority_min(int policy)
{
	int ret = -EINVAL;

	switch (policy) {
	case SCHED_FIFO:
	case SCHED_RR:
		ret = 1;
		break;
	case SCHED_OTHER:
	/** START HW2 */
	case SCHED_SHORT:
	/** END HW2 */
		ret = 0;
	}
	return ret;
}

asmlinkage long sys_sched_rr_get_interval(pid_t pid, struct timespec *interval)
{
	int retval = -EINVAL;
	struct timespec t;
	task_t *p;

	if (pid < 0)
		goto out_nounlock;

	retval = -ESRCH;
	read_lock(&tasklist_lock);
	p = find_process_by_pid(pid);
	if (p)
		jiffies_to_timespec(p->policy & SCHED_FIFO ?
					 0 : TASK_TIMESLICE(p), &t);
	read_unlock(&tasklist_lock);
	if (p)
		retval = copy_to_user(interval, &t, sizeof(t)) ? -EFAULT : 0;
out_nounlock:
	return retval;
}

static void show_task(task_t * p)
{
	unsigned long free = 0;
	int state;
	static const char * stat_nam[] = { "R", "S", "D", "Z", "T", "W" };

	printk("%-13.13s ", p->comm);
	state = p->state ? __ffs(p->state) + 1 : 0;
	if (((unsigned) state) < sizeof(stat_nam)/sizeof(char *))
		printk(stat_nam[state]);
	else
		printk(" ");
#if (BITS_PER_LONG == 32)
	if (p == current)
		printk(" current  ");
	else
		printk(" %08lX ", thread_saved_pc(&p->thread));
#else
	if (p == current)
		printk("   current task   ");
	else
		printk(" %016lx ", thread_saved_pc(&p->thread));
#endif
	{
		unsigned long * n = (unsigned long *) (p+1);
		while (!*n)
			n++;
		free = (unsigned long) n - (unsigned long)(p+1);
	}
	printk("%5lu %5d %6d ", free, p->pid, p->p_pptr->pid);
	if (p->p_cptr)
		printk("%5d ", p->p_cptr->pid);
	else
		printk("      ");
	if (p->p_ysptr)
		printk("%7d", p->p_ysptr->pid);
	else
		printk("       ");
	if (p->p_osptr)
		printk(" %5d", p->p_osptr->pid);
	else
		printk("      ");
	if (!p->mm)
		printk(" (L-TLB)\n");
	else
		printk(" (NOTLB)\n");

	{
		extern void show_trace_task(task_t *tsk);
		show_trace_task(p);
	}
}

char * render_sigset_t(sigset_t *set, char *buffer)
{
	int i = _NSIG, x;
	do {
		i -= 4, x = 0;
		if (sigismember(set, i+1)) x |= 1;
		if (sigismember(set, i+2)) x |= 2;
		if (sigismember(set, i+3)) x |= 4;
		if (sigismember(set, i+4)) x |= 8;
		*buffer++ = (x < 10 ? '0' : 'a' - 10) + x;
	} while (i >= 4);
	*buffer = 0;
	return buffer;
}

void show_state(void)
{
	task_t *p;

#if (BITS_PER_LONG == 32)
	printk("\n"
	       "                         free                        sibling\n");
	printk("  task             PC    stack   pid father child younger older\n");
#else
	printk("\n"
	       "                                 free                        sibling\n");
	printk("  task                 PC        stack   pid father child younger older\n");
#endif
	read_lock(&tasklist_lock);
	for_each_task(p) {
		/*
		 * reset the NMI-timeout, listing all files on a slow
		 * console might take alot of time:
		 */
		touch_nmi_watchdog();
		show_task(p);
	}
	read_unlock(&tasklist_lock);
}

/*
 * double_rq_lock - safely lock two runqueues
 *
 * Note this does not disable interrupts like task_rq_lock,
 * you need to do so manually before calling.
 */
static inline void double_rq_lock(runqueue_t *rq1, runqueue_t *rq2)
{
	if (rq1 == rq2)
		spin_lock(&rq1->lock);
	else {
		if (rq1 < rq2) {
			spin_lock(&rq1->lock);
			spin_lock(&rq2->lock);
		} else {
			spin_lock(&rq2->lock);
			spin_lock(&rq1->lock);
		}
	}
}

/*
 * double_rq_unlock - safely unlock two runqueues
 *
 * Note this does not restore interrupts like task_rq_unlock,
 * you need to do so manually after calling.
 */
static inline void double_rq_unlock(runqueue_t *rq1, runqueue_t *rq2)
{
	spin_unlock(&rq1->lock);
	if (rq1 != rq2)
		spin_unlock(&rq2->lock);
}

void __init init_idle(task_t *idle, int cpu)
{
	runqueue_t *idle_rq = cpu_rq(cpu), *rq = cpu_rq(idle->cpu);
	unsigned long flags;

	__save_flags(flags);
	__cli();
	double_rq_lock(idle_rq, rq);

	idle_rq->curr = idle_rq->idle = idle;
	deactivate_task(idle, rq);
	idle->array = NULL;
	idle->prio = MAX_PRIO;
	idle->state = TASK_RUNNING;
	idle->cpu = cpu;
	double_rq_unlock(idle_rq, rq);
	// HW2: Don't need to log this, no way a process
	// was forked / exited before this...
	set_tsk_need_resched(idle);
	__restore_flags(flags);
}

extern void init_timervecs(void);
extern void timer_bh(void);
extern void tqueue_bh(void);
extern void immediate_bh(void);

void __init sched_init(void)
{
	runqueue_t *rq;
	int i, j, k;

	for (i = 0; i < NR_CPUS; i++) {
		prio_array_t *array;

		rq = cpu_rq(i);
		rq->active = rq->arrays;
		rq->expired = rq->arrays + 1;
		/**
		 * HW2:
		 *
		 * Initialize the new array & list
		 */
		/** START HW2 */
		rq->active_SHORT = rq->arrays + 2;
		INIT_LIST_HEAD(&rq->overdue_SHORT);
		/** END HW2 */
		spin_lock_init(&rq->lock);
		INIT_LIST_HEAD(&rq->migration_queue);

		for (j = 0; j < /*2*/3; j++) {	/** HW2: from 0 to 3, not 2 */
			array = rq->arrays + j;
			for (k = 0; k < MAX_PRIO; k++) {
				INIT_LIST_HEAD(array->queue + k);
				__clear_bit(k, array->bitmap);
			}
			// delimiter for bitsearch
			__set_bit(MAX_PRIO, array->bitmap);
		}
	}
	/*
	 * We have to do a little magic to get the first
	 * process right in SMP mode.
	 */
	rq = this_rq();
	rq->curr = current;
	rq->idle = current;
	wake_up_process(current);

	init_timervecs();
	init_bh(TIMER_BH, timer_bh);
	init_bh(TQUEUE_BH, tqueue_bh);
	init_bh(IMMEDIATE_BH, immediate_bh);

	/*
	 * The boot idle thread does lazy MMU switching as well:
	 */
	atomic_inc(&init_mm.mm_count);
	enter_lazy_tlb(&init_mm, current, smp_processor_id());
}

#if CONFIG_SMP

/*
 * This is how migration works:
 *
 * 1) we queue a migration_req_t structure in the source CPU's
 *    runqueue and wake up that CPU's migration thread.
 * 2) we down() the locked semaphore => thread blocks.
 * 3) migration thread wakes up (implicitly it forces the migrated
 *    thread off the CPU)
 * 4) it gets the migration request and checks whether the migrated
 *    task is still in the wrong runqueue.
 * 5) if it's in the wrong runqueue then the migration thread removes
 *    it and puts it into the right queue.
 * 6) migration thread up()s the semaphore.
 * 7) we wake up and the migration is done.
 */

typedef struct {
	list_t list;
	task_t *task;
	struct semaphore sem;
} migration_req_t;

/*
 * Change a given task's CPU affinity. Migrate the process to a
 * proper CPU and schedule it away if the CPU it's executing on
 * is removed from the allowed bitmask.
 *
 * NOTE: the caller must have a valid reference to the task, the
 * task must not exit() & deallocate itself prematurely.  The
 * call is not atomic; no spinlocks may be held.
 */
void set_cpus_allowed(task_t *p, unsigned long new_mask)
{
	unsigned long flags;
	migration_req_t req;
	runqueue_t *rq;

	new_mask &= cpu_online_map;
	if (!new_mask)
		BUG();

	rq = task_rq_lock(p, &flags);
	p->cpus_allowed = new_mask;
	/*
	 * Can the task run on the task's current CPU? If not then
	 * migrate the process off to a proper CPU.
	 */
	if (new_mask & (1UL << p->cpu)) {
		task_rq_unlock(rq, &flags);
		goto out;
	}
	/*
	 * If the task is not on a runqueue (and not running), then
	 * it is sufficient to simply update the task's cpu field.
	 */
	if (!p->array && (p != rq->curr)) {
		p->cpu = __ffs(p->cpus_allowed);
		task_rq_unlock(rq, &flags);
		PRINT_IF(is_overdue(p), "POTENTIAL BUG: WE SHOULDN'T BE HERE (SCHED.C, LINE %d) WITH AN OVERDUE PROCESS\n",__LINE__);
		goto out;
	}
	init_MUTEX_LOCKED(&req.sem);
	req.task = p;
	list_add(&req.list, &rq->migration_queue);
	task_rq_unlock(rq, &flags);
	wake_up_process(rq->migration_thread);

	down(&req.sem);
out:
}

static int migration_thread(void * bind_cpu)
{
	struct sched_param param = { sched_priority: MAX_RT_PRIO-1 };
	int cpu = cpu_logical_map((int) (long) bind_cpu);
	runqueue_t *rq;
	int ret;

	daemonize();
	sigfillset(&current->blocked);
	set_fs(KERNEL_DS);
	/*
	 * The first migration thread is started on CPU #0. This one can
	 * migrate the other migration threads to their destination CPUs.
	 */
	if (cpu != 0) {
		while (!cpu_rq(cpu_logical_map(0))->migration_thread)
			yield();
		set_cpus_allowed(current, 1UL << cpu);
	}
	printk("migration_task %d on cpu=%d\n", cpu, smp_processor_id());
	ret = setscheduler(0, SCHED_FIFO, &param);

	rq = this_rq();
	rq->migration_thread = current;

	sprintf(current->comm, "migration_CPU%d", smp_processor_id());

	for (;;) {
		runqueue_t *rq_src, *rq_dest;
		struct list_head *head;
		int cpu_src, cpu_dest;
		migration_req_t *req;
		unsigned long flags;
		task_t *p;

		spin_lock_irqsave(&rq->lock, flags);
		head = &rq->migration_queue;
		current->state = TASK_INTERRUPTIBLE;
		if (list_empty(head)) {
			spin_unlock_irqrestore(&rq->lock, flags);
			schedule();
			continue;
		}
		req = list_entry(head->next, migration_req_t, list);
		list_del_init(head->next);
		spin_unlock_irqrestore(&rq->lock, flags);

		p = req->task;
		cpu_dest = __ffs(p->cpus_allowed);
		rq_dest = cpu_rq(cpu_dest);
repeat:
		cpu_src = p->cpu;
		rq_src = cpu_rq(cpu_src);

		local_irq_save(flags);
		double_rq_lock(rq_src, rq_dest);
		if (p->cpu != cpu_src) {
			double_rq_unlock(rq_src, rq_dest);
			local_irq_restore(flags);
			goto repeat;
		}
		if (rq_src == rq) {
			p->cpu = cpu_dest;
			if (p->array) {
				deactivate_task(p, rq_src);
				activate_task(p, rq_dest);
			}
		}
		double_rq_unlock(rq_src, rq_dest);
		local_irq_restore(flags);

		up(&req->sem);
	}
}

void __init migration_init(void)
{
	int cpu;

	current->cpus_allowed = 1UL << cpu_logical_map(0);
	for (cpu = 0; cpu < smp_num_cpus; cpu++)
		if (kernel_thread(migration_thread, (void *) (long) cpu,
				CLONE_FS | CLONE_FILES | CLONE_SIGNAL) < 0)
			BUG();
	current->cpus_allowed = -1L;

	for (cpu = 0; cpu < smp_num_cpus; cpu++)
		while (!cpu_rq(cpu_logical_map(cpu))->migration_thread)
			schedule_timeout(2);
}
#endif

#if LOWLATENCY_NEEDED
#if LOWLATENCY_DEBUG

static struct lolat_stats_t *lolat_stats_head;
static spinlock_t lolat_stats_lock = SPIN_LOCK_UNLOCKED;

void set_running_and_schedule(struct lolat_stats_t *stats)
{
	spin_lock(&lolat_stats_lock);
	if (stats->visited == 0) {
		stats->visited = 1;
		stats->next = lolat_stats_head;
		lolat_stats_head = stats;
	}
	stats->count++;
	spin_unlock(&lolat_stats_lock);

	if (current->state != TASK_RUNNING)
		set_current_state(TASK_RUNNING);
	schedule();
}

void show_lolat_stats(void)
{
	struct lolat_stats_t *stats = lolat_stats_head;

	printk("Low latency scheduling stats:\n");
	while (stats) {
		printk("%s:%d: %lu\n", stats->file, stats->line, stats->count);
		stats->count = 0;
		stats = stats->next;
	}
}

#else	/* LOWLATENCY_DEBUG */

void set_running_and_schedule()
{
	if (current->state != TASK_RUNNING)
		__set_current_state(TASK_RUNNING);
	schedule();
}

#endif	/* LOWLATENCY_DEBUG */

int ll_copy_to_user(void *to_user, const void *from, unsigned long len)
{
	while (len) {
		unsigned long n_to_copy = len;
		unsigned long remainder;

		if (n_to_copy > 4096)
			n_to_copy = 4096;
		remainder = copy_to_user(to_user, from, n_to_copy);
		if (remainder)
			return remainder + len;
		to_user = ((char *)to_user) + n_to_copy;
		from = ((char *)from) + n_to_copy;
		len -= n_to_copy;
		conditional_schedule();
	}
	return 0;
}

int ll_copy_from_user(void *to, const void *from_user, unsigned long len)
{
	while (len) {
		unsigned long n_to_copy = len;
		unsigned long remainder;

		if (n_to_copy > 4096)
			n_to_copy = 4096;
		remainder = copy_from_user(to, from_user, n_to_copy);
		if (remainder)
			return remainder + len;
		to = ((char *)to) + n_to_copy;
		from_user = ((char *)from_user) + n_to_copy;
		len -= n_to_copy;
		conditional_schedule();
	}
	return 0;
}

#ifdef CONFIG_LOLAT_SYSCTL
struct low_latency_enable_struct __enable_lowlatency = { 0, };
#endif

#endif	/* LOWLATENCY_NEEDED */

