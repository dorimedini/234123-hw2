#include "test_utils.h"
#include <stdio.h>


/* This tester should check if A SHORT Proc is working fine. we will check all the sys_calls we wrote.
 * In this tester the main proc will become short, so this is the reason why we use a different Tester
 */


bool non_short_test () {
	int pid = get_pid();

	// first check the functions are fine for non-SHORT proc
 	TEST_EQUALS(is_short(pid), -1);
 	TEST_EQUALS(errno, EINVAL);
 	TEST_EQUALS(is_short(-1), -1);
 	TEST_EQUALS(remaining_time(pid), -1);
 	TEST_EQUALS(errno, EINVAL);
 	TEST_EQUALS(remaining_time(-1), -1);
 	TEST_EQUALS(remaining_trails(pid), -1);
 	TEST_EQUALS(errno, EINVAL);
 	TEST_EQUALS(remaining_trials(-1), -1);
	return true;
}

bool short_test() {
	int pid = get_pid();

	TEST_TRUE(is_short(pid))
	TEST_TRUE(remaining_time(pid));
	TEST_TRUE(remaining_trials(pid));
	return true;
}


bool overdue_test() {
	int pid = get_pid();

	TEST_FALSE(is_short(pid))
	TEST_FALSE(remaining_time(pid));
	TEST_FALSE(remaining_trials(pid));
	return true;
}

int main() {
 	RUN_TEST(non_short_test());
 	
 	CHANGE_TO_SHORT(pid, 5000, 50);			// Be a very good short :)
	RUN_TEST(short_test());

	while (remaining_trials) {}				// Become overdue

	RUN_TEST(overdue_test());

 }
