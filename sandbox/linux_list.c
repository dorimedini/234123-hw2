/**
 * Sandbox for playing with list_t
 */

#include <linux/list.h>
#include <stdio.h>

/** 
 * Test the following:
 *
 * 1. Make sure we can successfully create an empty list,
 *    and we cannot accidentally read a non-existing item
 * 2. Make sure we can enqueue things at the head of the list
 * 3. Make sure we can remove something from the end of the list
 *    (what do we need to perform such a thing? a pointer?)
 * 4. Make sure we can keep a pointer to the last list element,
 *    read from it, update it if the last element is removed
 *    (make sure that if the list is empty, the pointer is
 *    invalid)
 */
int main() {

	list_t list;
	
	
	
	return 0;
}