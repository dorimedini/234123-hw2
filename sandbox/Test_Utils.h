/*
 * Test_Utils.h
 *
 *  Created on: Apr 30, 2015
 *      Author: shimonaz
 */

#ifndef TEST_UTILS_H_
#define TEST_UTILS_H_


#include <stdio.h>

#define TEST_EQUALS(a, b) if (((a) != (b))) { \
								return 0; \
							}

#define TEST_DIFFERENT(result, a, b) if ((result) && ((a) == (b))) { \
								result = false; \
							}

#define TEST_TRUE(result, bool) if ((result) && !(bool)) { \
								result = false; \
							}

#define TEST_FALSE(result, bool) if ((result) && (bool)) { \
								result = false; \
							}

#define RUN_TEST(name)  printf("Running "); \
						printf(#name);		\
						printf("... ");		\
						if (!name()) { \
							printf("[FAILED]\n");		\
							return -1; \
						}								\
						printf("[SUCCESS]\n");


#define SWITCH(proc1,proc2) if ((should_switch((proc1),(proc2)) != 1)) { \
									return 0; \
								}


#define NO_SWITCH(proc1,proc2) if ((should_switch((proc1),(proc2)) != 0)) { \
									return 0; \
								}

#endif /* TEST_UTILS_H_ */
